/* SPDX-License-Identifier: GPL-3.0-only */
/* Original, reusable fixed-point VA blocks. No hardware samples or circuit code.
 * Three trapezoidal one-poles with algebraically solved linear feedback. The
 * bounded soft knee in the loop is an approximation, not an OTA circuit model.
 * Q12 coefficients; signals/states bounded to +/-49152 keep products in int32.
 * Included after Felucca-Engines dsp.c. No allocation or shared writable state. */
typedef struct { int32_t g, g2, g3, k, inv; } va_lp3_t;

static inline int32_t va_q12(int32_t x, int32_t a) { return (x * a) >> 12; }
static inline int32_t va_pole(int32_t x, int32_t g, int32_t *z)
{
    int32_t d = va_q12(x - *z, g), y = *z + d;
    *z = clamp(y + d, -49152, 49152);
    return y;
}
static void va_lp3_coef(va_lp3_t *f, int32_t cut, int32_t res)
{
    cut = clamp(cut, 0, 127 * 256);
    int32_t i = cut >> 8, g = SVF_G[i];
    if (i < 127) g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    f->g = (g * 4096) / (4096 + g);
    f->g2 = va_q12(f->g, f->g);
    f->g3 = va_q12(f->g2, f->g);
    f->k = clamp(res, 0, 32767) * 28672 / 32767; /* 7.0: below the 3-pole oscillation threshold 8 */
    f->inv = (4096 * 4096) / (4096 + va_q12(f->g3, f->k));
}
static inline int32_t va_lp3(const va_lp3_t *f, int32_t in, int32_t z[3])
{
    int32_t s = va_q12(va_q12(z[0], f->g2) + va_q12(z[1], f->g) + z[2], 4096 - f->g);
    /* Compensate the linear DC loss of feedback: bass remains useful while
     * resonance rises. Wide multiply avoids overflow during live sweeps. */
    in = va_q12(in, 4096 + f->k);
    int32_t u = mulq16(in - va_q12(s, f->k), (uint32_t)f->inv << 4);
    u = soft_knee(u, 20000);
    return va_pole(va_pole(va_pole(u, f->g, z), f->g, z + 1), f->g, z + 2);
}
/* A single phase at f/4 derives all pitches: no detune between divider subs.
 * Pulse output is centred at every duty cycle; BLEP is inherited from dsp.c. */
static inline int32_t va_divider(uint32_t phase, uint32_t inc, uint32_t octave)
{
    return osc_pulse(phase << octave, inc << octave, 0x80000000u) * 2;
}
static inline int32_t va_slew(int32_t was, int32_t target)
{
    return was + clamp(target - was, -768, 768);
}
static inline int32_t va_pair(int32_t a, int32_t b) { return (int32_t)((uint32_t)a | (uint32_t)b << 16); }
static inline int32_t va_lo(int32_t pair) { return (int32_t)((uint32_t)pair & 65535u); }
static inline int32_t va_hi(int32_t pair) { return (int32_t)((uint32_t)pair >> 16); }
/* n is the block reciprocal 65536 / frames, computed outside the sample loop. */
static inline int32_t va_ramp(int32_t a, int32_t b, uint32_t i, uint32_t n)
{
    return a + (((b - a) * (int32_t)(i + 1u) * (int32_t)n) >> 16);
}
/* Post-filter soft drive, continuously dry at zero. Quadratic gain control,
 * fixed output headroom: no auto-gain pumping on a bass envelope. */
static inline int32_t va_drive(int32_t x, int32_t amount)
{
    int32_t a = amount >> 8, gain = 4096 + a * a * 2;
    int32_t wet = softclip(va_q12(clamp(x, -49152, 49152), gain));
    return x + mulq15(wet - x, amount);
}
