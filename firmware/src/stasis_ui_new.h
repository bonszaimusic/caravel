/* SPDX-License-Identifier: GPL-3.0-only
 * Four discrete UI steps for the 800 ms NEW SOURCE hold gesture.
 * Pure, allocation-free and safe when the millisecond counter wraps.
 */
#ifndef CARAVEL_STASIS_UI_NEW_H
#define CARAVEL_STASIS_UI_NEW_H
#include <stdint.h>
static inline uint8_t stasis_new_progress_step(uint32_t start_ms,
                                               uint32_t now_ms, uint8_t held)
{
    uint32_t elapsed;
    if (!held) return 0u;
    elapsed = (uint32_t)(now_ms - start_ms);
    return (uint8_t)(elapsed >= 800u ? 4u : elapsed / 200u);
}
#endif
