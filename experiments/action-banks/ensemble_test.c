/* SPDX-License-Identifier: GPL-3.0-only */
/* ENSEMBLE engine (eng_ensemble.c) on the real firmware in the host harness: the tables, all eight parameters at both ends, the timbre morph, the
 * ensemble and its bypass, engine switches and the tail, voice stealing, SPREAD, Pair, HOME / Random Morph / Journey, CC74, project save and reload,
 * and a four-track stress at the voice cap. The measures are plausibility checks of the DSP (energy shares, modulation depth), not a judgement of
 * how it sounds; nothing here is a hardware measurement. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN ensemble_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <math.h>
#define SEG 44100u
static float buf[SEG * 8u];
/* render ms of audio into buf (left channel); the float of each sample must be finite and inside the 16-bit range the firmware keeps */
static uint32_t grab(uint32_t ms)
{
    uint32_t n = 0;
    while (ms--) {
        web_render(44);
        for (uint32_t k = 0; k < 44u; k++) {
            CHECK(isfinite(out_l[k]) && isfinite(out_r[k]) && fabsf(out_l[k]) <= 1.0f && fabsf(out_r[k]) <= 1.0f);
            if (n < sizeof buf / sizeof buf[0])
                buf[n++] = out_l[k];
        }
    }
    return n;
}
typedef struct { double rms, peak, hf, lf, dc, mod, b1, b2, b3, lfe; } meas_t;
/* hf: the first difference's share of the energy; lf / lfe: a slow one-pole's share / its absolute energy; b1 b2 b3: the shares below about 200 Hz, up to
 * about 1 kHz and above (one-pole splits); mod: the 20 ms RMS windows' spread / mean */
static meas_t measure(uint32_t from, uint32_t to)
{
    meas_t m = {0};
    double e = 0, d2 = 0, lp = 0, lp2 = 0, prev = 0, s = 0, win[400], la = 0, lb = 0, e1 = 0, e2 = 0, e3 = 0;
    uint32_t nw = 0, wn = 0;
    double we = 0;
    for (uint32_t i = from; i < to; i++) {
        double x = buf[i];
        e += x * x; d2 += (x - prev) * (x - prev); prev = x; s += x;
        lp += (x - lp) * 0.02; lp2 += lp * lp;
        la += (x - la) * 0.03; lb += (x - lb) * 0.15;
        e1 += la * la; e2 += (lb - la) * (lb - la); e3 += (x - lb) * (x - lb);
        if (fabs(x) > m.peak) m.peak = fabs(x);
        we += x * x;
        if (++wn == 882u) { if (nw < 400u) win[nw++] = sqrt(we / 882.0); we = 0; wn = 0; }
    }
    uint32_t n = to - from ? to - from : 1u;
    m.rms = sqrt(e / n); m.dc = s / n;
    m.hf = e > 0 ? d2 / (4.0 * e) : 0; m.lf = e > 0 ? lp2 / e : 0; m.lfe = lp2 / n;
    { double t = e1 + e2 + e3; if (t > 0) { m.b1 = e1 / t; m.b2 = e2 / t; m.b3 = e3 / t; } }
    if (nw > 2u) { double mean = 0, var = 0; for (uint32_t i = 0; i < nw; i++) mean += win[i]; mean /= nw;
        for (uint32_t i = 0; i < nw; i++) var += (win[i] - mean) * (win[i] - mean); m.mod = mean > 0 ? sqrt(var / nw) / mean : 0; }
    return m;
}
static uint32_t voices_on(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active; return n; }
static void chord(uint32_t tr, int on)
{
    static const uint8_t notes[] = {52, 55, 59, 64};
    for (uint32_t i = 0; i < 4u; i++) { if (on) trk_note_on(&trk[tr], notes[i], 100); else trk_note_off(&trk[tr], notes[i]); }
}
static void setup(uint32_t preset)
{
    web_pair_panic(); run_ms(300);
    trk_all_off(&trk[0]); run_ms(900);
    host_preset(&trk[0], ENGI_ENSEMBLE, preset);
    trk[0].p[P_LEVEL] = 100; trk[0].p[P_REV] = 0; trk[0].p[P_CHOR] = 0; trk[0].p[P_DLY] = 0; trk[0].p[P_DIST] = 0; trk[0].p[P_ITYPE] = 0;
    trk[0].p[P_FILT] = 0; trk[0].p[P_SPRD] = 0; trk[0].p[P_PAN] = 0;
    run_ms(100);
}
/* one chord of a preset with the eight values overridden by e[] (-1 = keep), 1.5 s settle and 2 s measured */
static meas_t render_with(uint32_t preset, const int *e)
{
    setup(preset);
    for (uint32_t k = 0; k < 8u; k++) if (e && e[k] >= 0) trk[0].p[P_E0 + k] = (int16_t)e[k];
    chord(0, 1); run_ms(1500);
    uint32_t n = grab(2000);
    chord(0, 0);
    return measure(0, n);
}
static uint64_t hash_buf(uint32_t n) { uint64_t h = 1469598103934665603ull; for (uint32_t i = 0; i < n; i++) { int32_t v = (int32_t)(buf[i] * 32767.0f); h = (h ^ (uint32_t)v) * 1099511628211ull; } return h; }

int main(void)
{
    web_nor_erase(); web_boot(); run_ms(1400); normal_off(); pair_disable();

    /* 1. the tables: the engine's place and its eight controls */
    CHECK(NENGINES == 18u && ENGI_ENSEMBLE == 17u && ENGINES[ENGI_ENSEMBLE] == &ENG_ENSEMBLE && eng_ok(ENGI_ENSEMBLE));
    CHECK(!strcmp(ENG_ENSEMBLE.name, "ENSEMB") && !strcmp(N_ENGNAME[ENGI_ENSEMBLE], "ENSEMB"));   /* the display name fits the PRESETS cell (the UI lint) */
    CHECK(ENGINES[ENGI_HADES] == &ENG_HADES && ENGINES[ENGI_DRONE] == &ENG_DRONE && ENGINES[ENGI_SEM] == &ENG_SEM);   /* nothing renumbered */
    CHECK(eng_vis(NENG_SHOWN - 1u) == ENGI_ENSEMBLE && eng_rank(ENGI_ENSEMBLE) == NENG_SHOWN - 1u);
    CHECK(ENG_ENSEMBLE.npresets == 14u && !strcmp(ENG_ENSEMBLE.presets[0].name, "VINTAGE STRINGS") && !strcmp(ENG_ENSEMBLE.presets[1].name, "GLASS CHOIR")
          && !strcmp(ENG_ENSEMBLE.presets[2].name, "DUSK ORCHESTRA") && !strcmp(ENG_ENSEMBLE.presets[3].name, "ABYSS PAD")
          && !strcmp(ENG_ENSEMBLE.presets[4].name, "PROFILE A") && !strcmp(ENG_ENSEMBLE.presets[13].name, "RUNNING PAD"));   /* the order is the stored order; the rest: ensemble_presets_test */
    {   static const char *const L[8] = {"REG", "MORPH", "TONE", "DRIFT", "ENS", "RATE", "TEXT", "MIX"};
        for (uint32_t k = 0; k < 8u; k++) {
            const param_desc_t *d = &ENG_ENSEMBLE.edit[k];
            CHECK(!strcmp(d->label, L[k]) && d->fmt == F_PCT && d->min == 0 && d->max == 127 && d->def >= 0 && d->def <= 127);
            for (uint32_t pr = 0; pr < 4u; pr++) CHECK(ENG_ENSEMBLE.presets[pr].e[k] >= 0);
        }
        CHECK(ENG_ENSEMBLE.knob[0] == P_E1 && ENG_ENSEMBLE.knob[1] == P_E4 && ENG_ENSEMBLE.knob[2] == P_E2 && ENG_ENSEMBLE.knob[3] == P_E7);
        CHECK(ENG_ENSEMBLE.mem == sizeof(ens_t) && ENG_ENSEMBLE.mem <= ENG_MEM_PART && sizeof(ens_t) == 4096u + 40u);
    }

    /* 2. every preset sounds: level, no DC, nothing near full scale, silence after the release */
    for (uint32_t pr = 0; pr < 4u; pr++) {
        meas_t m = render_with(pr, 0);
        printf("preset %u %-16s rms %.4f peak %.3f hf %.3f lf %.3f mod %.3f\n", pr, ENG_ENSEMBLE.presets[pr].name, m.rms, m.peak, m.hf, m.lf, m.mod);
        CHECK(m.rms > 0.01 && m.peak > 0.03 && m.peak < 0.95 && fabs(m.dc) < 0.01);
        { uint32_t g = 0; while (voices_on(&trk[0]) && g++ < 200u) run_ms(100); CHECK(!voices_on(&trk[0])); }   /* the preset's own release */
        run_ms(1000);
        uint32_t n = grab(300);
        CHECK(voices_on(&trk[0]) == 0u && measure(0, n).peak < 0.0005);
    }

    /* 3. all eight parameters at both ends (the other seven at the VINTAGE STRINGS values): sound, finite, and an audible effect */
    {
        static const int lo[8][8] = {{0,-1,-1,-1,-1,-1,-1,-1},{-1,0,-1,-1,-1,-1,-1,-1},{-1,-1,0,-1,-1,-1,-1,-1},{-1,-1,-1,0,-1,-1,-1,-1},
                                     {-1,-1,-1,-1,0,-1,-1,-1},{-1,-1,-1,-1,-1,0,-1,-1},{-1,-1,-1,-1,-1,-1,0,-1},{-1,-1,-1,-1,-1,-1,-1,0}};
        meas_t a[8], b[8];
        for (uint32_t k = 0; k < 8u; k++) {
            int e[8]; for (uint32_t j = 0; j < 8u; j++) e[j] = -1;
            e[k] = 0; (void)lo; a[k] = render_with(0, e);
            e[k] = 127; b[k] = render_with(0, e);
            printf("param %-5s: 0 -> rms %.4f bands %.2f/%.2f/%.2f mod %.3f | 127 -> rms %.4f bands %.2f/%.2f/%.2f mod %.3f\n", ENG_ENSEMBLE.edit[k].label,
                   a[k].rms, a[k].b1, a[k].b2, a[k].b3, a[k].mod, b[k].rms, b[k].b1, b[k].b2, b[k].b3, b[k].mod);
            if (k != 7u) CHECK(a[k].rms > 0.005 && b[k].rms > 0.005);                 /* (MIX 0 is the dry sound: also sounding) */
            CHECK(a[k].peak < 0.98 && b[k].peak < 0.98);
        }
        CHECK(b[0].lfe > a[0].lfe * 1.3);                                           /* REG: the 16' and 4' registers add energy (the sub adds low end) */
        CHECK(b[2].b3 > a[2].b3 * 1.5 && b[2].rms > a[2].rms);                       /* TONE: brighter */
        CHECK(fabs(b[1].b1 - a[1].b1) + fabs(b[1].b2 - a[1].b2) + fabs(b[1].b3 - a[1].b3) > 0.03);   /* MORPH: another timbre */
        CHECK(b[4].mod > a[4].mod && b[5].mod > a[5].mod);                           /* ENS / RATE: the ensemble moves the level more */
        CHECK(fabs(b[3].rms - a[3].rms) > 0.001 && fabs(b[6].rms - a[6].rms) > 0.001);   /* DRIFT and TEXT change the sound */
    }

    /* 4. the timbre morph: STRINGS -> CHOIR -> DARK are three different spectra, and the steps in between are smooth (no jump in level) */
    {
        double bd[5][3], rms[5]; static const int mv[5] = {0, 32, 64, 96, 127};
        for (uint32_t i = 0; i < 5u; i++) { int e[8] = {-1,-1,-1,-1,-1,-1,-1,-1}; e[1] = mv[i]; meas_t m = render_with(0, e); bd[i][0] = m.b1; bd[i][1] = m.b2; bd[i][2] = m.b3; rms[i] = m.rms;
            printf("morph %3d: bands %.3f/%.3f/%.3f rms %.4f\n", mv[i], m.b1, m.b2, m.b3, m.rms); }
#define BDIST(x, y) (fabs(bd[x][0] - bd[y][0]) + fabs(bd[x][1] - bd[y][1]) + fabs(bd[x][2] - bd[y][2]))
        CHECK(BDIST(0, 2) > 0.02 && BDIST(2, 4) > 0.02 && BDIST(0, 4) > 0.02);       /* three distinct spectra */
        for (uint32_t i = 1; i < 5u; i++) { double r = rms[i] / rms[i - 1]; CHECK(r > 0.35 && r < 2.9); }
    }

    /* 5. the ensemble itself, on a synthetic signal (deterministic: no voices, no random seeds): MIX 0 leaves every sample alone whatever ENS and RATE
     * say; ENS 0 is a static comb of the three resting taps; ENS 127 moves the taps; after an impulse the taps run out inside the tail */
    {
        static int32_t in[CTL * 64u], out[CTL * 64u];
        uint32_t i, blk, rs = 12345u;
        web_pair_panic(); run_ms(300); trk_all_off(&trk[0]); run_ms(900);
        host_preset(&trk[0], ENGI_ENSEMBLE, 0); run_ms(100);
        set_engine_of(&trk[0], ENGI_ENSEMBLE); trk[0].engine = ENGI_ENSEMBLE; eng_mem_claim(&trk[0]);
        for (i = 0; i < CTL * 64u; i++) { rs = rs * 1664525u + 1013904223u; in[i] = (int32_t)(rs >> 12) - 262144; }
        for (uint32_t pass = 0; pass < 3u; pass++) {
            ens_t *s = (ens_t *)ENG_MEM(0);
            memset(s, 0, sizeof *s);
            trk[0].p[P_E4] = pass == 2u ? 127 : 0; trk[0].p[P_E5] = pass == 2u ? 127 : 0; trk[0].p[P_E7] = pass ? 127 : 0;
            if (!pass) { trk[0].p[P_E4] = 127; trk[0].p[P_E5] = 127; }                  /* MIX 0 with ENS and RATE up */
            memcpy(out, in, sizeof out);
            for (blk = 0; blk < 64u; blk++) ens_post(&trk[0], out + blk * CTL, CTL);
            if (pass == 0u) CHECK(!memcmp(out, in, sizeof out));                         /* bypass: bit for bit */
            if (pass == 1u) {                                                           /* ENS 0, MIX 127: (x[n-265] + x[n-375] + x[n-485]) * 26000 / 65536 */
                uint32_t bad = 0;
                for (i = 600; i < CTL * 64u; i++) { int32_t e = (int32_t)(((int64_t)(in[i - 265] + in[i - 375] + in[i - 485]) * ENS_WET_Q16) >> 16), d = out[i] - e; if (d < -128 || d > 128) bad++; }                /* (MIX 127 is 65532 / 65536, 1 / 3 is 21845 / 65536: a few parts in 10^5) */
                CHECK(bad == 0u);
            }
            if (pass == 2u) {                                                           /* moving taps: not the static comb, bounded by the input's range */
                uint32_t diff = 0; int32_t mx = 0;
                for (i = 700; i < CTL * 64u; i++) { int32_t e = (int32_t)(((int64_t)(in[i - 265] + in[i - 375] + in[i - 485]) * ENS_WET_Q16) >> 16); if (out[i] != e) diff++; if (abs(out[i]) > mx) mx = abs(out[i]); }
                printf("moving taps: %u samples differ from the static comb, max |out| %d\n", diff, mx);
                CHECK(diff > 1000u && mx <= 940000);                      /* (the input spans -262144 .. 786431: the taps never exceed it) */
            }
        }
        {   /* the tail: an impulse, then silence: the output is zero once the longest tap (485 + 127 samples) has passed */
            ens_t *s = (ens_t *)ENG_MEM(0);
            memset(s, 0, sizeof *s);
            trk[0].p[P_E4] = 127; trk[0].p[P_E5] = 127; trk[0].p[P_E7] = 127;
            memset(out, 0, sizeof out); out[0] = 400000;
            uint32_t last = 0;
            for (blk = 0; blk < 64u; blk++) { ens_post(&trk[0], out + blk * CTL, CTL); for (i = 0; i < CTL; i++) if (out[blk * CTL + i]) last = blk * CTL + i; }
            printf("ensemble tail: the last non-zero sample after the impulse is %u (%.1f ms), the engine keeps running %u blocks\n", last, last * 1000.0 / 44100.0, ENS_TAIL_BLOCKS);
            CHECK(last > 400u && last < ENS_TAIL_BLOCKS * CTL);
        }
        trk[0].p[P_E4] = 64; trk[0].p[P_E5] = 50; trk[0].p[P_E7] = 100;
    }

    /* 6. the REV send is fed by the ensembled part: with the send up a reverb tail rings after the voices are free, and nothing is lost or unbounded */
    {
        setup(0); trk[0].p[P_REV] = 100;
        chord(0, 1); run_ms(1500); uint32_t n = grab(500); CHECK(measure(0, n).rms > 0.01); chord(0, 0);
        { uint32_t g = 0; while (voices_on(&trk[0]) && g++ < 200u) run_ms(100); }
        run_ms(200); n = grab(400);
        meas_t m = measure(0, n);
        printf("REV send after the release: rms %.5f\n", m.rms);
        CHECK(m.rms > 0.0001 && m.peak < 0.5);
        run_ms(6000);
    }

    /* 7. engine switches: the ring is cleared (zeroed state), the old tail does not return, and no engine's output depends on the ring */
    {
        setup(1);
        chord(0, 1); run_ms(1200);
        ens_t *s = (ens_t *)ENG_MEM(0);
        CHECK(eng_mem_owner[0] == ENGI_ENSEMBLE + 1u && s->live == 1u);
        { int32_t any = 0; for (uint32_t i = 0; i < ENS_RING; i++) any |= s->ring[i]; CHECK(any != 0); }
        chord(0, 0); run_ms(1200);
        host_preset(&trk[0], ENGI_SEM, 2);                                           /* another engine: it claims the region */
        run_ms(60);
        chord(0, 1); run_ms(400);
        CHECK(trk[0].engine == ENGI_SEM && eng_mem_owner[0] == ENGI_ENSEMBLE + 1u);  /* SEM has no engine memory: the owner stays, ens_post does not run */
        chord(0, 0); run_ms(1500);
        {   /* back to ENSEMBLE with nothing in between (SEM left the region as it was): the old ring is not played into the new notes */
            host_preset(&trk[0], ENGI_ENSEMBLE, 1); run_ms(60);
            chord(0, 1); run_ms(2);
            uint32_t nz = 0; for (uint32_t i = 0; i < ENS_RING; i++) nz += s->ring[i] != 0;
            CHECK(nz < 200u);                                                           /* only the last few blocks are in it */
            chord(0, 0); run_ms(2500);
        }
        host_preset(&trk[0], ENGI_PHYS, 0);                                          /* an engine WITH memory claims it: zeroed */
        run_ms(60);
        CHECK(eng_mem_owner[0] == ENGI_PHYS + 1u);
        trk_note_on(&trk[0], 60, 100); run_ms(300); trk_all_off(&trk[0]); run_ms(1500);
        host_preset(&trk[0], ENGI_ENSEMBLE, 0);                                      /* back: a fresh ring */
        run_ms(100);
        chord(0, 1); run_ms(100);
        CHECK(eng_mem_owner[0] == ENGI_ENSEMBLE + 1u && s->live == 1u && s->w < 16000u);
        chord(0, 0); run_ms(1500);
    }

    /* 8. the tail: after the last voice the taps run out and the part goes silent (no stuck sound, no cut earlier than the voices' own release) */
    {
        setup(0);
        chord(0, 1); run_ms(800); chord(0, 0);
        uint32_t seen_active = 0, quiet = 0, idle_blocks = 0;
        for (uint32_t i = 0; i < 3000u; i++) {
            web_render(44);
            double pk = 0; for (uint32_t k = 0; k < 44u; k++) { double a = fabs(out_l[k]); if (a > pk) pk = a; }
            if (voices_on(&trk[0])) seen_active = i;
            else if (pk < 1e-4) { quiet++; if (!idle_blocks) idle_blocks = i; }
        }
        CHECK(seen_active > 0u && quiet > 1000u && trk[0].tail == 0u);
        printf("tail: voices free after %u ms, then %u ms of near silence\n", seen_active, quiet);
    }

    /* 9. voice cap and stealing: POLY holds at most 6 voices on this engine; every new note takes one, the sound stays finite */
    {
        setup(0);
        for (uint32_t i = 0; i < 8u; i++) { trk_note_on(&trk[0], 48 + i * 2u, 100); run_ms(20); CHECK(voices_on(&trk[0]) <= 6u); }
        CHECK(voices_on(&trk[0]) == 6u);
        uint32_t n = grab(400); CHECK(measure(0, n).peak < 0.98);
        trk_all_off(&trk[0]); run_ms(2000); CHECK(voices_on(&trk[0]) == 0u);
    }

    /* 10. SPREAD is ignored (the ensemble makes its own width, mono here): left and right are identical at PAN 0; SEM with the same SPRD spreads */
    {
        double d1 = 0, d2 = 0;
        setup(0); trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SPRD] = 100; chord(0, 1); run_ms(800);
        for (uint32_t i = 0; i < 400u; i++) { web_render(44); for (uint32_t k = 0; k < 44u; k++) d1 += fabs(out_l[k] - out_r[k]); }
        chord(0, 0); run_ms(3000);
        host_preset(&trk[0], ENGI_SEM, 0); trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SPRD] = 100; trk[0].p[P_REV] = 0; trk[0].p[P_CHOR] = 0; trk[0].p[P_PAN] = 0; run_ms(100);
        chord(0, 1); run_ms(800);
        for (uint32_t i = 0; i < 400u; i++) { web_render(44); for (uint32_t k = 0; k < 44u; k++) d2 += fabs(out_l[k] - out_r[k]); }
        chord(0, 0); run_ms(3000);
        printf("SPREAD: |L-R| ENSEMBLE %.6f, SEM %.3f\n", d1, d2);
        CHECK(d1 < 1e-6 && d2 > 1.0);
    }

    /* 11. Pair Morph accepts the engine (pitched notes, no key map, no own envelope) and sounds */
    {
        CHECK(pair_engine_ok(ENGI_ENSEMBLE));
        web_pair_panic(); run_ms(500);
        CHECK(web_pair_prepare(0, 2, ENGI_ENSEMBLE, ENGI_SEM));
        run_ms(100);
        trk_note_on(&trk[0], 52, 100); run_ms(1200);
        uint32_t n = grab(500);
        meas_t m = measure(0, n);
        CHECK(m.rms > 0.005 && m.peak < 0.98);
        printf("pair ENSEMBLE+SEM: rms %.4f\n", m.rms);
        trk_all_off(&trk[0]); run_ms(1500); web_pair_panic(); run_ms(800);
    }

    /* 12. HOME, Random Morph and Journey: all eight controls are morph-safe, and a journey leg moves them (and the sound) exactly */
    {
        setup(0);
        for (uint32_t k = 0; k < 8u; k++) {
            hp_ref_t r = hp_track_ref(0, P_E0 + k, 0xFFFF);
            CHECK(hp_rm_safe(r, hp_desc(r)) && hp_rm_random_allowed(r, hp_desc(r)));
        }
        hp_apply(0, 0);
        CHECK(web_home_assign(0, 0, 0, HP_TRACK, 0, P_E1) && web_home_assign(0, 0, 1, HP_TRACK, 0, P_E4));
        CHECK(web_home_assign(0, 0, 2, HP_TRACK, 0, P_E2) && web_home_assign(0, 0, 3, HP_TRACK, 0, P_E7));
        song.sel = 0; go_home();
        for (uint32_t t = 0; t < NTRK; t++) { hp_eng[t].scope = HP_RM_TRACK; hp_eng[t].bars = 1; hp_eng[t].shape = HP_RM_LINEAR; }
        hp_rm_snapshot_clear(); hp_rm_reset_all();
        trk[0].p[P_E1] = 0; trk[0].p[P_E4] = 20; trk[0].p[P_E2] = 40; trk[0].p[P_E7] = 100; action_snap_slot = 0; CHECK(hp_rm_snapshot_store(0));
        trk[0].p[P_E1] = 120; trk[0].p[P_E4] = 110; trk[0].p[P_E2] = 100; trk[0].p[P_E7] = 60; action_snap_slot = 1; CHECK(hp_rm_snapshot_store(1));
        CHECK(hp_rm_snapshot_count(0, 0) == 4u && hp_rm_snapshot_count(0, 1) == 4u);
        trk[0].p[P_E1] = 60; trk[0].p[P_E4] = 60; trk[0].p[P_E2] = 60; trk[0].p[P_E7] = 60;
        chord(0, 1); run_ms(300);
        song.g[G_BPM] = 240; hp_jn[0].wait = 1;   /* a rest of a bar after each arrival: phase 1 shows it */ action_snap_slot = 0;
        hp_journey_cc(HP_VFIRST + 8u + HJ_PLAY, 0); hp_journey_cc(HP_VFIRST + 8u + HJ_PLAY, 127);
        uint32_t g = 0; while (!(hp_jn[0].on && hp_jn[0].phase == 1u) && g++ < 6000u) run_ms(1);
        CHECK(hp_jn[0].on && trk[0].p[P_E1] == 0 && trk[0].p[P_E4] == 20 && trk[0].p[P_E2] == 40 && trk[0].p[P_E7] == 100);    /* S1 reached exactly */
        chord(0, 0); hp_jn[0].on = 0; hp_rm_reset_all(); run_ms(1500);
    }

    /* 13. MIDI CC74 sets TONE (E2); the preset's HOME knobs are MORPH ENS TONE MIX */
    {
        setup(0); midi(0xB0, 74, 0); CHECK(trk[0].p[P_E2] == 0); midi(0xB0, 74, 127); CHECK(trk[0].p[P_E2] == 127);
        midi(0xB0, 71, 64); CHECK(trk[0].p[P_E2] == 127);       /* CC71 (RES): none on this engine */
    }

    /* 14. a project keeps the engine and its eight controls */
    {
        setup(2);
        for (uint32_t k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = (int16_t)(11 + 13 * k);
        project_save(0); run_ms(300);
        trk[0].p[P_E0 + 3] = 0; set_engine_of(&trk[0], 0); trk[0].engine = trk[0].eng_req; run_ms(100);
        project_load(0); run_ms(300);
        CHECK(trk[0].eng_req == ENGI_ENSEMBLE);
        for (uint32_t k = 0; k < 8u; k++) CHECK(trk[0].p[P_E0 + k] == 11 + 13 * (int)k);
    }

    /* 15. stress: four tracks of the engine, the voice cap each (the global 8 voices share them), long run: finite, bounded, then silent */
    {
        web_pair_panic(); run_ms(300);
        for (uint32_t t = 0; t < NTRK; t++) { host_preset(&trk[t], ENGI_ENSEMBLE, t); trk[t].p[P_LEVEL] = 90; trk[t].p[P_REV] = 40; trk[t].p[P_FILT] = 0; }
        run_ms(100);
        for (uint32_t t = 0; t < NTRK; t++) for (uint32_t i = 0; i < 6u; i++) trk_note_on(&trk[t], 40 + t * 5u + i * 3u, 100);
        double pk = 0; uint32_t over = 0;
        for (uint32_t s = 0; s < 8u; s++) {
            uint32_t n = grab(1000); meas_t m = measure(0, n);
            if (m.peak > pk) pk = m.peak;
            if (m.peak > 0.99) over++;
            for (uint32_t t = 0; t < NTRK; t++) CHECK(voices_on(&trk[t]) <= 6u);
            if (s == 3u) for (uint32_t t = 0; t < NTRK; t++) { trk[t].p[P_E1] = (int16_t)(s * 30); trk[t].p[P_E5] = 127; }   /* live changes while sounding */
        }
        printf("stress 4 tracks x 6 voices, 8 s: peak %.3f, blocks over 0.99: %u\n", pk, over);
        CHECK(pk < 0.99 && over == 0u);
        for (uint32_t t = 0; t < NTRK; t++) trk_all_off(&trk[t]);
        run_ms(3000);
        uint32_t n = grab(300); CHECK(measure(0, n).peak < 0.001);
    }
    printf("ENSEMBLE PASS: %u checks.\n", checks);
    return 0;
}
