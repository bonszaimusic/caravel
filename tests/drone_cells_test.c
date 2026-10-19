/* SPDX-License-Identifier: GPL-3.0-only */
/* Cells-only contracts. Run with tests/run_drone_cells_tests.sh or run_tests.sh.
 * Uses the real gesture/allocator/scene paths, not engine.note_on as a key event.
 * Native timing is diagnostic, never an FM-1 utilization/deadline measurement. */
#ifdef C1_HOST
#include "c1_host.h"
#endif
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

/* Historical layout: additions must never move the original 104-byte prefix. */
typedef struct {
    uint32_t phase[4], drift_phase[4];
    int32_t prev[4], inc[4], ctrl[6];
    int32_t low, dc_in, dc_out;
    uint32_t ready;
} dc_legacy_t;
_Static_assert(sizeof(dc_legacy_t) == 104, "legacy state layout");
_Static_assert(offsetof(drone_part_t, held) == sizeof(dc_legacy_t), "append-only state");
_Static_assert(sizeof(drone_part_t) == 252, "cell state size contract");
#define DC_Q23 (32767 * 256)
static volatile int64_t dc_sink;

static void dc_setup(uint32_t preset, uint32_t mode)
{
    ui_power_on(); host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    voice_policy = voice_policy_active = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        host_preset(t, ENGI_DRONE, preset);
        t->p[P_VOICE] = mode;
        t->p[P_ATK] = t->p[P_DEC] = t->p[P_REL] = 0;
        t->p[P_SUS] = 127;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        t->p[P_AMODE] = 0;
        engine_block(t);
    }
    panic_req = 0;
}
static uint32_t dc_active(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t k = 0; k < NVOICE; k++) n += !!t->v[k].active;
    return n;
}
static int dc_held(const drone_part_t *s, uint32_t n)
{
    return (s->held[n / 32] & (1u << (n % 32))) != 0;
}
static int dc_empty(const drone_part_t *s)
{
    return !(s->held[0] | s->held[1] | s->held[2] | s->held[3]);
}
static void dc_host_blocks(track_t *t, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        int32_t out[CTL]; engine_block(t); track_render(t, out, CTL);
    }
}
static vmod_t dc_mod(uint32_t note)
{
    return (vmod_t){.inc = pitch_inc(note * 16), .pitch16 = note * 16,
        .amp0 = 32767, .amp1 = 32767, .shape = 64 * 256};
}
static void dc_direct(track_t *t, int32_t *out, uint32_t n, const vmod_t *m)
{
    memset(out, 0, n * sizeof *out);
    ENG_DRONE.render(t, &t->v[0], out, n, m);
}
static int dc_bounds(const drone_part_t *s)
{
    int ok = s->ready <= 2 && s->cells_ready <= 1 && s->rotate <= 3 &&
        s->ctrl[DR_TOPO] >= 0 && s->ctrl[DR_TOPO] <= 65536 &&
        s->cell_mix >= 0 && s->cell_mix <= 32767 && s->cell_exit <= 128;
    for (uint32_t k = 0; k < DR_NCTRL; k++) if (k != DR_TOPO)
        ok &= s->ctrl[k] >= 0 && s->ctrl[k] <= 32767;
    for (uint32_t k = 0; k < 4; k++) {
        ok &= llabs((int64_t)s->prev[k]) <= 32768 && s->inc[k] >= 0 && s->inc[k] <= 0x60000000;
        ok &= s->owner[k] <= 127 && s->pitch16[k] >= 0 && s->pitch16[k] <= 2032;
        ok &= s->ratio[k] <= 131072 && s->level[k] >= 0 && s->level[k] <= 32767;
        ok &= s->drive[k] >= 0 && s->drive[k] <= DC_Q23 &&
              s->amp[k] >= 0 && s->amp[k] <= DC_Q23 && s->activity[k] >= 0 && s->activity[k] <= DC_Q23;
    }
    ok &= llabs((int64_t)s->low) < 131072 && llabs((int64_t)s->dc_in) <= 32768 &&
          llabs((int64_t)s->dc_out) < 131072 && llabs((int64_t)s->cell_last) < 131072;
    return ok;
}
static int dc_metadata(void)
{
    dc_setup(5, V_LEGATO); track_t *t = &trk[0];
    int bad = check("cells: 252 bytes, legacy104 prefix, engine16, one host voice, TOPO0..5",
        ENGI_DRONE == 16 && NENGINES == 17 && ENG_DRONE.poly == 1 && trk_nvoice(t) == 1 &&
        ENG_DRONE.mem == 252 && ENG_DRONE.mem <= ENG_MEM_PART &&
        ENG_DRONE.npresets == 16 && ENG_DRONE.edit[3].min == 0 && ENG_DRONE.edit[3].max == 5);
    static const char *const names[] = {"PAIRS", "CHAIN", "RING", "CELLPAIR", "CELLCHN", "CELLRING"};
    int ok = 1;
    for (uint32_t k = 0; k < 6; k++) ok &= !strcmp(ENG_DRONE.edit[3].names[k], names[k]);
    bad += check("cells: append-only topology names and indices", ok);
    ok = 1;
    for (uint32_t pi = 5; pi < 16; pi++) {
        host_preset(t, ENGI_DRONE, pi);
        ok &= t->p[P_E3] >= 3 && t->p[P_E3] <= 5 && t->p[P_E3] == DRONE_PRESETS[pi].e[3];
        project_t q, restored; project_store_t wire;
        project_capture(&q);
        int packed = proj_pack(&wire, &q) && proj_unpack(&restored, wire.raw, sizeof wire);
        ok &= packed;
        if (packed) {
            host_preset(t, ENGI_SEM, 0); project_restore_runtime(&restored);
            ok &= t->eng_req == ENGI_DRONE && t->preset == pi;
            for (uint32_t k = 0; k < 8; k++) ok &= t->p[P_E0 + k] == DRONE_PRESETS[pi].e[k];
        }
        host_preset(t, ENGI_DRONE, pi);
        ok &= up_store(2, "CELL TEST") == 3;
        host_preset(t, ENGI_SEM, 0); up_load(2);
        ok &= t->eng_req == ENGI_DRONE && t->p[P_E3] == DRONE_PRESETS[pi].e[3];
        motion_clear(t); motion_set_lock(t, 0, P_E3, 5); motion_set_enabled(t, 1);
        motion_begin(); motion_step(t, 0, &motion, 1); ok &= t->p[P_E3] == 5;
        motion_end(); ok &= t->p[P_E3] == DRONE_PRESETS[pi].e[3];
    }
    bad += check("cells: presets5..15 project/user roundtrip and topology lock/base restore", ok);
    /* Variety: no two cell presets share parameters; new DRONE sounds start in a cell mode. */
    ok = ENG_DRONE.edit[3].def >= 3 && ENG_DRONE.edit[3].def <= 5;
    for (uint32_t a = 5; a < 16; a++)
        for (uint32_t b = a + 1; b < 16; b++)
            ok &= memcmp(DRONE_PRESETS[a].e, DRONE_PRESETS[b].e, sizeof DRONE_PRESETS[a].e) != 0;
    bad += check("cells: eleven distinct cell presets, default TOPO is a cell mode", ok);
    return bad;
}
/* Input-order regressions deliberately run events_block without a DSP render:
 * CC/edit/note-off ordering must not depend on a future audio callback. */
static void dc_input_reset(void)
{
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners); memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um); memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = 0; midi_in_overflow = 0; midi_beat_samples = 0;
    fm1_in.notes = kb_prev = 0; song.sel = 0;
    memset(song.mch, 0, sizeof song.mch); song.g[G_ROUTE] = 0;
    events_block(CTL);
}
static int dc_enqueue(uint32_t status, uint32_t a, uint32_t b)
{
    return midi_enqueue((status >> 4) | status << 8 | a << 16 | b << 24, 1);
}
static int dc_family_events(void)
{
    static const uint8_t keys[][2] = {{48, 60}, {0, 60}, {48, 0}};
    int bad = 0; uint32_t cases = 0;
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++) {
        int ok = 1;
        for (uint32_t prio = 0; prio < 3; prio++)
        for (uint32_t pair = 0; pair < NELEM(keys); pair++)
        for (uint32_t inverse = 0; inverse < 2; inverse++)
        for (uint32_t order = 0; order < 2; order++)
        for (uint32_t queued = 0; queued < 3; queued++) {
            dc_setup(inverse ? 0 : 5, mode); track_t *t = &trk[0];
            t->p[P_PRIO] = prio; dc_input_reset();
            uint32_t a = keys[pair][0], b = keys[pair][1];
            uint32_t first = keys[pair][order], last = keys[pair][1 - order];
            int pass = 1;
            if (queued) {
                pass &= dc_enqueue(0x90, a, 100) && dc_enqueue(0x90, b, 100);
                events_block(CTL);
                pass &= ml_learn(27, 0, P_E3) == 1;
            } else { trk_note_on(t, a, 100); trk_note_on(t, b, 100); }
            dc_host_blocks(t, 64); /* held sustain in the original family */
            uint32_t phase[4]; memcpy(phase, drone_state(t)->phase, sizeof phase);
            /* The direct parameter store models the explicit UI/editor edit,
             * whose descriptor must remain 0..5. A queued learned CC follows
             * that store, then note-off, all before the first new-family render. */
            const param_desc_t *d = track_desc(t, P_E3);
            pass &= d->min == 0 && d->max == 5;
            int32_t topo = inverse ? 3 : 2;
            if (queued == 2) { go_title("EDIT 1"); edit_param(3, topo - t->p[P_E3]); }
            else t->p[P_E3] = topo;
            pass &= t->p[P_E3] == topo;
            if (queued) {
                pass &= dc_enqueue(0xB0, 27, inverse ? 0 : 127) && dc_enqueue(0x80, first, 0);
                events_block(CTL);
            } else trk_note_off(t, first);
            pass &= !dc_held(drone_state(t), first) && dc_held(drone_state(t), last);
            if (inverse) pass &= t->v[0].active && t->v[0].gate && t->v[0].stage != 3;
            if (!inverse) {
                if (queued == 2) edit_param(3, 3 - t->p[P_E3]);
                else t->p[P_E3] = 3;
                pass &= t->p[P_E3] == 3;
            }
            if (queued) { pass &= dc_enqueue(0x80, last, 0); events_block(CTL); }
            else trk_note_off(t, last);
            pass &= dc_empty(drone_state(t)) && !t->v[0].gate &&
                !memcmp(phase, drone_state(t)->phase, sizeof phase);
            dc_host_blocks(t, 64);
            pass &= !dc_active(t) && !t->nmono && !t->xp_n;
            if (!pass) printf("CELLS event-order FAIL mode%u prio%u keys%u/%u inverse%u order%u queued%u held=%08x/%08x/%08x/%08x gate%u stage%u\n",
                mode, prio, a, b, inverse, order, queued, drone_state(t)->held[0], drone_state(t)->held[1],
                drone_state(t)->held[2], drone_state(t)->held[3], t->v[0].gate, t->v[0].stage);
            ok &= pass; cases++;
        }
        char label[140]; snprintf(label, sizeof label,
            "cells: mode%u pre-render family edits/off in both orders, note0, all priorities, hooks+queued CC+actual UI", mode);
        bad += check(label, ok);
    }
    printf("CELLS pre-render event-order: %u cases (no DSP call between edits and releases)\n", cases);
    return bad;
}
/* Release-before-roundtrip: a gated host voice may still name a released key
 * while another key is held. Family flips (with or without a render between
 * them) must neither resurrect that key's held bit nor hold the gate open. */
static int dc_ghost_release(void)
{
    static const uint8_t keys[][2] = {{48, 60}, {0, 60}, {48, 0}, {60, 59}, {127, 0}};
    int bad = 0; uint32_t cases = 0, ghosts = 0;
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++) {
        int ok = 1;
        for (uint32_t prio = 0; prio < 3; prio++)
        for (uint32_t pair = 0; pair < NELEM(keys); pair++)
        for (uint32_t release = 0; release < 2; release++)
        for (uint32_t render = 0; render < 2; render++)
        for (uint32_t start = 0; start < 2; start++) {
            dc_setup(start ? 0 : 5, mode); track_t *t = &trk[0];
            t->p[P_PRIO] = prio; dc_input_reset();
            uint32_t a = keys[pair][0], b = keys[pair][1];
            uint32_t gone = keys[pair][release], kept = keys[pair][1 - release];
            int pass = 1;
            if (start) t->p[P_E3] = 3; /* enter cells with both keys already down */
            trk_note_on(t, a, 100); trk_note_on(t, b, 100);
            if (render) dc_host_blocks(t, 16);
            trk_note_off(t, gone);
            if (render) dc_host_blocks(t, 4);
            drone_part_t *s = drone_state(t);
            pass &= !dc_held(s, gone) && dc_held(s, kept) && t->v[0].gate;
            ghosts += t->v[0].gate && (t->v[0].note & 127u) == gone;
            for (uint32_t topo = 2; topo <= 3; topo++) {
                t->p[P_E3] = topo;
                if (render) dc_host_blocks(t, 4);
                pass &= !dc_held(s, gone) && dc_held(s, kept) && t->v[0].gate;
            }
            if (render) {
                /* Re-entry seeds from the surviving key, never the released one. */
                pass &= s->cells_ready;
                for (uint32_t k = 0; k < 4; k++) pass &= s->owner[k] == kept;
            }
            trk_note_off(t, kept);
            pass &= dc_empty(s) && !t->v[0].gate;
            dc_host_blocks(t, 64);
            int32_t out[CTL]; memset(out, 0, sizeof out); engine_block(t); track_render(t, out, CTL);
            for (uint32_t i = 0; i < CTL; i++) pass &= out[i] == 0;
            pass &= !dc_active(t) && !t->nmono && !t->xp_n && dc_empty(s);
            if (!pass) printf("CELLS ghost-release FAIL mode%u prio%u keys%u/%u release%u render%u start%u held=%08x/%08x/%08x/%08x gate%u note%u\n",
                mode, prio, a, b, release, render, start, s->held[0], s->held[1], s->held[2], s->held[3],
                t->v[0].gate, t->v[0].note);
            ok &= pass; cases++;
        }
        char label[140]; snprintf(label, sizeof label,
            "cells: mode%u release-before-roundtrip never revives a released key, gate closes, exact silence", mode);
        bad += check(label, ok);
    }
    printf("CELLS ghost-release: %u cases (%u with a stale gated host-voice note)\n", cases, ghosts);
    return bad;
}
/* The renderers avoid hardware divides (the target compiler emits one even for
 * constants). The replacements must equal C's '/' for every value they can see. */
static int dc_division_exact(void)
{
    int ok = 1; uint64_t n = 0;
    static const int32_t edge[] = {0, 1, -1, 2, -2, 1023, 1024, 1025, -1023, -1024, -1025,
        0x7fffffff, -0x7fffffff, -0x7fffffff - 1};
    for (uint32_t s = 1; s <= 10; s++) {
        for (uint32_t k = 0; k < NELEM(edge); k++) { ok &= drone_div2s(edge[k], s) == edge[k] / (1 << s); n++; }
        for (int32_t d = -70000; d <= 70000; d++) { ok &= drone_div2s(d, s) == d / (1 << s); n++; }
        uint32_t x = 12345u + s;
        for (uint32_t i = 0; i < 200000; i++) {
            x = x * 1664525u + 1013904223u;
            int32_t d = (int32_t)x; ok &= drone_div2s(d, s) == d / (1 << s); n++;
        }
    }
    /* Q23 follower states: exhaustive over the reciprocal's quotient range,
     * random over the full +/-2^23 span, both used shift pairs, all four cells. */
    for (uint32_t pair = 0; pair < 2; pair++) {
        uint32_t sh = pair ? 13u : 15u;
        for (uint32_t m = 2; m <= 5; m++) {
            int32_t div = (int32_t)(m << sh);
            for (int32_t q = -1100; q <= 1100; q++)
                for (int32_t r = -2; r <= 2; r++) {
                    int32_t d = q * (int32_t)(1u << sh) * (int32_t)m + r * (int32_t)m + (q < 0 ? -(int32_t)m + 1 : 0);
                    int32_t step = d / div, ref = step ? step : (d > 0) - (d < 0);
                    ok &= drone_cell_follow(0, d, sh, m) == ref; n++;
                }
            uint32_t x = 99u + m + sh;
            for (uint32_t i = 0; i < 300000; i++) {
                x = x * 1664525u + 1013904223u;
                int32_t a = (int32_t)(x >> 9) & 0x7fffff, b;
                x = x * 1664525u + 1013904223u; b = (int32_t)(x >> 9) & 0x7fffff;
                int32_t d = b - a, step = d / div, ref = a + (step ? step : (d > 0) - (d < 0));
                ok &= drone_cell_follow(a, b, sh, m) == ref; n++;
            }
        }
    }
    /* The one-pole smoother used by both renderers. */
    for (int32_t d = -100000; d <= 100000; d += 7) {
        int32_t step = d / 1024; ok &= drone_follow(0, d) == (step ? step : (d > 0) - (d < 0)); n++;
    }
    printf("CELLS division-free helpers: %llu comparisons against C division\n", (unsigned long long)n);
    return check("cells: shift/reciprocal replacements equal C division (edges, exhaustive and random)", ok);
}
/* Cells must not be much duller than the legacy network at the same settings:
 * the PM source carries the cell amplitude with makeup (see drone_cell_render).
 * Brightness proxy: RMS of the first difference over RMS of the signal. */
static double dc_brightness(uint32_t topo, uint32_t coupling)
{
    dc_setup(3, V_LEGATO); track_t *t = &trk[0];
    t->p[P_E2] = (int16_t)coupling; t->p[P_E3] = (int16_t)topo; engine_block(t);
    trk_note_on(t, 48, 100);
    double d2 = 0, x2 = 0; int32_t prev = 0, out[CTL];
    for (uint32_t b = 0; b < 12u * FS / CTL; b++) {
        memset(out, 0, sizeof out); engine_block(t); track_render(t, out, CTL);
        if (b < 4u * FS / CTL) { prev = out[CTL - 1]; continue; }
        for (uint32_t i = 0; i < CTL; i++) {
            double d = (double)out[i] - prev; prev = out[i];
            d2 += d * d; x2 += (double)out[i] * out[i];
        }
    }
    return sqrt(d2 / (x2 + 1.0));
}
static int dc_richness(void)
{
    int ok = 1;
    for (uint32_t c = 0; c < 2; c++) {
        uint32_t coupling = c ? 127u : 78u;
        double legacy = dc_brightness(2, coupling), cells = dc_brightness(5, coupling);
        printf("CELLS richness COUPLE%u: legacy %.4f cells %.4f (ratio %.2f)\n", coupling, legacy, cells, cells / legacy);
        ok &= cells >= 0.6 * legacy && cells <= 1.6 * legacy;
    }
    return check("cells: spectral brightness within 0.6x..1.6x of legacy RING at equal settings", ok);
}
/* DRIFT adds slow breathing of PM depth and loudness, and a fresh phrase starts its
 * drift triangles elsewhere. Level variation over 30 s must grow with DRIFT, and two
 * phrases must differ; DRIFT 0 stays constant. */
static double dc_level_sd(uint32_t drift, uint32_t preset)
{
    dc_setup(preset, V_LEGATO); track_t *t = &trk[0];
    t->p[P_E5] = (int16_t)drift; t->p[P_E6] = 100; engine_block(t);
    trk_note_on(t, 48, 100);
    double s1 = 0, s2 = 0, e = 0; uint32_t n = 0, cnt = 0; int32_t out[CTL];
    for (uint32_t b = 0; b < 70u * FS / CTL; b++) {
        memset(out, 0, sizeof out); engine_block(t); track_render(t, out, CTL);
        if (b < 25u * FS / CTL) continue;
        for (uint32_t i = 0; i < CTL; i++) e += (double)out[i] * out[i];
        cnt += CTL;
        if (cnt >= FS / 4u) { /* 250 ms windows: many cycles of the ~130 Hz tone */
            double db = 10 * log10(e / cnt + 1.0); s1 += db; s2 += db * db; n++; e = 0; cnt = 0;
        }
    }
    return sqrt(s2 / n - (s1 / n) * (s1 / n));
}
static int dc_life(void)
{
    int ok = 1;
    for (uint32_t preset = 5; preset < 8; preset++) {
        double still = dc_level_sd(0, preset), live = dc_level_sd(100, preset);
        printf("CELLS life preset%u: level sd %.3f dB at DRIFT 0, %.3f dB at DRIFT 100\n", preset, still, live);
        ok &= live > 1.5 * still && live > 0.3;
    }
    /* Two phrases from the same part differ (the free-running phases and, with DRIFT, the drift start). */
    for (uint32_t drift = 1; drift < 2; drift++) {
        dc_setup(5, V_LEGATO); track_t *t = &trk[0]; t->p[P_E5] = drift ? 90 : 0; t->p[P_E6] = 100;
        engine_block(t);
        uint64_t h[2] = {1469598103934665603ull, 1469598103934665603ull};
        for (uint32_t ph = 0; ph < 2; ph++) {
            trk_note_on(t, 48, 100); int32_t out[CTL];
            for (uint32_t b = 0; b < 3u * FS / CTL; b++) {
                memset(out, 0, sizeof out); engine_block(t); track_render(t, out, CTL);
                for (uint32_t i = 0; i < CTL; i++) h[ph] = (h[ph] ^ (uint32_t)out[i]) * 1099511628211ull;
            }
            trk_note_off(t, 48); dc_host_blocks(t, 400); /* release to silence */
        }
        printf("CELLS life phrases DRIFT %s: %s\n", drift ? "90" : "0", h[0] == h[1] ? "identical" : "different");
        ok &= drift ? h[0] != h[1] : 1;
    }
    return check("cells: DRIFT breathing raises slow level variation and phrases differ", ok);
}
/* The eleven cell presets must stay audibly distinct. Metric: long-term spectrum of one held
 * note in 24 third-octave bands (30 Hz..), levels relative to the total, floored at -60 dB;
 * distance = mean absolute dB difference. Rendered with each preset's own envelope, level
 * halved as in the mix, analysed over 30..60 s. Deterministic; the optimizer reached 8.3 dB. */
#define DC_NB 24
#define DC_FN 8192
static void dc_fft(double *re, double *im)
{
    for (int i = 1, j = 0; i < DC_FN; i++) {
        int bit = DC_FN >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= DC_FN; len <<= 1) {
        double a = -2 * M_PI / len;
        for (int i = 0; i < DC_FN; i += len)
            for (int k = 0; k < len / 2; k++) {
                double wr = cos(a * k), wi = sin(a * k);
                double xr = re[i+k+len/2] * wr - im[i+k+len/2] * wi, xi = re[i+k+len/2] * wi + im[i+k+len/2] * wr;
                re[i+k+len/2] = re[i+k] - xr; im[i+k+len/2] = im[i+k] - xi; re[i+k] += xr; im[i+k] += xi;
            }
    }
}
static void dc_ltas(uint32_t preset, double *out)
{
    static int16_t pcm[60 * 44100]; static double re[DC_FN], im[DC_FN];
    dc_setup(preset, V_LEGATO); track_t *t = &trk[0];
    host_preset(t, ENGI_DRONE, preset);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0; engine_block(t);
    trk_note_on(t, 36, 100);
    uint32_t n = 0; int32_t buf[CTL];
    for (uint32_t b = 0; b < 60u * FS / CTL; b++) {
        memset(buf, 0, sizeof buf); engine_block(t); track_render(t, buf, CTL);
        for (uint32_t i = 0; i < CTL; i++) { int32_t v = buf[i] / 2; pcm[n++] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
    }
    double acc[DC_NB] = {0}; int frames = 0;
    for (uint32_t o = 30u * FS; o + DC_FN < n; o += FS / 2) {
        for (int i = 0; i < DC_FN; i++) { re[i] = pcm[o + i] * (0.5 - 0.5 * cos(2 * M_PI * i / DC_FN)); im[i] = 0; }
        dc_fft(re, im);
        double cur[DC_NB] = {0}, tot = 0;
        for (int k = 1; k < DC_FN / 2; k++) {
            double f = k * (double)FS / DC_FN; if (f < 30) continue;
            int b = (int)(3 * log2(f / 30)); if (b >= DC_NB) b = DC_NB - 1;
            cur[b] += re[k] * re[k] + im[k] * im[k];
        }
        for (int b = 0; b < DC_NB; b++) tot += cur[b];
        for (int b = 0; b < DC_NB; b++) { double db = 10 * log10(cur[b] / tot + 1e-12); acc[b] += db < -60 ? -60 : db; }
        frames++;
    }
    for (int b = 0; b < DC_NB; b++) out[b] = acc[b] / frames;
}
static int dc_preset_separation(void)
{
    static double l[16][DC_NB]; double minpair = 1e9; uint32_t ia = 0, ib = 0;
    for (uint32_t p = 5; p < 16; p++) dc_ltas(p, l[p]);
    for (uint32_t a = 5; a < 16; a++)
        for (uint32_t b = a + 1; b < 16; b++) {
            double d = 0; for (int k = 0; k < DC_NB; k++) d += fabs(l[a][k] - l[b][k]);
            d /= DC_NB; if (d < minpair) { minpair = d; ia = a; ib = b; }
        }
    printf("CELLS preset separation: closest pair %s / %s at %.2f dB (limit 8.0)\n",
        DRONE_PRESETS[ia].name, DRONE_PRESETS[ib].name, minpair);
    return check("cells: all eleven cell presets at least 8 dB apart in long-term spectrum", minpair >= 8.0);
}
static int dc_automated_family(void)
{
    int bad = 0, cc_ok = 1, mod_ok = 1, direct_ok = 1; uint32_t cc_cases = 0, mod_cases = 0;
    for (uint32_t base = 0; base < 6; base++) {
        dc_setup(base < 3 ? 0 : 5, V_LEGATO); track_t *t = &trk[0]; dc_input_reset();
        int32_t lo = base < 3 ? 0 : 3, hi = lo + 2;
        t->p[P_E3] = base; cc_ok &= ml_learn(27, 0, P_E3) == 1;
        for (uint32_t v = 0; v < 128; v++) {
            int32_t want = lo + ((int32_t)v * 2 + 63) / 127;
            t->p[P_E3] = base; midi_cc_set(t, P_E3, v); cc_ok &= t->p[P_E3] == want;
            t->p[P_E3] = base; cc_ok &= midi_learned(27, v) == 1 && t->p[P_E3] == want;
            t->p[P_E3] = base; cc_ok &= dc_enqueue(0xB0, 27, v); events_block(CTL);
            cc_ok &= t->p[P_E3] == want && track_desc(t, P_E3)->max == 5;
            cc_cases++;
        }
        /* Four shared-destination slots force both range calculation and final
         * clamping to use the original stored base, not an intermediate result. */
        static const uint8_t src[] = {MS_MODW, MS_AT, MS_EXPR, MS_LFO};
        for (uint32_t pattern = 0; pattern < 18; pattern++)
        for (uint32_t wheel = 0; wheel < 128; wheel++) {
            t->p[P_E3] = base; t->mw = wheel; t->at = 127 - wheel; t->ex_off = wheel;
            t->lfo_val = (int32_t)wheel * 516 - 32768; t->lfo_fade = 32767;
            int32_t x[] = {(int32_t)wheel * 258, (127 - (int32_t)wheel) * 258,
                (127 - (int32_t)wheel) * 258, mulq15(t->lfo_val, 32767)};
            int32_t delta = 0;
            for (uint32_t k = 0; k < 4; k++) {
                int32_t amt = pattern < 2 ? (k ? 0 : pattern ? -64 : 63) :
                    ((pattern - 2) & (1u << k)) ? 63 : -64;
                t->p[P_M1SRC + 3 * k] = src[k]; t->p[P_M1DST + 3 * k] = MD_E1 + 3;
                t->p[P_M1AMT + 3 * k] = amt;
                delta += (((x[k] * amt) >> 6) * 2) >> 15;
            }
            int32_t want = clamp((int32_t)base + delta, lo, hi);
            int pass = mod_begin(t) && t->p[P_E3] == want && t->p[P_E3] >= lo && t->p[P_E3] <= hi;
            if (pattern == 0 && wheel == 127 && base == 0) {
                pass &= t->p[P_E3] == 1;
                printf("CELLS MODW->E4 amount63 wheel127 legacybase0: TOPO%d (expected1, never4)\n", t->p[P_E3]);
            }
            mod_end(t); pass &= t->p[P_E3] == (int32_t)base;
            mod_ok &= pass; mod_cases++;
        }
        for (uint32_t k = 0; k < 4; k++) t->p[P_M1AMT + 3 * k] = 0;
        t->p[P_E3] = base; motion_clear(t); motion_set_lock(t, 0, P_E3, base < 3 ? 3 : 2);
        motion_set_enabled(t, 1); motion_begin(); motion_step(t, 0, &motion, 1);
        direct_ok &= t->p[P_E3] == (base < 3 ? 3 : 2); motion_end();
        direct_ok &= t->p[P_E3] == (int32_t)base;
        for (uint32_t topo = 0; topo < 6; topo++) {
            const param_desc_t *d = track_desc(t, P_E3);
            direct_ok &= d->min == 0 && d->max == 5;
            go_title("EDIT 1"); edit_param(3, (int32_t)topo - t->p[P_E3]);
            direct_ok &= t->p[P_E3] == (int32_t)topo;
        }
    }
    printf("CELLS automated-family: %u values x3 CC paths; %u single/multi-slot MOD cases, all bases0..5\n", cc_cases, mod_cases);
    bad += check("cells: all128 CC values/base0..5 via actual midi_cc_set, Learn and USB queue stay in stored family", cc_ok);
    bad += check("cells: MODW E4 reproducer + full-wheel bipolar/multi-slot sweeps use original base family and restore", mod_ok);
    bad += check("cells: generic descriptor, actual UI edits and locks still cross TOPO families across all six bases", direct_ok);
    return bad;
}
static int dc_pitch_boundaries(void)
{
    /* First two are the exact boundary repros; the rest combine fine TUNE,
     * fixed LFO/ENV pitch and matrix PITCH. No fabricated vmod_t is involved. */
    static const struct { uint8_t latest; int16_t bend, tune, fixed, matrix; } cases[] = {
        {127, 512, 0, 0, 0}, {0, -512, 0, 0, 0},
        {127, 512, 37, 4, 7}, {0, -512, -37, -4, -7},
        {127, 0, 37, 4, 7}, {0, 0, -37, -4, -7},
        {127, 0, 0, 0, 63}, {0, 0, 0, 0, -64},
    };
    int bad = 0; uint32_t count = 0;
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++) {
        int ok = 1;
        for (uint32_t c = 0; c < NELEM(cases); c++) {
            dc_setup(5, mode); track_t *t = &trk[0]; dc_input_reset();
            t->p[P_E0] = t->p[P_E1] = t->p[P_E2] = t->p[P_E4] = t->p[P_E5] = 0;
            trk_note_on(t, 48, 100); trk_note_on(t, 60, 100); trk_note_on(t, cases[c].latest, 100);
            drone_part_t *s = drone_state(t); int32_t local[4]; memcpy(local, s->pitch16, sizeof local);
            int pass = s->owner[0] == 48 && local[0] == 48 * 16 && s->ratio[0] == 65536 &&
                t->v[0].pitch_cur == (int32_t)cases[c].latest * 16;
            song.g[G_TUNE] = cases[c].tune;
            if (cases[c].bend) {
                pass &= dc_enqueue(0xE0, cases[c].bend > 0 ? 127 : 0, cases[c].bend > 0 ? 127 : 0);
                events_block(CTL); pass &= midi_bend_target[0] == cases[c].bend;
            }
            pass &= dc_enqueue(0xB0, 1, 127); events_block(CTL);
            t->p[P_LD_PIT] = cases[c].fixed; t->p[P_ED_PIT] = cases[c].fixed;
            t->p[P_M1SRC] = MS_MODW; t->p[P_M1DST] = MD_PITCH; t->p[P_M1AMT] = cases[c].matrix;
            for (uint32_t b = 0; b < 512; b++) {
                t->lfo_val = 12000; t->lfo_fade = 32767;
                mod_begin(t); dc_host_blocks(t, 1); mod_end(t);
                pass &= dc_bounds(s);
            }
            int32_t cents = cases[c].tune;
            int32_t tune = cents >= 0 ? cents * 16 / 100 : -((-cents * 16 + 99) / 100);
            int32_t fine = (cents * 16 - tune * 100) * 2367 / 16000;
            int32_t offset = tune + cases[c].bend / 16 +
                ((mulq15(12000, 32767) * cases[c].fixed * 3) >> 15) +
                (((t->v[0].env >> 9) * cases[c].fixed * 3) >> 15) +
                ((127 * 258 * cases[c].matrix * 3) >> 15);
            pass &= midi_bend_q8[0] == cases[c].bend && !memcmp(local, s->pitch16, sizeof local);
            if (c < 2) pass &= offset == (c == 0 ? 32 : -32);
            for (uint32_t k = 0; k < 4; k++) {
                uint32_t inc = pitch_inc(clamp(local[k] + offset, 0, 2047));
                int64_t tuned = (int64_t)inc + (int64_t)(inc >> 12) * fine;
                int32_t want = tuned > 0x60000000 ? 0x60000000 : (int32_t)tuned;
                pass &= s->ratio[k] == 65536 && llabs((int64_t)s->inc[k] - want) <= 1024;
                if (!pass) printf("CELLS pitch-boundary mode%u case%u cell%u latest%u offset%d fine%d inc%d expected%d\n",
                    mode, c, k, cases[c].latest, offset, fine, s->inc[k], want);
            }
            if (mode == V_LEGATO && c < 2) printf("CELLS raw pitch: latest%u bend%d anchor48 offset%d pitch16=%d inc%d\n",
                cases[c].latest, cases[c].bend / 256, offset, local[0] + offset, s->inc[0]);
            ok &= pass; count++;
        }
        char label[140]; snprintf(label, sizeof label,
            "cells: mode%u latest127/+2st and0/-2st, TUNE+LFO+ENV+matrix preserve unclipped offset for every local cell", mode);
        bad += check(label, ok);
    }
    printf("CELLS pitch-boundary: %u real-track cases, 512 settle blocks each, per-cell clamp after raw offset\n", count);
    return bad;
}
static int dc_gestures(void)
{
    int bad = 0;
    static const int32_t weight[4] = {32767, 28672, 24576, 20480};
    static const uint32_t interval[4] = {0, 16384, 32768, 65536};
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++)
    for (uint32_t prio = 0; prio < 3; prio++)
    for (uint32_t topo = 3; topo < 6; topo++) {
        dc_setup(topo + 2, mode); track_t *t = &trk[0]; t->p[P_PRIO] = prio;
        trk_note_on(t, 48, 96); drone_part_t *s = drone_state(t);
        int seed = s->cells_ready && s->rotate == 1 && dc_held(s, 48);
        for (uint32_t k = 0; k < 4; k++) seed &= s->owner[k] == 48 && s->pitch16[k] == 48 * 16 &&
            s->ratio[k] == 65536u + interval[k] * (uint32_t)t->p[P_E0] / 127 &&
            s->level[k] == mulq15(96 * 258, weight[k]) && s->drive[k] == s->level[k] * 256;
        dc_host_blocks(t, 64);
        drone_part_t before = *s; uint32_t age = t->v[0].age;
        trk_note_on(t, 48, 24);
        int repeat = s->rotate == before.rotate && s->owner[0] == 48 && s->level[0] == 24 * 258 &&
            s->drive[0] == 24 * 258 * 256;
        for (uint32_t k = 1; k < 4; k++) repeat &= s->level[k] == before.level[k] && s->drive[k] == before.drive[k];
        /* LEGATO/UNISON and unchanged priority suppress the engine callback. */
        if (mode == V_LEGATO || mode == V_UNISON) repeat &= t->v[0].age == age;
        repeat &= !memcmp(s->phase, before.phase, sizeof s->phase) && !memcmp(s->inc, before.inc, sizeof s->inc);
        int rotate = 1;
        for (uint32_t n = 60; n < 66; n++) {
            uint32_t slot = 1 + (n - 60) % 3; before = *s;
            trk_note_on(t, n, 40 + n);
            rotate &= s->owner[slot] == n && s->pitch16[slot] == (int32_t)n * 16 && s->ratio[slot] == 65536 &&
                s->rotate == slot % 3 + 1 && s->owner[0] == 48 && s->pitch16[0] == 48 * 16;
            for (uint32_t k = 0; k < 4; k++) if (k != slot)
                rotate &= s->pitch16[k] == before.pitch16[k] && s->ratio[k] == before.ratio[k] &&
                    s->level[k] == before.level[k] && s->drive[k] == before.drive[k];
        }
        before = *s; trk_note_on(t, 64, 7);
        repeat &= s->owner[2] == 64 && s->rotate == before.rotate && s->level[2] == 7 * 258;
        trk_note_off(t, 61); /* obsolete owner of slot2 must not release 64 */
        int stale = dc_held(s, 64) && !dc_held(s, 61) && s->owner[2] == 64 && s->drive[2] == 7 * 258 * 256;
        trk_note_on(t, 61, 72); /* held key that lost ownership reacquires next slot */
        repeat &= s->owner[1] == 61 && s->rotate == 2;
        trk_note_off(t, 48); dc_host_blocks(t, 32);
        int root = !dc_held(s, 48) && dc_held(s, 64) && t->v[0].gate && t->v[0].active && t->v[0].stage != 3;
        trk_all_off(t); dc_host_blocks(t, 16);
        int off = dc_empty(s) && !dc_active(t) && !t->nmono;
        char label[150]; snprintf(label, sizeof label,
            "cells: mode%u prio%u topo%u seed/local rotate/repeat/stale-off/root release/alloff", mode, prio, topo);
        if (!(seed && repeat && rotate && stale && root && off))
            printf("  seed=%d repeat=%d rotate=%d stale=%d root=%d off=%d\n", seed, repeat, rotate, stale, root, off);
        bad += check(label, seed && repeat && rotate && stale && root && off);
    }
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++) {
        dc_setup(5, mode); track_t *t = &trk[0]; drone_part_t *s = drone_state(t);
        trk_note_on(t, 0, 100); dc_host_blocks(t, 8);
        uint32_t age = t->v[0].age; trk_note_on(t, 0, 30);
        int zero = dc_held(s, 0) && s->owner[0] == 0 && s->level[0] == 30 * 258;
        if (mode == V_LEGATO || mode == V_UNISON) zero &= t->v[0].age == age;
        trk_note_on(t, 127, 80); trk_note_off(t, 127);
        zero &= dc_held(s, 0) && t->v[0].gate && t->v[0].stage != 3;
        trk_note_off(t, 0); dc_host_blocks(t, 16);
        zero &= dc_empty(s) && !dc_active(t);
        char label[140]; snprintf(label, sizeof label, "cells: mode%u MIDI0 repeat, fallback to0, final release", mode);
        bad += check(label, zero);
        dc_setup(7, mode); t = &trk[0]; s = drone_state(t);
        for (uint32_t n = 0; n < 128; n++) trk_note_on(t, n, 100);
        int all = 1;
        for (uint32_t k = 0; k < 4; k++) all &= s->held[k] == UINT32_MAX;
        all &= s->owner[0] == 0 && s->pitch16[0] == 0 && dc_active(t) == 1;
        /* Release everything except an evicted (>8-stack) key and its duplicate. */
        trk_note_on(t, 1, 60); trk_note_on(t, 1, 20);
        for (uint32_t n = 0; n < 128; n++) if (n != 1) trk_note_off(t, n);
        dc_host_blocks(t, 32);
        all &= s->held[0] == 2 && !s->held[1] && !s->held[2] && !s->held[3] &&
            t->v[0].active && t->v[0].gate && t->v[0].stage != 3;
        trk_note_off(t, 1); dc_host_blocks(t, 16);
        all &= dc_empty(s) && !dc_active(t);
        snprintf(label, sizeof label, "cells: mode%u all128 held bits, >8 eviction, duplicate on/on/off, final gate", mode);
        bad += check(label, all);
    }
    return bad;
}
/* Independent route/follow expressions keep the sample oracle separate from
 * drone_route/follow and deliberately read only the saved activity snapshot. */
static int32_t dc_follow_ref(int32_t a, int32_t b, int32_t d)
{
    int32_t delta = b - a, q = delta / d;
    return a + (q ? q : delta > 0 ? 1 : delta < 0 ? -1 : 0);
}
static int32_t dc_route_ref(const int32_t x[4], uint32_t k, int32_t topo)
{
    uint32_t pair = k ^ 1u, ring = (k + 3u) % 4;
    int32_t chain = k ? x[k - 1] : 0;
    return topo <= 32768 ? x[pair] + mulq15(chain - x[pair], topo) :
        chain + mulq15(x[ring] - chain, topo - 32768);
}
static int dc_snapshot(void)
{
    int ok = 1; uint32_t cases = 0; int32_t min_gain = 32767, max_inhibit = 0;
    static const int32_t routes[] = {0, 1, 16384, 32768, 49152, 65536};
    for (uint32_t route = 0; route < NELEM(routes); route++)
    for (uint32_t coupling = 0; coupling < 2; coupling++) {
        dc_setup(5, V_LEGATO); track_t *t = &trk[0]; trk_note_on(t, 48, 100);
        drone_part_t *s = drone_state(t); int32_t out[CTL]; vmod_t m = dc_mod(48);
        dc_direct(t, out, CTL, &m);
        t->p[P_E2] = coupling ? 127 : 0;
        t->p[P_E5] = 0; s->ctrl[DR_DRIFT] = 0; /* breathing is exactly neutral at DRIFT 0 */
        s->ctrl[DR_COUPLE] = t->p[P_E2] * 256;
        s->ctrl[DR_TOPO] = routes[route];
        for (uint32_t k = 0; k < 4; k++) {
            s->owner[k] = 48 + k; s->held[1] |= 1u << (16 + k);
            s->activity[k] = (k == 0 ? 32767 : (int32_t)k * 5000) * 256;
            s->amp[k] = (30000 - (int32_t)k * 6000) * 256;
            s->drive[k] = (31000 - (int32_t)k * 5000) * 256;
            s->level[k] = 32000 - (int32_t)k * 4000;
            s->prev[k] = (int32_t)k * 10000 - 15000;
        }
        s->held[1] &= ~(1u << 18); /* one cell releases while neighbors hold */
        drone_part_t before = *s;
        int32_t target_topo = 0, topo = dc_follow_ref(before.ctrl[DR_TOPO], target_topo, 1024);
        int32_t activity[4], predicted[4], pmsrc[4], sum = 0;
        for (uint32_t k = 0; k < 4; k++) activity[k] = before.activity[k] / 256;
        int32_t drive[4], amp[4], act[4];
        for (uint32_t k = 0; k < 4; k++) {
            int32_t hold = dc_held(&before, before.owner[k]) ? before.level[k] * 192 : 0;
            drive[k] = dc_follow_ref(before.drive[k], hold, 65536 + (int32_t)k * 32768);
            int32_t neighbor = clamp(dc_route_ref(activity, k, topo), 0, 32767);
            int32_t inhibition = mulq15(neighbor, before.ctrl[DR_COUPLE]) / 2;
            if (inhibition > max_inhibit) max_inhibit = inhibition;
            if (32767 - inhibition < min_gain) min_gain = 32767 - inhibition;
            ok &= inhibition >= 0 && inhibition <= 16383;
            int32_t goal = mulq15(drive[k] / 256, 32767 - inhibition) * 256;
            amp[k] = dc_follow_ref(before.amp[k], goal, 1024);
            act[k] = dc_follow_ref(before.activity[k], amp[k], 16384 + (int32_t)k * 8192);
            int32_t pm = mulq15(dc_route_ref(before.prev, k, topo), before.ctrl[DR_COUPLE]);
            int32_t x = sine_i(before.phase[k] + (uint32_t)pm * 32768u);
            int32_t cube = mulq15(mulq15(x, x), x);
            x += mulq15((x - cube) / 2, before.ctrl[DR_CHAR]);
            int32_t gn = mulq15(amp[k] / 256, 32767);
            predicted[k] = mulq15(x, gn);
            pmsrc[k] = mulq15(x, gn > 8191 ? 32767 : gn * 4); /* PM: 4x makeup, saturating */
            sum += predicted[k];
        }
        dc_direct(t, out, 1, &m);
        for (uint32_t k = 0; k < 4; k++)
            ok &= s->drive[k] == drive[k] && s->amp[k] == amp[k] && s->activity[k] == act[k] && s->prev[k] == pmsrc[k];
        int32_t x = sum / 4, hp = x - before.dc_in + before.dc_out - before.dc_out / 512;
        int32_t g = 128 + s->ctrl[DR_TONE] * 30000 / 32512;
        int32_t low = before.low + (int32_t)(((int64_t)(hp - before.low) * g) / 32768);
        ok &= s->dc_in == x && s->dc_out == hp && s->low == low &&
            out[0] == voice_amp(soft_knee(low, 16000) * 2, &m, 0) * 2;
        cases++;
    }
    printf("CELLS snapshot: %u cases; inhibition max %d/32768, gain min %d/32768\n", cases, max_inhibit, min_gain);
    return check("cells: one-sample oracle, snapshot inhibition <=50%, weighted audible output and made-up PM source", ok);
}
static int dc_velocity_pitch(void)
{
    int bad = 0; dc_setup(5, V_LEGATO); track_t *t = &trk[0];
    t->p[P_E0] = t->p[P_E1] = t->p[P_E5] = 0;
    trk_note_on(t, 48, 100); trk_note_on(t, 55, 100); trk_note_on(t, 62, 100); trk_note_on(t, 69, 100);
    dc_host_blocks(t, 512); drone_part_t *s = drone_state(t), before = *s;
    int32_t env = t->v[0].env_out; trk_note_on(t, 55, 1); dc_host_blocks(t, 1);
    int local = t->v[0].env_out == env && s->level[1] == 258;
    for (uint32_t k = 0; k < 4; k++) if (k != 1) local &= s->level[k] == before.level[k];
    bad += check("cells: low-velocity repeated gesture changes local drive, not whole-network host gain", local);
    int pitch_ok = 1;
    /* Host glide/last note can move hundreds of pitch16 units without
     * transposing the existing local cells. Only explicit pitch offsets apply. */
    t->p[P_GLIDE] = 127; trk_note_on(t, 100, 90);
    for (uint32_t b = 0; b < 512; b++) {
        dc_host_blocks(t, 1);
        for (uint32_t k = 0; k < 4; k++) if (k != 1)
            pitch_ok &= llabs((int64_t)s->inc[k] - pitch_inc(s->pitch16[k])) <= 1024;
    }
    t->p[P_TRANS] = 12; dc_host_blocks(t, 64); /* global key map cannot retune held cells */
    for (uint32_t k = 0; k < 4; k++) {
        int32_t target = (int32_t)pitch_inc(s->pitch16[k]);
        pitch_ok &= llabs((int64_t)s->inc[k] - target) <= 1024;
    }
    bad += check("cells: host glide/new high note preserves every local pitch instead of global transpose", pitch_ok);
    /* Exercise the real voice tuning/bend/LFO/ENV and matrix pitch path. */
    t->p[P_GLIDE] = 0; t->v[0].pitch_cur = t->v[0].pitch16;
    song.g[G_TUNE] = 37; midi_bend_q8[0] = midi_bend_target[0] = 256;
    dc_host_blocks(t, 512);
    pitch_ok = 1;
    int32_t tune = 37 * 16 / 100, fine = (37 * 16 - tune * 100) * 2367 / 16000;
    for (uint32_t k = 0; k < 4; k++) {
        uint32_t inc = pitch_inc(clamp(s->pitch16[k] + tune + 16, 0, 2047));
        int32_t target = (int32_t)(inc + (inc >> 12) * fine);
        pitch_ok &= llabs((int64_t)s->inc[k] - target) <= 1024;
    }
    bad += check("cells: real TUNE37 cents and +1 semitone MIDI bend apply to each local pitch (fine retained)", pitch_ok);
    song.g[G_TUNE] = 0; midi_bend_q8[0] = midi_bend_target[0] = 0;
    t->p[P_LD_PIT] = 2; t->p[P_ED_PIT] = 3;
    t->p[P_M1SRC] = MS_MODW; t->p[P_M1DST] = MD_PITCH; t->p[P_M1AMT] = 7;
    t->mw = 80;
    for (uint32_t b = 0; b < 512; b++) {
        t->lfo_val = 12000; t->lfo_fade = 32767;
        mod_begin(t); dc_host_blocks(t, 1); mod_end(t);
    }
    int32_t offset = ((mulq15(12000, 32767) * 2 * 3) >> 15) +
        (((t->v[0].env >> 9) * 3 * 3) >> 15) + ((80 * 258 * 7 * 3) >> 15);
    pitch_ok = dc_bounds(s);
    for (uint32_t k = 0; k < 4; k++)
        pitch_ok &= llabs((int64_t)s->inc[k] - pitch_inc(clamp(s->pitch16[k] + offset, 0, 2047))) <= 1024;
    bad += check("cells: real LFO/ENV/matrix pitch offsets reach each local carrier independently", pitch_ok);
    /* Direct pitch-offset equivalence rejects any accidental use of m.inc or
     * later host glide, including residual fine modulation. */
    vmod_t m = dc_mod(48); m.pitch16 = t->v[0].pitch_cur + 13; m.fine = -17;
    before = *s; int32_t a[CTL], b[CTL]; dc_direct(t, a, CTL, &m); drone_part_t after = *s;
    *s = before; int32_t saved = t->v[0].pitch_cur; t->v[0].pitch_cur += 400;
    m.pitch16 += 400; m.inc = 1; dc_direct(t, b, CTL, &m); t->v[0].pitch_cur = saved;
    bad += check("cells: equal relative pitch/fine offsets ignore m.inc and host global pitch/glide exactly",
        !memcmp(a, b, sizeof a) && !memcmp(&after, s, sizeof after));
    return bad;
}
static int dc_recovery(void)
{
    dc_setup(5, V_LEGATO); track_t *t = &trk[0]; trk_note_on(t, 48, 127);
    drone_part_t *s = drone_state(t); t->p[P_E2] = 127;
    vmod_t m = dc_mod(48); int32_t out[CTL];
    for (uint32_t b = 0; b < 12u * FS / CTL; b++) dc_direct(t, out, CTL, &m);
    int32_t inhibited[4]; memcpy(inhibited, s->amp, sizeof inhibited);
    t->p[P_E2] = 0;
    /* drive time constants: 1.49/2.23/2.97/3.72 s; activity: .37.. .93 s;
     * recovery includes 23ms COUPLE/amp slew. 12s is >3 slowest drive taus,
     * not an assertion that the inhibition itself takes 12s to disappear. */
    for (uint32_t b = 0; b < 12u * FS / CTL; b++) dc_direct(t, out, CTL, &m);
    int ok = s->ctrl[DR_COUPLE] == 0;
    for (uint32_t k = 0; k < 4; k++) {
        int32_t unity = mulq15(s->drive[k] / 256, 32767) * 256;
        printf("CELLS recover cell%u: inhibited %.4f -> %.4f, unity target %.4f (12s COUPLE0)\n",
            k, inhibited[k] / (double)DC_Q23, s->amp[k] / (double)DC_Q23, unity / (double)DC_Q23);
        ok &= s->amp[k] > inhibited[k] && abs(s->amp[k] - unity) < 8192;
    }
    int bad = check("cells: coupling0 removes neighbor suppression and recovers local gain within12s", ok);
    trk_note_off(t, 48); int32_t old_drive[4]; memcpy(old_drive, s->drive, sizeof old_drive);
    for (uint32_t b = 0; b < 2u * FS / CTL; b++) dc_direct(t, out, CTL, &m);
    ok = 1; for (uint32_t k = 0; k < 4; k++) ok &= s->drive[k] < old_drive[k] && s->drive[k] >= 0;
    bad += check("cells: individual note-off makes each owned drive decay (no self-latch)", ok);
    return bad;
}
static int dc_lifecycle(void)
{
    int bad = 0;
    for (uint32_t action = 0; action < 4; action++) {
        dc_setup(6, V_LEGATO); track_t *t = &trk[0];
        trk_note_on(t, 0, 90); trk_note_on(t, 63, 70); dc_host_blocks(t, 64);
        if (action == 0) trk_all_off(t);
        if (action == 1) { panic_req |= 1u; int32_t out[2 * CTL]; mix_block(out, CTL); }
        if (action == 2) { t->p[P_MUTE] = 1; panic_req |= 1u; int32_t out[2 * CTL]; mix_block(out, CTL); }
        if (action == 3) { apply_preset_to(t, 7); int32_t out[2 * CTL]; mix_block(out, CTL); }
        int ok = dc_empty(drone_state(t)); dc_host_blocks(t, 64);
        if (action == 2) {
            drone_part_t before = *drone_state(t); trk_note_on(t, 99, 127);
            ok &= !memcmp(&before, drone_state(t), sizeof before); t->p[P_MUTE] = 0;
        }
        if (action != 3) ok &= !dc_active(t) && !t->nmono && !t->xp_n;
        trk_note_on(t, 72, 100);
        for (uint32_t k = 0; k < 4; k++) ok &= drone_state(t)->owner[k] == 72;
        ok &= dc_held(drone_state(t), 72) && !dc_held(drone_state(t), 0) && !dc_held(drone_state(t), 63);
        char label[140]; snprintf(label, sizeof label, "cells: %s clears gestures, muted on ignored, next note reseeds all owners",
            (const char *const[]){"alloff", "panic", "mute/panic", "same-engine preset load"}[action]);
        bad += check(label, ok);
    }
    /* Actual mixer mute, without a synthetic panic: new note-ons are ignored
     * but releases still reach the held mask and cannot leave a stuck phrase. */
    for (uint32_t mode = V_POLY; mode <= V_UNISON; mode++) {
        dc_setup(5, mode); track_t *t = &trk[0]; trk_note_on(t, 0, 100); trk_note_on(t, 63, 100);
        tracks_edit(3, 1);
        drone_part_t before = *drone_state(t); trk_note_on(t, 99, 127);
        int ok = t->p[P_MUTE] && !memcmp(&before, drone_state(t), sizeof before);
        trk_note_off(t, 0); dc_host_blocks(t, 8);
        ok &= !dc_held(drone_state(t), 0) && dc_held(drone_state(t), 63) && t->v[0].gate;
        trk_note_off(t, 63); dc_host_blocks(t, 16);
        ok &= dc_empty(drone_state(t)) && !dc_active(t);
        tracks_edit(3, -1); trk_note_on(t, 72, 100);
        ok &= !t->p[P_MUTE] && dc_held(drone_state(t), 72);
        for (uint32_t k = 0; k < 4; k++) ok &= drone_state(t)->owner[k] == 72;
        char label[140]; snprintf(label, sizeof label,
            "cells: mode%u real mixer MUTE ignores on, accepts off, no stuck ownership after unmute", mode);
        bad += check(label, ok);
    }
    static const uint8_t dest[] = {0, ENGI_SEM, ENGI_HADES, ENGI_FM6};
    for (uint32_t k = 0; k < NELEM(dest); k++) {
        dc_setup(5, V_LEGATO); track_t *t = &trk[0]; trk_note_on(t, 48, 90); trk_note_on(t, 60, 100);
        dc_host_blocks(t, 16); host_preset_req(t, dest[k], 0); engine_block(t);
        drone_part_t saved = *drone_state(t);
        trk_note_on(t, 77, 100); trk_note_on(t, 78, 100); trk_note_off(t, 78);
        int ok = !memcmp(&saved, drone_state(t), sizeof saved) && t->xp_n == 1;
        dc_host_blocks(t, XF_BLOCKS + 2);
        ok &= t->engine == dest[k] && !t->xf_on && !t->xp_n;
        if (eng_mem_owner[0] == ENGI_DRONE + 1u) ok &= dc_empty(drone_state(t));
        host_preset_req(t, ENGI_DRONE, 6); engine_block(t);
        trk_note_on(t, 84, 100); trk_note_on(t, 85, 100); trk_note_off(t, 85);
        dc_host_blocks(t, XF_BLOCKS + 2);
        ok &= t->engine == ENGI_DRONE && !t->xf_on && !t->xp_n && dc_held(drone_state(t), 84) &&
            !dc_held(drone_state(t), 48) && !dc_held(drone_state(t), 60) && !dc_held(drone_state(t), 85);
        for (uint32_t c = 0; c < 4; c++) ok &= drone_state(t)->owner[c] == 84;
        char label[140]; snprintf(label, sizeof label, "cells: departure/queued swap via engine%u, cancelled queued off, clean return ownership", dest[k]);
        bad += check(label, ok);
    }
    return bad;
}
static void dc_full_boundary(void)
{
    trk[0].seq_idx = trk[0].p[P_SLEN] - 1u;
    trk[0].seq_pos = step_samples(&trk[0], div_samples(trk[0].p[P_SDIV]), trk[0].seq_idx) - CTL;
    project_pattern_tick(CTL);
}
static int dc_full_load(void)
{
    int bad = 0;
    for (uint32_t sounding = 0; sounding < 2; sounding++) {
        dc_setup(7, V_LEGATO); track_t *t = &trk[0]; menu_put(MI_LIVELOAD, 1);
        for (uint32_t k = 0; k < NTRK; k++) {
            memset(trk[k].step, 0, sizeof trk[k].step); trk[k].p[P_SLEN] = 8;
        }
        int ok = project_save(1) == 0;
        host_preset(t, ENGI_DRONE, 5); trk_note_on(t, 0, 100); trk_note_on(t, 61, 80);
        dc_host_blocks(t, 32);
        if (!sounding) { t->p[P_REL] = 0; trk_all_off(t); dc_host_blocks(t, 32); }
        ok &= !!dc_active(t) == !!sounding;
        seq_start();
        if (sounding) { trk_note_on(t, 0, 100); trk_note_on(t, 61, 80); dc_host_blocks(t, 8); }
        ok &= project_pattern_queue(1) == 0 && chain.live_full;
        dc_full_boundary(); ok &= !chain.live_pending && t->eng_req == ENGI_DRONE && t->p[P_E3] == 5;
        if (sounding) {
            trk_note_on(t, 72, 100); trk_note_on(t, 73, 100); trk_note_off(t, 73);
            dc_host_blocks(t, XF_BLOCKS + 2);
        } else trk_note_on(t, 72, 100);
        drone_part_t *s = drone_state(t);
        ok &= dc_held(s, 72) && !dc_held(s, 0) && !dc_held(s, 61) && !dc_held(s, 73);
        for (uint32_t k = 0; k < 4; k++) ok &= s->owner[k] == 72 && s->pitch16[k] == 72 * 16;
        if (!ok) printf("CELLS FULL same-engine sounding%u: held=%08x/%08x/%08x/%08x owners=%u,%u,%u,%u xf=%u xp=%u\n",
            sounding, s->held[0], s->held[1], s->held[2], s->held[3], s->owner[0], s->owner[1], s->owner[2], s->owner[3], t->xf_on, t->xp_n);
        bad += check(sounding ? "cells: FULL same-engine sounding sound swap clears old ownership before queued note" :
            "cells: FULL same-engine silent sound swap starts with only new ownership", ok);
        seq_stop();
    }
    return bad;
}
static int dc_corners(void)
{
    static const uint8_t notes[] = {0, 24, 48, 84, 108, 127};
    uint32_t cases = 0; int ok = 1; int64_t peak = 0;
    for (uint32_t topo = 0; topo < 6; topo++)
    for (uint32_t ni = 0; ni < NELEM(notes); ni++)
    for (uint32_t bits = 0; bits < 128; bits++) {
        dc_setup(5, V_LEGATO); track_t *t = &trk[0]; t->p[P_E3] = topo;
        for (uint32_t k = 0, bit = 0; k < 8; k++) if (k != 3)
            t->p[P_E0 + k] = bits & (1u << bit++) ? 127 : 0;
        memset((uint8_t *)ENG_MEM(0) + ENG_DRONE.mem, 0xA7, ENG_MEM_PART - ENG_DRONE.mem);
        trk_note_on(t, notes[ni], 127);
        vmod_t m = dc_mod(notes[ni]);
        for (uint32_t b = 0; b < 80; b++) {
            int32_t out[CTL]; m.cutoff = b & 1 ? 127 * 256 : -127 * 256;
            m.shape = b & 1 ? 127 * 256 : 0;
            m.fine = b & 1 ? 4095 : -4095;
            m.pitch16 = t->v[0].pitch_cur + (b & 1 ? 2047 : -2047);
            if (b == 20 && topo >= 3) trk_note_on(t, (notes[ni] + 7) & 127, 1);
            if (b == 40 && topo >= 3) trk_note_off(t, notes[ni]);
            dc_direct(t, out, CTL, &m);
            ok &= dc_bounds(drone_state(t));
            for (uint32_t i = 0; i < CTL; i++) {
                int64_t a = llabs((int64_t)out[i]); if (a > peak) peak = a; ok &= a < 200000;
            }
        }
        for (uint32_t k = ENG_DRONE.mem; k < ENG_MEM_PART; k++) ok &= ((uint8_t *)ENG_MEM(0))[k] == 0xA7;
        cases++;
    }
    printf("CELLS corners: %u cases x %u samples, peak %lld, state %u, legacy prefix %zu bytes\n",
        cases, 80u * CTL, (long long)peak, ENG_DRONE.mem, sizeof(dc_legacy_t));
    return check("cells: TOPO0..5 x6 pitches x128 control corners, extreme mods, all state bounds/canaries", ok && peak > 100);
}
static int dc_live_isolation(void)
{
    enum { BLOCKS = 1200 }; static int32_t ref[BLOCKS * CTL];
    dc_setup(5, V_LEGATO); track_t *a = &trk[0], *b = &trk[1]; trk_note_on(a, 48, 100);
    vmod_t m = dc_mod(48);
    drone_part_t initial = *drone_state(a);
    for (uint32_t i = 0; i < BLOCKS; i++) dc_direct(a, ref + i * CTL, CTL, &m);
    drone_part_t final = *drone_state(a); *drone_state(a) = initial;
    trk_note_on(b, 24, 127); int ok = 1;
    for (uint32_t i = 0; i < BLOCKS; i++) {
        int32_t out[CTL], other[CTL]; b->p[P_E3] = i % 6;
        if (i % 31 == 0) trk_note_on(b, (i / 31 + 24) % 128, 127);
        if (i % 47 == 0) trk_note_off(b, (i / 47 + 24) % 128);
        dc_direct(b, other, CTL, &m); dc_direct(a, out, CTL, &m);
        ok &= !memcmp(out, ref + i * CTL, sizeof out) && dc_bounds(drone_state(b));
    }
    int bad = check("cells: other part gestures/live TOPO0..5 cannot change first part samples or any state byte",
        ok && !memcmp(&final, drone_state(a), sizeof final));
    *drone_state(a) = initial; int32_t out[CTL], split[CTL]; dc_direct(a, out, CTL, &m); final = *drone_state(a);
    *drone_state(a) = initial; memset(split, 0, sizeof split);
    for (uint32_t i = 0; i < CTL; i++) ENG_DRONE.render(a, &a->v[0], split + i, 1, &m);
    bad += check("cells: block/single-sample rendering identical including slow activity snapshot state",
        !memcmp(out, split, sizeof out) && !memcmp(&final, drone_state(a), sizeof final));
    initial = *drone_state(a); out[0] = 12345; ENG_DRONE.render(a, &a->v[0], out, 0, &m);
    bad += check("cells: zero-size call is a complete no-op", out[0] == 12345 && !memcmp(&initial, drone_state(a), sizeof initial));
    int64_t peak = 0, step = 0; int32_t last = 0; ok = 1;
    for (uint32_t i = 0; i < 4u * FS / CTL; i++) {
        if (i % 97 == 0) {
            a->p[P_E3] = (i / 97) % 6;
            for (uint32_t k = 0; k < 8; k++) if (k != 3) a->p[P_E0 + k] = (i / 97 + k) & 1 ? 127 : 0;
            trk_note_on(a, 24 + (i / 97) % 80, 1 + i % 127);
        }
        dc_direct(a, out, CTL, &m); ok &= dc_bounds(drone_state(a));
        for (uint32_t k = 0; k < CTL; k++) {
            int64_t v = llabs((int64_t)out[k]), d = llabs((int64_t)out[k] - last);
            if (v > peak) peak = v; if (d > step) step = d; last = out[k]; ok &= v < 200000;
        }
    }
    printf("CELLS live TOPO0..5: peak %lld, largest sample step %lld (not a listening/click verdict)\n", (long long)peak, (long long)step);
    bad += check("cells: live old/new topology and all-control jumps remain bounded", ok && peak > 100);
    m.amp0 = m.amp1 = 0; ok = 1;
    for (uint32_t topo = 0; topo < 6; topo++) {
        a->p[P_E3] = topo; dc_direct(a, out, CTL, &m);
        for (uint32_t k = 0; k < CTL; k++) ok &= out[k] == 0;
    }
    bad += check("cells: AMP-zero remains exact silence even across cell/legacy bridge", ok);
    return bad;
}
static void dc_bench(void)
{
    dc_setup(7, V_LEGATO); vmod_t m[NPART];
    for (uint32_t p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 36 + 12 * p, 127); m[p] = dc_mod(36 + 12 * p);
        for (uint32_t k = 0; k < 8; k++) trk[p].p[P_E0 + k] = k == 3 ? 5 : 127;
    }
    const uint32_t blocks = 4096; clock_t start = clock();
    for (uint32_t b = 0; b < blocks; b++) for (uint32_t p = 0; p < NPART; p++) {
        int32_t out[CTL]; dc_direct(&trk[p], out, CTL, &m[p]); dc_sink += out[b % CTL];
    }
    double sec = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("CELLS native cost: four tracks /16 cells, %.1f ns/frame total, %.3fx audio duration; direct DSP, no FX, not device CPU\n",
        sec * 1e9 / (blocks * CTL), sec / ((double)blocks * CTL / FS));
}
#ifdef C1_HOST
typedef struct { uint64_t hash, state_hash; int32_t peak[NPART]; uint32_t taken, healthy; } dc_parity_t;
static uint64_t dc_hash(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p; for (size_t k = 0; k < n; k++) { h ^= b[k]; h *= 0x100000001B3ull; } return h;
}
static dc_parity_t dc_scene(uint32_t scene, int dual, const char *mask)
{
    dc_setup(5, V_LEGATO); c1h_off = !dual; setenv("C1_MASK", mask, 1);
    dc_parity_t r = {.hash = 0xCBF29CE484222325ull, .state_hash = 0xCBF29CE484222325ull, .healthy = 1};
    static const uint8_t mixed[] = {ENGI_DRONE, ENGI_HADES, ENGI_SEM, ENGI_FM6};
    for (uint32_t p = 0; p < NPART; p++) {
        track_t *t = &trk[p]; uint32_t e = scene ? mixed[p] : ENGI_DRONE;
        host_preset(t, e, e == ENGI_DRONE ? 5 + p % 3 : p % ENGINES[e]->npresets);
        t->p[P_VOICE] = p % 4; t->p[P_ATK] = 10; t->p[P_DEC] = 20; t->p[P_SUS] = 110; t->p[P_REL] = 30;
        t->p[P_DIST] = 0; t->p[P_CHOR] = 12 + p * 3; t->p[P_DLY] = 10 + p * 4; t->p[P_REV] = 18 + p * 3;
        t->p[P_ITYPE] = p + 1; t->p[P_IMIX] = 50;
        t->p[P_LWAVE] = 4; t->p[P_LRATE] = 90 + 5 * p;
        t->p[P_M1SRC] = MS_SH; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 12;
        t->p[P_M2SRC] = MS_LFO; t->p[P_M2DST] = MD_RATE; t->p[P_M2AMT] = 10;
        trk_note_on(t, 36 + p * 5, 100);
    }
    for (uint32_t b = 0; b < 3u * FS / CTL; b++) {
        for (uint32_t p = 0; p < NPART; p++) {
            track_t *t = &trk[p];
            if (t->engine == ENGI_DRONE && b % 97 == 0) {
                t->p[P_E3] = 3 + (b / 97 + p) % 3; t->p[P_E2] = (b / 97 * 19 + p * 31) % 128;
                trk_note_on(t, (b / 97 * 7 + 48 + p) % 128, 1 + b % 127);
            }
            if (b == 400 || b == 500) trk_note_on(t, 60 + p * 5, 20);
            if (b == 800) trk_note_off(t, 36 + p * 5);
            if (b == 1000) trk_all_off(t);
        }
        int32_t out[2 * CTL]; mix_block(out, CTL); r.hash = dc_hash(r.hash, out, sizeof out);
        for (uint32_t p = 0; p < NPART; p++) {
            if (trk[p].peak > r.peak[p]) r.peak[p] = trk[p].peak;
            if (trk[p].engine == ENGI_DRONE) r.healthy &= dc_bounds(drone_state(&trk[p]));
        }
    }
    for (uint32_t p = 0; p < NPART; p++) if (trk[p].engine == ENGI_DRONE)
        r.state_hash = dc_hash(r.state_hash, drone_state(&trk[p]), sizeof(drone_part_t));
    r.taken = c1h_taken; return r;
}
static int dc_scene_fork(uint32_t scene, int dual, const char *mask, dc_parity_t *r)
{
    int fd[2], status = 0; if (pipe(fd)) return 0; fflush(NULL); pid_t pid = fork();
    if (pid < 0) { close(fd[0]); close(fd[1]); return 0; }
    if (!pid) {
        close(fd[0]); dc_parity_t got = dc_scene(scene, dual, mask);
        int ok = write(fd[1], &got, sizeof got) == sizeof got; close(fd[1]); _exit(ok ? 0 : 2);
    }
    close(fd[1]); int ok = read(fd[0], r, sizeof *r) == sizeof *r; close(fd[0]);
    ok &= waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return ok;
}
static int dc_cpu2(void)
{
    static const char *const masks[] = {"rot", "auto", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};
    int bad = 0;
    /* Before parent DSP/threads: fork isolates RNG, FX, pool and worker state. */
    for (uint32_t scene = 0; scene < 2; scene++) {
        dc_parity_t ref = {0}; int ok = dc_scene_fork(scene, 0, "rot", &ref) && ref.healthy;
        for (uint32_t p = 0; p < NPART; p++) ok &= ref.peak[p] > 200;
        for (uint32_t k = 0; k < NELEM(masks); k++) {
            dc_parity_t got = {0}; int same = dc_scene_fork(scene, 1, masks[k], &got) && got.healthy && got.taken &&
                got.hash == ref.hash && got.state_hash == ref.state_hash && !memcmp(got.peak, ref.peak, sizeof ref.peak);
            if (!same) printf("CELLS CPU2 scene%u mask%s: output %016llx/%016llx state %016llx/%016llx jobs%u healthy%u FAIL\n",
                scene, masks[k], (unsigned long long)got.hash, (unsigned long long)ref.hash,
                (unsigned long long)got.state_hash, (unsigned long long)ref.state_hash, got.taken, got.healthy);
            ok &= same;
        }
        char label[150]; snprintf(label, sizeof label, "cells: CPU2 %s, 16 masks, samples/peaks/full cell state bit-identical, healthy nonzero parts",
            scene ? "DRONE+HADES+SEM+FM6" : "4 cells tracks presets5..7");
        bad += check(label, ok);
    }
    return bad;
}
#endif
int main(int argc, char **argv)
{
#ifdef C1_HOST
    return !!dc_cpu2();
#else
    if (argc > 1 && !strcmp(argv[1], "--dsp-only")) {
        int bad = dc_snapshot() + dc_recovery() + dc_corners() + dc_live_isolation();
        printf("DRONE CELLS isolated DSP: %s (%d failed checks)\n", bad ? "FAIL" : "PASS", bad);
        return !!bad;
    }
    int bad = dc_metadata() + dc_family_events() + dc_ghost_release() + dc_division_exact() + dc_richness() + dc_life() + dc_preset_separation() + dc_automated_family() + dc_pitch_boundaries() +
        dc_gestures() + dc_snapshot() + dc_velocity_pitch() + dc_recovery() +
        dc_lifecycle() + dc_full_load() + dc_corners() + dc_live_isolation();
    dc_bench(); printf("DRONE CELLS: %s (%d failed checks)\n", bad ? "FAIL" : "PASS", bad); return !!bad;
#endif
}
