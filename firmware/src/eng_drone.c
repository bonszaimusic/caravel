/* SPDX-License-Identifier: GPL-3.0-only */
/* Original coupled drone DSP, inspired by the instrument concept, not a
 * circuit model or clone. See docs/drone-engine.md for lifecycle and limits.
 * Included by engines.c after dsp.c and the ENG_MEM definitions. */
enum { DR_COUPLE, DR_TOPO, DR_CHAR, DR_DRIFT, DR_RATE, DR_TONE, DR_NCTRL };
typedef struct {
    uint32_t phase[4], drift_phase[4];
    int32_t prev[4], inc[4], ctrl[DR_NCTRL];
    int32_t low, dc_in, dc_out;
    uint32_t ready;
    /* Append-only: the legacy DSP prefix above is unchanged. Q23 activity
     * retains fractional steps on the multi-second cell time scales. */
    uint32_t held[4], owner[4], ratio[4], rotate, cells_ready;
    int32_t pitch16[4], level[4], drive[4], activity[4], amp[4], cell_mix;
    int32_t cell_last;
    uint32_t cell_exit;
    /* Append-only (0.3.2, cell modes only): per-cell interval roughness (Q15, smoothed), the PM feedback
     * low-pass state and the phase of the slow loudness movement. The slow quantities (roughness, BREATH)
     * are updated every 8th sample (slow counts samples, so any block split renders the same): fcpl is cell k's
     * COUPLE factor, chrk its CHAR, bred the loudness taken away by BREATH. All zero on a fresh part. */
    int32_t rough[4], pmlp[4];
    uint32_t breath[4];
    uint32_t slow;
    int32_t fcpl[4], chrk[4], bred[4];
} drone_part_t;
ENG_MEM_FITS("DRONE", sizeof(drone_part_t));

static const char *const N_DRONE_TOPO[] = {"PAIRS", "CHAIN", "RING", "CELLPAIR", "CELLCHN", "CELLRING"};
static drone_part_t *drone_state(const track_t *t)
{
    return (drone_part_t *)ENG_MEM((uint32_t)(t - trk));
}
/* The pi32v2 compiler turns even a division by a constant into a hardware
 * divide (tens of cycles; the cell renderer had ~53 per sample). Divisions by
 * 2^s use this shift form: the same result as C's '/' (truncation toward zero)
 * for every int32 d, so rendered samples are bit-identical. */
static inline int32_t drone_div2s(int32_t d, uint32_t s)
{
    return (d + ((d >> 31) & (int32_t)((1u << s) - 1u))) >> s;
}
/* Sample-rate exponential smoothing, with an exact integer endpoint.
 * ~23 ms time constant at 44.1 kHz; no block-size-dependent slew. */
static inline int32_t drone_follow(int32_t a, int32_t b)
{
    int32_t d = b - a, step = drone_div2s(d, 10);
    return a + (step ? step : (d > 0) - (d < 0));
}
static inline int32_t drone_follow_s(int32_t a, int32_t b, uint32_t sh)   /* drone_follow with a time constant of 2^sh samples */
{
    int32_t d = b - a, step = drone_div2s(d, sh);
    return a + (step ? step : (d > 0) - (d < 0));
}
static void drone_controls(const track_t *t, const vmod_t *m, int32_t *c)
{
    c[DR_COUPLE] = clamp(t->p[P_E2] * 256 + (m ? m->shape - 64 * 256 : 0), 0, 32512);
    c[DR_TOPO] = clamp(t->p[P_E3], 0, 2) * 32768;
    c[DR_CHAR] = clamp(t->p[P_E4], 0, 127) * 258;
    c[DR_DRIFT] = clamp(t->p[P_E5], 0, 127) * 258;
    c[DR_RATE] = clamp(t->p[P_E6], 0, 127) * 258;
    c[DR_TONE] = clamp(t->p[P_E7] * 256 + (m ? m->cutoff : 0), 0, 32512);
}
static void drone_note_on(track_t *t, voice_t *v)
{
    drone_part_t *s = drone_state(t);
    (void)v;
    /* voice_start has already changed active/stage. Do not use those, env_out
     * (which AMP modulation can zero), or the host's partial keep restoration
     * to decide whether a sounding network should be reset. Pool ownership is
     * the reset boundary. Warm notes, steals and retriggers preserve all DSP. */
    if (s->ready) return;
    for (uint32_t k = 0; k < 4u; k++) {
        s->phase[k] = k * 0x19234567u;
        s->drift_phase[k] = k * 0x39123457u;
        s->prev[k] = s->inc[k] = 0;
    }
    s->low = s->dc_in = s->dc_out = 0;
    drone_controls(t, 0, s->ctrl);
    s->ready = 1u; /* first render installs pitch without a glide from zero */
}
static __attribute__((noinline)) void drone_legacy_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (!n) return;
    drone_part_t *s = drone_state(t);
    int32_t target[DR_NCTRL], freq[4];
    /* SPREAD continuously opens unison to 1 : 5/4 : 3/2 : 2. */
    static const uint32_t interval[4] = {0, 16384, 32768, 65536};
    static const int32_t detune[4] = {-3, -1, 1, 3};
    static const uint8_t pairs[4] = {1, 0, 3, 2};
    static const uint8_t ring[4] = {3, 0, 1, 2};
    if (!s->ready) drone_note_on(t, v);
    drone_controls(t, m, target);
    for (uint32_t k = 0; k < 4u; k++) {
        uint32_t ratio = 65536u + interval[k] * (uint32_t)clamp(t->p[P_E0], 0, 127) / 127u;
        int32_t fine = clamp(t->p[P_E1], 0, 127) * detune[k] * 252 / 127;
        uint64_t hz = ((uint64_t)m->inc * ratio) >> 16;
        hz = (hz * (uint32_t)(65536 + fine)) >> 16;
        freq[k] = (int32_t)(hz > 0x60000000u ? 0x60000000u : hz);
        if (s->ready == 1u) s->inc[k] = freq[k];
    }
    s->ready = 2u;
    for (uint32_t i = 0; i < n; i++) {
        int32_t next[4], sum = 0;
        for (uint32_t c = 0; c < DR_NCTRL; c++)
            s->ctrl[c] = drone_follow(s->ctrl[c], target[c]);
        int32_t topo = s->ctrl[DR_TOPO];
        /* Adjacent topologies crossfade their source signals, not oscillators.
         * All sources come from sample i-1, independent of evaluation order. */
        for (uint32_t k = 0; k < 4u; k++) {
            int32_t a = s->prev[pairs[k]];
            int32_t b = k ? s->prev[k - 1u] : 0;
            int32_t c = s->prev[ring[k]];
            int32_t src = topo <= 32768 ? a + mulq15(b - a, topo)
                                                  : b + mulq15(c - b, topo - 32768);
            /* Bounded PM: <= a quarter turn. It never alters the carrier's
             * accumulated phase or increment, so feedback cannot run away. */
            int32_t pm = mulq15(src, s->ctrl[DR_COUPLE]);
            uint32_t ph = s->phase[k] + (uint32_t)pm * 32768u;
            int32_t x = sine_i(ph);
            /* Odd, monotonic cubic soft character: 1.5*x - 0.5*x^3.
             * Blend with the sine; both endpoints remain bounded by Q15. */
            int32_t cube = mulq15(mulq15(x, x), x);
            next[k] = x + mulq15((x - cube) / 2, s->ctrl[DR_CHAR]);
            sum += next[k];
            /* Distinct deterministic triangle drifts; no shared RNG, no
             * discontinuous random steps and no accumulation of detuning. */
            uint32_t q = s->drift_phase[k] >> 16;
            int32_t tri = q < 32768u ? (int32_t)q * 2 - 32768
                                           : 98303 - (int32_t)q * 2;
            int32_t drift = mulq15(tri, s->ctrl[DR_DRIFT]);
            s->inc[k] = drone_follow(s->inc[k], freq[k]);
            int32_t step = s->inc[k] + ((s->inc[k] >> 16) * drift) / 128;
            s->phase[k] += (uint32_t)clamp(step, 0, 0x60000000);
            uint32_t rate = 1461u + (uint32_t)s->ctrl[DR_RATE] * 11u / 20u;
            s->drift_phase[k] += rate * (8u + k) / 8u;
        }
        for (uint32_t k = 0; k < 4u; k++) s->prev[k] = next[k];
        /* Coupled PM can have a nonzero mean even with odd generator waves. */
        int32_t x = sum / 4;
        int32_t hp = x - s->dc_in + s->dc_out - s->dc_out / 512;
        s->dc_in = x;
        s->dc_out = hp;
        int32_t g = 128 + s->ctrl[DR_TONE] * 30000 / 32512;
        s->low += (int32_t)(((int64_t)(hp - s->low) * g) / 32768);
        /* Match the HADES/SEM voice_amp and output scaling conventions. */
        out[i] += voice_amp(soft_knee(s->low, 16000) * 2, m, i) * 2;
    }
}
/* Gesture hooks are deliberately separate from engine.note_on: LEGATO may
 * skip that callback, and mono fallback must not synthesize another gesture. */
static int drone_cells(const track_t *t)
{
    return clamp(t->p[P_E3], 0, 5) >= 3;
}
/* Automated sources stay in the stored TOPO family. Explicit parameter edits
 * and locks still use ENG_DRONE.edit's full 0..5 descriptor. */
static const param_desc_t DRONE_TOPO_FAMILY[] = {
    {"TOPO", F_ENUM, 0, 2, 0, N_DRONE_TOPO, 0},
    {"TOPO", F_ENUM, 3, 5, 3, N_DRONE_TOPO, 0},
};
/* mod_begin's loops are budgeted (tests/target_budget.txt): it hoists the
 * engine test and passes `topo` = "this id is DRONE's TOPO". The scaling pass
 * needs only the family span (2 in both), the final clamp the family bounds. */
static inline const param_desc_t *drone_topo_span(uint32_t topo, const param_desc_t *full)
{
    return topo ? &DRONE_TOPO_FAMILY[0] : full;
}
static inline const param_desc_t *drone_topo_desc(uint32_t topo, int32_t base, const param_desc_t *full)
{
    return topo ? &DRONE_TOPO_FAMILY[clamp(base, 0, 5) >= 3] : full;
}
static inline const param_desc_t *drone_automated_desc(uint32_t engine, uint32_t id, int32_t base,
                                                      const param_desc_t *full)
{
    return drone_topo_desc(engine == ENGI_DRONE && id == P_E3, base, full);
}
static int drone_held(const drone_part_t *s, uint32_t note)
{
    return (s->held[note >> 5] >> (note & 31u)) & 1u;
}
static int drone_any_held(const drone_part_t *s)
{
    return (s->held[0] | s->held[1] | s->held[2] | s->held[3]) != 0;
}
static void drone_gesture_clear(track_t *t)
{
    if (eng_mem_owner[(uint32_t)(t - trk)] != ENGI_DRONE + 1u) return;
    drone_part_t *s = drone_state(t);
    for (uint32_t k = 0; k < 4u; k++) s->held[k] = 0;
}
static void drone_cell_seed(track_t *t, uint32_t note, uint32_t vel)
{
    static const uint32_t interval[4] = {0, 16384, 32768, 65536};
    static const int32_t weight[4] = {32767, 28672, 24576, 20480};
    drone_part_t *s = drone_state(t);
    for (uint32_t k = 0; k < 4u; k++) {
        s->owner[k] = note;
        s->pitch16[k] = (int32_t)note * 16;
        s->ratio[k] = 65536u + interval[k] * (uint32_t)clamp(t->p[P_E0], 0, 127) / 127u;
        s->level[k] = mulq15((int32_t)vel * 258, weight[k]);
        s->drive[k] = s->level[k] * 256;
    }
    /* A fresh phrase starts its drift triangles elsewhere (the free-running oscillator
     * phases depend on elapsed time), so repeated phrases are not identical. */
    for (uint32_t k = 0; k < 4u; k++) s->drift_phase[k] += s->phase[(k + 1u) & 3u];
    s->rotate = 1u;
    s->cells_ready = 1u;
    if (s->ready == 1u) {
        /* A fresh cell sound attacks from silence, not from legacy unity. */
        s->cell_mix = 32767;
        s->ctrl[DR_TOPO] = (clamp(t->p[P_E3], 3, 5) - 3) * 32768;
    }
}
/* Events can cross families and release keys before any render. The held
 * mask follows actual note events in BOTH families; only cell mode uses it
 * as host-gate authority. Never reset DSP/entry gain for an input-only exit. */
static void drone_mode_sync(track_t *t)
{
    drone_part_t *s = drone_state(t);
    if (!drone_cells(t)) {
        s->cells_ready = 0;
        return;
    }
    if (s->cells_ready || !t->v[0].active || !t->v[0].gate || !drone_any_held(s)) return;
    /* The event mask is authoritative in both families. A gated POLY host
     * voice can name a released key while another cell is held: never import
     * that stale note from voice state or the truncated mono stack. */
    uint32_t note = t->v[0].note & 127u;
    if (!drone_held(s, note))
        for (note = 0; note < 128u && !drone_held(s, note); note++) ;
    drone_note_on(t, &t->v[0]);
    drone_cell_seed(t, note, t->v[0].vel > 127u ? 127u : t->v[0].vel);
}
static void drone_gesture_on(track_t *t, uint32_t note, uint32_t vel)
{
    drone_part_t *s = drone_state(t);
    drone_mode_sync(t);
    note &= 127u;
    if (!drone_cells(t)) {
        s->held[note >> 5] |= 1u << (note & 31u);
        return;
    }
    vel = vel > 127u ? 127u : vel;
    drone_note_on(t, &t->v[0]);
    if (!drone_any_held(s) || !s->cells_ready) {
        drone_gesture_clear(t);
        drone_cell_seed(t, note, vel);
    } else {
        uint32_t k;
        /* All initial cells belong to the root; root repeats choose cell 0.
         * A replaced owner remains in held[], but can no longer release the
         * replacement. Repeating it acquires the next rotating cell. */
        for (k = 0; k < 4u && s->owner[k] != note; k++) ;
        if (k == 4u) {
            k = s->rotate;
            s->rotate = k == 3u ? 1u : k + 1u;
            s->owner[k] = note;
            s->pitch16[k] = (int32_t)note * 16;
            s->ratio[k] = 65536u;
        }
        s->level[k] = (int32_t)vel * 258;
        s->drive[k] = s->level[k] * 256;
    }
    /* Same-note on/on/off is one held key, matching the host's dedup policy.
     * MIDI note 0 is an ordinary key, never an empty-slot sentinel. */
    s->held[note >> 5] |= 1u << (note & 31u);
}
static void drone_gesture_off(track_t *t, uint32_t note)
{
    note &= 127u;
    drone_state(t)->held[note >> 5] &= ~(1u << (note & 31u));
}
/* d / (m << s) for m = 2..5 and |d| <= 2^23 (Q23 states): truncation toward
 * zero, as C's '/'. First divide by 2^s (shift), then by m via a 16-bit
 * reciprocal; exact for |q| <= 16384 (tests check the whole range used). */
static inline int32_t drone_cell_follow(int32_t a, int32_t b, uint32_t s, uint32_t m)
{
    static const uint16_t recip[6] = {0, 0, 32768, 21846, 16384, 13108};
    int32_t d = b - a, q = drone_div2s(d, s);
    int32_t step = q < 0 ? -(int32_t)(((uint32_t)-q * recip[m]) >> 16)
                         : (int32_t)(((uint32_t)q * recip[m]) >> 16);
    return a + (step ? step : (d > 0) - (d < 0));
}
static inline int32_t drone_route(const int32_t *x, uint32_t k, int32_t topo)
{
    static const uint8_t pairs[4] = {1, 0, 3, 2};
    static const uint8_t ring[4] = {3, 0, 1, 2};
    int32_t a = x[pairs[k]], b = k ? x[k - 1u] : 0, c = x[ring[k]];
    return topo <= 32768 ? a + mulq15(b - a, topo)
                        : b + mulq15(c - b, topo - 32768);
}
/* Interval roughness of the cells (ROUGH). Cents of 1 + i/16 for i = 0..16 (log2, rounded): the position of a
 * frequency inside its octave, read with a linear interpolation (error < 1 cent). A cell's interval to the anchor
 * cell 0 is the difference of two such positions, folded into one octave: no division, no float. */
static inline int32_t drone_oct_cents(uint32_t f)
{
    static const uint16_t L[17] = {0, 105, 204, 298, 386, 471, 551, 628, 702, 773, 841, 906, 969, 1030, 1088, 1145, 1200};
    uint32_t m = f << __builtin_clz(f | 1u);                 /* leading one at bit 31 */
    uint32_t i = (m >> 27) & 15u, fr = (m >> 15) & 0xFFFu;
    return L[i] + (int32_t)(((uint32_t)(L[i + 1] - L[i]) * fr) >> 12);
}
/* 0 = pure (unison, fifth) .. 1 = rough (minor second, major seventh), Q15, at 100 cent steps over one octave. */
static int32_t drone_rough_of(int32_t cents)
{
    static const int16_t R[13] = {0, 32767, 19661, 9830, 8192, 6554, 26214, 3277, 9830, 9830, 19661, 32767, 0};
    int32_t i = (cents * 5243) >> 19, fr = cents - i * 100;
    return R[i] + (((R[i + 1] - R[i]) * fr * 41) >> 12);
}
static const uint16_t BREATH_INC[4] = {16556u, 22400u, 30191u, 41878u};   /* 0.17 .. 0.43 Hz at 44.1 kHz */
/* Slow tick, every 8th sample: ROUGH (smoothed, with the time constant of 1024 samples as drone_follow), the
 * per-cell COUPLE and CHAR factors that follow from it, and BREATH (each cell's loudness moves on its own slow
 * triangle, 0.17 / 0.23 / 0.31 / 0.43 Hz, up to 25 % deep at DRIFT 127; DRIFT 0 takes nothing). A call of its
 * own: the sample loop stays small, and the cost it is budgeted at (tests/target_budget.py) is the per-sample one. */
static __attribute__((noinline)) void drone_slow_tick(drone_part_t *s, const int32_t *rough_goal, int32_t drift)
{
    int32_t bdepth = mulq15(8192, drift);
    for (uint32_t k = 0; k < 4u; k++) {
        s->breath[k] += 8u * BREATH_INC[k];
        int32_t qb = (int32_t)(s->breath[k] >> 16);          /* triangle 0..32767 */
        s->bred[k] = mulq15(bdepth, qb < 32768 ? qb : 65535 - qb);
        if (!k) continue;
        s->rough[k] = drone_follow_s(s->rough[k], rough_goal[k], 7);
        s->fcpl[k] = 29491 + mulq15(22938, s->rough[k]);         /* COUPLE x0.9 .. x1.6 */
        int32_t c = (s->ctrl[DR_CHAR] * (26214 + mulq15(19661, s->rough[k]))) >> 15;   /* CHAR x0.8 .. x1.4 */
        s->chrk[k] = c > 32512 ? 32512 : c;
    }
}
static __attribute__((noinline)) void drone_cell_render(track_t *t, voice_t *v, int32_t *out,
                                                       uint32_t n, const vmod_t *m)
{
    drone_part_t *s = drone_state(t);
    int32_t target[DR_NCTRL], freq[4];
    static const int32_t detune[4] = {-3, -1, 1, 3};
    drone_controls(t, m, target);
    /* Never let appended enum values enter the legacy route interpolator. */
    target[DR_TOPO] = (clamp(t->p[P_E3], 0, 5) % 3) * 32768;
    int64_t offset = (int64_t)m->pitch16 - v->pitch_cur;
    for (uint32_t k = 0; k < 4u; k++) {
        uint64_t hz;
        int32_t fine = clamp(t->p[P_E1], 0, 127) * detune[k] * 252 / 127;
        /* Ignore common host glide/transposition from later gestures;
         * retain TUNE, bend and fixed/matrix pitch offsets, including fine.
         * Local retunes use the existing ~23 ms increment smoother. */
        int64_t pitch = s->pitch16[k] + offset;
        uint32_t inc = pitch_inc(pitch < 0 ? 0 : pitch > 2047 ? 2047 : (int32_t)pitch);
        int64_t tuned = (int64_t)inc + (int64_t)(inc >> 12) * clamp(m->fine, -4095, 4095);
        hz = ((uint64_t)(tuned < 0 ? 0 : tuned) * s->ratio[k]) >> 16;
        hz = (hz * (uint32_t)(65536 + fine)) >> 16;
        freq[k] = (int32_t)(hz > 0x60000000u ? 0x60000000u : hz);
        if (s->ready == 1u) s->inc[k] = freq[k];
    }
    s->ready = 2u;
    /* ROUGH: cell k's interval to the anchor cell 0, folded into one octave, sets how rough it sounds. */
    int32_t rough_goal[4] = {0, 0, 0, 0};
    {
        int32_t c0 = drone_oct_cents((uint32_t)freq[0]);
        for (uint32_t k = 1; k < 4u; k++) {
            int32_t c = drone_oct_cents((uint32_t)freq[k]) - c0;
            rough_goal[k] = drone_rough_of(c < 0 ? c + 1200 : c);
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        int32_t next[4], pmsrc[4], activity[4], sum = 0;
        for (uint32_t c = 0; c < DR_NCTRL; c++)
            s->ctrl[c] = drone_follow(s->ctrl[c], target[c]);
        s->cell_mix = drone_follow(s->cell_mix, 32767);
        /* ctrl[DR_RATE] is constant over the sample: one divide, not four. */
        uint32_t rate = 1461u + (uint32_t)s->ctrl[DR_RATE] * 11u / 20u;
        for (uint32_t k = 0; k < 4u; k++) activity[k] = drone_div2s(s->activity[k], 8);
        /* Life: DRIFT also lets each cell's PM depth (down to 0.4x COUPLE, never above)
         * and loudness (down to 0.3x, from its neighbor's phase) breathe on the existing
         * deterministic drift triangles. DRIFT 0 leaves both exactly constant. */
        int32_t tri[4], dmt = s->ctrl[DR_DRIFT], swing = mulq15(dmt, 22938), pmdep = mulq15(dmt, 19660);
        for (uint32_t k = 0; k < 4u; k++) {
            uint32_t q = s->drift_phase[k] >> 16;
            tri[k] = q < 32768u ? (int32_t)q * 2 - 32768 : 98303 - (int32_t)q * 2;
        }
        /* SHARP rides on CHAR: 0 is a pure sine, CHAR 89 reaches 0.65 (sine -> triangle at 0.5 -> a rounded
         * square at 1), capped at 0.8. sh is Q15; drive and level makeup are the same for all four cells. */
        int32_t sh = (int32_t)(((uint32_t)s->ctrl[DR_CHAR] * 60791u) >> 16);
        if (sh > 26214) sh = 26214;
        int32_t sdrive = sh > 16384 ? 32768 + 4 * (sh - 16384) : 0, scomp = 32768 - (sh >> 2);
        if (!(s->slow++ & 7u)) drone_slow_tick(s, rough_goal, dmt);
        for (uint32_t k = 0; k < 4u; k++) {
            int32_t hold = drone_held(s, s->owner[k]) ? s->level[k] * 192 : 0;
            s->drive[k] = drone_cell_follow(s->drive[k], hold, 15, 2 + k);
            /* Snapshot activity -> at most half-scale inhibition. No physical
             * energy model: a bounded, contractive competition law, not P3. */
            int32_t neighbor = clamp(drone_route(activity, k, s->ctrl[DR_TOPO]), 0, 32767);
            int32_t inhibition = drone_div2s(mulq15(neighbor, s->ctrl[DR_COUPLE]), 1);
            int32_t goal = mulq15(drone_div2s(s->drive[k], 8), 32767 - inhibition) * 256;
            s->amp[k] = drone_follow(s->amp[k], goal);
            s->activity[k] = drone_cell_follow(s->activity[k], s->amp[k], 13, 2 + k);
            int32_t src = drone_route(s->prev, k, s->ctrl[DR_TOPO]);
            int32_t cple = s->ctrl[DR_COUPLE] -
                           mulq15(s->ctrl[DR_COUPLE], mulq15(pmdep, 32767 - ((tri[k] + 32768) >> 1)));
            int32_t chr = s->ctrl[DR_CHAR];
            if (k) {
                /* ROUGH: a cell off a pure interval to the anchor leans harder on its neighbours and is brighter. */
                cple = (cple * s->fcpl[k]) >> 15;
                if (cple > 32512) cple = 32512;
                chr = s->chrk[k];
            }
            int32_t pm = mulq15(src, cple);
            uint32_t ph = s->phase[k] + (uint32_t)pm * 32768u;
            int32_t x = sine_i(ph);
            if (sh) {
                /* SHARP: triangle from the same (phase-modulated) angle, then a soft clip with drive 1..3. */
                uint32_t qs = ((ph >> 16) + 16384u) & 0xFFFFu;
                int32_t tw = qs < 32768u ? (int32_t)qs * 2 - 32768 : 98303 - (int32_t)qs * 2;
                if (!sdrive) {
                    x += mulq15(tw - x, sh * 2);
                } else {
                    int32_t y = (tw >> 1) * (sdrive >> 1) >> 13;
                    y = y > 32767 ? 32767 : y < -32767 ? -32767 : y;
                    x = y + drone_div2s(y - mulq15(mulq15(y, y), y), 1);   /* 1.5y - 0.5y^3: |x| <= 32768 */
                }
                x = mulq15(x, scomp);
            }
            int32_t cube = mulq15(mulq15(x, x), x);
            x += mulq15(drone_div2s(x - cube, 1), chr);
            int32_t gain = 32767 - s->cell_mix + mulq15(drone_div2s(s->amp[k], 8), s->cell_mix);
            gain -= mulq15(gain, mulq15(swing, (tri[(k + 1u) & 3u] + 32768) >> 1)); /* exactly neutral at DRIFT 0 */
            gain -= mulq15(gain, s->bred[k]);                                   /* BREATH: exactly neutral at DRIFT 0 */
            next[k] = mulq15(x, gain);
            /* PM source: the same amplitude weight with 2x makeup, saturating at
             * unity. A cell at about half level modulates its neighbors as a
             * legacy generator does; a silent cell still modulates nothing. */
            pmsrc[k] = mulq15(x, gain > 8191 ? 32767 : gain * 4);
            /* PMLP: one pole (1/2 per sample) in the feedback path takes the loop gain away near fs/4,
             * where the 1-sample delayed cell loop would whistle. */
            s->pmlp[k] += (pmsrc[k] - s->pmlp[k]) >> 1;
            pmsrc[k] = s->pmlp[k];
            sum += next[k];
            int32_t drift = mulq15(tri[k], s->ctrl[DR_DRIFT]);
            s->inc[k] = drone_follow(s->inc[k], freq[k]);
            int32_t step = s->inc[k] + drone_div2s((s->inc[k] >> 16) * drift, 7);
            s->phase[k] += (uint32_t)clamp(step, 0, 0x60000000);
            s->drift_phase[k] += rate * (8u + k) >> 3;
        }
        /* Audible and PM signals both follow the cell amplitude; PM has makeup. */
        for (uint32_t k = 0; k < 4u; k++) s->prev[k] = pmsrc[k];
        int32_t x = drone_div2s(sum, 2);
        int32_t hp = x - s->dc_in + s->dc_out - drone_div2s(s->dc_out, 9);
        s->dc_in = x;
        s->dc_out = hp;
        int32_t g = 128 + s->ctrl[DR_TONE] * 30000 / 32512;
        int64_t lp = (int64_t)(hp - s->low) * g; /* shift form of a 64-bit divide */
        s->low += (int32_t)((lp + ((lp >> 63) & 32767)) >> 15);
        s->cell_last = soft_knee(s->low, 16000) * 2; /* pre-host-amp declick endpoint */
        out[i] += voice_amp(s->cell_last, m, i) * 2;
    }
}
static void drone_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (!n) return;
    drone_part_t *s = drone_state(t);
    if (!s->ready) drone_note_on(t, v);
    drone_mode_sync(t);
    if (drone_cells(t)) {
        if (!s->cells_ready) {
            drone_cell_seed(t, v->note & 127u, v->vel > 127u ? 127u : v->vel);
            /* Release-only/direct DSP rendering must not invent held keys. */
        }
        drone_cell_render(t, v, out, n, m);
        s->cell_exit = 128u;
    } else {
        /* Values 0..2 always execute the exact legacy DSP arithmetic/state.
         * After an opt-in cell session only, bridge its last output to legacy
         * over 128 samples (~2.9 ms). No second render or legacy-state reset. */
        s->cell_mix = 0;
        if (!m->amp0 && !m->amp1) s->cell_exit = 0; /* host AMP-zero remains exact silence */
        if (s->cell_exit) {
            int32_t bridge[CTL] = {0};
            drone_legacy_render(t, v, bridge, n, m);
            for (uint32_t i = 0; i < n; i++) {
                uint32_t left = s->cell_exit;
                int32_t from = voice_amp(s->cell_last, m, i) * 2;
                out[i] += (int32_t)(((int64_t)from * left +
                                     (int64_t)bridge[i] * (128u - left)) / 128);
                if (left) s->cell_exit--;
            }
        } else {
            drone_legacy_render(t, v, out, n, m);
        }
    }
}
static const preset_t DRONE_PRESETS[] = {
    {"STILL AIR", {0, 24, 12, 0, 18, 32, 20, 76}, {82, 90, 112, 96}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"OPEN FIFTH", {127, 18, 28, 0, 28, 24, 28, 88}, {76, 88, 116, 94}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"SLOW CHAIN", {72, 38, 54, 1, 48, 64, 12, 62}, {90, 96, 120, 104}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"DARK RING", {108, 28, 78, 2, 74, 48, 36, 42}, {72, 92, 110, 100}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"BEAT FIELD", {8, 90, 42, 0, 40, 90, 58, 80}, {84, 94, 118, 108}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"CELL AIR", {127, 127, 63, 3, 2, 80, 0, 75}, {96, 90, 118, 110}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"CELL CHN", {68, 61, 0, 4, 0, 19, 3, 35}, {84, 90, 112, 100}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"CELL RNG", {31, 20, 107, 5, 20, 33, 59, 31}, {70, 90, 108, 92}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"GLASS CHOIR", {0, 72, 91, 3, 89, 53, 73, 127}, {100, 90, 118, 112}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"TIDE", {78, 87, 24, 5, 0, 23, 127, 59}, {105, 90, 120, 115}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"IRON BELLS", {113, 17, 127, 5, 47, 14, 60, 103}, {40, 100, 100, 110}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"SUB FOG", {0, 36, 8, 4, 70, 127, 72, 15}, {95, 90, 125, 100}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"SWARM", {30, 60, 42, 3, 88, 123, 48, 63}, {80, 90, 112, 90}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"STATIC", {15, 80, 127, 3, 90, 13, 84, 120}, {75, 90, 112, 95}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"RUST", {75, 44, 70, 5, 102, 45, 67, 127}, {60, 90, 110, 100}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
    {"MOON PULSE", {0, 2, 89, 5, 0, 19, 113, 26}, {85, 90, 115, 105}, 0, 1, FX(0, 0, 0, 0), PAT(5)},
};
static const engine_t ENG_DRONE = {
    .name = "DRONE", .page_title = {"NETWORK", "DRIFT TONE"},
    .edit = {
        {"SPREAD", F_PCT, 0, 127, 64, 0, 0},
        {"DETUNE", F_PCT, 0, 127, 30, 0, 0},
        {"COUPLE", F_PCT, 0, 127, 40, 0, 0},
        {"TOPO", F_ENUM, 0, 5, 3, N_DRONE_TOPO, 0},
        {"CHAR", F_PCT, 0, 127, 30, 0, 0},
        {"DRIFT", F_PCT, 0, 127, 60, 0, 0},
        {"RATE", F_PCT, 0, 127, 40, 0, 0},
        {"TONE", F_PCT, 0, 127, 76, 0, 0},
    },
    .presets = DRONE_PRESETS, .npresets = NELEM(DRONE_PRESETS),
    .knob = {P_E2, P_E4, P_E5, P_E7},
    .note_on = drone_note_on, .render = drone_render,
    .poly = 1, .mem = sizeof(drone_part_t),
    .keep = 0, /* no voice_t phase/state slots; pool state survives retriggers */
};
