/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS2 Scope V6 - UI-only refresh gate. No target DSP dependency.
 * 167 ms yields at most 6 audio-driven refreshes in any half-open second.
 * The caller records last_blit_ms ONLY after a completed, non-overlapping
 * band update. A skipped busy-frame must not accumulate pending repaints.
 */
#ifndef CARAVEL_STASIS_SCOPE_RATE_H
#define CARAVEL_STASIS_SCOPE_RATE_H
#include <stdint.h>
#define STASIS_SCOPE_MIN_BLIT_MS 167u
static inline uint8_t stasis_scope_refresh_due(uint32_t now_ms,
                                               uint32_t last_blit_ms,
                                               uint8_t has_last_blit)
{
    return (uint8_t)(!has_last_blit ||
           (uint32_t)(now_ms - last_blit_ms) >= STASIS_SCOPE_MIN_BLIT_MS);
}
#endif
