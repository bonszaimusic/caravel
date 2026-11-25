/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS2 FORMAT-2 source-region settings in genuinely unused HARMONY bits.
 * Requires native FUNW marker 0xA6, never reinterprets old 0x00/0xA5.
 * The low nibbles of HARMONY roots and low two bits of durations stay intact.
 * The old FX extension owns 21/24 duration high bits. The remaining three
 * bits and the four root high nibbles are exactly 19 region setting bits.
 */
#ifndef CARAVEL_STASIS_REGION_WIRE_H
#define CARAVEL_STASIS_REGION_WIRE_H
#include <stdint.h>
typedef struct {
    uint8_t pos;     /* 0..127, old runtime SCAN position */
    uint8_t length;  /* 0..127 source REGION span */
    uint8_t grain;   /* 0=93 ms, 1=186 ms, 2=372 ms */
    uint8_t quality; /* 0=HQ, 1=LONG, 2=DRONE */
    uint8_t reverse; /* 0=forward, 1=reverse */
} stasis_region_saved_t;
static inline stasis_region_saved_t streg_wire_default(void)
{ return (stasis_region_saved_t){0u,64u,3u,0u,0u}; }   /* GRAIN 3 = 186 ms = ST_GRAIN_DEFAULT (core.h): an untouched project stays in the legacy format */
static inline int streg_wire_valid(stasis_region_saved_t s)
{ return s.pos<=127u && s.length<=127u && s.grain<=3u && s.quality<=2u && s.reverse<=1u; }
static inline uint32_t streg_wire_bits(stasis_region_saved_t s)
{
    return (uint32_t)s.pos | ((uint32_t)s.length << 7u) |
           ((uint32_t)s.grain << 14u) | ((uint32_t)s.quality << 16u) |
           ((uint32_t)s.reverse << 18u);
}
static inline int streg_wire_nondefault(stasis_region_saved_t s)
{ return streg_wire_bits(s)!=streg_wire_bits(streg_wire_default()); }
/* hb is the 24-byte HARMONY region after the normal FX-hi packing. */
static inline void streg_wire_pack(uint8_t hb[24], stasis_region_saved_t s)
{
    uint32_t v=streg_wire_bits(s);
    for(uint32_t i=0;i<4u;i++)
        hb[i]=(uint8_t)((hb[i]&15u)|((v>>(4u*i)&15u)<<4u));
    hb[7]=(uint8_t)((hb[7]&31u)|((v>>16u&7u)<<5u));
}
/* On marker 0xA6, remove the bits before standard HARMONY/FX decoding. */
static inline stasis_region_saved_t streg_wire_unpack(uint8_t hb[24])
{
    uint32_t v=0u;
    for(uint32_t i=0;i<4u;i++) {
        v|=(uint32_t)(hb[i]>>4u)<<(4u*i);
        hb[i]&=15u;
    }
    v|=(uint32_t)(hb[7]>>5u)<<16u;
    hb[7]&=31u;
    return (stasis_region_saved_t){
        (uint8_t)(v&127u),(uint8_t)(v>>7u&127u),
        (uint8_t)(v>>14u&3u),(uint8_t)(v>>16u&3u),
        (uint8_t)(v>>18u&1u)
    };
}
#endif
