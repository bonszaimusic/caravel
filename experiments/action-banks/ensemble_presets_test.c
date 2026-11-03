/* SPDX-License-Identifier: GPL-3.0-only */
/* ENSEMBLE factory presets, measured on the real firmware in the host harness: every preset sounds (level, peak, DC), the names are unique and fit the UI,
 * the presets are really different from each other (a feature vector of spectral centroid, four energy bands, level movement depth and rate, attack, and a
 * minimum distance between every pair), and the two-track layer recipes (ENSEMBLE with SAMPLE piano, FM6 bells, DRONE, and a Pair) sound, stay inside the
 * voice budget and the level. The numbers are measures of the signal, not a judgement of how it sounds. PRINT=1 prints the feature table. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN ensemble_presets_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <math.h>
#ifndef ENS_PRESET_MIN_DISTANCE
#define ENS_PRESET_MIN_DISTANCE 2.5              /* the weakest pair measured 3.2 in 0.10.0; with the slow tap LFOs spread (0.10.1) the common waver, which had kept presets of different RATE / ENS apart, is gone and it measures 2.65 (two chords, see dist) */
#endif
#define HOLD 10u                               /* seconds the chord is held */
#define SECS 15u                               /* the take: 10 s held, 5 s of release */
#define NS (SECS * 44100u)
static float L[NS], R[NS];
static void render(uint32_t ms, uint32_t at)           /* ms of audio from sample index `at` */
{
    while (ms--) {
        web_render(44);
        for (uint32_t k = 0; k < 44u; k++, at++) {
            CHECK(isfinite(out_l[k]) && isfinite(out_r[k]) && fabsf(out_l[k]) <= 1.0f && fabsf(out_r[k]) <= 1.0f);
            if (at < NS) { L[at] = out_l[k]; R[at] = out_r[k]; }
        }
    }
}
static uint32_t voices_on(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active; return n; }
/* the two reference chords: low (E2 B2 E3 G3) and high (E3 G3 B3 E4) */
static const uint8_t CHORD[2][4] = {{40, 47, 52, 55}, {52, 55, 59, 64}};
static void chord(uint32_t tr, const uint8_t *n, int on) { for (uint32_t i = 0; i < 4u; i++) { if (on) trk_note_on(&trk[tr], n[i], 100); else trk_note_off(&trk[tr], n[i]); } }
static void silence_all(void)
{
    web_pair_panic(); run_ms(300);
    for (uint32_t t = 0; t < NTRK; t++) trk_all_off(&trk[t]);
    for (uint32_t g = 0; g < 400u; g++) { run_ms(50); uint32_t n = 0; for (uint32_t t = 0; t < NTRK; t++) n += voices_on(&trk[t]); if (!n) break; }
    run_ms(800);
}
/* ---- features ---- */
static void fft(double *re, double *im, uint32_t n)
{
    for (uint32_t i = 1, j = 0; i < n; i++) { uint32_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; } }
    for (uint32_t len = 2; len <= n; len <<= 1) {
        double a = -2.0 * M_PI / len, wr = cos(a), wi = sin(a);
        for (uint32_t i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (uint32_t k = 0; k < len / 2u; k++) {
                double ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2u] * cr - im[i + k + len / 2u] * ci, vi = re[i + k + len / 2u] * ci + im[i + k + len / 2u] * cr;
                re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2u] = ur - vr; im[i + k + len / 2u] = ui - vi;
                double t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}
typedef struct { double rms, peak, dc, cent, lo, mid, hi, air, slow, fast, attack, release, line, bloom; } feat_t;
static double re_[65536], im_[65536];
/* the take (L from note-on, the chord held HOLD s, then released): the level in 20 ms windows gives the attack (a 60 ms average reaching 80 % of the
 * settled level) and the release (the time to 30 dB below the settled level); the settled part (from the attack, at least 0.5 s) gives the level, the
 * spectrum (a 65536-point Hann FFT: centroid and the energy shares below 200 Hz, up to 1 kHz, up to 4 kHz and above) and the level movement:
 * the 300 .. 3000 Hz band's 200 Hz envelope over 5.12 s, its variation (std / mean) inside 0.2 .. 2 Hz (slow) and 4 .. 12 Hz (fast) */
static feat_t features(const float *l)
{
    feat_t f;
    static double w[760];
    uint32_t nw = 0, hw = HOLD * 50u;
    memset(&f, 0, sizeof f);
    for (uint32_t i = 0; i + 882u <= NS && nw < 750u; i += 882u, nw++) {
        double e = 0;
        for (uint32_t k = 0; k < 882u; k++) e += (double)l[i + k] * l[i + k];
        w[nw] = sqrt(e / 882.0);
    }
    double ref = 0;                                    /* the level the chord has settled at: the last 3 s of the hold */
    for (uint32_t i = hw - 150u; i < hw; i++) ref += w[i];
    ref /= 150.0;
    uint32_t a = 0;                                    /* a 60 ms average reaches 80 % of it */
    for (; a + 3u < hw; a++) { double m3 = 0; for (uint32_t k = 0; k < 3u; k++) m3 += w[a + k]; if (m3 / 3.0 >= 0.8 * ref) break; }
    f.attack = a * 0.02 + 0.03;
    {                                                   /* bloom: the loudest 100 ms of the hold above the settled level, in dB (a struck start is > 0) */
        double bm = 0;
        for (uint32_t i = 0; i + 5u < hw; i++) { double m5 = 0; for (uint32_t k = 0; k < 5u; k++) m5 += w[i + k]; if (m5 / 5.0 > bm) bm = m5 / 5.0; }
        f.bloom = 20 * log10(bm / (ref + 1e-12) + 1e-12);
    }
    uint32_t t0 = (uint32_t)((f.attack > 0.5 ? f.attack : 0.5) * 44100.0);
    if (t0 > 4u * 44100u + 22050u) t0 = 4u * 44100u + 22050u;
    double e = 0, s = 0, pk = 0;
    for (uint32_t i = t0; i < t0 + 65536u; i++) { double x = l[i]; e += x * x; s += x; if (fabs(x) > pk) pk = fabs(x); }
    f.rms = sqrt(e / 65536.0); f.dc = s / 65536.0; f.peak = pk;
    for (uint32_t i = 0; i < 65536u; i++) { double h = 0.5 - 0.5 * cos(2 * M_PI * i / 65535.0); re_[i] = l[t0 + i] * h; im_[i] = 0; }
    fft(re_, im_, 65536u);
    double P = 0, Pf = 0, B[4] = {0, 0, 0, 0};
    for (uint32_t k = 1; k < 32768u; k++) { double hz = k * 44100.0 / 65536.0, q = re_[k] * re_[k] + im_[k] * im_[k]; P += q; Pf += q * hz; B[hz < 200 ? 0 : hz < 1000 ? 1 : hz < 4000 ? 2 : 3] += q; }
    f.cent = Pf / (P + 1e-30);
    {                                                   /* line sharpness: the strongest line's +-2 bins against +-40 bins (a vibrato smears it) */
        uint32_t pk_k = 41; double pm = 0;
        for (uint32_t k = 41; k < 8192u; k++) { double q = re_[k] * re_[k] + im_[k] * im_[k]; if (q > pm) { pm = q; pk_k = k; } }
        double nn = 0, ww = 0;
        for (int d = -40; d <= 40; d++) { uint32_t k = (uint32_t)((int)pk_k + d); double q = re_[k] * re_[k] + im_[k] * im_[k]; ww += q; if (d >= -2 && d <= 2) nn += q; }
        f.line = 10 * log10(nn / (ww + 1e-30) + 1e-12);
    }
    f.lo = 10 * log10(B[0] / (P + 1e-30) + 1e-12); f.mid = 10 * log10(B[1] / (P + 1e-30) + 1e-12);
    f.hi = 10 * log10(B[2] / (P + 1e-30) + 1e-12); f.air = 10 * log10(B[3] / (P + 1e-30) + 1e-12);
    {
        static double env[1024]; double hp = 0, lp = 0, lp2 = 0, acc = 0, mean = 0; uint32_t cnt = 0, ne = 0;
        const double ah = exp(-2 * M_PI * 300.0 / 44100.0), al = 1.0 - exp(-2 * M_PI * 3000.0 / 44100.0);
        for (uint32_t i = t0; i < t0 + 225792u && ne < 1024u; i++) {
            double x = l[i]; hp = ah * hp + (1 - ah) * x; double xh = x - hp; lp += al * (xh - lp); lp2 += al * (lp - lp2);
            acc += fabs(lp2); if (++cnt == 220u) { env[ne++] = acc / 220.0; acc = 0; cnt = 0; }
        }
        for (uint32_t i = 0; i < ne; i++) mean += env[i];
        mean /= (ne ? ne : 1u);
        double sw2 = 0;
        for (uint32_t i = 0; i < 1024u; i++) { double h = 0.5 - 0.5 * cos(2 * M_PI * i / 1023.0); re_[i] = i < ne ? (env[i] - mean) * h : 0; im_[i] = 0; sw2 += h * h; }
        fft(re_, im_, 1024u);
        double ps = 0, pf = 0;
        for (uint32_t k = 1; k < 512u; k++) { double hz = k * 200.0 / 1024.0, q = re_[k] * re_[k] + im_[k] * im_[k]; if (hz >= 0.2 && hz <= 2.0) ps += q; if (hz >= 4.0 && hz <= 12.0) pf += q; }
        f.slow = mean > 0 ? sqrt(2.0 * ps / (1024.0 * sw2)) / mean : 0;
        f.fast = mean > 0 ? sqrt(2.0 * pf / (1024.0 * sw2)) / mean : 0;
    }
    {
        uint32_t r = hw; while (r < nw && w[r] > 0.0316 * ref) r++;
        f.release = (r - hw) * 0.02;
    }
    return f;
}
/* one take: the preset's controls on track 0 (its sends, DIST, FILTER and PAN neutral), the chord held, released, the tail rendered */
static feat_t take(uint32_t preset, const int8_t *e, const uint8_t *env, int fenv, int mono, const uint8_t *notes)
{
    silence_all();
    host_preset(&trk[0], ENGI_ENSEMBLE, preset);
    trk[0].p[P_LEVEL] = 100; trk[0].p[P_REV] = 0; trk[0].p[P_CHOR] = trk[0].p[P_DLY] = 0; trk[0].p[P_DIST] = trk[0].p[P_ITYPE] = 0; trk[0].p[P_SPRD] = 0;
    trk[0].p[P_FILT] = 0; trk[0].p[P_PAN] = 0;
    for (uint32_t k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = e[k];
    for (uint32_t k = 0; k < 4u; k++) trk[0].p[P_ATK + k] = env[k];
    trk[0].p[P_ED_FLT] = (int16_t)fenv; trk[0].p[P_VOICE] = mono ? V_LEGATO : V_POLY;
    run_ms(100);
    memset(L, 0, sizeof L); memset(R, 0, sizeof R);
    chord(0, notes, 1);
    render(HOLD * 1000u, 0);
    chord(0, notes, 0);
    render((SECS - HOLD) * 1000u, HOLD * 44100u);
    return features(L);
}
static double dist(const feat_t *a, const feat_t *b)
{
    double d[12];
    d[0] = log(a->cent / b->cent) / log(1.35);                    /* a third of an octave step is 1 */
    d[1] = (a->lo - b->lo) / 4.0; d[2] = (a->mid - b->mid) / 4.0; d[3] = (a->hi - b->hi) / 4.0; d[4] = (a->air - b->air) / 6.0;
    d[5] = (a->slow - b->slow) / 0.05;
    d[6] = (a->fast - b->fast) / 0.05;
    d[7] = log((a->attack + 0.05) / (b->attack + 0.05)) / log(1.6);
    d[8] = log((a->release + 0.2) / (b->release + 0.2)) / log(1.6);
    d[9] = (20 * log10((a->rms + 1e-9) / (b->rms + 1e-9))) / 3.0;
    d[10] = (a->line - b->line) / 3.0;
    d[11] = (a->bloom - b->bloom) / 3.0;
    double s = 0; for (int i = 0; i < 12; i++) s += d[i] * d[i];
    return sqrt(s);
}
static uint32_t idx_of(const char *name)
{
    for (uint32_t i = 0; i < ENG_ENSEMBLE.npresets; i++) if (!strcmp(ENG_ENSEMBLE.presets[i].name, name)) return i;
    CHECK(0);
    return 0;
}
static void print_feat(const char *name, const feat_t *f, const char *tag)
{
    printf("%-16s %s rms %5.1f dB peak %.3f dc %+.4f cent %6.0f Hz bands %6.1f %6.1f %6.1f %6.1f dB  mod slow %.2f fast %.2f  line %5.1f dB  bloom %4.1f dB  attack %.2f s release %.2f s\n", name, tag,
           20 * log10(f->rms + 1e-9), f->peak, f->dc, f->cent, f->lo, f->mid, f->hi, f->air, f->slow, f->fast, f->line, f->bloom, f->attack, f->release);
}
int main(void)
{
    web_nor_erase(); web_boot(); run_ms(1400); normal_off(); pair_disable();
    const uint32_t np = ENG_ENSEMBLE.npresets;
    static feat_t F[32][2];
    int pr = getenv("PRINT") != NULL;
    const char *try_ = getenv("TRY");
    if (try_) {                                        /* design aid: TRY="e0,..,e7,atk,dec,sus,rel;..." prints the features of each candidate */
        while (*try_) {
            int v[14] = {0}, n = 0; char *end;
            while (n < 14) { v[n++] = (int)strtol(try_, &end, 10); try_ = end; if (*try_ == ',') try_++; else break; }
            if (*try_ == ';') try_++;
            if (n < 12) break;
            int8_t e[8]; uint8_t env[4];
            for (int k = 0; k < 8; k++) e[k] = (int8_t)v[k];
            for (int k = 0; k < 4; k++) env[k] = (uint8_t)v[8 + k];
            for (int c = 0; c < 2; c++) { feat_t f = take(0, e, env, v[12], v[13], CHORD[c]); print_feat("try", &f, c ? "high" : "low "); }
        }
        return 0;
    }

    /* 1. names: unique, printable, fit the UI (at most 15 characters, the longest name any engine has had since the first factory presets) */
    CHECK(np >= 13u && np <= 32u);
    for (uint32_t i = 0; i < np; i++) {
        const char *n = ENG_ENSEMBLE.presets[i].name;
        CHECK(strlen(n) >= 3u && strlen(n) <= 15u);
        for (const char *c = n; *c; c++) CHECK((*c >= 'A' && *c <= 'Z') || *c == ' ' || (*c >= '0' && *c <= '9'));
        for (uint32_t j = 0; j < i; j++) CHECK(strcmp(n, ENG_ENSEMBLE.presets[j].name));
        for (uint32_t k = 0; k < 8u; k++) CHECK(ENG_ENSEMBLE.presets[i].e[k] >= 0 && ENG_ENSEMBLE.presets[i].e[k] <= 127);
    }
    CHECK(strlen(PRESET_CAT[ENGI_ENSEMBLE]) == np);

    /* 2. every preset sounds, with both chords */
    for (uint32_t i = 0; i < np; i++) {
        const preset_t *pr_ = &ENG_ENSEMBLE.presets[i];
        for (int c = 0; c < 2; c++) {
            F[i][c] = take(i, pr_->e, pr_->env, pr_->fenv, pr_->mono, CHORD[c]);
            const feat_t *f = &F[i][c];
            if (pr) print_feat(pr_->name, f, c ? "high" : "low ");
            CHECK(f->rms > 0.012 && f->peak > 0.04 && f->peak < 0.9 && fabs(f->dc) < 0.01);
            CHECK(f->release < 4.9);                  /* every tail ends inside the 5 s */
        }
    }
    /* 3. really different: a minimum distance between every pair in the feature space, over both chords */
    double mind = 1e9; uint32_t mi = 0, mj = 0;
    for (uint32_t i = 0; i < np; i++) for (uint32_t j = 0; j < i; j++) {
        double d0 = dist(&F[i][0], &F[j][0]), d1 = dist(&F[i][1], &F[j][1]), d = sqrt(d0 * d0 + d1 * d1);
        if (d < mind) { mind = d; mi = i; mj = j; }
    }
    printf("closest pair: %s / %s, distance %.2f (1 = a third of an octave of centroid, 4 dB of a band, 0.05 of level variation, a 1.6 x attack or release, 3 dB of level; two chords)\n",
           ENG_ENSEMBLE.presets[mi].name, ENG_ENSEMBLE.presets[mj].name, mind);
    if (pr) {
        for (uint32_t i = 0; i < np; i++) {
            printf("%-16s", ENG_ENSEMBLE.presets[i].name);
            for (uint32_t j = 0; j < np; j++) printf(" %4.1f", i == j ? 0.0 : sqrt(pow(dist(&F[i][0], &F[j][0]), 2) + pow(dist(&F[i][1], &F[j][1]), 2)));
            printf("\n");
        }
    }
    CHECK(mind >= ENS_PRESET_MIN_DISTANCE);

    /* 4. each new preset does the job its name says, as a measured fact of the signal (relations to the others, not absolute sound judgements) */
    {
        uint32_t A = idx_of("PROFILE A"), SM = idx_of("STRING MACHINE"), BC = idx_of("BREATH CHOIR"), DD = idx_of("DEEP DRONE"), GS = idx_of("GLASS SHIMMER");
        uint32_t SW = idx_of("SLOW SWELL"), RP = idx_of("ROUGH PULSE"), SD = idx_of("SOLO DRIFT"), HE = idx_of("HUGE ENSEMBLE"), RN = idx_of("RUNNING PAD");
        uint32_t i;
        /* PROFILE A: the controls of the best match to the reference profile, and the measured profile of the render (the reference: 380 Hz, -4.1 / -2.7 / -11.9 / -23.5 dB) */
        static const int8_t VA[8] = {120, 15, 80, 60, 70, 71, 31, 126};
        for (uint32_t k = 0; k < 8u; k++) CHECK(ENG_ENSEMBLE.presets[A].e[k] == VA[k]);
        CHECK(F[A][0].cent > 360 && F[A][0].cent < 440 && F[A][0].lo > -6.0 && F[A][0].lo < -2.0 && F[A][0].mid > -4.5 && F[A][0].mid < -1.5 && F[A][0].hi > -13.5 && F[A][0].hi < -9.5 && F[A][0].air > -31.5 && F[A][0].air < -26.5);
        for (i = 0; i < np; i++) if (i != DD) CHECK(F[DD][0].cent < F[i][0].cent && F[DD][1].cent < F[i][1].cent);       /* the lowest, the darkest, the slowest to open and to close */
        CHECK(F[DD][0].cent < 300 && F[DD][0].air < -45 && F[DD][0].release > 2.5);
        for (i = 0; i < np; i++) if (i != SW) CHECK(F[SW][0].attack > F[i][0].attack && F[SW][1].attack > F[i][1].attack);
        CHECK(F[SW][0].attack > 2.5 && ENG_ENSEMBLE.presets[SW].fenv > 0);
        for (i = 0; i < np; i++) if (i != BC) CHECK(F[BC][0].cent > F[i][0].cent);                                         /* the formant: the highest centroid, a few dB quieter than the strings */
        CHECK(F[BC][0].rms < F[SM][0].rms * 0.5);
        CHECK(F[GS][0].bloom > 3.0 && F[GS][1].bloom > 3.0 && F[GS][0].attack < 0.1);                                    /* struck: loud first, then it settles */
        for (i = 0; i < np; i++) if (i != SW) CHECK(F[SW][0].bloom < 2.0);
        CHECK(F[RP][0].attack < 0.1 && F[RP][0].release < 0.5 && ENG_ENSEMBLE.presets[RP].e[1] == 127 && ENG_ENSEMBLE.presets[RP].e[6] == 127);   /* rough: MORPH DARK, TEXT full */
        CHECK(ENG_ENSEMBLE.presets[SD].mono == 1 && ENG_ENSEMBLE.presets[SD].e[3] == 127);                               /* solo: MONO, DRIFT full */
        CHECK(ENG_ENSEMBLE.presets[HE].e[4] == 127 && ENG_ENSEMBLE.presets[HE].e[7] == 127);                             /* the deepest ensemble: ENS and MIX full */
        /* the fast RATE smears a held line: most of all but ROUGH PULSE (its ring-modulated pulse smears more). Before 0.10.1 it was the most (below -6 dB): the slow taps' LFOs ran in phase, a common
         * vibrato that smeared a line at the fastest RATE; with them spread the pad measures -4.7 dB */
        for (i = 0; i < np; i++) if (i != RN && i != RP) CHECK(F[RN][1].line < F[i][1].line);
        CHECK(F[RN][1].line < -4.0);
    }
    printf("ENSEMBLE PRESETS PASS: %u checks, %u presets.\n", checks, np);
    return 0;
}
