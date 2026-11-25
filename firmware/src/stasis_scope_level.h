/* SPDX-License-Identifier: GPL-3.0-only
 * Scope V6/V7 source peak scale, shared by the audio task and the host tests.
 * 8 bit, companded: fine steps for quiet material (the REV send bus is rarely loud),
 * coarser above 4096. No division, one compare.
 *   |pcm| <  4096: |pcm| / 32            -> 0..127   (step 32, i.e. 1 LSB = -60 dBFS)
 *   |pcm| >= 4096: 128 + (|pcm|-4096)/256 -> 128..240 (full scale 32768 -> 240)
 */
#ifndef CARAVEL_STASIS_SCOPE_LEVEL_H
#define CARAVEL_STASIS_SCOPE_LEVEL_H
#include <stdint.h>
static inline uint32_t stasis_scope_amp(int32_t pcm)
{
    uint32_t a = (uint32_t)(pcm < 0 ? -pcm : pcm), v;
    v = a < 4096u ? a >> 5u : 128u + ((a - 4096u) >> 8u);
    return v > 255u ? 255u : v;
}
#endif
