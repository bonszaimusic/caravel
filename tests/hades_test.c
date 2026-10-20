/* SPDX-License-Identifier: GPL-3.0-only */
/* HADES: independent signal checks, parameter extremes, real Learn/motion/
 * project/preset paths, PER6 migration and native render cost. No hardware claim. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>

static void had_setup(void)
{
    ui_power_on();
    host_preset(&trk[0], ENGI_HADES, 0);
    memset(engine_favorites, 0, sizeof engine_favorites);
    for (uint32_t p = 0; p < NPART; p++)
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
}
static double had_bin(const int32_t *s, uint32_t n, double hz)
{
    double re = 0, im = 0;
    for (uint32_t i = 0; i < n; i++) {
        double ph = 6.283185307179586 * hz * i / FS;
        re += s[i] * cos(ph); im += s[i] * sin(ph);
    }
    return sqrt(re * re + im * im) / n;
}
static int had_dividers(void)
{
    static int32_t sig[FS];
    const double root = (double)FS / 256.0;
    int bad = 0;
    for (uint32_t oct = 0; oct < 3u; oct++) {
        uint32_t ph = 0, inc = 1u << 22;
        int64_t sum = 0;
        for (uint32_t i = 0; i < FS; i++) { sig[i] = va_divider(ph, inc, oct); ph += inc; sum += sig[i]; }
        double fundamental = had_bin(sig, FS, root * (1u << oct) / 4.0);
        double wrong = had_bin(sig, FS, root * (1u << oct) / 8.0);
        printf("HADES divider %u: %.3f Hz, amplitude %.1f, mean %.2f\n", oct,
               root * (1u << oct) / 4.0, fundamental, (double)sum / FS);
        bad += check("divider pitch at f/4, f/2, f; no strong half-frequency or DC",
                     fundamental > 7000 && wrong < fundamental * .03 && llabs(sum) < FS * 150);
    }
    return bad;
}
static double had_filter_response(double hz, int32_t res)
{
    va_lp3_t f; va_lp3_coef(&f, 64 * 256, res);
    int32_t z[3] = {0}; double re = 0, im = 0;
    for (uint32_t i = 0; i < FS; i++) {
        double ph = 6.283185307179586 * hz * i / FS;
        int32_t y = va_lp3(&f, (int32_t)(4000 * sin(ph)), z);
        if (i >= FS / 2u) { re += y * cos(ph); im += y * sin(ph); }
    }
    return sqrt(re * re + im * im) * 4 / FS / 4000;
}
static int had_filter_checks(void)
{
    double cut = atan(SVF_G[64] / 4096.0) * FS / 3.141592653589793;
    double low = had_filter_response(cut / 8, 0), at = had_filter_response(cut, 0);
    double hi = had_filter_response(cut * 4, 0), higher = had_filter_response(cut * 8, 0);
    double res = had_filter_response(cut * 1.6, 30000), plain = had_filter_response(cut * 1.6, 0);
    printf("HADES LP3: fc %.1f Hz; low %.3f, fc %.3f, 4fc %.4f, 8fc %.4f; resonance %.2fx\n",
           cut, low, at, hi, higher, res / plain);
    return check("three-pole lowpass passes bass, attenuates highs and resonance creates a peak",
                 low > .85 && at > .28 && at < .43 && hi < .035 && higher < hi * .25 && res > plain * 2);
}
static int had_corners(void)
{
    static const uint8_t notes[] = {0, 24, 48, 84, 108, 127};
    int ok = 1; int32_t peak = 0;
    had_setup(); track_t *t = &trk[0]; voice_t *v = &t->v[0];
    for (uint32_t wave = 0; wave < 3; wave++)
    for (uint32_t note = 0; note < NELEM(notes); note++)
    for (uint32_t bits = 0; bits < 128u; bits++) {
        memset(v, 0, sizeof *v);
        t->p[P_E0] = wave;
        for (uint32_t j = 1; j < 8; j++) t->p[P_E0 + j] = bits & (1u << (j - 1u)) ? 127 : 0;
        hades_note_on(t, v);
        vmod_t m = {.inc = pitch_inc(notes[note] * 16), .pitch16 = notes[note] * 16,
                    .amp0 = 32767, .amp1 = 32767, .shape = 64 * 256};
        for (uint32_t b = 0; b < 80; b++) {
            int32_t out[CTL] = {0};
            m.cutoff = b & 1 ? 127 * 256 : -127 * 256;
            m.shape = b & 1 ? 0 : 127 * 256;
            t->p[P_E0] = b % 3;
            hades_render(t, v, out, CTL, &m);
            for (uint32_t i = 0; i < CTL; i++) {
                int32_t p = abs(out[i]); if (p > peak) peak = p; ok &= p < 200000;
            }
            ok &= abs(v->s[0]) <= 49152 && abs(v->s[1]) <= 49152 &&
                  abs(v->s[4]) <= 49152 && abs(v->s[5]) <= 49152;
        }
    }
    printf("HADES endpoint peak %d; extra engine memory %u bytes\n", peak, ENG_HADES.mem);
    return check("all waves, 128 parameter corners, MIDI 0..127 and live modulation stay bounded", ok && !ENG_HADES.mem);
}
static int had_controls(void)
{
    int bad = 0, ok = 1; had_setup(); track_t *t = &trk[0];
    bad += check("HADES appended at 15, old IDs and parameter layout retained",
                 ENGI_HADES == 15 && ENGI_SEM == 14 && P_COUNT == 114 && eng_ok(ENGI_HADES));
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t id = P_E0 + k;
        ok &= ml_learn(20 + k, 0, id) == 1;
        midi_learned(20 + k, 127); ok &= t->p[id] == track_desc(t, id)->max;
        midi_learned(20 + k, 0); ok &= t->p[id] == track_desc(t, id)->min;
        ok &= motion_param(id) && ev_id_ok(t, id);
    }
    bad += check("all eight HADES controls accept real MIDI Learn CCs and motion/locks", ok);
    for (uint32_t k = 0; k < 8; k++) {
        host_preset(t, ENGI_HADES, k); motion_set_lock(t, 0, P_E6, 89);
        project_t q, restored; project_store_t wire; project_capture(&q);
        ok &= proj_pack(&wire, &q) && proj_unpack(&restored, wire.raw, sizeof wire);
        project_restore_runtime(&restored);
        ok &= t->eng_req == ENGI_HADES && t->p[P_E7] == HADES_PRESETS[k].e[7];
    }
    host_preset(t, ENGI_HADES, 4); t->p[P_E6] = 89;
    ok &= up_store(2, "HADES USR") == 3;
    host_preset(t, 0, 0); up_load(2); ok &= t->eng_req == ENGI_HADES && t->p[P_E6] == 89;
    bad += check("all factory presets, a drive lock and user preset survive store/load", ok);
    for (uint32_t k = 0; k < 8; k++) {
        host_preset(t, ENGI_HADES, 0);
        uint32_t id = P_E0 + k;
        int16_t base = t->p[id], target = track_desc(t, id)->max;
        motion_clear(t); motion_set_lock(t, 0, id, target);
        motion_set_enabled(t, 1); motion_begin(); motion_step(t, 0, &motion, 1);
        ok &= t->p[id] == target; motion_end(); ok &= t->p[id] == base;
    }
    bad += check("all eight HADES controls execute locks and restore base values", ok);
    return bad;
}
static int had_storage(void)
{
    int bad = 0, ok = 1; had_setup();
    uint8_t prefs[32], learn[32];
    favorites.factory[15][10] = 1; favorites.factory[15][11] = 3;
    memcpy(prefs, favorites.factory[15], 32); memcpy(learn, favorites.factory[14], 32);
    for (uint32_t k = 0; k < 8; k++) {
        ok &= favorite_set(ENGI_SEM, k, 1) && favorite_set(ENGI_HADES, k, 1);
        ok &= favorite_has(ENGI_HADES, k) && favorite_has(ENGI_SEM, k);
    }
    ok &= !memcmp(prefs, favorites.factory[15], 32) && !memcmp(learn, favorites.factory[14], 32);
    bad += check("SEM/HADES favorites cannot alter Learn, idle animation, palette or category", ok);
    persist_t saved = {0}; settings_export(&saved);
    memset(engine_favorites, 0, sizeof engine_favorites);
    ok = settings_import(&saved, sizeof saved) == 1 && favorite_has(ENGI_SEM, 7) && favorite_has(ENGI_HADES, 7);
    ok &= !memcmp(prefs, favorites.factory[15], 32);
    bad += check("PER7 stores both engine favorite maps independently of preferences", ok);
    saved.magic = 0x50455236u; saved.favorites.factory[15][10] = 0xA5;
    saved.voice_allocation = VOICE_POLICY_FIXED | 0x1111u;
    ok = settings_import(&saved, PERSIST_PREENGINE_SIZE) == 2 && favorite_has(ENGI_SEM, 7) &&
         favorite_has(ENGI_SEM, 5) && !favorite_has(ENGI_HADES, 0) && favorites.factory[15][10] == 1 &&
         favorites.factory[15][11] == 3 && voice_policy == (VOICE_POLICY_FIXED | 0x1111u);
    persist_t migrated = saved;
    ok &= settings_import(&saved, sizeof saved) == 1 && !memcmp(&saved, &migrated, sizeof saved);
    bad += check("PER6 migrates SEM bits once, retains idle/category/Learn and fixed voice caps", ok);
    saved.magic = 0x50455235u;
    ok = settings_import(&saved, PERSIST_PREVOICE_SIZE) == 2 && !voice_policy && !favorite_has(ENGI_SEM, 0);
    bad += check("PER5 defaults new engine favorites and allocation without reading absent fields", ok);
    return bad;
}
static double had_bench(uint32_t engine)
{
    had_setup(); track_t *t = &trk[0]; host_preset(t, engine, 0);
    if (engine == ENGI_HADES) { t->p[P_E0] = 2; t->p[P_E2] = t->p[P_E3] = t->p[P_E6] = t->p[P_E7] = 127; }
    vmod_t mods[NVOICE];
    for (uint32_t i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i]; ENGINES[engine]->note_on(t, v);
        mods[i] = (vmod_t){.inc = pitch_inc((36 + i * 3) * 16), .pitch16 = (36 + i * 3) * 16,
                          .amp0 = 4096, .amp1 = 4096, .shape = 64 * 256};
    }
    clock_t begin = clock();
    for (uint32_t b = 0; b < 8192; b++) {
        int32_t out[CTL] = {0};
        for (uint32_t i = 0; i < NVOICE; i++) ENGINES[engine]->render(t, &t->v[i], out, CTL, &mods[i]);
    }
    return (double)(clock() - begin) / CLOCKS_PER_SEC * 1e9 / (8192u * CTL);
}
int main(void)
{
    int bad = had_dividers() + had_filter_checks() + had_corners() + had_controls() + had_storage();
    double best[3] = {1e30, 1e30, 1e30}; uint32_t engines[] = {0, ENGI_SEM, ENGI_HADES};
    for (uint32_t rep = 0; rep < 3; rep++) for (uint32_t e = 0; e < 3; e++) {
        double ns = had_bench(engines[e]); if (ns < best[e]) best[e] = ns;
    }
    printf("HADES native timing (8 voices), ns/frame: ANALOG %.1f; SEM %.1f; HADES worst controls %.1f (%.2fx ANALOG)\n",
           best[0], best[1], best[2], best[2] / best[0]);
    printf("HADES ENGINE: %s\n", bad ? "FAIL" : "PASS");
    return !!bad;
}
