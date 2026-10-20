/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* The slow movement of the eleven DRONE cell presets (firmware/src/ui.c DRONE_MOTION, drone_motion_load): a factory load of
 * presets 5..15 sets two free-running LFOs on two engine values through the matrix; presets 0..4, every other engine, a
 * reload and user edits behave as before; the movement is audible (the same preset without it renders differently) and
 * bounded. Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int check_(const char *what, int ok)
{
    printf("drone motion: %-84s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static int matrix_off(const track_t *t)
{
    for (uint32_t k = 0; k < 4u; k++)
        if (t->p[P_M1SRC + 3u * k] || t->p[P_M1DST + 3u * k] || t->p[P_M1AMT + 3u * k]) return 0;
    return 1;
}
static void load(track_t *t, uint32_t eng, uint32_t preset)
{
    ui_power_on();
    set_engine_of(t, eng);
    apply_preset_to(t, preset);
}

int main(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    uint32_t pi;
    ui_power_on();
    ok = 1;
    for (pi = 0; pi < 5u; pi++) {
        load(t, ENGI_DRONE, pi);
        ok &= matrix_off(t) && t->p[P_LRATE] == TP[P_LRATE].def && t->p[P_LTRIG] == TP[P_LTRIG].def;
    }
    bad += check_("presets 0..4 (legacy): no matrix slot, LFO as default", ok);
    ok = 1;
    for (pi = 5; pi < 16u; pi++) {
        load(t, ENGI_DRONE, pi);
        ok &= t->p[P_M1SRC] == MS_LFO && t->p[P_M2SRC] == MS_LFO2 && t->p[P_M1DST] >= MD_E1 && t->p[P_M1DST] < MD_E1 + 8 &&
              t->p[P_M2DST] >= MD_E1 && t->p[P_M2DST] < MD_E1 + 8 && t->p[P_M1AMT] && t->p[P_M2AMT] &&
              !t->p[P_M3SRC] && !t->p[P_M4SRC] && t->p[P_LTRIG] == 1 && lfo2_config[0].p[L2_TRIG] == 1 && t->p[P_LWAVE] == 0 &&
              t->p[P_LRATE] <= 26 && lfo2_config[0].p[L2_RATE] <= 26 && t->p[P_LRATE] != lfo2_config[0].p[L2_RATE] &&
              t->p[P_LD_PIT] == 0 && t->p[P_LD_AMP] == 0;                    /* no vibrato, no tremolo of its own */
    }
    bad += check_("presets 5..15: LFO and LFO2 on two engine values (E1..E8), FREE, slow, unrelated rates, nothing else", ok);
    load(t, ENGI_DRONE, 8);
    t->p[P_M1AMT] = 0; t->p[P_M2AMT] = 0;
    apply_preset_to(t, 8);
    bad += check_("a reload gives the movement back (a preset sounds the same after any edit)", t->p[P_M1AMT] && t->p[P_M2AMT]);
    load(t, 0u, 0);
    bad += check_("another engine's preset: no matrix, as before", matrix_off(t));
    load(t, ENGI_HADES, 0);
    bad += check_("HADES preset: no matrix, as before", matrix_off(t));
    {   /* audible: preset 11 (SUB FOG, TONE +-14 % over 20 s) against the same sound with the matrix cleared; bounded output */
        int16_t *a = malloc(60u * FS * sizeof *a), *b = malloc(60u * FS * sizeof *b);
        double diff = 0, ea = 0;
        uint32_t n = 60u * FS, i, pos;
        for (int pass = 0; pass < 2; pass++) {
            load(t, ENGI_DRONE, 11);
            song.master_q12 = 4096;
            t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
            if (pass) { t->p[P_M1AMT] = 0; t->p[P_M2AMT] = 0; }
            for (i = 0; i < 400u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }      /* (the engine change fades in first) */
            engine_block(t);
            trk_note_on(t, 36, 100);
            for (pos = 0; pos < n;) {
                int32_t out[2 * CTL];
                uint32_t m = n - pos < CTL ? n - pos : CTL;
                mix_block(out, m);
                for (i = 0; i < m; i++) (pass ? b : a)[pos + i] = (int16_t)clamp(out[2 * i], -32768, 32767);
                pos += m;
            }
        }
        for (i = 0; i < n; i++) { diff += fabs((double)a[i] - b[i]); ea += fabs((double)a[i]); }
        bad += check_("SUB FOG with the movement renders differently from the same sound without it", diff / n > 20.0);
        bad += check_("  and stays at the same loudness (mean |x| within a factor 2)", ea > 0 && diff < 2.0 * ea);
        free(a); free(b);
    }
    puts(bad ? "DRONE MOTION TEST FAILED" : "drone motion test passed");
    return bad != 0;
}
