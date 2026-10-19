/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* Host test of the track FILTER (Caravel 0.3.0; P_FILT, src/fx.c track_filter), on the real sources:
 *   build/host/trackfilter_test [DEMO_DIR]      (run_tests.sh builds and runs it; demos in build/trackfilter_demo/)
 * 1. the parameter: id, range, default, the card's text (OFF, share + LP / HP), the pages (INSERT 2's fourth card), automation, FX HOLD
 * 2. OFF: nothing runs and the track is bit for bit as before (a block, a phrase, with SPREAD); at rest again after the fade
 * 3. response: LP and HP at nine settings each against the analytic 12 dB/oct response of the coefficients in use, the cutoffs
 *    where the knob says (-64: 120 Hz, +63: 3.8 kHz)
 * 4. smooth: the cutoff glides at most 192 / 256 of a row a block, the wet share 2.9 ms; no jump past the input's own on a knob
 *    jump, a sweep, a side change; turning to 0 opens first, then fades out and clears
 * 5. the same output for any split of the signal into blocks once settled; SPREAD's side takes the same filter
 * 6. extremes: all 128 settings on 8 loud saws, full-scale squares at the cutoff, DC, noise, impulses: against a 64-bit
 *    shadow of the same arithmetic (no 32-bit overflow), bounded, decaying to silence (no limit cycle, no DC)
 * 7. in the chain: after the INSERT, before the level, pan and sends (the sends take the filtered sound); a part's filter state
 *    runs out when its voices have ended
 * 8. stored: FUN10 round trip (114 parameters), a FUN10 / user preset of 0.2.0 (113) loads with the filter OFF and everything
 *    else (engine values, motion) in place; automation and locks; MIDI LEARN
 * 9. the cost per track (instructions / sample, host) against an 8-voice ANALOG part */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void ck(const char *what, int ok)
{
    printf("trackfilter: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void queued(uint32_t st, uint32_t d1, uint32_t d2, uint32_t source)
{
    midi_enqueue((st >= 0xF8u ? 0xFu : st >> 4) | st << 8 | d1 << 16 | d2 << 24, source);
    events_block(CTL);
}
static int32_t out_buf[2 * CTL];
static void mixb(uint32_t n)
{
    while (n--)
        mix_block(out_buf, CTL);
}
static uint32_t preset_by_name(uint32_t e, const char *n)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n))
            return k;
    return 0;
}
static void fresh(uint32_t e, uint32_t pi)        /* the boot state (no FX tails), track 1 = engine e preset pi */
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    memset(&pf, 0, sizeof pf);
    memset(ins, 0, sizeof ins);
    memset(tf, 0, sizeof tf);
    for (k = 0; k < NTRK; k++)
        pf.mg[k] = 32768;
    perf_held = perf_latched = 0;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    vage = 0;
    rng_state = 0x1234567u;
    mod_seed = 0x2545F491u;
    song.playing = 0;
    memset(trk, 0, sizeof trk);
    MOD_W_CLEAR();
    memset(sl, 0, sizeof sl);
    memset(&motion, 0, sizeof motion);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, k ? 0u : pi);
    song.sel = 0;
}

/* ------------------------------------------------------------ the unit --- */
#define LEN 24576u                                   /* a signal for the unit tests: 0.56 s */
static int32_t sig[LEN], sg2[LEN], res[LEN], res2[LEN];
static uint32_t lcg = 12345u;
static int32_t rnd(void) { lcg = lcg * 1664525u + 1013904223u; return (int32_t)(lcg >> 8) - 0x800000; }   /* +-8388608 */
static void make_sine(int32_t *x, uint32_t n, double hz, double amp)
{
    for (uint32_t i = 0; i < n; i++)
        x[i] = (int32_t)floor(amp * sin(2.0 * M_PI * hz * (double)i / FS) + 0.5);
}
static void make_saws(int32_t *x, uint32_t n, uint32_t voices)   /* a voice's level (24000 each), a chord's pitches */
{
    static const double HZ[8] = {110.0, 139.0, 165.0, 220.0, 277.0, 330.0, 440.0, 554.0};
    for (uint32_t i = 0; i < n; i++) {
        double s = 0;
        for (uint32_t v = 0; v < voices; v++)
            s += (fmod(i * HZ[v] / FS + v * 0.37, 1.0) * 2.0 - 1.0) * 24000.0;
        x[i] = (int32_t)s;
    }
}
/* what mix_part does: the call only while P_FILT is set or the filter still runs */
static void tf_call(track_t *t, int32_t *b, int32_t *d, uint32_t n)
{
    if (t->p[P_FILT] | t->tf_run)
        track_filter(t, b, d, n);
}
/* x through track 1's filter at knob v from rest in blocks of blk into y */
static void run_tf(int v, const int32_t *x, int32_t *y, uint32_t n, uint32_t blk)
{
    track_t *t = &trk[0];
    memset(tf, 0, sizeof tf);
    t->tf_run = 0;
    t->p[P_FILT] = (int16_t)v;
    memcpy(y, x, n * sizeof *x);
    for (uint32_t i = 0; i < n; i += blk)
        tf_call(t, y + i, 0, i + blk <= n ? blk : n - i);
}
static double rms(const int32_t *x, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static int32_t maxabs(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    for (uint32_t i = 0; i < n; i++)
        m = abs(x[i]) > m ? abs(x[i]) : m;
    return m;
}
static int32_t maxjump(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    for (uint32_t i = 1; i < n; i++)
        m = abs(x[i] - x[i - 1]) > m ? abs(x[i] - x[i - 1]) : m;
    return m;
}

/* the track's mix as the host builds it, summed to L (after MASTER etc.): the first-difference energy over the signal energy */
static double hf_share(const int32_t *x, uint32_t n)
{
    double d = 0, e = 1e-9;
    for (uint32_t i = 1; i < n; i++) {
        d += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        e += (double)x[i] * x[i];
    }
    return d / e;
}

/* ------------------------------------------------------------------ 1 --- */
static void tft_parameter(void)
{
    char val[12];
    const char *u;
    int ok;
    const param_desc_t *d = &TP[P_FILT];
    uint32_t p, pi = 0, pi2 = 0;
    ck("layout: P_FILT 105 just before P_E0 106, P_COUNT 114 = MOTION_FX_FIRST (the last common id), P_LATCH still 8",
       P_FILT == 105 && P_FILT + 1 == P_E0 && P_E0 == 106 && P_COUNT == 114 && P_COUNT == MOTION_FX_FIRST && P_LATCH == 8);
    ck("descriptor: FILT, -64 .. 63, default 0 (OFF)", str_eq(d->label, "FILT") && d->fmt == F_BIPCT && d->min == -64 && d->max == 63 && d->def == 0);
    ok = 1;
    param_format(d, 0, val, &u);
    ok &= str_eq(val, "OFF") && str_eq(u, "");
    param_format(d, -64, val, &u);
    ok &= str_eq(val, "100") && str_eq(u, "LP");
    param_format(d, -32, val, &u);
    ok &= str_eq(val, "50") && str_eq(u, "LP");
    param_format(d, -1, val, &u);
    ok &= str_eq(val, "1") && str_eq(u, "LP");
    param_format(d, 1, val, &u);
    ok &= str_eq(val, "1") && str_eq(u, "HP");
    param_format(d, 63, val, &u);
    ok &= str_eq(val, "100") && str_eq(u, "HP");
    param_format(d, 32, val, &u);
    ok &= str_eq(val, "50") && str_eq(u, "HP");
    param_format(&TP[P_PAN], -64, val, &u);
    ok &= str_eq(val, "-100") && str_eq(u, "%");      /* (the other bipolar cards are as they were) */
    ck("the card: OFF at 0, else the share (1 .. 100) with LP / HP as its unit; PAN and the other bipolar ones as before", ok);
    for (p = 0; p < NPAGES; p++) {
        if (str_eq(PAGES[p].title, "INSERT")) pi = p;
        if (str_eq(PAGES[p].title, "INSERT 2")) pi2 = p;
    }
    ck("pages: INSERT = TYPE MIX (FX HOLD, the curve beside them); INSERT 2 = A B C FILT", pi2 == pi + 1u && PAGES[pi].id[0] == P_ITYPE &&
       PAGES[pi].id[1] == P_IMIX && PAGES[pi].id[2] == 0xFF && PAGES[pi].id[3] == 0xFF && PAGES[pi2].id[0] == P_IA &&
       PAGES[pi2].id[1] == P_IB && PAGES[pi2].id[2] == P_IC && PAGES[pi2].id[3] == P_FILT);
    ck("automation and locks: P_FILT is a motion parameter; part of the track's FX (FX HOLD), not param_kept", motion_param(P_FILT) &&
       !param_kept(P_FILT) && param_insert(P_FILT) && !param_send(P_FILT) && !motion_engine_id(P_FILT));
    ck("not a modulation destination (MOD DST, ENV DEST, LFO DEST have no FILT)", !str_eq(N_MDST[NELEM(N_MDST) - 1], "FILT") &&
       NELEM(N_MDST) == 25u);
}

/* ------------------------------------------------------------------ 2 --- */
static int32_t sprd_pin;
static uint64_t hash;
static void hash_out(void)
{
    for (uint32_t i = 0; i < 2u * CTL; i++)
        for (uint32_t k = 0; k < 4u; k++) {
            hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu;
            hash *= 0x100000001B3ull;
        }
}
static int32_t ph_filt;
/* the hash of a phrase on track 1 (ANALOG SOFT PAD, POLY, the preset's sends) in a fork()ed child, FILT ph_filt (0: the defaults),
 * SPREAD sprd_pin */
static uint64_t phrase(void)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        track_t *t = &trk[0];
        uint32_t i;
        fresh(0, preset_by_name(0, "SOFT PAD"));
        hash = 0xCBF29CE484222325ull;
        t->p[P_VOICE] = V_POLY;
        t->p[P_SPRD] = (int16_t)sprd_pin;
        t->p[P_FILT] = (int16_t)ph_filt;
        trk_note_on(t, 48, 100);
        trk_note_on(t, 55, 70);
        trk_note_on(t, 64, 120);
        for (i = 0; i < FS / 2u / CTL; i++) { mixb(1); hash_out(); }
        trk_note_on(t, 72, 90);
        for (i = 0; i < FS / 4u / CTL; i++) { mixb(1); hash_out(); }
        trk_note_off(t, 48);
        trk_note_off(t, 55);
        trk_note_off(t, 64);
        trk_note_off(t, 72);
        for (i = 0; i < FS / CTL; i++) { mixb(1); hash_out(); }
        h = hash;
        if (write(fd[1], &h, sizeof h) != sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}
static void tft_off(void)
{
    track_t *t = &trk[0];
    uint64_t h0, h1, h2;
    int ok;
    uint32_t i;
    fresh(0, 0);
    for (i = 0; i < LEN; i++)
        sig[i] = rnd() >> 6;
    memcpy(res, sig, sizeof sig);
    for (i = 0; i < LEN; i += CTL)
        tf_call(t, res + i, 0, CTL);
    ck("FILT 0 from rest: a block is not touched (bit for bit), nothing runs (no state, tf_run 0)", !memcmp(res, sig, sizeof sig) &&
       !t->tf_run && !tf[0].mode && !tf[0].w && !tf[0].z[0] && !tf[0].z[1] && !tf[0].cut);
    for (sprd_pin = 0; sprd_pin <= 64; sprd_pin += 64) {
        ph_filt = 0;
        h0 = phrase();
        ph_filt = 40;
        h1 = phrase();
        ph_filt = -40;
        h2 = phrase();
        ck(sprd_pin ? "SPREAD 64: a phrase with FILT set renders differently from FILT 0, LP and HP alike"
                    : "a phrase with FILT set renders differently from FILT 0, LP and HP alike", h0 && h1 && h2 && h1 != h0 && h2 != h0 && h1 != h2);
    }
    sprd_pin = 0;
    /* turned to 0 after the filter ran: at rest again, the track as it is */
    fresh(0, preset_by_name(0, "SOFT PAD"));
    t->p[P_FILT] = -50;
    t->p[P_SUS] = 127;
    trk_note_on(t, 48, 100);
    mixb(FS / 4u / CTL);
    ok = t->tf_run && tf[0].mode == 1 && tf[0].w == 32768;
    t->p[P_FILT] = 0;
    mixb(FS / 4u / CTL);
    ck("FILT turned back to 0: after the fade the filter is at rest (mode 0, state clear, tf_run 0)", ok && !t->tf_run && !tf[0].mode &&
       !tf[0].w && !tf[0].z[0] && !tf[0].z[1] && !tf[0].z[2] && !tf[0].z[3]);
}

/* ------------------------------------------------------------------ 3 --- */
/* the discrete filter's analytic gain: two identical trapezoidal poles (g_p = G / (1 - G) of the coefficient in use),
 * r = tan(pi f / FS) / g_p: LP 1 / (1 + r^2), HP r^2 / (1 + r^2); -3 dB where r = sqrt(sqrt(2) - 1) */
#define R3DB 0.6435942529055826
static double gain_expected(uint32_t g16, double hz, int hp)
{
    double G = g16 / 65536.0, gp = G / (1.0 - G), r = tan(M_PI * hz / FS) / gp, r2 = r * r;
    return (hp ? r2 : 1.0) / (1.0 + r2);
}
static double fc_of(uint32_t g16)
{
    double G = g16 / 65536.0, gp = G / (1.0 - G);
    return atan(R3DB * gp) * FS / M_PI;
}
static void tft_response(void)
{
    static const int LPV[9] = {-64, -56, -48, -40, -32, -24, -16, -8, -1};
    static const int HPV[9] = {1, 8, 16, 24, 32, 40, 48, 56, 63};
    static const double MUL[5] = {0.25, 0.5, 1.0, 2.0, 4.0};
    track_t *t = &trk[0];
    double worst = 0, wfc = 0;
    int ok = 1, okfc = 1;
    uint32_t a, m;
    fresh(0, 0);
    for (a = 0; a < 18u; a++) {
        int v = a < 9u ? LPV[a] : HPV[a - 9u], hp = v > 0;
        double fc, nominal;
        make_sine(sig, LEN, 440.0, 1.0);                  /* (any signal: the coefficients settle at the knob's place) */
        run_tf(v, sig, res, LEN, CTL);
        fc = fc_of(tf[0].g);
        for (m = 0; m < 5u; m++) {
            double hz, got, want;
            hz = fc * MUL[m];
            if (hz > 19000.0 || hz < 15.0)
                continue;
            make_sine(sig, LEN, hz, 20000.0);
            run_tf(v, sig, res, LEN, CTL);
            got = rms(res + LEN - 8192u, 8192u) / rms(sig + LEN - 8192u, 8192u);
            want = gain_expected(tf[0].g, hz, hp);
            if (want > 0.01) {
                double db = fabs(20.0 * log10(got / want));
                if (db > worst)
                    worst = db;
                ok &= db < 0.5;
                if (db >= 0.5)
                    printf("trackfilter:   %s %d at %.0f Hz (fc %.0f): got %.3f want %.3f (%.2f dB)\n", hp ? "HP" : "LP", v, hz, fc, got, want, db);
            }
        }
        /* the cutoff where the knob says: LP -64 row 14 (120 Hz) .. -1 near the top; HP +1 row 0 (30 Hz) .. +63 row 49 (3.84 kHz) */
        make_sine(sig, LEN, 440.0, 1.0);
        run_tf(v, sig, res, LEN, CTL);
        nominal = 30.0 * pow(2.0, ((double)tf[0].cut / 256.0) / 7.0);
        if (fabs(fc - nominal) / nominal > wfc)
            wfc = fabs(fc - nominal) / nominal;
        okfc &= fabs(fc - nominal) / nominal < 0.02;
        if (v == -64) okfc &= fabs(fc - 120.0) < 3.0;
        if (v == 63) okfc &= fabs(fc - 3840.0) < 100.0;
        if (v == 1) okfc &= fabs(fc - 32.0) < 2.0;
    }
    printf("trackfilter:   the response of 18 settings x 5 frequencies: worst %.2f dB off the analytic one; -3 dB points within %.1f %% of 30 Hz * 2^(row / 7)\n",
           worst, wfc * 100);
    ck("LP -64 .. -1 and HP +1 .. +63: the gain at fc/4, fc/2, fc, 2fc, 4fc within 0.5 dB of the 12 dB/oct response of the coefficient in use", ok);
    ck("the cutoffs: LP -64 = 120 Hz, HP +1 = 32 Hz, HP +63 = 3.84 kHz; every setting within 2 % of its table row (30 Hz * 2^(row / 7))", okfc);
    {   /* TF_G is G = g / (1 + g), g = tan(pi f / FS) / 0.6436, f = 30 Hz * 2^(row / 7), Q16 */
        int okt = 1;
        for (uint32_t r = 0; r < 64u; r++) {
            double g = tan(M_PI * 30.0 * pow(2.0, r / 7.0) / FS) / R3DB, G = g / (1.0 + g);
            okt &= fabs(G * 65536.0 - TF_G[r]) <= 0.5001;
        }
        ck("TF_G: the 64 rows are G = g / (1 + g), g = tan(pi f / FS) / 0.6436, f = 30 Hz * 2^(row / 7), Q16 (rounded)", okt);
    }
    /* it is a filter: LP removes, HP removes, and neither adds energy to a sine of the passband */
    make_sine(sig, LEN, 8000.0, 20000.0);
    run_tf(-40, sig, res, LEN, CTL);
    make_sine(sg2, LEN, 60.0, 20000.0);
    run_tf(40, sg2, res2, LEN, CTL);
    ck("LP -40 takes an 8 kHz sine below -30 dB; HP +40 takes a 60 Hz sine below -30 dB", rms(res + LEN - 8192u, 8192u) < 0.03 * rms(sig, LEN) &&
       rms(res2 + LEN - 8192u, 8192u) < 0.03 * rms(sg2, LEN));
    (void)t;
}

/* ------------------------------------------------------------------ 4 --- */
static void tft_smooth(void)
{
    track_t *t = &trk[0];
    uint32_t i, blocks;
    int ok, okw, okc;
    int32_t pc, jmp, jin;
    fresh(0, 0);
    make_saws(sig, LEN, 3);
    /* a knob jump 0 -> -64 */
    memset(tf, 0, sizeof tf);
    t->tf_run = 0;
    t->p[P_FILT] = -64;
    memcpy(res, sig, sizeof sig);
    tf_call(t, res, 0, CTL);
    ok = tf[0].w == CTL * SL_SLOPE && tf[0].mode == 1 && tf[0].cut == PF_TOP - TF_STEP;
    for (i = 1; i < 4u; i++)
        tf_call(t, res + i * CTL, 0, CTL);
    ok &= tf[0].w == 32768;
    ck("FILT 0 -> LP 100 %: the wet share glides in over 2.9 ms (128 samples), from an open cutoff", ok);
    pc = tf[0].cut;
    okc = 1;
    for (blocks = 4; blocks < LEN / CTL; blocks++) {
        tf_call(t, res + blocks * CTL, 0, CTL);
        okc &= pc - tf[0].cut <= TF_STEP && pc - tf[0].cut >= 0;
        pc = tf[0].cut;
        if (tf[0].cut == PF_LOW) break;
    }
    ck("  the cutoff glides to the knob's place at most 192 / 256 of a row a block (the range in 66 blocks = 48 ms)", okc &&
       tf[0].cut == PF_LOW && blocks >= 64u && blocks <= 70u);
    printf("trackfilter:   the cutoff reached its place after %u blocks (%.1f ms)\n", blocks + 1u, (blocks + 1u) * 1000.0 * CTL / FS);
    jmp = maxjump(res, LEN);
    jin = maxjump(sig, LEN);
    ck("  no click: the largest step of the output over that jump is within the saws' own", jmp <= jin);
    /* back to 0: opens first, then fades out, clears, nothing runs */
    t->p[P_FILT] = 0;
    {
        uint32_t n = 0, open_at = 0, off_at = 0;
        okw = 1;
        ok = 1;
        for (n = 0; n < 200u; n++) {
            int32_t x[CTL];
            memcpy(x, sig, sizeof x);
            pc = tf[0].cut;
            tf_call(t, x, 0, CTL);
            if (!open_at && tf[0].cut == PF_TOP)
                open_at = n + 1u;
            if (!open_at)
                okw &= tf[0].w == 32768;                   /* (full wet while it opens) */
            ok &= tf[0].cut - pc <= TF_STEP;
            if (!t->tf_run) {
                off_at = n + 1u;
                break;
            }
        }
        printf("trackfilter:   to 0 from LP 100 %%: open after %u blocks, at rest after %u (%.1f ms)\n", open_at, off_at, off_at * 1000.0 * CTL / FS);
        ck("FILT -> 0: the cutoff opens first (full wet), then the wet share fades out in 4 blocks; then at rest, state clear", okw && ok &&
           open_at >= 64u && off_at == open_at + 4u && !tf[0].mode && !tf[0].z[0] && !tf[0].z[1] && !tf[0].w);
        {
            int32_t x[CTL];
            memcpy(x, sig + CTL, sizeof x);
            tf_call(t, x, 0, CTL);
            ck("  and the next block is the input, bit for bit", !memcmp(x, sig + CTL, sizeof x) && !t->tf_run);
        }
    }
    /* the sign changes while it runs: LP -40 -> HP +40 at once */
    memset(tf, 0, sizeof tf);
    t->tf_run = 0;
    memcpy(res, sig, sizeof sig);
    t->p[P_FILT] = -40;
    for (i = 0; i < 200u; i++)
        tf_call(t, res + i * CTL, 0, CTL);
    t->p[P_FILT] = 40;
    {
        uint32_t seq = 0, lp = 0, hp = 0, switch_blk = 0;
        ok = 1;
        for (i = 200; i < 500u; i++) {
            uint8_t m0 = tf[0].mode;
            tf_call(t, res + i * CTL, 0, CTL);
            if (tf[0].mode == 2 && m0 != 2)
                switch_blk = i;
            lp += tf[0].mode == 1;
            hp += tf[0].mode == 2;
            seq += m0 != tf[0].mode;
        }
        jmp = maxjump(res + 200u * CTL, 300u * CTL);
        jin = maxjump(sig + 200u * CTL, 300u * CTL);
        ck("LP -40 -> HP +40 at once: opens, fades out, clears, starts the other side (two switches), no jump past the input's", seq == 2u && switch_blk &&
           tf[0].mode == 2 && jmp <= jin + jin / 8);
    }
    /* a sweep: the knob one step each block, there and back: no zipper */
    memset(tf, 0, sizeof tf);
    t->tf_run = 0;
    memcpy(res, sig, sizeof sig);
    {
        int32_t v;
        uint32_t b = 0;
        for (v = 0; v >= -64; v--, b++) { t->p[P_FILT] = (int16_t)v; tf_call(t, res + b * CTL, 0, CTL); }
        for (v = -63; v <= 0; v++, b++) { t->p[P_FILT] = (int16_t)v; tf_call(t, res + b * CTL, 0, CTL); }
        for (v = 1; v <= 63; v++, b++) { t->p[P_FILT] = (int16_t)v; tf_call(t, res + b * CTL, 0, CTL); }
        for (v = 62; v >= 0; v--, b++) { t->p[P_FILT] = (int16_t)v; tf_call(t, res + b * CTL, 0, CTL); }
        jmp = maxjump(res, b * CTL);
        jin = maxjump(sig, b * CTL);
        ck("a knob sweep 0 .. -64 .. 0 .. +63 .. 0 (a step a block): no zipper, no jump past the saws' own", jmp <= jin + jin / 8 && b < LEN / CTL);
    }
    /* a knob jittering by one detent every block on a loud chord */
    memset(tf, 0, sizeof tf);
    t->tf_run = 0;
    memcpy(res, sig, sizeof sig);
    for (i = 0; i < LEN / CTL; i++) {
        t->p[P_FILT] = (int16_t)(-30 + (int)(i & 1u) * 3);
        tf_call(t, res + i * CTL, 0, CTL);
    }
    ck("a knob jittering by 3 steps every block: no jump past the saws' own", maxjump(res, LEN) <= maxjump(sig, LEN) + maxjump(sig, LEN) / 8);
}

/* ------------------------------------------------------------------ 5 --- */
static void tft_blocks_side(void)
{
    track_t *t = &trk[0];
    uint32_t i, s;
    int ok = 1;
    static const uint32_t BLK[3] = {32, 8, 5};
    int32_t ref[LEN];
    make_saws(sig, LEN, 4);
    for (i = 0; i < LEN; i++)
        sig[i] += rnd() >> 10;
    for (s = 0; s < 4u; s++) {
        int v = s == 0 ? -40 : s == 1 ? -3 : s == 2 ? 20 : 63;
        uint32_t k;
        fresh(0, 0);
        run_tf(v, sig, ref, LEN, CTL);
        /* settled: feed the same signal again in other block sizes from the settled state */
        for (k = 0; k < 3u; k++) {
            tfilt_t keep = tf[0];
            int32_t a[LEN], b[LEN];
            memcpy(a, sig, sizeof a);
            memcpy(b, sig, sizeof b);
            for (i = 0; i < LEN; i += CTL)
                tf_call(t, a + i, 0, CTL);
            tf[0] = keep;
            t->tf_run = 1;
            for (i = 0; i < LEN; i += BLK[k])
                tf_call(t, b + i, 0, i + BLK[k] <= LEN ? BLK[k] : LEN - i);
            ok &= !memcmp(a, b, sizeof a);
        }
    }
    ck("settled: the same output for the signal in blocks of 32, 8 and 5 (the state carries over a call exactly), LP and HP", ok);
    /* SPREAD's side: the same filter, its own state */
    ok = 1;
    for (s = 0; s < 2u; s++) {
        int v = s ? 25 : -45;
        int32_t bb[LEN], dd[LEN];
        fresh(0, 0);
        memset(tf, 0, sizeof tf);
        t->tf_run = 0;
        t->p[P_FILT] = (int16_t)v;
        memcpy(bb, sig, sizeof bb);
        memcpy(dd, sig, sizeof dd);
        for (i = 0; i < LEN; i += CTL)
            tf_call(t, bb + i, dd + i, CTL);
        run_tf(v, sig, ref, LEN, CTL);
        ok &= !memcmp(bb, ref, sizeof bb) && !memcmp(dd, ref, sizeof dd);
    }
    ck("SPREAD: the side signal d takes exactly the filter b takes (b alone gives b's result), both with their own state", ok);
}

/* ------------------------------------------------------------------ 6 --- */
/* a 64-bit shadow of one block of track_filter's arithmetic from the states it started with, G as it used: the same results
 * (so no step overflowed 32 bits), every intermediate inside 2^30 */
static int shadow_bad;
static int64_t sh_pole(int64_t x, int64_t *z, int64_t g)
{
    int64_t d = ((x - *z) * g) >> 16, y = *z + d;
    if (llabs(x - *z) > (1ll << 30) || llabs(y) > (1ll << 30))
        shadow_bad++;
    *z = y + d;
    return y;
}
static void shadow_block(const tfilt_t *before, const tfilt_t *after, const int32_t *in, const int32_t *out, uint32_t n, int hp, int32_t w0)
{
    int64_t z[2] = {before->z[0], before->z[1]}, w = w0, g = after->g;
    for (uint32_t j = 0; j < n; j++) {
        int64_t x = in[j], u = x > 524287 ? 524287 : x < -524287 ? -524287 : x, y, wet;
        u *= 256;
        w += 32768 > w ? (32768 - w > SL_SLOPE ? SL_SLOPE : 32768 - w) : 0;
        y = sh_pole(u, &z[0], g);
        if (hp) {
            u -= y;
            y = u - sh_pole(u, &z[1], g);
        } else {
            y = sh_pole(y, &z[1], g);
        }
        y = (y + 128) >> 8;
        wet = w == 32768 ? y : x + (((y - x) * (w << 1)) >> 16);
        if (llabs(wet - out[j]) > 1)
            shadow_bad++;
    }
    if (z[0] != after->z[0] || z[1] != after->z[1])
        shadow_bad++;
}
static void tft_extremes(void)
{
    track_t *t = &trk[0];
    int v, ok_dc = 1, ok_fin = 1, ok_peak = 1, ok_shadow = 1;
    uint32_t sig_n, i;
    int32_t worst_peak_lp = 0, worst_peak_hp = 0, worst_rest = 0;
    static int32_t x[40960], y[40960];
    fresh(0, 0);
    for (sig_n = 0; sig_n < 6u; sig_n++) {
        for (v = -64; v <= 63; v++) {
            uint32_t n = 24576u, pos;
            int32_t pk, in_pk;
            if (!v)
                continue;
            switch (sig_n) {
            case 0: make_saws(x, n, 8); break;                                                         /* 8 loud voices (192000) */
            case 1: {                                                                                           /* a full-scale square at the cutoff */
                double row = v < 0 ? 63.0 + v * 196.0 / 256.0 : v * 49.0 / 63.0, hz = 30.0 * pow(2.0, row / 7.0);
                uint32_t half = (uint32_t)(FS / (2.0 * hz) + 0.5);
                for (i = 0; i < n; i++) x[i] = ((i / (half ? half : 1u)) & 1u) ? 262143 : -262144;
                break;
            }
            case 2: for (i = 0; i < n; i++) x[i] = i < 12000u ? 262143 : -262144; break;                       /* DC steps */
            case 3: for (i = 0; i < n; i++) x[i] = rnd() >> 5; break;                                          /* noise +-262144 */
            case 4: for (i = 0; i < n; i++) x[i] = (i % 97u) ? 0 : ((i / 97u) & 1u ? 262143 : -262144); break;  /* impulses */
            default: for (i = 0; i < n; i++) x[i] = (i / 3u) & 1u ? 262143 : -262144; break;                   /* 7.35 kHz square */
            }
            memset(tf, 0, sizeof tf);
            t->tf_run = 0;
            t->p[P_FILT] = (int16_t)v;
            memcpy(y, x, n * sizeof *x);
            in_pk = maxabs(x, n);
            for (pos = 0; pos + CTL <= n; pos += CTL) {
                tfilt_t b0 = tf[0];
                int32_t w0 = tf[0].w;
                int mode_was = tf[0].mode;
                int32_t blk[CTL];
                memcpy(blk, y + pos, sizeof blk);
                tf_call(t, y + pos, 0, CTL);
                if (!mode_was) { b0.z[0] = tf[0].mode == 1 ? clamp(blk[0], -524287, 524287) * 256 : clamp(blk[0], -524287, 524287) * 256; b0.z[1] = tf[0].mode == 1 ? b0.z[0] : 0; w0 = 0; }
                shadow_block(&b0, &tf[0], blk, y + pos, CTL, tf[0].mode == 2u, w0);
                ok_shadow &= !shadow_bad;
                shadow_bad = 0;
            }
            pk = maxabs(y, n);
            if (v < 0) worst_peak_lp = pk > worst_peak_lp ? pk : worst_peak_lp; else worst_peak_hp = pk > worst_peak_hp ? pk : worst_peak_hp;
            ok_peak &= pk < in_pk * 3;
            ok_fin &= llabs(tf[0].z[0]) < (1 << 29) && llabs(tf[0].z[1]) < (1 << 29);
            if (sig_n == 2) {                                        /* DC: LP follows it, HP ends at 0 (before the step back) */
                int32_t a = y[11000], b = y[LEN - 1];
                if (v < 0)
                    ok_dc &= abs(a - 262143) <= 2 && abs(b + 262144) <= 2;
                else
                    ok_dc &= abs(a) <= 2 && abs(b) <= 2;
            }
        }
    }
    ck("all 127 settings on 8 loud saws, full-scale squares, DC steps, noise, impulses: the 64-bit shadow agrees sample for sample "
       "(no 32-bit overflow, states and products inside bounds)", ok_shadow);
    ck("  the output stays below three times the input's peak (a square's swing is twice its peak: no wrap, no blow-up)", ok_peak);
    printf("trackfilter:   worst peak out / in: LP %d, HP %d (in 262144 at most)\n", worst_peak_lp, worst_peak_hp);
    ck("  states bounded (< 2^29); DC: LP passes it exactly (+-2), HP takes it out (+-2)", ok_fin && ok_dc);
    /* silence after a burst: no limit cycle, no DC left, every setting */
    for (v = -64; v <= 63; v++) {
        uint32_t n = 40960u;
        if (!v)
            continue;
        for (i = 0; i < n; i++)
            x[i] = i < 4096u ? (rnd() >> 5) : 0;
        memset(tf, 0, sizeof tf);
        t->tf_run = 0;
        t->p[P_FILT] = (int16_t)v;
        memcpy(y, x, n * sizeof *x);
        for (i = 0; i + CTL <= n; i += CTL)
            tf_call(t, y + i, 0, CTL);
        {
            int32_t m = maxabs(y + n - 4096u, 4096u);
            worst_rest = m > worst_rest ? m : worst_rest;
        }
    }
    printf("trackfilter:   0.93 s after a full-scale noise burst the worst setting is at %d (of 262144)\n", worst_rest);
    ck("after a full-scale noise burst and 0.8 s of silence every setting is silent (|y| <= 1): no limit cycle, no DC (the low cutoffs exact: 8 fractional bits)", worst_rest <= 1);
}

/* ------------------------------------------------------------------ 7 --- */
static void tft_chain(void)
{
    track_t *t = &trk[0];
    static int32_t l1[8192], l0[8192], d1[8192], d0[8192], r1[8192];
    uint32_t i, k;
    double h0, h1, hd0, hd1;
    for (k = 0; k < 2u; k++) {
        int32_t *l = k ? l1 : l0, *d = k ? d1 : d0;
        fresh(0, preset_by_name(0, "SAW LEAD"));
        t->p[P_VOICE] = V_POLY;
        t->p[P_DLY] = 100;
        t->p[P_ITYPE] = IT_CRUSH; t->p[P_IA] = 20; t->p[P_IB] = 60; t->p[P_IC] = 127; t->p[P_IMIX] = 127;   /* ~3 bits: lots of harmonics */
        t->p[P_FILT] = (int16_t)(k ? -48 : 0);
        t->p[P_SUS] = 127;
        trk_note_on(t, 60, 120);
        mixb(FS / 4u / CTL);
        for (i = 0; i < 8192u / CTL; i++) {
            mixb(1);
            for (uint32_t j = 0; j < CTL; j++) {
                l[i * CTL + j] = mix_l[j];
                d[i * CTL + j] = send_d[j];
                if (k) r1[i * CTL + j] = mix_r[j];
            }
        }
    }
    h0 = hf_share(l0, 8192u); h1 = hf_share(l1, 8192u);
    hd0 = hf_share(d0, 8192u); hd1 = hf_share(d1, 8192u);
    printf("trackfilter:   CRUSH ~3 bits into LP 75 %%: the mix's high-frequency share %.4f -> %.4f, the delay send's %.4f -> %.4f\n", h0, h1, hd0, hd1);
    ck("the filter is after the INSERT (a CRUSH's harmonics are taken out: LP 75 % takes the mix's high end down by 10x)", h1 < h0 / 10.0);
    ck("  and the sends take the filtered sound (the delay send's high end down by 10x as the mix's)", hd1 < hd0 / 10.0 && rms(d1, 8192u) > 100.0);
    /* SPREAD: left, right and the side all filtered */
    {
        double sl0, sl1, ss0, ss1;
        static int32_t L[2][4096], R[2][4096];
        for (k = 0; k < 2u; k++) {
            fresh(0, preset_by_name(0, "SAW LEAD"));
            t->p[P_VOICE] = V_UNISON;
            t->p[P_SPRD] = 127;
            t->p[P_FILT] = (int16_t)(k ? -48 : 0);
            t->p[P_SUS] = 127;
            t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0;
            trk_note_on(t, 52, 120);
            mixb(FS / 4u / CTL);
            for (i = 0; i < 4096u / CTL; i++) {
                mixb(1);
                for (uint32_t j = 0; j < CTL; j++) {
                    L[k][i * CTL + j] = mix_l[j];
                    R[k][i * CTL + j] = mix_r[j];
                }
            }
        }
        for (i = 0; i < 4096u; i++) { sig[i] = L[0][i] - R[0][i]; sg2[i] = L[1][i] - R[1][i]; }
        sl0 = hf_share(L[0], 4096u); sl1 = hf_share(L[1], 4096u);
        ss0 = hf_share(sig, 4096u); ss1 = hf_share(sg2, 4096u);
        printf("trackfilter:   UNISON, SPREAD 127, LP 75 %%: left's high-frequency share %.4f -> %.4f, the side's %.4f -> %.4f\n", sl0, sl1, ss0, ss1);
        ck("SPREAD 127 (UNISON): left and the side (left - right) both lose their high end", sl1 < sl0 / 5.0 && ss1 < ss0 / 5.0 && rms(sg2, 4096u) > 50.0);
    }
    /* the state runs out with the part: no stale state, the part is skipped after TF_TAIL blocks */
    {
        int ok = 1;
        for (k = 0; k < 2u; k++) {
            fresh(0, preset_by_name(0, "PLUCK"));
            t->p[P_FILT] = (int16_t)(k ? 50 : -64);
            trk_note_on(t, 48, 127);
            mixb(FS / 8u / CTL);
            trk_note_off(t, 48);
            mixb(2u * FS / CTL);
            ok &= abs(tf[0].z[0]) <= 2048 && abs(tf[0].z[1]) <= 2048;
        }
        ck("a part whose voices ended: the filter's state has run out before the part is skipped (|z| <= 8 of a 262144 full scale), LP and HP", ok);
        fresh(0, preset_by_name(0, "PLUCK"));
        t->p[P_FILT] = -64;
        trk_note_on(t, 48, 127);
        mixb(FS / 8u / CTL);
        trk_note_off(t, 48);
        mixb(2u * FS / CTL);
        t->p[P_FILT] = 0;
        mixb(1);
        ck("  FILT turned to 0 while the part is silent: at rest at once (tf_rest), no fade left for the next note", !t->tf_run && !tf[0].mode &&
           !tf[0].w && !tf[0].z[0]);
    }
}

/* ------------------------------------------------------------------ 8 --- */
static uint32_t legacy113(project_store_t *wire, const project_t *q)     /* a FUN10 as 0.2.0 wrote it: 113 values */
{
    project_store_t cur;
    uint8_t *b = wire->raw;
    uint32_t pos = 68u, i, t, hash32;
    if (!proj_pack(&cur, q))
        return 0;
    memcpy(wire, &cur, sizeof *wire);
    b[66] = 113;
    for (t = 0; t < NTRK; t++) {
        uint32_t at = 68u + t * (P_COUNT + 2u + NSTEP * 9u);
        for (i = 0; i < 113u; i++)
            b[pos++] = cur.raw[at + (i < P_FILT ? i : i + 1u)];
        memcpy(b + pos, cur.raw + at + P_COUNT, 2u + NSTEP * 9u);
        pos += 2u + NSTEP * 9u;
    }
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    {
        motion_store_t motion = q->motion;
        for (i = 0; i < motion.count; i++) {
            uint32_t id = MOTION_ID(&motion.event[i]);
            if (id >= P_E0 && id < P_COUNT)
                motion.event[i].param = (uint8_t)((motion.event[i].param & MOTION_LOCK) | (id - 1u));
        }
        memcpy(b + pos, &motion, sizeof motion); pos += sizeof motion;
    }
    memset(b + pos, 0, PROJ_MIDI_OFF - pos);
    hash32 = proj_hash(b, sizeof *wire - 4u);
    memcpy(b + sizeof *wire - 4u, &hash32, 4);
    return 1;
}
static void tft_stored(void)
{
    static project_t a, b;
    static project_store_t st, old;
    up_rec_t r;
    int16_t v[P_COUNT], def[P_COUNT];
    uint32_t i, k;
    int ok;
    fresh(0, 0);
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_FILT] = (int16_t)(-64 + (int)k * 37);          /* -64, -27, 10, 47 */
        trk[k].p[P_E7] = (int16_t)(3 + k);
    }
    motion_set_event(&trk[1], 3, P_FILT, -50);
    motion_set_lock(&trk[2], 5, P_FILT, 30);
    motion_set_event(&trk[3], 2, P_E5, 77);
    project_capture(&a);
    ok = proj_pack(&st, &a) && st.raw[66] == P_COUNT && proj_import(&b, &st, sizeof st) && !memcmp(&a, &b, sizeof a);
    for (k = 0; k < NTRK; k++)
        ok &= b.t[k].p[P_FILT] == (int16_t)(-64 + (int)k * 37) && b.t[k].p[P_E7] == (int16_t)(3 + k);
    ok &= motion_find(&trk[1], 3, P_FILT) >= 0;
    ck("project: FUN10 round trip of the four tracks' FILT (-64 .. 47), its automation and a lock, E values in place", ok);
    /* a FUN10 of 0.2.0 (113 values): the filter OFF, the engine values and the motion on them where they were */
    ok = legacy113(&old, &a) && proj_import(&b, &old, sizeof old);
    for (k = 0; k < NTRK; k++)
        ok &= b.t[k].p[P_FILT] == 0 && b.t[k].p[P_E7] == (int16_t)(3 + k) && b.t[k].p[P_ESYNC] == a.t[k].p[P_ESYNC] &&
              b.t[k].p[P_E0] == a.t[k].p[P_E0];
    {
        int found = 0;
        for (i = 0; i < b.motion.count; i++)
            if (MOTION_ID(&b.motion.event[i]) == P_E5 && b.motion.event[i].value == 77) found = 1;
        ok &= found;
    }
    ck("a FUN10 of 0.2.0 (113 values) loads with FILT OFF, ENGINE values and their motion in place (ids moved up by one)", ok);
    /* user presets */
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
    for (i = 0; i < P_COUNT; i++) { up_set_value(&r, i, trk[0].p[i]); def[i] = TP[i].def; }
    up_params(&r, v, def);
    ok = v[P_FILT] == -64 && v[P_E7] == 3;
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = 113;
    for (i = 0; i < 105u; i++) up_set_value(&r, i, 5);
    for (i = 0; i < 8u; i++) up_set_value(&r, 105u + i, (int16_t)(40 + i));
    up_params(&r, v, def);
    ok &= v[P_FILT] == def[P_FILT] && v[P_FILT] == 0 && v[P_E0] == 40 && v[P_E7] == 47 && v[104] == 5;
    ck("user presets: a record of 114 keeps the filter; one of 113 (0.2.0) gets it OFF, its engine values in place", ok);
    /* a sound load: a user preset stores the filter and loads its own */
    ui_power_on();
    {
        track_t *t = TSEL;
        t->p[P_FILT] = -22;
        up_store(5, "FILTER");
        up_values(up_rec(5), v);
        t->p[P_FILT] = 17;
        up_load(5);
        ck("a user preset stores the filter and loads its own", v[P_FILT] == -22 && t->p[P_FILT] == -22);
        t->p[P_FILT] = 17;
        apply_preset_to(t, 2);
        ck("a factory preset load sets FILT OFF (no preset has one) with FX HOLD off ..", t->p[P_FILT] == 0);
        menu_put(MI_FXHOLD, 1);
        t->p[P_FILT] = 17;
        apply_preset_to(t, 3);
        ck(".. and keeps it with FX HOLD on", t->p[P_FILT] == 17);
        sound_init();
        ck("SOUND INIT starts from OFF, FX HOLD or not; CLEAN INIT too", t->p[P_FILT] == 0);
        t->p[P_FILT] = 17;
        clean_engine();
        ck("  CLEAN INIT", t->p[P_FILT] == 0);
        menu_put(MI_FXHOLD, 0);
    }
    /* MIDI LEARN: a CC sweeps the knob over its whole range */
    {
        track_t *t = &trk[1];
        ui_power_on();
        memset(favorites.factory[14], 0, 32);
        ok = ml_learn(40, 1, P_FILT) == 1u && ml_get(0) == (40u | 1u << 7 | (P_FILT + 1u) << 9) && ml_id(ml_code(P_FILT)) == P_FILT;
        queued(0xB0, 40, 0, 1);
        ok &= t->p[P_FILT] == -64;
        queued(0xB0, 40, 127, 1);
        ok &= t->p[P_FILT] == 63;
        queued(0xB0, 40, 64, 1);
        ok &= abs(t->p[P_FILT]) <= 1;
        ck("MIDI LEARN: a CC reaches FILT (code 106), 0 = -64 (LP), 127 = +63 (HP), the middle OFF", ok);
    }
}

/* ------------------------------------------------------------------ 9 --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static double cost(int v, uint32_t sprd)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    track_t *t = &trk[0];
    uint32_t i, rep;
    double best = 1e30;
    fresh(0, preset_by_name(0, "SOFT PAD"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    t->p[P_SPRD] = (int16_t)sprd;
    t->p[P_FILT] = (int16_t)v;
    for (i = 0; i < 8u; i++)
        trk_note_on(t, NOTES[i], 100);
    mixb(FS / 4u / CTL);
    for (rep = 0; rep < 3u; rep++) {
        uint64_t i0 = instr_now();
        double ipc;
        mixb(FS / 2u / CTL);
        ipc = (double)(instr_now() - i0) / (FS / 2u);
        best = ipc < best ? ipc : best;
    }
    return best;
}
static void tft_cost(void)
{
    double c0, cl, ch, cs;
    if (!instr_now()) {
        printf("trackfilter: cost: no instruction counter on this host (proc_pid_rusage), skipped\n");
        return;
    }
    c0 = cost(0, 0);
    cl = cost(-40, 0) - c0;
    ch = cost(40, 0) - c0;
    cs = cost(-40, 100) - cost(0, 100);
    printf("trackfilter: cost, ANALOG SOFT PAD 8 voices on track 1: OFF %.0f instructions / sample; LP adds %.1f (%.1f %%), HP %.1f, with SPREAD (b and d) %.1f\n",
           c0, cl, cl * 100 / c0, ch, cs);
    ck("cost: a fixed small cost per track, below 6 % of an 8-voice part (below 10 % with SPREAD's two channels)", cl < c0 * 0.06 && ch < c0 * 0.06 && cs < c0 * 0.10);
}

/* ------------------------------------------------------------------ 10 -- */
static FILE *demo_open(const char *dir, const char *name, uint32_t frames)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (f)
        wav_hdr(f, frames);
    return f;
}
/* the acid loop with FILT swept by the track's own automation: LP down and back, then HP up and back (bar 1..4) */
static int demos(const char *dir)
{
    static const uint8_t LINE[16] = {36, 36, 48, 36, 0, 39, 36, 46, 36, 0, 48, 43, 36, 39, 0, 41};
    uint32_t i, k, frames = 4u * 4u * FS / 2u / CTL * CTL;
    FILE *f;
    fresh(0, 0);
    host_preset(&trk[1], ENGI_DRUM, 0);
    for (i = 0; i < 16u; i++) {
        uint8_t nt = LINE[i];
        put_step(&trk[0], i, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST, (i % 4u) == 2u ? SF_ACCENT : 0u);
        put_step(&trk[1], i, 0, 0, ST_NOTE, 0);
        trk[1].step[i].hit = (uint8_t)((i % 4u == 0u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 8u : 0u));
        trk[1].step[i].vel = 100;
    }
    trk[0].p[P_SLEN] = trk[1].p[P_SLEN] = 16;
    trk[0].seq_active = trk[1].seq_active = 1;
    trk[0].p[P_REV] = trk[1].p[P_REV] = 20;
    if (!(f = demo_open(dir, "filter_sweep", frames)))
        return 0;
    seq_start();
    for (k = 0; k < frames / CTL; k++) {
        double bar = (double)k * CTL / FS / 2.0;                  /* bars at 120 BPM */
        trk[0].p[P_FILT] = (int16_t)(bar < 1 ? 0 : bar < 2 ? -(int)((bar - 1) * 64) : bar < 3 ? -(int)((3 - bar) * 64) : (int)((bar - 3) * 63));
        mixb(1);
        for (i = 0; i < CTL; i++)
            wav_put(f, out_buf[2u * i], out_buf[2u * i + 1u]);
    }
    seq_stop();
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    tft_parameter();
    tft_off();
    tft_response();
    tft_smooth();
    tft_blocks_side();
    tft_extremes();
    tft_chain();
    tft_stored();
    tft_cost();
    if (argc > 1)
        printf("trackfilter: %d demo in %s (the acid loop, LP down and back, HP up and back)\n", demos(argv[1]), argv[1]);
    if (bad) {
        printf("TRACKFILTER TEST FAILED (%d)\n", bad);
        return 1;
    }
    printf("trackfilter: all ok\n");
    return 0;
}
