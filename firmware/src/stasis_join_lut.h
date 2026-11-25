/* SPDX-License-Identifier: GPL-3.0-only
 * Optional HQ source-join LUT probe. Shares existing 257-entry grain
 * envelope, so it adds NO new 514-byte table and no source PCM buffer.
 * This is an A/B PERFORMANCE experiment, not a released DSP change.
 */
#ifndef CARAVEL_STASIS_SOURCE_JOIN_LUT_H
#define CARAVEL_STASIS_SOURCE_JOIN_LUT_H
#include <stdint.h>
#include "stasis_join.h"
_Static_assert(STASIS_SOURCE_JOIN_FRAMES == 512u, "shared grain LUT requires 512-step join");
static inline int32_t stasis_source_join_lut(int32_t held_pcm,
        int32_t live_pcm, uint32_t frames_left, const uint16_t *env257)
{
    uint32_t t, k, weight;
    if (!frames_left) return live_pcm;
    if (frames_left >= STASIS_SOURCE_JOIN_FRAMES) return held_pcm;
    t = STASIS_SOURCE_JOIN_FRAMES - frames_left;
    k = t >> 1u;
    weight = env257[k];
    if (t & 1u) weight = (weight + env257[k + 1u] + 1u) >> 1u;
    return (held_pcm * (int32_t)(32768u-weight) +
            live_pcm * (int32_t)weight) >> 15u;
}
#endif
