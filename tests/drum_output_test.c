/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* Exercise the production DRUM and DRUM-X output stages with UBSan shifts enabled.
 * Keep the renderer bipolar, bounded, additive and silent at zero gain. */
#define main hostsim_main
#include "hostsim.c"
#undef main

int main(int argc, char **argv)
{
    static const uint8_t kits[] = {DK_STD, DK_80, DK_10, DK_66, DK_55, DK_77, DK_DEEP, DK_TIGHT, DK_PERC, DK_DX};
    static const uint8_t velocities[] = {1, 96, 127}, levels[] = {0, 64, 127};
    static const int32_t ramps[][2] = {{0, 0}, {32767, 32767}, {0, 32767}, {32767, 0}};
    uint32_t k, lane, vel, level, drive, ramp, block, i, cases = 0, bad = 0;
    uint32_t positive[2] = {0, 0}, negative[2] = {0, 0};
    uint64_t hash = 1469598103934665603ull;
    int dx_only = argc > 1 && !strcmp(argv[1], "dx");
    for (k = 0; k < NELEM(kits); k++) {
        uint32_t dx = kits[k] == DK_DX;
        if (dx_only && !dx) continue;
        for (lane = 0; lane < NLANE; lane++)
            for (vel = 0; vel < NELEM(velocities); vel++)
                for (level = 0; level < NELEM(levels); level++)
                    for (drive = 0; drive < 2u; drive++)
                        for (ramp = 0; ramp < NELEM(ramps); ramp++) {
                            track_t *t = &trk[0];
                            voice_t *v = &t->v[0];
                            vmod_t m = {0};
                            host_tracks_init();
                            memset(t->v, 0, sizeof t->v);
                            memset(eng_mem, 0, sizeof eng_mem);
                            dx_reset(0);
                            host_preset(t, ENGI_DRUM, 0);
                            t->p[P_E0] = kits[k];
                            t->p[P_E7] = drive ? 127 : 0;
                            t->p[P_LN0 + lane] = levels[level];
                            drum_mute[0] = 0;
                            v->active = v->gate = 1;
                            v->note = DRUM_LANE_NOTE[lane];
                            v->vel = velocities[vel];
                            drum_note_on(t, v);
                            m.amp0 = ramps[ramp][0];
                            m.amp1 = ramps[ramp][1];
                            for (block = 0; block < 16u; block++) {
                                int32_t out[CTL + 1u];
                                for (i = 0; i < CTL; i++) out[i] = i & 1u ? -101 : 101;
                                out[CTL] = 0x12345678;
                                drum_render(t, v, out, CTL, &m);
                                bad += out[CTL] != 0x12345678;
                                for (i = 0; i < CTL; i++) {
                                    int32_t sample = out[i] - (i & 1u ? -101 : 101);
                                    bad += sample < -60000 || sample > 60000;
                                    if (!levels[level] || !ramp) bad += sample != 0;
                                    positive[dx] += sample > 0;
                                    negative[dx] += sample < 0;
                                    hash = (hash ^ (uint32_t)sample) * 1099511628211ull;
                                }
                            }
                            /* Muting drops the lane's output without disturbing the accumulator. */
                            {
                                int32_t out[CTL];
                                drum_mute[0] = (uint8_t)(1u << lane);
                                for (i = 0; i < CTL; i++) out[i] = 101;
                                drum_render(t, v, out, CTL, &m);
                                for (i = 0; i < CTL; i++) bad += out[i] != 101;
                            }
                            cases++;
                        }
    }
    for (i = dx_only ? 1u : 0u; i < 2u; i++)
        bad += !positive[i] || !negative[i];
    /* Captured from the pre-fix native renderer: defined arithmetic keeps every sample. */
    if (!dx_only) bad += hash != 0xe1b7fafa9f2193b1ull;
    printf("drum output: %u cases, positive %u/%u, negative %u/%u, hash %016llx, %s\n",
           cases, positive[0], positive[1], negative[0], negative[1],
           (unsigned long long)hash, bad ? "FAIL" : "ok");
    return bad != 0;
}
