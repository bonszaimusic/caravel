/* SPDX-License-Identifier: GPL-3.0-only
 * Scope V6 UI policy. No audio, LCD or source buffer dependencies.
 * The same audio scope belongs on all five SPACE groups when STASIS
 * is selected. MIDI CAPTURE note rendering remains a separate widget.
 */
#ifndef CARAVEL_STASIS_SCOPE_POLICY_H
#define CARAVEL_STASIS_SCOPE_POLICY_H
#include <stdint.h>
enum { STASIS_SCOPE_SPACE_GROUPS = 5u, STASIS_SCOPE_RTYPE = 3u };
static inline uint8_t stasis_scope_show(uint8_t play, uint8_t space,
                                        uint32_t group, uint32_t reverb_type)
{
    return (uint8_t)(play && space &&
           group < STASIS_SCOPE_SPACE_GROUPS &&
           reverb_type == STASIS_SCOPE_RTYPE);
}
/* 0 = coolest/darkest, 4 = warmest/brightest.
 * Only temperature is encoded here; the renderer must retain equivalent
 * shape/brightness cues in LINE and MONO and show FREEZE by geometry/text.
 */
static inline uint8_t stasis_scope_tone_tier(uint32_t tone)
{
    if (tone > 127u) tone = 127u;
    return (uint8_t)((tone * 5u) >> 7u);
}
#endif
