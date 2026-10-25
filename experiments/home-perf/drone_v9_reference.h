/* SPDX-License-Identifier: GPL-3.0-only; unchanged V9 renderer for oracle testing. */
static __attribute__((noinline)) void drone_cell_v9_reference(track_t *t, voice_t *v, int32_t *out,
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