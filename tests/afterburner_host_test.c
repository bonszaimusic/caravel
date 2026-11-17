/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 *
 * Standalone native test of the ACTUAL firmware AFTERBURNER source:
 *   cc -std=c11 -O2 -Wall -Wextra -Werror tests/afterburner_host_test.c -lm -o /tmp/afterburner_test
 *   /tmp/afterburner_test
 *
 * This is not a JieLi FM-1 / WASM / dual-core or USB-audio test.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
/* The target's ELF pool section has no Mach-O equivalent in this host test. */
#define __attribute__(x)
#endif
#define FS 44100
#define CTL 128
#define CARAVEL_AUDIO_HOT
#define CARAVEL_AFTERBURNER 1
static int32_t clamp(int32_t x, int32_t lo, int32_t hi)
{ return x < lo ? lo : x > hi ? hi : x; }
static struct { int16_t on, time, amt, damp; } afterburner = {0, 30, 32, 70};
#include "../firmware/src/afterburner.c"

static uint32_t rnd = 0x4E3F70D1u;
static int32_t audio_noise(void)
{
    rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;
    return (int32_t)(rnd & 65535u) - 32768;
}
static double tail_rms(void)
{
    double p = 0;
    for (uint32_t i = 0; i < AFB_N; i++) {
        double s = (double)afb_line[i] / AFB_Q;
        p += s * s;
    }
    return sqrt(p / AFB_N);
}
int main(void)
{
    int32_t wet[CTL], side[CTL], pre[CTL];
    double at5 = 0, at30 = 0, at60 = 0;
    assert(sizeof afb_line == AFB_N * sizeof(int32_t));
    assert(AFB_N <= 3000u); /* bounded deterministic pool usage */
    for (unsigned b = 0; b < 12; b++) {
        for (uint32_t i = 0; i < CTL; i++) wet[i] = 30000, side[i] = pre[i] = 0;
        afb_process(wet, side, pre, CTL, 0u, 1u);
        assert(!afb.active);
        for (uint32_t i = 0; i < CTL; i++) assert(wet[i] == 30000 && side[i] == 0);
    }
    afterburner.on = 1;
    afterburner.time = 60;
    afterburner.amt = 127;  /* ensure a measurable full-tail monitor */
    afterburner.damp = 70;
    /* Two seconds of noise then silence until 65 seconds; TYPE change at
     * 10s simulates HALL -> another type with no new REV input.
     * Processing uses 128-frame device block sizing, never malloc. */
    uint32_t samples = 0;
    for (unsigned sec = 0; sec < 65u; sec++) {
        const uint32_t end = (uint32_t)(sec + 1u) * FS;
        while (samples < end) {
            for (uint32_t i = 0; i < CTL; i++) {
                wet[i] = samples + i < 2u * FS ? audio_noise() / 3 : 0;
                side[i] = pre[i] = 0;
            }
            afb_process(wet, side, pre, CTL, 0u, (uint8_t)(samples < 10u * FS));
            samples += CTL;
        }
        if (sec == 4u) at5 = tail_rms();
        if (sec == 29u) at30 = tail_rms();
        if (sec == 59u) at60 = tail_rms();
    }
    assert(at5 > 0.01 && at30 > 0.0001);
    assert(at30 < at5 && at60 < at30);
    assert(afb.al1 == 0 && afb.al2 == 0 && afb.ar1 == 0 && afb.ar2 == 0);
    assert(at60 < 1.0); /* no amplified DC input from lowpass rounding */
    double diff = 20.0 * log10((at60 + 1e-12) / at5);
    printf("Afterburner: RT60=60s DAMP=70, internal tail 5s %.3f, 30s %.3f, 60s %.3f, 5->60s %.1fdB\n",
           at5, at30, at60, diff);
    /* 55 s is almost one RT60: broad tolerance for 32-bit quantization,
     * finite-bandwidth filtering and measurement window. */
    assert(diff < -40.0 && diff > -85.0);

    afterburner.on = 0;
    for (unsigned b = 0; b < 150; b++) {
        memset(wet, 0, sizeof wet); memset(side, 0, sizeof side);
        afb_process(wet, side, pre, CTL, 0u, 0u);
    }
    assert(!afb.active && !afb.clearing && tail_rms() == 0);
    afterburner.on = 1;
    for (unsigned b = 0; b < 8; b++) {
        memset(wet, 0, sizeof wet); memset(side, 0, sizeof side);
        afb_process(wet, side, pre, CTL, 0u, 0u);
        for (uint32_t i = 0; i < CTL; i++) assert(wet[i] == 0 && side[i] == 0);
    }
    puts("AFTERBURNER HOST TEST PASS (DSP-only; no FM-1 measurement)");
    return 0;
}
