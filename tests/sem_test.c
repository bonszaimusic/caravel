/* SPDX-License-Identifier: GPL-3.0-only */
/* SEM: audible/filter behavior, control endpoints, real Learn/storage paths,
 * fixed allocation, favorites isolation and a comparative host render timing. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <time.h>

static void sem_setup(void)
{
    ui_power_on();
    host_preset(&trk[0], ENGI_SEM, 0);
    for (uint32_t p = 0; p < NPART; p++)
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
}

static double fundamental(uint32_t state, uint32_t type)
{
    sem_setup();
    track_t *t = &trk[0]; voice_t *v = &t->v[0];
    double freq = 440.0 * pow(2.0, (60.0 - 69.0) / 12.0);
    double g = tan(3.141592653589793 * freq / FS) * 4096.0;
    uint32_t cut = 0;
    while (cut < 126u && SVF_G[cut + 1u] < g) cut++;
    int32_t finecut = (int32_t)((g - SVF_G[cut]) * 256.0 / (SVF_G[cut + 1u] - SVF_G[cut]));
    t->p[P_E0] = t->p[P_E1] = 2; t->p[P_E2] = t->p[P_E3] = t->p[P_E7] = 0;
    t->p[P_E4] = cut; t->p[P_E5] = 30; t->p[P_E6] = state; t->p[P_FTYPE] = type;
    v->age = 1; sem_note_on(t, v);
    vmod_t m = {.inc = pitch_inc(60 * 16), .pitch16 = 60 * 16, .amp0 = 32767, .amp1 = 32767,
                .shape = 64 * 256, .cutoff = finecut};
    double re = 0, im = 0;
    for (uint32_t block = 0; block < 1500u; block++) {
        int32_t out[CTL] = {0}; sem_render(t, v, out, CTL, &m);
        if (block < 100u) continue;
        for (uint32_t i = 0; i < CTL; i++) {
            double ph = 6.283185307179586 * freq * (block * CTL + i) / FS;
            re += out[i] * cos(ph); im += out[i] * sin(ph);
        }
    }
    return sqrt(re * re + im * im);
}

static int sem_filters(void)
{
    double lp = fundamental(0, 0), notch = fundamental(64, 0), hp = fundamental(127, 0);
    double bp = fundamental(64, 1), fixedhp = fundamental(0, 2);
    printf("SEM filter at cutoff: notch/LP %.4f, HP/LP %.4f, BP/LP %.4f\n", notch / lp, hp / lp, bp / lp);
    return check("SEM LP and HP meet at cutoff; notch rejects it; BP passes it; fixed HP matches",
        lp > 1e6 && notch < lp * .1 && hp > lp * .8 && hp < lp * 1.2 &&
        bp > lp * .5 && fabs(fixedhp - hp) < hp * .001);
}

static int sem_corners(void)
{
    static const uint8_t notes[] = {0, 24, 60, 96, 120, 127};
    int ok = 1; int32_t peak = 0;
    sem_setup(); track_t *t = &trk[0]; voice_t *v = &t->v[0];
    for (uint32_t a = 0; a < 4u; a++) for (uint32_t b = 0; b < 4u; b++)
    for (uint32_t note = 0; note < NELEM(notes); note++) for (uint32_t type = 0; type < 3u; type++)
    for (uint32_t corner = 0; corner < 8u; corner++) {
        memset(v, 0, sizeof *v); v->age = 1;
        t->p[P_E0] = a; t->p[P_E1] = b; t->p[P_E2] = corner & 1u ? 50 : 0;
        t->p[P_E3] = corner & 2u ? 127 : 0; t->p[P_E4] = corner & 1u ? 127 : 0;
        t->p[P_E5] = corner & 2u ? 127 : 0; t->p[P_E6] = corner & 4u ? 127 : 0;
        t->p[P_E7] = corner & 4u ? 127 : 0; t->p[P_FTYPE] = type;
        sem_note_on(t, v);
        vmod_t m = {.pitch16 = notes[note] * 16, .inc = pitch_inc(notes[note] * 16),
                    .amp0 = 32767, .amp1 = 32767, .shape = corner & 1u ? 127 * 256 : 0};
        for (uint32_t block = 0; block < 40u; block++) {
            int32_t out[CTL] = {0};
            /* live endpoint jumps and extreme modulation while a voice rings */
            t->p[P_E6] = block & 1u ? 127 : 0;
            m.cutoff = block & 1u ? 127 * 256 : -127 * 256;
            sem_render(t, v, out, CTL, &m);
            for (uint32_t i = 0; i < CTL; i++) {
                int32_t p = abs(out[i]); if (p > peak) peak = p;
                ok &= p <= 200000;
            }
            ok &= v->s[6] >= 0 && v->s[6] <= 32768;
        }
    }
    printf("SEM endpoint peak: %d\n", peak);
    return check("SEM wave pairs, MIDI 0..127, resonance, PWM, drift, live STATE and cutoff stay bounded", ok);
}

static int sem_integration(void)
{
    int bad = 0, ok = 1; sem_setup(); track_t *t = &trk[0];
    bad += check("SEM appended at 14, existing sound ids and 113-parameter layout retained",
        ENGI_SEM == 14u && P_COUNT == 113 && ENGINES[0] == &ENG_ANALOG &&
        ENGINES[12] == &ENG_FM6 && ENGINES[ENGI_SEM] == &ENG_SEM && eng_ok(ENGI_SEM));
    for (uint32_t k = 0; k < 8u; k++) {
        uint32_t id = P_E0 + k, cc = 20u + k;
        ok &= ml_learn(cc, 0, id) == 1u;
        midi_learned(cc, 127);
        ok &= t->p[id] == track_desc(t, id)->max && motion_param(id) && ev_id_ok(t, id);
        midi_learned(cc, 0);
        ok &= t->p[id] == track_desc(t, id)->min;
    }
    ok &= ml_learn(28, 0, P_FTYPE) == 1u;
    midi_learned(28, 127); ok &= t->p[P_FTYPE] == 2;
    bad += check("every SEM control and TYPE accepts real learned CCs; eight engine controls support automation", ok);
    uint8_t learnt[32]; memcpy(learnt, favorites.factory[14], sizeof learnt);
    uint8_t prefs[32]; memcpy(prefs, favorites.factory[15], sizeof prefs);
    for (uint32_t k = 0; k < 8u; k++) ok &= favorite_set(ENGI_SEM, k, 1) && favorite_has(ENGI_SEM, k);
    ok &= !memcmp(learnt, favorites.factory[14], sizeof learnt);
    for (uint32_t k = 0; k < sizeof prefs; k++) if (k != 10u) ok &= prefs[k] == favorites.factory[15][k];
    bad += check("SEM favorites cannot overwrite legacy MIDI Learn, custom palette or device preferences", ok);
    persist_t saved = {0}; settings_export(&saved);
    memset(&favorites, 0, sizeof favorites); memset(ml_extra, 0, sizeof ml_extra);
    ok = settings_import(&saved, sizeof saved) == 1 && ml_count() == 9u && favorite_has(ENGI_SEM, 7);
    bad += check("SEM favorites and learned targets survive settings save/load together", ok);
    for (uint32_t k = 0; k < 8u; k++) {
        host_preset(t, ENGI_SEM, k); t->p[P_FTYPE] = k % 3u;
        motion_set_lock(t, 0, P_E6, 64);
        project_t q, restored; project_store_t wire;
        project_capture(&q);
        ok &= proj_pack(&wire, &q) && proj_unpack(&restored, wire.raw, sizeof wire) &&
              restored.t[0].engine == ENGI_SEM;
        project_restore_runtime(&restored);
        ok &= t->eng_req == ENGI_SEM && t->p[P_FTYPE] == k % 3u &&
              t->p[P_E7] == SEM_PRESETS[k].e[7];
    }
    bad += check("all eight SEM presets, filter TYPE and STATE locks survive project round trips", ok);
    host_preset(t, ENGI_SEM, 4); t->p[P_E6] = 89; t->p[P_FTYPE] = 1;
    ok = up_store(2, "SEM USER") == 3;
    host_preset(t, 0, 0); up_load(2);
    ok &= t->eng_req == ENGI_SEM && t->p[P_E6] == 89 && t->p[P_FTYPE] == 1;
    bad += check("SEM user presets preserve engine, filter state and TYPE", ok);
    sem_setup(); t = &trk[0];
    for (uint32_t id = P_E0; id <= P_E7; id++) {
        motion_clear(t); t->p[id] = 0;
        int32_t high = track_desc(t, id)->max;
        ok &= motion_set_event(t, 0, id, high) == 0 && motion_set_lock(t, 1, id, 0) == 0;
        motion_set_enabled(t, 1); motion_begin();
        motion_step(t, 0, &motion, 1); ok &= t->p[id] == high;
        motion_step(t, 1, &motion, 1); ok &= t->p[id] == 0;
        motion_step(t, 2, &motion, 1); ok &= t->p[id] == high;
        motion_end(); ok &= t->p[id] == 0;
    }
    bad += check("all eight SEM controls play automation and step locks and restore their base", ok);
    sem_setup(); voice_policy = VOICE_POLICY_FIXED | 0x2222u;
    for (uint32_t p = 0; p < NPART; p++) {
        host_preset(&trk[p], p == 3u ? ENGI_DRUM : ENGI_SEM, p == 3u ? 4u : p);
        trk[p].p[P_VOICE] = V_POLY;
        trk_note_on(&trk[p], 48u + p, 100); trk_note_on(&trk[p], 55u + p, 100);
    }
    int32_t out[CTL * 2u];
    for (uint32_t b = 0; b < 2000u; b++) mix_block(out, CTL);
    ok = voices_busy() <= NVOICE;
    for (uint32_t p = 0; p < NPART; p++) {
        uint32_t n = 0; for (uint32_t v = 0; v < NVOICE; v++) n += trk[p].v[v].active && trk[p].v[v].stage != 4u;
        ok &= n <= 2u;
    }
    bad += check("SEM chords, DRUM-X and FX share the unchanged fixed voice budget", ok);
    return bad;
}

/* Comparative native timing, not an FM-1 CPU percentage or a deadline guarantee.
 * Both cases render eight voices, saw or PWM, at the same notes and cutoff.
 * SEM additionally runs full VINT and the notch blend. Print the best of three
 * CPU-time measurements to reduce scheduling noise; no timing-based assertion. */
static double sem_bench(uint32_t engine, int pwm)
{
    sem_setup(); track_t *t = &trk[0]; host_preset(t, engine, 0);
    t->p[P_E0] = engine == ENGI_SEM ? (pwm ? 3 : 0) : (pwm ? 4 : 0);
    t->p[P_E1] = engine == ENGI_SEM ? (pwm ? 3 : 0) : 8;
    t->p[P_E2] = engine == ENGI_SEM ? 8 : 64;
    t->p[P_E3] = engine == ENGI_SEM ? 64 : 0;
    t->p[P_E4] = 80; t->p[P_E5] = 40;
    t->p[P_E6] = engine == ENGI_SEM ? 64 : 0;
    t->p[P_E7] = engine == ENGI_SEM ? 127 : 0;
    vmod_t m[NVOICE];
    for (uint32_t i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i]; v->age = i + 1;
        ENGINES[engine]->note_on(t, v);
        m[i] = (vmod_t){.inc = pitch_inc((48 + i * 3) * 16), .pitch16 = (48 + i * 3) * 16,
                       .amp0 = 4096, .amp1 = 4096, .shape = 80 * 256};
    }
    clock_t begin = clock();
    for (uint32_t b = 0; b < 16384u; b++) {
        int32_t out[CTL] = {0};
        for (uint32_t i = 0; i < NVOICE; i++) ENGINES[engine]->render(t, &t->v[i], out, CTL, &m[i]);
    }
    return (double)(clock() - begin) / CLOCKS_PER_SEC * 1e9 / (16384u * CTL);
}

int main(void)
{
    int bad = sem_filters() + sem_corners() + sem_integration();
    for (uint32_t pwm = 0; pwm < 2u; pwm++) {
        double an = 1e30, sem = 1e30;
        for (uint32_t repeat = 0; repeat < 3u; repeat++) {
            double a = sem_bench(0, pwm), b = sem_bench(ENGI_SEM, pwm);
            if (a < an) an = a; if (b < sem) sem = b;
        }
        printf("SEM native timing, 8 voices %s: ANALOG %.1f ns/frame; SEM %.1f ns/frame; ratio %.3f\n",
               pwm ? "PWM" : "SAW", an, sem, sem / an);
    }
    printf("SEM ENGINE: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
