/* SPDX-License-Identifier: GPL-3.0-only
 * HQ STASIS2 source-seam smoothing after a frozen capture resumes writing.
 * Standalone integer helper: no allocation, divisions or audio state changes.
 */
#ifndef CARAVEL_STASIS_SOURCE_JOIN_H
#define CARAVEL_STASIS_SOURCE_JOIN_H
#include <stdint.h>
#define STASIS_SOURCE_JOIN_FRAMES 512u
static inline int32_t stasis_source_join(int32_t held_pcm, int32_t live_pcm,
                                        uint32_t frames_left)
{
    /* 512 steps, 0..32768 Q15. Smoothstep has zero endpoint slope.
     * The source ring's sample just before the seam is held_pcm. */
    uint32_t x, x2, x3, weight;
    if (!frames_left) return live_pcm;
    if (frames_left >= STASIS_SOURCE_JOIN_FRAMES) return held_pcm;
    x = (STASIS_SOURCE_JOIN_FRAMES - frames_left) << 6u;
    x2 = (x * x) >> 15u;
    x3 = (x2 * x) >> 15u;
    weight = 3u * x2 - 2u * x3;
    return (held_pcm * (int32_t)(32768u-weight) +
            live_pcm * (int32_t)weight) >> 15u;
}
#endif
