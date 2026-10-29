/* SPDX-License-Identifier: GPL-3.0-only */
/* Frozen rc4 arithmetic for differential audio/state regression. */
static __attribute__((noinline)) void drone_rc4_legacy_reference(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
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
