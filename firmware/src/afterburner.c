/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 *
 * AFTERBURNER: optional post-REV ambience, NOT a synth voice.
 * Included by fx.c; CARAVEL_AFTERBURNER=0 removes every buffer and control.
 * HALL/STASIS stereo REV mid/side -> two-pole anti-alias filter -> 11.025 kHz
 * four-line orthogonal FDN -> linear 44.1 kHz reconstruction -> REV return.
 * 32-bit Q12 delay samples preserve a 60s tail; int16 failed quantization.
 * No sample recording, malloc or work while switched OFF.
 * RT60 is the nominal low-frequency -60 dB decay, not an exact audible time.
 */
#define AFB_N0 419u
#define AFB_N1 577u
#define AFB_N2 733u
#define AFB_N3 929u
#define AFB_N (AFB_N0 + AFB_N1 + AFB_N2 + AFB_N3)
#define AFB_Q 4096
_Static_assert(FS == 44100 && CTL % 4u == 0u, "AFTERBURNER needs 44.1kHz and blocks divisible by four");
static int32_t afb_line[AFB_N] __attribute__((section(".pool")));
/* CHO+DLY before REV: subtract the copy to feed ONLY the post-REV tail. */
static int32_t afb_pre[CTL] __attribute__((section(".pool")));
/* Q15 feedback loss at 11.025 kHz per line, RT60 10..60s, generated offline:
 * g = 10^(-3 * length / (11025 * RT60)). DAMP makes highs decay sooner. */
static const uint16_t AFB_GAIN[51][4] = {
    {31919, 31605, 31297, 30915}, /* 10s */
    {31995, 31709, 31428, 31079}, /* 11s */
    {32059, 31796, 31538, 31216}, /* 12s */
    {32113, 31869, 31631, 31333}, /* 13s */
    {32159, 31933, 31711, 31434}, /* 14s */
    {32199, 31988, 31780, 31521}, /* 15s */
    {32235, 32036, 31841, 31597}, /* 16s */
    {32266, 32079, 31895, 31665}, /* 17s */
    {32294, 32116, 31943, 31725}, /* 18s */
    {32318, 32150, 31985, 31779}, /* 19s */
    {32341, 32181, 32024, 31828}, /* 20s */
    {32361, 32209, 32059, 31872}, /* 21s */
    {32379, 32234, 32091, 31912}, /* 22s */
    {32396, 32257, 32120, 31949}, /* 23s */
    {32412, 32278, 32147, 31983}, /* 24s */
    {32426, 32298, 32172, 32014}, /* 25s */
    {32439, 32316, 32194, 32043}, /* 26s */
    {32451, 32332, 32215, 32069}, /* 27s */
    {32462, 32348, 32235, 32094}, /* 28s */
    {32473, 32362, 32253, 32117}, /* 29s */
    {32483, 32375, 32270, 32138}, /* 30s */
    {32492, 32388, 32286, 32158}, /* 31s */
    {32500, 32400, 32301, 32177}, /* 32s */
    {32508, 32411, 32315, 32195}, /* 33s */
    {32516, 32421, 32328, 32212}, /* 34s */
    {32523, 32431, 32341, 32228}, /* 35s */
    {32530, 32441, 32353, 32242}, /* 36s */
    {32536, 32449, 32364, 32257}, /* 37s */
    {32542, 32458, 32374, 32270}, /* 38s */
    {32548, 32466, 32384, 32283}, /* 39s */
    {32554, 32473, 32394, 32295}, /* 40s */
    {32559, 32480, 32403, 32306}, /* 41s */
    {32564, 32487, 32412, 32317}, /* 42s */
    {32569, 32494, 32420, 32327}, /* 43s */
    {32573, 32500, 32428, 32337}, /* 44s */
    {32577, 32506, 32435, 32347}, /* 45s */
    {32582, 32511, 32442, 32356}, /* 46s */
    {32585, 32517, 32449, 32365}, /* 47s */
    {32589, 32522, 32456, 32373}, /* 48s */
    {32593, 32527, 32462, 32381}, /* 49s */
    {32596, 32532, 32468, 32389}, /* 50s */
    {32600, 32537, 32474, 32396}, /* 51s */
    {32603, 32541, 32480, 32403}, /* 52s */
    {32606, 32545, 32485, 32410}, /* 53s */
    {32609, 32549, 32490, 32417}, /* 54s */
    {32612, 32553, 32496, 32423}, /* 55s */
    {32615, 32557, 32500, 32429}, /* 56s */
    {32617, 32561, 32505, 32435}, /* 57s */
    {32620, 32564, 32510, 32441}, /* 58s */
    {32623, 32568, 32514, 32446}, /* 59s */
    {32625, 32571, 32518, 32452}, /* 60s */
};
static struct {
    uint16_t w[4], clear_at;
    int32_t lp[4];                 /* damped Q12 FDN feedback */
    int32_t al1, al2, ar1, ar2;   /* 2x input lowpass before 4x decimation */
    int32_t out_m, out_s, next_m, next_s;
    int32_t wet_gain;              /* Q12, ramped on/off */
    uint8_t phase, active, clearing;
} afb;

/* Multiply Q12 feedback error by alpha/Q15 using 32-bit pieces.
 * The low 16 bits of a signed two's-complement difference are positive
 * after the high part has taken the sign: exact to 1 Q12 unit, no int64
 * multiplication in the audio ISR. */
static inline int32_t afb_lp_step(int32_t delta, int32_t alpha)
{
    return (delta >> 16) * (alpha * 2)
         + (int32_t)((((uint32_t)delta & 65535u) * (uint32_t)alpha) >> 15);
}

/* Runs at FS/4, in the ISR. Feedback gains < 1 and a Q12 decay loss make
 * the tail finite and prevent the audible int16 limit cycles found in
 * the first attempt. Do not replace this with a Q15 int16 ring. */
static CARAVEL_AUDIO_HOT __attribute__((noinline))
void afb_tick(int32_t left, int32_t right, int32_t rt60, int32_t damp)
{
    const int32_t *b = afb_line;
    const int32_t d0 = b[afb.w[0]], d1 = b[AFB_N0 + afb.w[1]];
    const int32_t d2 = b[AFB_N0 + AFB_N1 + afb.w[2]];
    const int32_t d3 = b[AFB_N0 + AFB_N1 + AFB_N2 + afb.w[3]];
    const int32_t h[4] = {
        (d0 + d1 + d2 + d3) >> 1,
        (d0 - d1 + d2 - d3) >> 1,
        (d0 + d1 - d2 - d3) >> 1,
        (d0 - d1 - d2 + d3) >> 1
    };
    const uint32_t starts[4] = {0u, AFB_N0, AFB_N0 + AFB_N1, AFB_N0 + AFB_N1 + AFB_N2};
    const uint16_t lens[4] = {AFB_N0, AFB_N1, AFB_N2, AFB_N3};
    const int32_t alpha = 30000 - clamp(damp, 0, 127) * 160;
    const uint16_t *g = AFB_GAIN[clamp(rt60, 10, 60) - 10];
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int32_t delta = h[k] - afb.lp[k], step = afb_lp_step(delta, alpha);
        int32_t abs_lp, loss, x;
        if (!step && delta) step = delta > 0 ? 1 : -1;
        afb.lp[k] += step;
        abs_lp = afb.lp[k] < 0 ? -afb.lp[k] : afb.lp[k];
        /* g is close to 1. Subtract the Q15 loss after splitting off 8
         * fractional bits: all 32-bit operations, with a guaranteed final
         * decrement if the feedback state is too quiet to round normally. */
        loss = ((abs_lp >> 8) * (32768 - (int32_t)g[k])) >> 7;
        if (!loss && abs_lp) loss = 1;
        x = afb.lp[k] - (afb.lp[k] < 0 ? -loss : loss);
        x += (k == 0u || k == 2u ? left : right) * AFB_Q;
        afb_line[starts[k] + afb.w[k]] = clamp(x, -32768 * AFB_Q, 32767 * AFB_Q);
        afb.w[k]++;
        if (afb.w[k] == lens[k]) afb.w[k] = 0;
    }
    afb.next_m = (d0 + d1 + d2 + d3) >> 14;  /* Q12 / four */
    afb.next_s = (d0 - d1 + d2 - d3) >> 15;  /* Q12 / eight */
}

/* Clearing 64 Q12 cells per audio block, rather than thousands of samples
 * in one ISR. Turning ON before completion does not reveal an old tail. */
static __attribute__((noinline)) void afb_clear_step(void)
{
    uint32_t n = AFB_N - afb.clear_at;
    if (n > 64u) n = 64u;
    for (uint32_t i = 0; i < n; i++) afb_line[afb.clear_at + i] = 0;
    afb.clear_at = (uint16_t)(afb.clear_at + n);
    if (afb.clear_at == AFB_N)
        memset(&afb, 0, sizeof afb);
}

/* AFTER inputs are HALL/STASIS wet-only; the caller captures CHO+DLY first.
 * TYPE change leaves this bank untouched. No synth voice is allocated. */
static CARAVEL_AUDIO_HOT __attribute__((noinline))
void afb_process(int32_t *wet, int32_t *side, const int32_t *pre,
                 uint32_t n, uint8_t rev_has_side, uint8_t valid_input)
{
    uint32_t i;
    int32_t target;
    uint8_t enabled = (uint8_t)(afterburner.on && afterburner.amt);
    if (afb.clearing) { afb_clear_step(); return; }
    if (!enabled && !afb.active) return; /* default OFF = no DSP work */
    afb.active = 1;
    target = enabled ? clamp(afterburner.amt, 0, 127) * 24 : 0;
    for (i = 0; i < n; i++) {
        int32_t l = 0, r = 0, delta, mid, s, om, os;
        if (valid_input && enabled) {
            mid = clamp(wet[i] - pre[i], -131072, 131071);
            s = rev_has_side ? clamp(side[i], -131072, 131071) : 0;
            l = clamp(mid + s, -131072, 131071) >> 2;
            r = clamp(mid - s, -131072, 131071) >> 2;
        }
        afb.al1 += (l - afb.al1) >> 2;
        afb.al2 += (afb.al1 - afb.al2) >> 2;
        afb.ar1 += (r - afb.ar1) >> 2;
        afb.ar2 += (afb.ar1 - afb.ar2) >> 2;
        afb.phase++;
        if (afb.phase == 4u) {
            afb.phase = 0;
            afb.out_m = afb.next_m;
            afb.out_s = afb.next_s;
            afb_tick(afb.al2, afb.ar2, afterburner.time, afterburner.damp);
        }
        om = afb.out_m + (((afb.next_m - afb.out_m) * (int32_t)afb.phase) >> 2);
        os = afb.out_s + (((afb.next_s - afb.out_s) * (int32_t)afb.phase) >> 2);
        delta = target - afb.wet_gain;
        if (delta) afb.wet_gain += delta > 0 ? (delta >> 6 ? delta >> 6 : 1)
                                           : (delta < -64 ? -((-delta) >> 6) : -1);
        wet[i] += (om * afb.wet_gain) >> 12;
        side[i] += (os * afb.wet_gain) >> 12;
    }
    if (!enabled && afb.wet_gain == 0) { afb.clearing = 1; afb.clear_at = 0; }
}
