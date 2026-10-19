/* SPDX-License-Identifier: GPL-3.0-only */
/* Device preference: four 1..5 voice caps, sum <= 8, and AUTO / FIXED.
 * A zero record is AUTO with 2/2/2/2 prepared. One aligned word publishes
 * the complete preference to the audio ISR; only that ISR changes voices. */
#ifndef FELUCCA_VOICE_POLICY_H
#define FELUCCA_VOICE_POLICY_H
#include <stdint.h>
#define VOICE_POLICY_FIXED (1u << 16)
#define VOICE_POLICY_DEFAULT 0x2222u
static volatile uint32_t voice_policy;
static uint32_t voice_policy_cap(uint32_t policy, uint32_t part)
{
    return ((policy & 0xFFFFu) ? policy : VOICE_POLICY_DEFAULT) >> (part * 4u) & 15u;
}
static uint32_t voice_policy_normalize(uint32_t policy)
{
    uint32_t sum = 0;
    if (!policy) return 0;
    if (policy & ~0x1FFFFu) return 0;
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t n = policy >> (i * 4u) & 15u;
        if (n < 1u || n > 5u) return 0;
        sum += n;
    }
    return sum <= 8u ? policy : 0u;
}
#endif
