/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* Independent wide-integer references for signed feedback and 64-bit host sample objects. */
#include <stdint.h>
static uint32_t feedback_slots[3][0x14000u / 4u];
#define SMP_USER_XIP(k) ((const uint8_t *)feedback_slots[(k)])
#define main hostsim_main
#include "hostsim.c"
#undef main
_Static_assert(sizeof feedback_slots[0] == SMP_USER_SIZE, "sample slot fixture size");

static int64_t floor_div(int64_t x, int64_t by)
{
    return x >= 0 ? x / by : -((-x + by - 1) / by);
}

int main(void)
{
    int32_t dc = 0, err = 0, lc[2] = {0, 0}, le[2] = {0, 0};
    int64_t rd = 0, re = 0, rl[2] = {0, 0}, rle[2] = {0, 0};
    uint32_t rng = 0x12345678u, i, j, bad = 0;
    for (i = 0; i < 262144u; i++) {
        int32_t x, actual;
        int64_t e, d, expected;
        rng = rng * 1664525u + 1013904223u;
        x = i < 32768u ? -1 : i < 65536u ? -32767 : i < 98304u ? 32767 :
            i < 131072u ? (i & 1u ? -1000000 : 1000000) :
            i < 196608u ? (int32_t)(rng % 2000001u) - 1000000 : 0;
        e = (int64_t)x * 64 - rd + re;
        d = floor_div(e, 4096);
        re = e - d * 4096;
        rd += d;
        expected = x - floor_div(rd + 32, 64);
        actual = dc_block(x, &dc, &err);
        bad += actual != expected || dc != rd || err != re || err < 0 || err >= 4096;
        for (j = 0; j < 2u; j++) {
            uint32_t sh = 5u + j;
            e = (int64_t)x - rl[j] + rle[j];
            d = floor_div(e, 1u << sh);
            rle[j] = e - d * (1u << sh);
            rl[j] += d;
            expected = x - rl[j];
            actual = lowcut1(x, &lc[j], &le[j], sh);
            bad += actual != expected || lc[j] != rl[j] || le[j] != rle[j] || le[j] < 0 || le[j] >= (1 << sh);
        }
    }
    bad += dc != 0 || lc[0] != 0 || lc[1] != 0;
    bad += smp_data_byte(0) != SMP_DATA[0] || smp_data_byte(sizeof SMP_DATA - 1u) != SMP_DATA[sizeof SMP_DATA - 1u];
    for (j = 0; j < SMP_USER_SLOTS; j++) {
        uint8_t *p = (uint8_t *)feedback_slots[j] + SMP_USER_DATA;
        uint32_t base = (uint32_t)(uintptr_t)p - (uint32_t)(uintptr_t)SMP_DATA;
        p[0] = (uint8_t)(0x31u + j);
        p[SMP_USER_SIZE - SMP_USER_DATA - 1u] = (uint8_t)(0x91u + j);
        bad += smp_data_byte(base) != p[0];
        bad += smp_data_byte(base + SMP_USER_SIZE - SMP_USER_DATA - 1u) != p[SMP_USER_SIZE - SMP_USER_DATA - 1u];
    }
    printf("feedback: 262144 bipolar/quiet/silence samples, DC and both lowcut references, built-in and 3 user slots: %s\n",
           bad ? "FAIL" : "ok");
    return bad != 0;
}
