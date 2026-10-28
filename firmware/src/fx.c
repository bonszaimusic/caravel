/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST and INSERT (1.5), then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix. */
#define DLY_SLEW_MAX 1024u          /* the delay time's changes up to this many samples glide (fx_buses), larger ones jump */
#define DLY_LEN 65536u           /* 1.49 s: 1/4 fits from 41 BPM, 1/4D from 61, 1/2 from 81 */
#define CHO_LEN 2048u
static int16_t dly_buf[DLY_LEN] __attribute__((section(".pool")));
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};
static int16_t rev_comb[1116 + 1188 + 1277 + 1356] __attribute__((section(".pool")));
static union {                          /* ROOM's allpasses; SPRING's allpass chain (int32: no clamps) */
    int16_t ap[556 + 441];
    int32_t sp[(556 + 441) / 2];
} rev_u __attribute__((section(".pool")));
#define rev_ap (rev_u.ap)
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    uint32_t dly_cur;            /* the delay's TIME as it is read now, Q8 samples (0: not set yet): follows delay_samples() */
    int32_t dly_lp;
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* persisted: 0 ROOM, 1 SPRING, 2 HALL, 3 STASIS */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
} fx;

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_A 2867                        /* their coefficient, Q12 (0.7: Q12 keeps (x - o) * a in 32 bits up to
                                          * |x - o| < 749000, far past any peak the chain reaches) */
_Static_assert(sizeof rev_comb / 2u >= SP_LEN && sizeof rev_u.sp / 4u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");

/* HALL (G_RTYPE 2, 1.2): a smooth, dense, stereo reverb, again in ROOM's buffers (its few states and one block
 * of work in the pool, no .bss). ROOM's 4 combs each ring on their own (the metallic tone); here every echo
 * goes round 8 lines that all feed each other, and they run at half the rate (22.05 kHz: the same memory holds
 * twice the time, so twice the resonances, at half the work; the tail stops near 10 kHz, as a hall's air
 * does). The send (every other sample of it, low-passed 1 2 1; ROOM's level) -> a low cut (~110 Hz: no boom
 * in a long tail) -> 2 allpass diffusers (rev_ap: a drum's click smeared into a burst) -> into the 8 lines
 * (rev_comb, mutually prime lengths, 21 .. 34 ms). Lines 1, 2, 4, 7 are read through slow sines (+-HL_MOD
 * samples, between samples; 0.37 or 0.74 Hz, each in its own phase): the resonances drift instead of ringing.
 * Lines 0, 3, 5, 6, read at their ends, go through a one-pole low-pass (DAMP: the mixing takes every echo
 * through them); every
 * line through its gain (from its length: all decay alike; RT60 below the damping 0.5 s at SIZE 0, 4 s at
 * 90, 9 s at 127; ~2.4 s heard at 90 with DAMP 60); then the 8 x 8 mixing: a conference matrix (Paley,
 * order 8: every line feeds the 7 others, +-1 each, none itself: no flutter at a line's own length), with
 * the send added. Stereo: left lines 0 3 4 -7, right 1 2 5 6 (sums spread over the matrix's rows); the wet
 * bus (mono) gets left + right, rev_side left - right, which mix_block adds to the left and takes from the
 * right (rev_side_mix). Back to 44.1 kHz between samples. Rounding: a loud line's gain rounds to the nearest
 * (the tail decays as it should), a quiet one's towards zero (it ends in exact silence: no limit cycle), and
 * the low-passes keep their steps' remainders (no offset). */
#define HL_NL 8u                         /* lines */
#define HL_NAP 2u                        /* diffusers */
#define HL_NC (CTL / 2u)                 /* a block at half the rate */
#define HL_MOD 8                         /* the taps' swing, samples (read 2 .. 18 samples short of the end) */
#define HL_LPM 0x69u                     /* the lines with a low-pass */
#define HL_KD(d) (32767 - (d) * 180)     /* its coefficient (Q15) for DAMP d: none at 0 .. ~1 kHz at 127 */
#define HL_LOUD 32                       /* a line this loud (two samples' sum) rounds its gain to the nearest */
#define HL_OUT 4                         /* the outputs' gain: the level of ROOM's tail */
static const uint16_t HL_LEN[HL_NL] = {467, 509, 547, 593, 631, 673, 709, 743};   /* (+1 each: the mirror) */
static const uint16_t HL_AP[HL_NAP] = {131, 307};
static const int16_t HL_APG[HL_NAP] = {11469, 10240};               /* Q14: 0.7, 0.625 */
_Static_assert(467 + 509 + 547 + 593 + 631 + 673 + 709 + 743 + HL_NL <= sizeof rev_comb / 2u &&
               131 + 307 <= sizeof rev_u.ap / 2u, "HALL in ROOM's buffers");
_Static_assert(CTL % 2u == 0u && HL_NC + 2u * HL_MOD + 2u < 467u && HL_NC <= 131u, "HALL: one wrap a block at most");
static struct {                          /* HALL's state (in the pool: no .bss) */
    uint16_t i[HL_NL], d[HL_NAP];        /* the lines' write indices, the diffusers' */
    int32_t lp[HL_NL], le[HL_NL];        /* the low-passes and their steps' remainders */
    int32_t x, m, s, hp, he;             /* the send's last sample, its low cut and that step's remainder; the
                                          * last mid and side out (half rate) */
    int16_t g[HL_NL], sz;                /* the lines' gains / sqrt(7) (Q14), for SIZE sz - 1 (0: none yet) */
    uint32_t ph;                         /* the taps' drift */
    uint8_t side;                        /* sd holds this block's stereo difference (HALL running) */
    int32_t sd[CTL];                     /* the block's left minus right (mix_block: rev_side_mix) */
    int32_t bx[HL_NC], l[HL_NC], r[HL_NC];   /* the block at half the rate: the diffused send, left, right */
    int16_t y[HL_NL][HL_NC];             /* the lines' ends, then what goes back into them */
} hl __attribute__((section(".pool")));
#define rev_side (hl.sd)

/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz). State per part (track_t dist_*). */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
/* the MASTER pot (ADC 0..1023, smoothed in main.c) to its level, square law: 0 .. 4088 (Q12) */
static inline uint32_t master_of_pot(uint32_t k10) { return (k10 * k10) >> 8; }
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)song.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl + (sb_hl >> 1);      /* x1.5 (#180: x3 peaked at 1.5 .. 3.5 x a full-scale kick, the limiter */
}                                     /* pulled the mix down up to 16 dB on each hit and the buzz took over) */

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}

#include "perform.c"                                 /* the FX hold layer's effects (the master) */
#include "click.c"                                   /* the metronome's click (after the master: audio.c) */

/* the delay's TIME (params.c N_DTIME): N_DIV's ten (div_samples), then 1.5.1's dotted 1/8D 1/16D 1/4D (10, 11, 12: 3/4,
 * 3/8, 3/2 of a quarter); at most DLY_LEN - 1 samples (1.486 s): 1/4D clamps at 60 BPM and below (1/2 below 81) */
static const uint8_t DIV_DOT[3] = {2, 3, 1};           /* 3 quarters >> this */
static uint32_t delay_samples(void)
{
    uint32_t d = (uint32_t)song.g[G_DTIME];
    uint32_t s = d - 10u < 3u ? (beat_samples() * 3u) >> DIV_DOT[d - 10u] : div_samples(d);
    return s < 16u ? 16u : s >= DLY_LEN ? DLY_LEN - 1u : s;
}

#if !CARAVEL_REVERB_MINIMAL
/* In the experimental minimal-FX build the classic ROOM and SPRING DSP
 * disappear; their persisted numeric IDs still fall back to HALL. */
/* ROOM (G_RTYPE 0): 4 damped combs + 2 allpasses (Freeverb-like, mono), added to out */
static __attribute__((noinline)) void rev_room(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    int32_t size = 25000 + song.g[G_RSIZE] * 50, damp = 32767 - song.g[G_RDAMP] * 200;
    for (i = 0; i < n; i++) {
        int32_t a = 0;
        int16_t *c = rev_comb;
        int32_t in = mulq15(rev_in[i], 2580);           /* 1/8 at -4 dB: level as before the allpass fix */
        for (k = 0; k < 4u; k++) {
            int32_t o = c[fx.comb_i[k]];
            fx.comb_lp[k] = o + mulq15(fx.comb_lp[k] - o, 32767 - damp);
            c[fx.comb_i[k]] = (int16_t)clamp(in + mulq15(fx.comb_lp[k], size), -32768, 32767);
            if (++fx.comb_i[k] >= REV_COMB[k])
                fx.comb_i[k] = 0;
            a += o;
            c += REV_COMB[k];
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[fx.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;                                  /* Freeverb: out = buf - in (o - v is a notch comb) */
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
}

/* SPRING (see the top), added to out */
static __attribute__((noinline)) void rev_spring(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)song.g[G_RSIZE];
    int32_t g = 19661 + (int32_t)s * 85;                /* the loop's gain: 0.6 .. 0.93 */
    int32_t kl = 26000 - song.g[G_RDAMP] * 160;         /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w;
    int16_t *ln = rev_comb;
    int32_t *ap = rev_u.sp;
    if (!fx.sp_size)
        fx.sp_size = len;
    fx.sp_size += clamp(len - fx.sp_size, -256, 256);   /* SIZE glides (a sample a block at most) */
    L = fx.sp_size >> 8;
    fx.sp_ph += 2u * LFO_INC[24];                       /* the wobble: a slow sine, 1.5 samples deep */
    w = (fx.sp_size >> 1) + ((osc_sine(fx.sp_ph) * 3) >> 8);   /* the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    for (i = 0; i < n; i++) {
        uint32_t wp = fx.sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        fx.sp_lp += mulq15(r - fx.sp_lp, kl);
        o = fx.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - fx.sp_hp + fx.sp_he;                    /* the low cut, its step's remainder kept (as */
        fx.sp_he = o & 63;                              /* dc_block): no dead band to hold an offset in the loop */
        fx.sp_hp += o >> 6;
        x -= fx.sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= SP_N; k++) {                   /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;         /* towards 0, as the loop's gain: floors would feed */
            o = ap[j + k];                              /* the loop a little offset and noise for ever */
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        fx.sp_w = (uint16_t)(wp + 1u);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2;
    }
}

#endif /* !CARAVEL_REVERB_MINIMAL */

/* HALL's helpers: m / 2^sh rounded towards zero (a product that feeds back must not floor: a loop of floors
 * holds an offset and a little noise for ever) */
static inline int32_t sh_tz(int32_t m, uint32_t sh) { return (m + ((m >> 31) & ((1 << sh) - 1))) >> sh; }
/* 2^-x, x Q16 (0 .. 16): Q15 (32768 at x = 0). 2^-x = 2^u / 2^(i + 1), u = 1 - frac(x); 2^u by a cubic (1e-4) */
static uint32_t exp2_neg(uint32_t x)
{
    uint32_t i = x >> 16, u = 32768u - ((x & 0xFFFFu) >> 1), p = 2579u;
    p = ((p * u) >> 15) + 7409u;
    p = ((p * u) >> 15) + 22780u;
    p = ((p * u) >> 15) + 32768u;
    return i < 15u ? p >> (i + 1u) : 0u;
}
/* the lines' gains for SIZE s: RT60 = 0.5 s * 2^(s / 30); a line of D samples (22.05 kHz) loses 60 dB in
 * RT60 * 22050 / D passes: g = 2^(-D * 3 log2(10) / (22050 * RT60)) = 2^(-D * 59.2 / 65536 * 2^(-s / 30)),
 * at most 0.99, over sqrt(7) (the matrix's rows have 7 entries of +-1: / sqrt(7) makes it orthogonal) */
static __attribute__((noinline)) void hall_gains(int32_t s)
{
    uint32_t q = exp2_neg((uint32_t)s * 65536u / 30u), k, g;
    for (k = 0; k < HL_NL; k++) {
        g = exp2_neg((HL_LEN[k] * 59u * q) >> 15);
        hl.g[k] = (int16_t)(((g > 32440u ? 32440u : g) * 6193u) >> 15);
    }
    hl.sz = (int16_t)(s + 1);
}

/* an allpass diffuser over x[0 .. n) from q (its next samples, no wrap): its state rounded towards zero */
static inline void hall_ap(int16_t *q, int32_t g, int32_t *x, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t y = q[i], v = x[i] + sh_tz(y * g, 14);
        q[i] = (int16_t)clamp(v, -32768, 32767);
        x[i] = y - ((v * g + 8192) >> 14);
    }
}
/* a line's end over y[0 .. n) from b (no wrap; b[n] is still the line's: its mirror at the end): between b[i]
 * and b[i + 1], f / 256 of the way (rounded to the nearest: it stays between the two), times its gain g (Q14).
 * The gain's product: rounded to the nearest while the line is loud (lo 0: the tail decays as it should; a
 * truncation would make it die early, the sooner the quieter), towards zero once it is quiet (lo 1: the last
 * steps of the tail, which then ends in exact silence) */
static inline void hall_end(const int16_t *b, int32_t f, int16_t *y, uint32_t n, int32_t g, int lo)
{
    uint32_t i;
    if (lo)
        for (i = 0; i < n; i++)
            y[i] = (int16_t)sh_tz((b[i] + (((b[i + 1u] - b[i]) * f + 128) >> 8)) * g, 14);
    else
        for (i = 0; i < n; i++)
            y[i] = (int16_t)(((b[i] + (((b[i + 1u] - b[i]) * f + 128) >> 8)) * g + 8192) >> 14);
}
/* a line's end over y[0 .. n) from b (no wrap), through the low-pass (kd, Q15; the state *lp and its step's
 * remainder *le), times its gain g (rounded as hall_end's) */
static inline void hall_end_lp(const int16_t *b, int16_t *y, uint32_t n, int32_t g, int lo, int32_t *lp, int32_t *le,
                               int32_t kd)
{
    uint32_t i;
    int32_t l = *lp, r = *le, e;
    if (lo)
        for (i = 0; i < n; i++) {
            e = (b[i] - l) * kd + r, r = e & 32767, l += e >> 15;
            y[i] = (int16_t)sh_tz(l * g, 14);
        }
    else
        for (i = 0; i < n; i++) {
            e = (b[i] - l) * kd + r, r = e & 32767, l += e >> 15;
            y[i] = (int16_t)((l * g + 8192) >> 14);
        }
    *lp = l;
    *le = r;
}
/* the block's HL_NC samples of a circular buffer of len from index d in at most two runs, no wrap inside them:
 * body with s (the run's start in the block) and k (its length); d moves on */
#define HL_RUNS(d, len, k, s, body)                                                         \
    for (s = 0; s < HL_NC; s += k) {                                                        \
        k = (len) - (d) < HL_NC - s ? (len) - (d) : HL_NC - s;                              \
        body;                                                                               \
        d = (d) + k >= (len) ? 0u : (d) + k;                                                \
    }

/* HALL (see the top): a block of the send (n = CTL samples: audio.c) -> the mid added to out, the side into
 * rev_side. At half the rate (HL_NC samples) in passes over the block, each with few states (every line is
 * longer than a block: what the lines give in this block was written before it): the send, every other
 * sample, low cut, diffusers (hl.bx); the lines' ends at their drifting taps, low-passed, scaled (hl.y); the
 * outputs and the matrix; back into the lines; the outputs back to 44.1 kHz. */
#define HL_LFO ((uint32_t)(0.37 * CTL / FS * 4294967296.0))   /* the drift: 0.37 Hz (even lines), 0.74 Hz (odd) */
static __attribute__((noinline)) void rev_hall(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int16_t *b = rev_comb, *a = rev_ap;
    uint32_t i, k, s, m, d;
    /* STASIS needs its own long, dark hall envelope; SIZE and DAMP remain
     * unchanged for the conventional HALL selection. */
    int32_t size = song.g[G_RTYPE] == 3 ? 115 : song.g[G_RSIZE];
    int32_t damp = song.g[G_RTYPE] == 3
        ? clamp(50 + osc_sine(hl.ph) * (int32_t)clamp(song.g[G_DRREV], 0, 127) / 400000, 36, 64)
        : song.g[G_RDAMP];
    int32_t kd = HL_KD(damp);
    int32_t xp = hl.x, hp = hl.hp, he = hl.he, mp = hl.m, sp = hl.s;
    (void)n;
    if (hl.sz != size + 1)
        hall_gains(size);
    for (i = 0; i < HL_NC; i++) {
        int32_t x = (xp + 2 * rev_in[2u * i] + rev_in[2u * i + 1u]) >> 2, v;   /* every other sample, 1 2 1 */
        xp = rev_in[2u * i + 1u];
        x = mulq15(clamp(x, -262143, 262143), 2580);    /* (ROOM's level) */
        v = x - hp + he;                                /* the low cut, its step's remainder kept */
        he = v & 31;
        hp += v >> 5;
        hl.bx[i] = x - hp;
    }
    hl.x = xp, hl.hp = hp, hl.he = he;
    for (k = 0; k < HL_NAP; k++) {                      /* the diffusers */
        d = hl.d[k];
        HL_RUNS(d, HL_AP[k], m, s, hall_ap(a + d, HL_APG[k], hl.bx + s, m));
        hl.d[k] = (uint16_t)d;
        a += HL_AP[k];
    }
    hl.ph += HL_LFO;
    for (k = 0; k < HL_NL; k++) {                       /* the lines' ends */
        int32_t g = hl.g[k], f = 0, lo;
        if ((HL_LPM >> k) & 1u)
            d = hl.i[k];                                /* (low-passed: the line's end) */
        else {                                          /* (drifting: HL_MOD + 2 +- HL_MOD samples short of it) */
            uint32_t r = (uint32_t)(((HL_MOD + 2) << 8) +
                                    ((osc_sine(hl.ph * (1u + (k & 1u)) + k * 0x2C000000u) * HL_MOD) >> 7));
            f = (int32_t)(r & 255u);
            d = hl.i[k] + (r >> 8);
            d = d >= HL_LEN[k] ? d - HL_LEN[k] : d;
        }
        s = d + HL_LEN[k] / 2u;                         /* quiet? (two samples half a line apart) */
        s = s >= HL_LEN[k] ? s - HL_LEN[k] : s;
        lo = (b[d] < 0 ? -b[d] : b[d]) + (b[s] < 0 ? -b[s] : b[s]) < HL_LOUD;
        if ((HL_LPM >> k) & 1u) {
            HL_RUNS(d, HL_LEN[k], m, s, hall_end_lp(b + d, hl.y[k] + s, m, g, lo, &hl.lp[k], &hl.le[k], kd));
        } else {
            HL_RUNS(d, HL_LEN[k], m, s, hall_end(b + d, f, hl.y[k] + s, m, g, lo));
        }
        b += HL_LEN[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {
        int32_t y0 = hl.y[0][i], y1 = hl.y[1][i], y2 = hl.y[2][i], y3 = hl.y[3][i];
        int32_t y4 = hl.y[4][i], y5 = hl.y[5][i], y6 = hl.y[6][i], y7 = hl.y[7][i], x = hl.bx[i];
        int32_t t, p0, p1, p2, p3, p4, p5, p6;
        hl.l[i] = y0 + y3 + y4 - y7;                    /* the outputs */
        hl.r[i] = y1 + y2 + y5 + y6;
        /* the matrix: C = [0 1; -1 Q], Q (7 x 7) = chi(j - i), the quadratic character mod 7 (+1 at distances
         * 1 2 4, -1 at 3 5 6): row 1 + i of Q y = 2 (the three at +1) - (the sum of the 7) + y_1+i */
        t = y1 + y2 + y3 + y4 + y5 + y6 + y7;
        p0 = y2 + y3 + y5, p1 = y3 + y4 + y6, p2 = y4 + y5 + y7, p3 = y5 + y6 + y1;
        p4 = y6 + y7 + y2, p5 = y7 + y1 + y3, p6 = y1 + y2 + y4;
        y0 += t;
        hl.y[0][i] = (int16_t)clamp(t + x, -32768, 32767);   /* (+ the send: signs spread over the rows) */
        hl.y[1][i] = (int16_t)clamp(2 * p0 + y1 - y0 + x, -32768, 32767);
        hl.y[2][i] = (int16_t)clamp(2 * p1 + y2 - y0 + x, -32768, 32767);
        hl.y[3][i] = (int16_t)clamp(2 * p2 + y3 - y0 + x, -32768, 32767);
        hl.y[4][i] = (int16_t)clamp(2 * p3 + y4 - y0 + x, -32768, 32767);
        hl.y[5][i] = (int16_t)clamp(2 * p4 + y5 - y0 + x, -32768, 32767);
        hl.y[6][i] = (int16_t)clamp(2 * p5 + y6 - y0 + x, -32768, 32767);
        hl.y[7][i] = (int16_t)clamp(2 * p6 + y7 - y0 - x, -32768, 32767);
    }
    b = rev_comb;
    for (k = 0; k < HL_NL; k++) {                       /* back into the lines (and the mirror after a wrap) */
        d = hl.i[k];
        HL_RUNS(d, HL_LEN[k], m, s, for (i = 0; i < m; i++) b[d + i] = hl.y[k][s + i]; if (!d) b[HL_LEN[k]] = b[0]);
        hl.i[k] = (uint16_t)d;
        b += HL_LEN[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {                       /* back to 44.1 kHz, between samples */
        int32_t mm = (hl.l[i] + hl.r[i]) * HL_OUT, ss = (hl.l[i] - hl.r[i]) * HL_OUT;
        out[2u * i] += (mp + mm) >> 1;
        out[2u * i + 1u] += mm;
        rev_side[2u * i] = (sp + ss) >> 1;
        rev_side[2u * i + 1u] = ss;
        mp = mm;
        sp = ss;
    }
    hl.m = mp, hl.s = sp;
    hl.side = 1;
}

/* the reverb's buffers and states to silence (the model changed) */
static void rev_clear(void)
{
    uint32_t i;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        rev_comb[i] = 0;
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)          /* (int16: 997 of them, an odd count the int32 view misses one of) */
        rev_u.ap[i] = 0;
    for (i = 0; i < 4u; i++)
        fx.comb_lp[i] = 0;
    for (i = 0; i < HL_NL; i++)
        hl.lp[i] = hl.le[i] = 0;
    fx.sp_lp = fx.sp_hp = fx.sp_he = 0;
    hl.x = hl.m = hl.s = hl.hp = hl.he = 0;
}

#if FELUCCA_CPU2
/* the dry mix and the sends, as each core adds its parts into them (after Jangada: mx[1] is the second core's,
 * mix_block), and a part's own block (b; mx[0].b also the fade of a reverb model change, after the parts) */
typedef struct { int32_t l[CTL], r[CTL], c[CTL], d[CTL], v[CTL], b[CTL]; } mixsum_t;
static mixsum_t mx[2];
#define mix_l (mx[0].l)
#define mix_r (mx[0].r)
#define send_c (mx[0].c)
#define send_d (mx[0].d)
#define send_r (mx[0].v)
#define part_buf (mx[0].b)
#define SM_DECL , mixsum_t *sm                          /* mix_part / mix_spread: the sums they add into */
#define SM_ARG(x) , (x)
#define SM(f) (sm->f)
#else
/* one core: the arrays of 1.6, the same code (SM(l) = mix_l, ...; send_c .. mix_r are declared below) */
static int32_t part_buf[CTL];                            /* a part's block (mix_part); the fade of a model change */
#define SM_DECL
#define SM_ARG(x)
#define SM(f) SM1_##f
#define SM1_l mix_l
#define SM1_r mix_r
#define SM1_c send_c
#define SM1_d send_d
#define SM1_v send_r
#define SM1_b part_buf
#endif

/* STASIS V0.6: MOTN couples fixed-pitch grain-tap crossfades, LP filter and HALL damping. STOP releases HOLD and fades cloud to HALL tail.
 * CAPT is the ONLY recording hold control. EVOLVE drives spectral-source
 * wandering and stereo movement; STR changes textured wet prominence. MORPH
 * crossfades in reverse grains. Their IDs are preserved for saved projects.
 * A continuous last-second send buffer feeds two long crossfaded stereo
 * grain pairs; low-pass and a seven-second HALL diffusion stage spread the
 * texture into a slowly changing, sustained sound. This uses the ordinary
 * tempo-delay's 128 KiB pool allocation, so the normal delay is suspended.
 *
 * CAPT is now a retroactive hold: it freezes already recorded sound.
 * CAPT=0 returns to LIVE, CAPT=1 holds. If the ring is not full enough,
 * holding automatically begins after its safe prefill threshold. No other
 * freeze switch exists: CAPT LIVE/HOLD owns the recording write head.
 */
#define ST_RING_LEN (CARAVEL_REVERB_MINIMAL ? DLY_LEN / 2u : DLY_LEN)
#define ST_RING_MASK (ST_RING_LEN - 1u)
#define ST_GRAIN_LEN 8192u               /* 186 ms windows, four times V0.1 */
#define ST_HALF (ST_GRAIN_LEN / 2u)
#define ST_LAG 12288u                   /* ~279ms back; unity-rate forward head never reads future */
#define ST_OFFSET 1901u
#define ST_DRIFT 1024u                  /* slow, smooth ±1k-sample source scan */
#define ST_MIN_VALID (ST_LAG + ST_OFFSET + 544u + ST_DRIFT + ST_GRAIN_LEN + 128u)
_Static_assert(ST_LAG > 2560u + 256u && ST_MIN_VALID < (FS * 3u / 4u) &&
               ST_MIN_VALID < ST_RING_LEN &&
               (ST_GRAIN_LEN & (ST_GRAIN_LEN - 1u)) == 0u,
               "STASIS grain window must fit a 0.75 s captured history");
static struct {
    uint32_t w, valid, phase, drift_ph, delay_fill, evolve_q8;
    uint32_t gr_l[2], gr_r[2];        /* fixed taps for overlapping playback heads */
    int32_t stop_gain, release_gain;   /* independent STOP and HOLD->LIVE envelopes */
    int32_t smooth_l, smooth_r;
    uint8_t cap_prev, auto_hold, cap_pending, delay_dirty, gr_ready, stopping, releasing;
#if CARAVEL_STASIS_EXPAND
    uint8_t active_quality, active_grain, record_n, reverse_mask;
    int32_t record_acc;
#endif
} st __attribute__((section(".pool")));

#if CARAVEL_STASIS_EXPAND
/* Longer recording WITHOUT increasing DLY_LEN: each of the two fixed 32768
 * sample capture banks stores mono samples at 44.1/22.05/11.025 kHz. Window
 * sizes are 0.743, 1.486, 2.972 seconds respectively. All grains still
 * output at 44.1 kHz; playback decimation uses linear interpolation.
 * GRAIN 4096/8192/16384 samples is independent of recording duration.
 */
static inline uint32_t stasis_grain_samples(void)
{ return 4096u << st.active_grain; }
static inline uint32_t stasis_record_factor(void)
{ return 1u << st.active_quality; }
static inline uint32_t stasis_lag_samples(void)
{ return (stasis_grain_samples()+4096u) >> st.active_quality; }
static inline uint32_t stasis_min_history(void)
{
 /* Conservative maximum: grain-start lookback + far stereo offset + tap
  * modulation + second head's displacement + guard. SCAN uses remaining
  * history only: never reads unrecorded samples, even during LIVE prefill. */
 return stasis_lag_samples() +
       ((ST_OFFSET+421u+4096u+256u+stasis_record_factor()-1u) >> st.active_quality);
}
_Static_assert(16384u+4096u+ST_OFFSET+421u+4096u+256u<ST_RING_LEN,
               "STASIS largest grain and stereo scans must fit HQ capture bank");
#endif

#include "stasis_layer.c"

/* Read one captured source sample per output sample: grain playback pitch
 * never depends on MOTN. Taps are changed only at zero-crossfade weight. */
static inline void st_grain_start(uint32_t head, uint32_t tap_l, uint32_t tap_r)
{
#if CARAVEL_STASIS_EXPAND
    uint32_t room=st.valid>stasis_min_history()?st.valid-stasis_min_history():0u;
    uint32_t scan=room*(uint32_t)clamp(mod_stasis.scan,0,127)/127u;
    uint32_t lag=stasis_lag_samples();
    st.gr_l[head] = (st.w - lag - tap_l - scan) & ST_RING_MASK;
    st.gr_r[head] = (st.w - lag - (ST_OFFSET>>st.active_quality) - tap_r - scan) & ST_RING_MASK;
    if(st_expand.reverse)st.reverse_mask|=(uint8_t)(1u<<head);
    else st.reverse_mask&=(uint8_t)~(1u<<head);
#else
    st.gr_l[head] = (st.w - ST_LAG - tap_l) & ST_RING_MASK;
    st.gr_r[head] = (st.w - ST_LAG - ST_OFFSET - tap_r) & ST_RING_MASK;
#endif
}
#if CARAVEL_STASIS_EXPAND
/* An expansion grain remains at unity speed: resampling changes only the
 * CAPTURE density, not pitch. Interpolate stored data at the original clock.
 * Reverse is latched per head at its silent crossfade boundary. */
static inline int32_t stasis_grain_read(uint32_t start,uint32_t pos)
{
 uint32_t sub=pos&((1u<<st.active_quality)-1u);
 uint32_t sample=pos>>st.active_quality;
 int32_t a=stasis_read(start+sample);
 if(!sub)return a;
 int32_t b=stasis_read(start+sample+1u);
 return a+(((b-a)*(int32_t)sub)>>st.active_quality);
}
#endif
static inline void stasis_transport_stop(void)
{
    if (song.g[G_RTYPE] != 3u) return;
    song.g[G_DRLVL] = 0;               /* CAPT shows LIVE */
    st.cap_prev = st.auto_hold = st.cap_pending = st.releasing = 0;
    st.stopping = 1;
    stasis_layer_stop();
    st.stop_gain = 32768;              /* wet grains fade; HALL naturally decays */
}
static inline void stasis_transport_start(void)
{
    if (song.g[G_RTYPE] != 3u) return;
    st.stopping = 0;
    st.stop_gain = st.release_gain = 32768;
    st.releasing = 0;
    st.gr_ready = 0;
    st.valid = 0;                      /* no stale STOP tail in the next performance */
    st.smooth_l = st.smooth_r = 0;     /* don't resurrect old grains on new PLAY */
#if CARAVEL_STASIS_EXPAND
    st.record_acc=0;st.record_n=0;st.reverse_mask=0;
#endif
    stasis_layer_reset();
}

/* Independent STASIS texture: the raw short grains never substitute for the
 * long decay; they slowly excite HALL. No new audio delay buffers required.
 * Wet stereo adds a quiet textured layer over the already diffused tail. */
static __attribute__((noinline)) void stasis_block(const int32_t *rev_in, int32_t *wet, uint32_t n)
{
    int32_t hall_in[CTL], mid[CTL], side[CTL];
#if CARAVEL_REVERB_MINIMAL
    uint32_t i, strength = (uint32_t)clamp(mod_stasis.strength, 0, 127);
    uint32_t target = (uint32_t)clamp(mod_stasis.motion, 0, 127) << 8u;
#else
    uint32_t i, strength = (uint32_t)clamp(song.g[G_RSIZE], 0, 127);
    uint32_t target = (uint32_t)clamp(song.g[G_DRREV], 0, 127) << 8u;
#endif
    int32_t err = (int32_t)target - (int32_t)st.evolve_q8;
    st.evolve_q8 = (uint32_t)((int32_t)st.evolve_q8 +
        (err / 384 ? err / 384 : err > 0 ? 1 : err < 0 ? -1 : 0));
    uint32_t motion = st.evolve_q8 >> 8u;
#if CARAVEL_STASIS_EXPAND
    /* Never reinterpret held grains with a different recording sample rate.
     * Quality changes are applied only in LIVE, and restart prefill. */
    if(st.active_quality!=st_expand.quality && !st.auto_hold &&
       !st.cap_pending && !st.releasing && !st.stopping &&
       st_layer.phase==STL_IDLE && !mod_stasis.capt){
        st.active_quality=(uint8_t)clamp(st_expand.quality,0,2);
        st.w=st.valid=st.phase=0;
        st.gr_ready=0;st.record_acc=0;st.record_n=0;
        st.smooth_l=st.smooth_r=0;st.reverse_mask=0;
        stasis_layer_reset();
    }
    /* Grain-length changes land at a cycle boundary. This keeps both
     * read heads phase aligned, and does not rewrite any frozen bank. */
    uint32_t requested_min=(((4096u<<(uint32_t)clamp(st_expand.grain,0,2))+4096u+
            ST_OFFSET+421u+4096u+256u+stasis_record_factor()-1u) >> st.active_quality);
    if(st.active_grain!=st_expand.grain &&
      (!st.auto_hold || st.valid>=requested_min) &&
      (!st.gr_ready ||
      ((st.phase&(stasis_grain_samples()-1u))==0u))){
       st.active_grain=(uint8_t)clamp(st_expand.grain,0,2);
       st.gr_ready=0;st.phase=0;st.reverse_mask=0;
    }
    uint32_t grain_len=stasis_grain_samples(),grain_half=grain_len>>1u;
    uint32_t min_valid=stasis_min_history(),factor=stasis_record_factor();
#endif
#if CARAVEL_REVERB_MINIMAL
    uint32_t cap = mod_stasis.capt;
#else
    uint32_t cap = song.g[G_DRLVL] != 0;
#endif
    int32_t sine_l = osc_sine(st.drift_ph);
    int32_t sine_r = osc_sine(st.drift_ph + 0x50000000u);
    /* Macro modulation calculated once per 32-sample block, not per sample.
     * Stereo pole cutoff ~1.5..10 kHz, moving in opposing directions. */
    int32_t cut_l = clamp(17000 + sine_l * (int32_t)motion / 300, 6500, 28000);
    int32_t cut_r = clamp(17000 + sine_r * (int32_t)motion / 300, 6500, 28000);
    /* Variable echo taps: no pitch sweep because tap changes are only made
     * at a silent-grain crossfade boundary. */
    uint32_t tap_l = 256u + (uint32_t)(((uint32_t)(sine_l + 32768) * motion * 28u) >> 16u);
    uint32_t tap_r = 256u + (uint32_t)(((uint32_t)(sine_r + 32768) * motion * 28u) >> 16u);
#if CARAVEL_STASIS_EXPAND
    tap_l>>=st.active_quality;
    tap_r>>=st.active_quality;
#endif
    int32_t layer_gain = (int32_t)((strength * 2304u + 63u) / 127u);
    int32_t hall_gain = (int32_t)((strength * 1024u + 63u) / 127u);
    if (cap && !st.cap_prev && !st.stopping) {
        /* Retake HOLD without erasing the previous sound. */
        if (!st.releasing) stasis_layer_reset();
        st.releasing = 0;
#if CARAVEL_STASIS_EXPAND
        st.cap_pending = st.valid < min_valid;
#else
        st.cap_pending = st.valid < ST_MIN_VALID;
#endif
        st.auto_hold = !st.cap_pending;
    }
    if (!cap && st.cap_prev && !st.stopping) {
        /* Keep the ring unchanged while the old held texture fades into
         * HALL. Resume writing new LIVE audio only after that fade. */
        st.cap_pending = 0;
        st.releasing = st.auto_hold != 0u;
        if (!st.releasing) st.auto_hold = 0;
    }
    st.cap_prev = (uint8_t)cap;
    stasis_layer_block(rev_in, n);
    for (i = 0; i < n; i++) {
        stasis_layer_tick(rev_in[i]);
#if CARAVEL_STASIS_EXPAND
        uint32_t p = st.phase & (grain_len - 1u);
        uint32_t q = (p + grain_half) & (grain_len - 1u);
        uint32_t weight = p < grain_half ? p : grain_len - p;
#else
        uint32_t p = st.phase & (ST_GRAIN_LEN - 1u);
        uint32_t q = (p + ST_HALF) & (ST_GRAIN_LEN - 1u);
        uint32_t weight = p < ST_HALF ? p : ST_GRAIN_LEN - p;
#endif
        if (!st.auto_hold && !st.stopping) {
            int32_t v = clamp(rev_in[i] >> 2, -32768, 32767);
#if CARAVEL_STASIS_EXPAND
            /* Boxcar decimator; bounded +/-32768*4. The one source ring
             * gains 2x/4x history but not a single extra sample of RAM. */
            st.record_acc+=v;
            if(++st.record_n>=factor){
                dly_buf[st.w++ & ST_RING_MASK] =
                    (int16_t)clamp(st.record_acc/(int32_t)factor,-32768,32767);
                st.record_acc=0;st.record_n=0;
                if(st.valid<ST_RING_LEN)st.valid++;
            }
            if(st.cap_pending && st.valid>=min_valid){
#else
            dly_buf[st.w++ & ST_RING_MASK] = (int16_t)v;
            if (st.valid < ST_RING_LEN) st.valid++;
            if (st.cap_pending && st.valid >= ST_MIN_VALID) {
#endif
                st.cap_pending = 0;
                st.auto_hold = 1;
            }
        }
#if CARAVEL_STASIS_EXPAND
        if (st.valid >= min_valid && !st.gr_ready) {
#else
        if (st.valid >= ST_MIN_VALID && !st.gr_ready) {
#endif
#if CARAVEL_STASIS_EXPAND
            st.phase = p = 0; q = grain_half; weight = 0;
            st_grain_start(0, tap_l, tap_r);
            st_grain_start(1, tap_l + (743u>>st.active_quality),
                           tap_r + (421u>>st.active_quality));
#else
            st.phase = p = 0; q = ST_HALF; weight = 0;
            st_grain_start(0, tap_l, tap_r);
            st_grain_start(1, tap_l + 743u, tap_r + 421u);
#endif
            st.gr_ready = 1;
        }
        if (st.gr_ready) {
            /* Two heads 180 degrees apart. Reposition only where gain is 0. */
#if CARAVEL_STASIS_EXPAND
            if(p==0)st_grain_start(0,tap_l,tap_r);
            if(p==grain_half)st_grain_start(1,tap_l+(743u>>st.active_quality),
                                             tap_r+(421u>>st.active_quality));
            uint32_t p0=st.reverse_mask&1u?grain_len-1u-p:p;
            uint32_t p1=st.reverse_mask&2u?grain_len-1u-q:q;
            int32_t la=stasis_grain_read(st.gr_l[0],p0);
            int32_t lb=stasis_grain_read(st.gr_l[1],p1);
            int32_t ra=stasis_grain_read(st.gr_r[0],p0);
            int32_t rb=stasis_grain_read(st.gr_r[1],p1);
            int32_t left=(la*(int32_t)weight+lb*(int32_t)(grain_half-weight))>>12u;
            int32_t right=(ra*(int32_t)weight+rb*(int32_t)(grain_half-weight))>>12u;
#else
            if (p == 0) st_grain_start(0, tap_l, tap_r);
            if (p == ST_HALF) st_grain_start(1, tap_l + 743u, tap_r + 421u);
            int32_t la = stasis_read(st.gr_l[0] + p);
            int32_t lb = stasis_read(st.gr_l[1] + q);
            int32_t ra = stasis_read(st.gr_r[0] + p);
            int32_t rb = stasis_read(st.gr_r[1] + q);
            int32_t left = (la * (int32_t)weight + lb * (int32_t)(ST_HALF - weight)) >> 12u;
            int32_t right = (ra * (int32_t)weight + rb * (int32_t)(ST_HALF - weight)) >> 12u;
#endif
            st.smooth_l += ((left - st.smooth_l) * cut_l) >> 15u;
            st.smooth_r += ((right - st.smooth_r) * cut_r) >> 15u;
        } else {
            st.smooth_l -= st.smooth_l >> 4u;
            st.smooth_r -= st.smooth_r >> 4u;
        }
        int32_t gain = 32768;
        if (st.stopping) {
            st.stop_gain -= (st.stop_gain + 8191) >> 13u;
            if (st.stop_gain < 0) st.stop_gain = 0;
            gain = st.stop_gain;
        } else {
            if (st.releasing) {
                st.release_gain -= (st.release_gain + 8191) >> 13u;
                if (st.release_gain < 96) {
                    st.release_gain = 0;
                    st.releasing = st.auto_hold = 0;
                    st.gr_ready = 0;
                    st.valid = 0;      /* new live capture, no frozen taps */
#if CARAVEL_STASIS_EXPAND
                    st.record_acc=0;st.record_n=0;
#endif
                    st.smooth_l = st.smooth_r = 0;
                    stasis_layer_reset();
                }
            } else if (st.release_gain < 32768) {
                st.release_gain += 32; /* soft return if HOLD is re-engaged */
                if (st.release_gain > 32768) st.release_gain = 32768;
            }
            gain = st.release_gain;
        }
        mid[i] = (((st.smooth_l + st.smooth_r) >> 1u) * gain) >> 15u;
        side[i] = (((st.smooth_l - st.smooth_r) >> 1u) * gain) >> 15u;
        hall_in[i] = clamp(rev_in[i] + ((mid[i] * hall_gain) >> 8u), -262143, 262143);
        st.phase++;
    }
    st.drift_ph += (motion * 190u) * n;  /* slow linked filter/tap motion */
    rev_hall(hall_in, wet, n);
    for (i = 0; i < n; i++) {
        wet[i] += (mid[i] * layer_gain) >> 8u;
        rev_side[i] += (side[i] * layer_gain) >> 8u;
    }
    hl.side = 1;
}

/* the model changed (to rt): the old one's block fades out (HALL's stereo difference with it), its buffers are
 * cleared, the new one starts from silence */
static __attribute__((noinline)) void rev_switch(const int32_t *rev_in, int32_t *wet, uint32_t n, int32_t rt)
{
    int32_t *t = part_buf, g = 65536, d = 65536 / (int32_t)n;
    uint32_t i;
    for (i = 0; i < n; i++)
        t[i] = 0;
    if (fx.rtype == 3u)
        stasis_block(rev_in, t, n);
    else if (fx.rtype == 2u)
        rev_hall(rev_in, t, n);
#if !CARAVEL_REVERB_MINIMAL
    else if (fx.rtype)
        rev_spring(rev_in, t, n);
    else
        rev_room(rev_in, t, n);
#else
    else
        rev_hall(rev_in, t, n);
#endif
    for (i = 0; i < n; i++, g -= d) {
        wet[i] += mulq16(t[i], (uint32_t)g);
        rev_side[i] = mulq16(rev_side[i], (uint32_t)g);   /* (HALL only: hl.side set) */
    }
    rev_clear();
    fx.rtype = (uint8_t)rt;
}

/* process the three buses for one block; sends in, wet stereo-equal out */
static void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet,
                     uint32_t n)
{
    uint32_t i, dl = delay_samples();
    int32_t fb = song.g[G_DFDBK] * 230, col = 2000 + song.g[G_DCOLOR] * 240;
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t cdepth = song.g[G_CDEPTH] * 6, rt;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    rt = song.g[G_RTYPE];
    rt = rt >= 0 && rt <= 3 ? rt : 2;
#if CARAVEL_REVERB_MINIMAL
    if (rt == 3u)
        mod_stasis_update(); /* shared FX: after track renders, only once per block */
#endif
#if CARAVEL_REVERB_MINIMAL
    if (rt < 2)
        rt = 2; /* old ROOM / SPRING projects play HALL, but remain stored unchanged */
#endif
    if (rt == 3u && fx.rtype != 3u) {
        stasis_layer_reset();
        st.w = st.valid = st.phase = st.drift_ph = 0u;
#if CARAVEL_STASIS_EXPAND
        st.active_quality=(uint8_t)clamp(st_expand.quality,0,2);
        st.active_grain=(uint8_t)clamp(st_expand.grain,0,2);
        st.record_acc=0;st.record_n=0;st.reverse_mask=0;
#endif
#if CARAVEL_REVERB_MINIMAL
        st.evolve_q8 = (uint32_t)clamp(mod_stasis.motion, 0, 127) << 8u;
#else
        st.evolve_q8 = (uint32_t)clamp(song.g[G_DRREV], 0, 127) << 8u;
#endif
        st.gr_ready = st.stopping = 0;
        st.stop_gain = st.release_gain = 32768;
        st.releasing = 0;
        st.smooth_l = st.smooth_r = 0;
        st.auto_hold = st.cap_pending = 0u;
        st.cap_prev = 0u;                 /* CAPT already ON is honored on entry */
        st.delay_dirty = 1u;
    } else if (rt != 3u && fx.rtype == 3u) {
        /* Previous STASIS audio is not a tempo-delay tail: suppress reads
         * until the conventional delay line has been refilled. */
        fx.dly_w = fx.dly_cur = fx.dly_lp = 0;
        st.delay_fill = 0;
    }
    if (pf.ew1 && fb < THR_FB)
        fb += mulq16(THR_FB - fb, (uint32_t)pf.ew1 << 1);   /* ECHO THROW (perform.c): more feedback while held, 0.7
                                                             * at most (the loop's gain below 1: it never runs away) */
    for (i = 0; i < n; i++) {
        int32_t y = 0, x, r;
        /* chorus: modulated short delay, 5..15 ms */
        cho_buf[fx.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        fx.cho_ph += cinc;
        r = (400 << 8) + ((osc_sine(fx.cho_ph) + 32768) * cdepth >> 8);   /* Q8 delay: read between samples */
        {
            uint32_t ri = (uint32_t)r >> 8;
            int32_t f = r & 255, c0 = cho_buf[(fx.cho_w - ri) & (CHO_LEN - 1u)];
            int32_t c1 = cho_buf[(fx.cho_w - ri - 1u) & (CHO_LEN - 1u)];
            y += (c0 + (((c1 - c0) * f) >> 8)) << 1;
        }
        fx.cho_w++;
        if (rt == 3u) {
            wet[i] = y;
            continue; /* STASIS owns dly_buf: ordinary DELAY is unavailable */
        }
        /* delay with a low-passed feedback */
        {   /* The TIME follows the tempo: on the external clock the measured beat is re-estimated every quarter note and
             * moves by a few samples, and a read position that jumps is a click in the echo (tests/delay_ext_test.c).
             * Small changes (up to DLY_SLEW_MAX samples: the estimate, a drifting tempo) are followed slowly, with the
             * read between two samples (time constant 2^14 samples, a few cents of pitch while it moves); a larger one
             * (the TIME knob, a new note value) jumps as it always did. */
            int32_t dq = (int32_t)(dl << 8) - (int32_t)fx.dly_cur;
            uint32_t ri, fr;
            int32_t x0, x1;
            if (!fx.dly_cur || dq > (int32_t)(DLY_SLEW_MAX << 8) || dq < -(int32_t)(DLY_SLEW_MAX << 8))
                fx.dly_cur = dl << 8;
            else if (dq)
                fx.dly_cur = (uint32_t)((int32_t)fx.dly_cur + ((dq >> 14) ? (dq >> 14) : dq > 0 ? 1 : -1));
            ri = fx.dly_cur >> 8;
            fr = fx.dly_cur & 255u;
            x0 = dly_buf[(fx.dly_w - ri) & (DLY_LEN - 1u)];
            x1 = dly_buf[(fx.dly_w - ri - 1u) & (DLY_LEN - 1u)];
            x = fr ? x0 + (((x1 - x0) * (int32_t)fr) >> 8) : x0;
            if (st.delay_dirty && st.delay_fill <= ri)
                x = 0; /* do not replay captured STASIS ring after mode exit */
        }
        fx.dly_lp += mulq15(x - fx.dly_lp, col);
        dly_buf[fx.dly_w & (DLY_LEN - 1u)] =
            (int16_t)clamp((dly_in[i] >> 1) + mulq15(fx.dly_lp, fb), -32768, 32767);
        fx.dly_w++;
        if (st.delay_dirty) {
            if (++st.delay_fill >= DLY_LEN)
                st.delay_dirty = 0;
        }
        y += mulq15(x << 1, dmix);
        wet[i] = y;
    }
    hl.side = 0;
    if (rt != fx.rtype)
        rev_switch(rev_in, wet, n, rt);
    else if (rt == 3u)
        stasis_block(rev_in, wet, n);
    else if (rt == 2u)
        rev_hall(rev_in, wet, n);
#if !CARAVEL_REVERB_MINIMAL
    else if (rt)
        rev_spring(rev_in, wet, n);
    else
        rev_room(rev_in, wet, n);
#endif
}

/* after the buses: HALL's stereo difference to the left and from the right of the dry mix (the wet bus adds the
 * same to both) */
static __attribute__((noinline)) void rev_side_mix(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        l[i] += rev_side[i];
        r[i] -= rev_side[i];
    }
}

/* INSERT (1.5, Discussions #78 and #177): one effect per track, after DIST and before the SLICER, with a dry / wet
 * MIX. TYPE (P_ITYPE, N_ITYPE; stored, append-only) and its three values A B C (P_IA .. P_IC, what they mean: params.c
 * ins_desc):
 *   SOFT HARD FOLD FUZZ  DRIVE (1x .. ~13x, squared), TONE (two one-pole low-passes at F_CUTOFF), LEVEL (F_DB, as the
 *                        track's LEVEL: 112 is 0 dB). A slow low cut in front (~27 Hz), a DC blocker after (FUZZ is
 *                        lopsided). SOFT tanh, HARD a flat clip at a voice's level, FOLD a triangle folder (the wave
 *                        reflected at +-16384: every fold more harmonics), FUZZ a hard top and a soft, halved bottom;
 *   CRUSH                BITS (1 .. 16, of a voice's full scale), RATE (a held sample, 689 Hz .. 44.1 kHz, INS_HOLD:
 *                        aliasing as the old samplers), LPF (two one-pole low-passes at F_CUTOFF after them, #177);
 *   PHASR                RATE (F_LFOHZ), DEPTH (the sweep around ~850 Hz: 0 still .. the whole 150 Hz .. 4.8 kHz),
 *                        FDBK (0 .. 0.7): the FX layer's four all-pass stages (perform.c PH_A), mono, per track;
 *   FLANG CHOR           RATE, DEPTH, FDBK: a 256-sample line per track (5.8 ms); FLANG swept 0.25 .. 5 ms (the FX
 *                        layer's squared triangle), feedback 0 .. 0.85; CHOR a sine around 3.2 ms (+-2.3 ms at DEPTH
 *                        127), feedback 0 .. 0.4. A DC blocker on the swept copy (a clipped loud input is lopsided).
 * The swept ones are the pedals': their wet is (dry + swept) / 2, the notches need both, so MIX 100 % is the whole
 * effect and less a lighter one. MIX (P_IMIX) blends dry and wet linearly; MIX 0 or TYPE OFF: the track bit for bit
 * as before (nothing runs). MIX glides (2.9 ms, SL_SLOPE); a TYPE change fades the old one out, clears the state,
 * fades the new one in; so does a start from MIX 0 (no stale line or filter). With SPREAD (mix_spread) the insert
 * takes the mono sum b, as DIST does; the side passes it by.
 * Order: DIST -> INSERT -> SLICER (-> the FILTER, 0.3.0: track_filter): the insert colours the sound as DIST does; the SLICER's gate (and its STUT
 * recordings) comes after, so its edges stay sharp (no flanger feedback or phaser ringing smears them) and a STUT
 * repeats the processed sound. State in the pool, per track (ins[], 580 B each); noinline, its own loops: the audio
 * ISR's inlined path gains one test per part (track_t ins_run) */
enum { IT_OFF, IT_SOFT, IT_HARD, IT_FOLD, IT_FUZZ, IT_CRUSH, IT_PHASER, IT_FLANGER, IT_CHORUS, IT_N };
#define IL_LEN 256u                     /* FLANG / CHOR: the line, samples (a power of 2: masked) */
static const uint8_t INS_HOLD[16] = {64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};   /* CRUSH RATE */
typedef struct {
    int32_t w;                          /* the wet share now, Q15 */
    uint8_t type;                       /* the type running */
    uint8_t cnt;                        /* CRUSH: samples left of the held one */
    uint16_t wp;                        /* FLANG / CHOR: the line's next write */
    uint32_t ph;                        /* the sweep's phase */
    int32_t hp, lp1, lp2, dc, hold;     /* low cut (Q8), the two low-passes, DC blocker (Q8), CRUSH's held sample */
    int32_t px[PH_ST], py[PH_ST], fb;   /* PHASR: each stage's last input and output, the fed back */
    int16_t line[IL_LEN];               /* FLANG / CHOR, at a quarter (headroom) */
} ins_t;
static ins_t ins[NTRK] __attribute__((section(".pool")));

static inline int32_t ins_mixq(int32_t m) { return m >= 127 ? 32768 : m <= 0 ? 0 : m * 258; }   /* MIX -> Q15 */
/* the one-pole low-pass (topology-preserving) coefficient of cutoff index c, Q16: G = g / (1 + g), g = tan(pi fc / FS) */
static int32_t ins_lpk(int32_t c)
{
    uint32_t g = SVF_G[clamp(c, 0, 127)];
    return (int32_t)((g << 16) / (4096u + g));
}
/* the drives' and CRUSH's shape of v (a voice's full scale 32768; v already through DRIVE): the curve the INSERT
 * graph draws too */
static inline int32_t ins_shape(uint32_t ty, int32_t v, uint32_t sh)
{
    switch (ty) {
    case IT_SOFT:
        return softclip(v);
    case IT_HARD:
        return clamp(v, -24000, 24000);
    case IT_FOLD: {
        int32_t u = (int32_t)((uint32_t)(v + 16384) & 0xFFFFu) - 32768;   /* a triangle of period 65536: +-16384 */
        u = 16384 - (u < 0 ? -u : u);
        return u + (u >> 1);                            /* x 1.5 (no divide) */
    }
    case IT_FUZZ:
        return v > 0 ? (v > 20000 ? 20000 : v) : softclip(v) >> 1;
    case IT_CRUSH:                                      /* sh: 16 - BITS; to the nearest step */
        return sh ? ((v + (1 << (sh - 1u))) >> sh) << sh : v;
    default:
        return v;
    }
}
static inline uint32_t ins_bits_sh(int32_t a) { return 15u - (uint32_t)(clamp(a, 0, 127) >> 3); }   /* BITS 1..16 */

/* SOFT .. FUZZ: low cut -> DRIVE -> shape -> TONE -> LEVEL -> DC blocker; CRUSH: held sample -> bits -> LPF */
static __attribute__((noinline)) void ins_drive(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, ty = s->type, crush = ty == IT_CRUSH, sh = ins_bits_sh(t->p[P_IA]);
    uint32_t hold = INS_HOLD[clamp(t->p[P_IB], 0, 127) >> 3];
    int32_t a = t->p[P_IA], g = 4096 + a * a * 3;       /* DRIVE, Q12: 1x .. 12.8x, gentle at first */
    int32_t k = ins_lpk(crush ? t->p[P_IC] : t->p[P_IB]), lvl = crush ? 4096 : LEVEL_Q12[t->p[P_IC] & 127];
    if (ty == IT_SOFT && s->w == 32768 && tw == 32768) {
        for (i = 0; i < n; i++) {
            int32_t x = b[i], v, y;
            s->hp += ((x << 8) - s->hp) >> 8;
            v = clamp(x - (s->hp >> 8), -230000, 230000);
            y = softclip(((v >> 3) * g) >> 9) << 4;
            v = mulq16(y - s->lp1, (uint32_t)k);
            y = v + s->lp1; s->lp1 = y + v;
            v = mulq16(y - s->lp2, (uint32_t)k);
            y = v + s->lp2; s->lp2 = y + v;
            y = (((y + 8) >> 4) * lvl) >> 12;
            s->dc += ((y << 8) - s->dc) >> 9;
            b[i] = y - (s->dc >> 8);
        }
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t x = b[i], y, v;
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        if (crush) {
            if (!s->cnt) {
                s->hold = ins_shape(IT_CRUSH, clamp(x, -262144, 262143), sh);
                s->cnt = (uint8_t)hold;
            }
            s->cnt--;
            y = s->hold;
        } else {
            s->hp += ((x << 8) - s->hp) >> 8;            /* ~27 Hz low cut (Q8: no offset left by the rounding) */
            v = clamp(x - (s->hp >> 8), -230000, 230000);
            y = ins_shape(ty, ((v >> 3) * g) >> 9, 0);    /* (>> 3 first: 32 bits) */
        }
        y <<= 4;                                        /* (x16: the low cutoffs' rounding leaves no offset) */
        v = mulq16(y - s->lp1, (uint32_t)k);             /* TONE / LPF: two one-pole low-passes */
        y = v + s->lp1;
        s->lp1 = y + v;
        v = mulq16(y - s->lp2, (uint32_t)k);
        y = v + s->lp2;
        s->lp2 = y + v;
        y = (y + 8) >> 4;
        if (!crush) {
            y = (y * lvl) >> 12;                        /* LEVEL */
            s->dc += ((y << 8) - s->dc) >> 9;           /* DC blocker, ~14 Hz (FUZZ's lopsided halves) */
            y -= s->dc >> 8;
        }
        b[i] = pf_mix(x, y, s->w);
    }
}

/* PHASR, FLANG, CHOR: the sweep at RATE (one LFO per track), DEPTH, FDBK */
static __attribute__((noinline)) void ins_swept(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, inc = LFO_INC[clamp(t->p[P_IA], 0, 127)] / CTL, dep = (uint32_t)clamp(t->p[P_IB], 0, 127);
    int32_t c = clamp(t->p[P_IC], 0, 127);
    if (s->type == IT_PHASER) {                         /* the coefficients a block at a time (at its middle) */
        uint32_t tr = pf_tri(s->ph + (n >> 1) * inc), q = 32768u + (uint32_t)(((int32_t)tr - 32768) * (int32_t)dep / 127);
        int32_t a = PH_A[q >> 11] + (((PH_A[(q >> 11) + 1u] - PH_A[q >> 11]) * (int32_t)(q & 2047u)) >> 11);
        int32_t fbk = c * 180;                          /* 0 .. 0.7, Q15 */
        for (i = 0; i < n; i++) {
            int32_t x = b[i], xs = clamp((x + 4) >> 3, -16383, 16383), u = xs + ((s->fb * fbk + 16384) >> 15), y = u;
            uint32_t st;
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            for (st = 0; st < PH_ST; st++) {            /* y = a (u - y1) + u1 */
                y = clamp(s->px[st] + ((a * (u - s->py[st]) + 8192) >> 14), -65535, 65535);
                s->px[st] = u;
                s->py[st] = y;
                u = y;
            }
            s->fb = clamp(y, -32767, 32767);
            y <<= 2;                                    /* (y: at an eighth) */
            s->dc += ((y << 8) - s->dc) >> 9;           /* DC blocker, ~14 Hz (a clipped loud input is lopsided) */
            b[i] = pf_mix(x, (x >> 1) + y - (s->dc >> 8), s->w);   /* (dry + all-passed) / 2 */
        }
        s->ph += n * inc;
        return;
    }
    {
        int32_t chorus = s->type == IT_CHORUS, fbk = c * (chorus ? 103 : 219);   /* 0 .. 0.4 / 0 .. 0.85, Q15 */
        uint32_t dq = dep * 516u;                       /* DEPTH, Q16 */
        for (i = 0; i < n; i++, s->ph += inc) {
            int32_t x = b[i], r0, r1, v, fr;
            uint32_t d;                                 /* the delay, Q16 samples */
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            if (chorus) {
                d = (uint32_t)((140 << 16) + ((osc_sine(s->ph) * (int32_t)dep * 50) >> 5));   /* 3.2 +- 2.3 ms */
            } else {
                uint32_t tr = pf_tri(s->ph), sw = (tr * tr) >> 16;   /* longer near the short end (perform.c) */
                d = (11u << 16) + 209u * (((sw >> 1) * dq) >> 15);  /* 0.25 .. 5 ms */
            }
            r0 = (int32_t)(s->wp - (d >> 16)) & (int32_t)(IL_LEN - 1u);
            r1 = (r0 - 1) & (int32_t)(IL_LEN - 1u);
            fr = (int32_t)((d >> 1) & 0x7FFFu);
            v = s->line[r0] + (((s->line[r1] - s->line[r0]) * fr) >> 15);
            s->line[s->wp] = (int16_t)clamp((x >> 2) + softclip(mulq15(v, fbk)), -32767, 32767);
            s->wp = (uint16_t)((s->wp + 1u) & (IL_LEN - 1u));
            v <<= 1;                                    /* (v: at a quarter) */
            s->dc += ((v << 8) - s->dc) >> 9;           /* DC blocker (the line's clip on a loud lopsided wave) */
            b[i] = pf_mix(x, (x >> 1) + v - (s->dc >> 8), s->w);   /* (dry + delayed) / 2 */
        }
    }
}

/* the INSERT at rest at once (mix_part: TYPE turned OFF while the part was silent, its fade never ran) */
static __attribute__((noinline)) void ins_rest(track_t *t)
{
    memset(&ins[(uint32_t)(t - trk) % NTRK], 0, sizeof(ins_t));
    t->ins_run = 0;
}

/* the track's INSERT on its block b (mix_part, mix_spread): nothing while TYPE is OFF or MIX is 0 and faded */
static __attribute__((noinline)) void track_insert(track_t *t, int32_t *b, uint32_t n)
{
    ins_t *s = &ins[(uint32_t)(t - trk) % NTRK];
    int32_t ty = clamp(t->p[P_ITYPE], 0, IT_N - 1), tw;
    if (s->type != ty && !s->w)                         /* faded out: the new type from rest */
        s->type = (uint8_t)ty;
    tw = s->type == ty && ty ? ins_mixq(t->p[P_IMIX]) : 0;
    if (!s->w) {
        if (!tw) {
            t->ins_run = 0;
            return;                                     /* dry: bit for bit */
        }
        memset(s, 0, sizeof *s);                        /* starting: no stale line or filter */
        s->type = (uint8_t)ty;
    }
    if (s->type >= IT_PHASER)
        ins_swept(s, t, b, n, tw);
    else
        ins_drive(s, t, b, n, tw);
    t->ins_run = s->w != 0;                             /* (mix_part calls again while it fades) */
}

/* The track's FILTER (Caravel 0.3.0; P_FILT, the INSERT 2 page's fourth card): one bipolar knob, -64 .. 63: negative a low-pass, positive a
 * high-pass, 0 OFF. The last stage of the track's chain (engine voices -> DIST -> INSERT -> SLICER -> FILTER -> level, pan and sends), so the sends
 * to the chorus, delay and reverb take the filtered sound; on every engine, those without a filter of their own too. The knob works as the FX
 * layer's FILTER does (perform.c): the LP from 14.2 kHz (-1) down to 120 Hz (-64), the HP from 32 Hz (+1) up to 3.8 kHz (+63), an octave every 7
 * rows of 30 Hz * 2^(row / 7). 12 dB/oct: two identical trapezoidal one-poles in a row (the HP: each one's input less its low-pass), their
 * cutoff set so the filter is -3 dB at the row's frequency (TF_G: G = g / (1 + g), g = tan(pi f / FS) / 0.6436, Q16). No resonance, so it cannot ring or
 * oscillate, and it is stable at every setting (each pole is a weighted mean of its input and its state). The states carry 8 fractional bits, which
 * keeps the low cutoffs exact (no dead band: a pole whose G is 0.003 would otherwise hold an offset of a few hundred).
 * Smooth: the cutoff glides to the knob's place at most 192 / 256 of a row each 32-sample block (the whole range in 47 ms, as the layer's), G blended
 * between two rows; the wet share (Q15) glides 2.9 ms (SL_SLOPE) in and out. Starting, the states are the input's (no transient). Turning to 0 (or over
 * to the other side) first opens the cutoff, then fades the filter out, clears it and switches: no click, nothing left running. 0 with the filter
 * faded out: nothing runs and the track is bit for bit as before (mix_part calls it only while P_FILT is set or track_t tf_run says it still runs).
 * State: tfilt_t per track (a few words, no pool). With SPREAD (mix_spread) the side signal d (left - right) takes the same filter in a second pair
 * of states. DRUM-X's per-lane image and lane sends (drumx_mix) are added after the strip, as they pass DIST, the INSERT and the SLICER by */
#define TF_TAIL 32u                     /* blocks of the filter's state to run out after the last voice (23 ms: the LP at 120 Hz rings 12 ms) */
#define TF_STEP 192                     /* the cutoff's glide a block, Q8 rows (perform.c) */
#define TF_ROWS 63                      /* the last row */
#define TF_IN 524287                    /* the input is held to +-2^19 (8 loud voices are 192000): with 8 fractional bits every product fits 32 bits */
static const uint16_t TF_G[64] = {      /* G of the row, Q16 (the formula above; tests/trackfilter_test.c checks it) */
    217, 239, 264, 292, 322, 355, 392, 432,
    477, 526, 581, 640, 706, 779, 859, 947,
    1044, 1151, 1269, 1398, 1540, 1696, 1868, 2056,
    2263, 2489, 2738, 3010, 3307, 3633, 3989, 4376,
    4799, 5260, 5760, 6304, 6893, 7531, 8220, 8964,
    9764, 10624, 11546, 12532, 13585, 14706, 15896, 17157,
    18491, 19897, 21377, 22931, 24560, 26266, 28050, 29915,
    31867, 33911, 36058, 38324, 40731, 43309, 46106, 49191,
};
typedef struct {
    int32_t z[4];                       /* the two poles' states (Q8) for b, then for the side d */
    int32_t cut;                        /* the cutoff now: a row, Q8 (30 Hz * 2^(row / 7)) */
    int32_t w;                          /* the wet share now, Q15 */
    uint32_t g;                         /* G of cut, Q16 */
    uint8_t mode;                       /* running: 0 none, 1 LP, 2 HP */
} tfilt_t;
static tfilt_t tf[NTRK];

/* one trapezoidal pole on x (Q8): its low-pass, the state moved on */
static inline int32_t tf_pole(int32_t x, int32_t *z, uint32_t g)
{
    int32_t d = mulq16(x - *z, g), y = *z + d;
    *z = y + d;
    return y;
}
/* a sample of the 12 dB/oct filter (x as b, Q0): LP the two poles in a row, HP each one's input less its low-pass */
static inline int32_t tf_sample(int32_t x, int32_t *z, uint32_t g, uint32_t hp)
{
    int32_t u = clamp(x, -TF_IN, TF_IN) << 8, y = tf_pole(u, z, g);
    if (hp) {
        u -= y;
        y = u - tf_pole(u, z + 1, g);
    } else {
        y = tf_pole(y, z + 1, g);
    }
    return (y + 128) >> 8;
}
/* A separate block kernel keeps the mutually exclusive fast/transition loops
 * individually visible to the existing static loop-budget checker. One call
 * per block, never per sample; full wet is exactly pf_mix(x,y,32768)==y. */
static __attribute__((noinline)) void tf_wet_block(tfilt_t *s, int32_t *b, int32_t *d, uint32_t n, uint32_t hp)
{
    for (uint32_t j = 0; j < n; j++) {
        b[j] = tf_sample(b[j], s->z, s->g, hp);
        if (d) d[j] = tf_sample(d[j], s->z + 2, s->g, hp);
    }
}
/* the filter at rest at once (nothing sounds: P_FILT turned to 0 while the part was silent, its fade never ran) */
static __attribute__((noinline)) void tf_rest(track_t *t)
{
    memset(&tf[(uint32_t)(t - trk) % NTRK], 0, sizeof(tfilt_t));
    t->tf_run = 0;
}
/* the track's FILTER on its block b and, with SPREAD, the side d (else 0) */
static __attribute__((noinline)) void track_filter(track_t *t, int32_t *b, int32_t *d, uint32_t n)
{
    tfilt_t *s = &tf[(uint32_t)(t - trk) % NTRK];
    int32_t v = clamp(t->p[P_FILT], -64, 63), want = v < 0 ? 1 : v > 0 ? 2 : 0, tgt, tw, i, f;
    uint32_t hp;
    if (!s->mode) {
        if (!want) {
            t->tf_run = 0;
            return;                                     /* off: bit for bit */
        }
        memset(s, 0, sizeof *s);                        /* starting: open, the poles at the input (no transient), the wet share from 0 */
        s->mode = (uint8_t)want;
        s->cut = want == 1 ? PF_TOP : 0;
        if (want == 1) {
            s->z[0] = s->z[1] = clamp(b[0], -TF_IN, TF_IN) << 8;
            if (d)
                s->z[2] = s->z[3] = clamp(d[0], -TF_IN, TF_IN) << 8;
        } else {
            s->z[0] = clamp(b[0], -TF_IN, TF_IN) << 8;
            if (d)
                s->z[2] = clamp(d[0], -TF_IN, TF_IN) << 8;
        }
    }
    hp = s->mode == 2u;
    if (want == s->mode) {                              /* the knob's place: LP from the top down, HP from the bottom up */
        tgt = hp ? v * (PF_TOP - PF_LOW) / 63 : PF_TOP + v * ((PF_TOP - PF_LOW) / 64);
        tw = 32768;
    } else {                                            /* off or the other side: open the cutoff, then fade out */
        tgt = hp ? 0 : PF_TOP;
        tw = s->cut == tgt ? 0 : 32768;
    }
    s->cut += clamp(tgt - s->cut, -TF_STEP, TF_STEP);
    i = s->cut >> 8;
    f = s->cut & 255;
    s->g = (uint32_t)((int32_t)TF_G[i] + (((int32_t)TF_G[i < TF_ROWS ? i + 1 : TF_ROWS] - (int32_t)TF_G[i]) * f >> 8));   /* between two rows */
    /* Fully wet at entry stays fully wet for this block. Keep the original
     * slew path for startup, bypass and LP/HP transitions. */
    if (s->w == 32768 && tw == 32768)
        tf_wet_block(s, b, d, n, hp);
    else
    for (uint32_t j = 0; j < n; j++) {
        int32_t x = b[j];
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        b[j] = pf_mix(x, tf_sample(x, s->z, s->g, hp), s->w);
        if (d) {
            x = d[j];
            d[j] = pf_mix(x, tf_sample(x, s->z + 2, s->g, hp), s->w);
        }
    }
    if (!s->w && !tw)                                   /* faded out: at rest (the other side starts with the next block) */
        memset(s, 0, sizeof *s);
    t->tf_run = s->mode != 0;
}

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> INSERT -> SLICER -> FILTER -> level / pan / sends -> buses -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
#if FELUCCA_CPU2
static int32_t wet[CTL];                                 /* (the dry mix and the sends: mx[0], above) */
#else
static int32_t send_c[CTL], send_d[CTL], send_r[CTL], wet[CTL], mix_l[CTL], mix_r[CTL];
#endif

/* SPREAD (#148): a POLY or UNISON part whose SPRD is above 0. Its voices start left and right of PAN in turn (voice.c
 * voice_start; UNISON's detuned voices alternate), at PAN -/+ SPRD / 2 (SPRD 127: hard left and right), with the pan
 * law PAN has (the far side down, the near one as it is). track_render gives every voice (b, as always) and the left
 * ones (spread_buf); here d = left - right, the side signal, so for a pair of pans L = b (gla + glb) / 2 + d (gla - glb)
 * / 2 (R alike): the mono sum is b's, the level of a centre pan less by SPRD / 256 at most. b goes through DIST, the
 * SLICER and the FX layer's mute key as always; d gets the SLICER's gate and the mute's ramp (slicer_track_sd,
 * perf_mute_sd) and passes DIST by (the spread of a distorted chord is its voices' clean difference); the FILTER (0.3.0)
 * takes b and d alike, the last stage before the mute key. The sends
 * take b, mono, as before. SPRD 0, MONO or LEGATO: mix_part's own loop, bit for bit as before. The cost: a block's
 * clear and sum of spread_buf, the side and a wider mix loop, per part; per voice none (it picks its buffer) */
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n SM_DECL)
{
    int32_t *d = SPREAD_BUF(t);
    uint32_t i;
    int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN], s = (t->p[P_SPRD] + 1) >> 1;
    int32_t pa = clamp(pan - s, -64, 63), pb = clamp(pan + s, -64, 63);
    int32_t gla = 4096 - (pa > 0 ? pa * 64 : 0), gra = 4096 + (pa < 0 ? pa * 64 : 0);
    int32_t glb = 4096 - (pb > 0 ? pb * 64 : 0), grb = 4096 + (pb < 0 ? pb * 64 : 0);
    int32_t glm = (gla + glb) >> 1, grm = (gra + grb) >> 1, gld = (gla - glb) >> 1, grd = (gra - grb) >> 1;
    int32_t c = t->p[P_CHOR] * 258, dl = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
    int32_t xmax = c > dl ? c : dl;
    xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);
    for (i = 0; i < n; i++)
        d[i] = (d[i] << 1) - b[i];                      /* left - right */
    track_dist(t, b, n);
    if (t->p[P_ITYPE] | t->ins_run)
        track_insert(t, b, n);                          /* (the mono sum, as DIST) */
    slicer_track_sd(t, b, d, n);
    if (t->p[P_FILT] | t->tf_run)
        track_filter(t, b, d, n);                       /* the FILTER (0.3.0): the sum and the side */
    if ((pf.mute >> (t - trk)) & 1u)
        perf_mute_sd((uint32_t)(t - trk), b, d, n);
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x, y = ((d[i] >> 2) * lvl) >> 10;
        int32_t xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (c)
            SM(c)[i] += mulq15(xs, c);
        if (dl)
            SM(d)[i] += mulq15(xs, dl);
        if (r)
            SM(v)[i] += mulq15(xs, r);
        SM(l)[i] += ((x * glm) >> 12) + ((y * gld) >> 12);
        SM(r)[i] += ((x * grm) >> 12) + ((y * grd) >> 12);
    }
    t->peak = pk;
}

/* a part's cost for the second core's split (mix_split, after Jangada's): its voices x its engine's cost per
 * voice (ENG_VCOST: the host's instructions per sample, tests/regress.c CPU at 8 notes held less the idle mix, over the
 * engine's voices), and the DIST, INSERT and sends while it sounds or its tail runs. ENGINES[] order; DIGITAL as FM6
 * when built, nothing when retired. An estimate: it only steers the split, the sound does not depend on it */
#if FELUCCA_CPU2
static uint16_t part_cost[NTRK];                        /* the last block's, per part */
static const uint16_t ENG_VCOST[NENGINES] = {163, FELUCCA_FM4 ? 181 : 0, 162, 67, 70, 174, 144, 143, 189, 309, 113, 134,
                                             181,
                                             FELUCCA_SLICE ? 129 : 0,
                                             190, 400, 1100 /* SEM, HADES, DRONE incl. cells: provisional split estimates */
};
#endif

/* one synth part into the dry mix and the sends of sm (mx[0]; mx[1] on the second core); a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static __attribute__((noinline)) void drumx_mix(const track_t *t, uint32_t n SM_DECL)
{
    uint32_t i;
    int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127];
    if (((pf.mute >> (t - trk)) & 1u) || t->p[P_MUTE]) return;
    for (i = 0; i < n; i++) {
        SM(l)[i] -= ((DX_SIDE(t).sl[i] >> 2) * lvl) >> 10;
        SM(r)[i] -= ((DX_SIDE(t).sr[i] >> 2) * lvl) >> 10;
        SM(c)[i] += ((DX_SIDE(t).send[0][i] >> 2) * lvl) >> 10;
        SM(d)[i] += ((DX_SIDE(t).send[1][i] >> 2) * lvl) >> 10;
        SM(v)[i] += ((DX_SIDE(t).send[2][i] >> 2) * lvl) >> 10;
    }
}
static void mix_part_inner(track_t *t, uint32_t n SM_DECL)
{
    int32_t *b = SM(b);
    uint32_t i;
#if FELUCCA_CPU2
    uint32_t nr;
#endif
    dx_side_begin(t, n);

#if FELUCCA_HOME_PERF
    hp_begin(t);
#endif
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    T_SPREAD(t) = !dx_track(t) && t->p[P_SPRD] > 0 && (trk_vmode(t) == V_POLY || trk_vmode(t) == V_UNISON);   /* (SPREAD, mix_spread) */
#if FELUCCA_CPU2
    nr = track_render(t, b, n);
    part_cost[(uint32_t)(t - trk) % NTRK] = (uint16_t)(nr * ENG_VCOST[t->engine % NENGINES] + (nr || t->tail ? 60u : 0u));
    if (nr)
#else
    if (track_render(t, b, n))
#endif
        t->tail = t->p[P_ITYPE] ? 64 : t->p[P_FILT] ? TF_TAIL : 16;   /* blocks of DIST / INSERT / FILTER state to run out after
                                                         * the last voice (the INSERT's: 46 ms, a flanger's feedback) */
    else if ((!t->tail || !(t->p[P_DIST] | t->p[P_ITYPE] | t->p[P_FILT]) || !--t->tail) && !slicer_busy(t)) {
        slicer_track(t, 0, n);                          /* (the SLICER's step clock runs on) */
        if (t->ins_run && !t->p[P_ITYPE])
            ins_rest(t);                                /* TYPE OFF while silent: no fade-out left for the next note */
        if (t->tf_run && !t->p[P_FILT])
            tf_rest(t);                                 /* FILT 0 while silent: likewise */
        if (MOD(t).on)
            mod_end(t);

#if FELUCCA_HOME_PERF
        hp_end(t);
#endif
        return;
    }
    if (T_SPREAD(t))
        mix_spread(t, b, n SM_ARG(sm));
    else {
        track_dist(t, b, n);
        if (t->p[P_ITYPE] | t->ins_run)
            track_insert(t, b, n);                      /* the INSERT (1.5): off, nothing */
        slicer_track(t, b, n);                          /* slicer.c: before the level, pan and sends */
        if (t->p[P_FILT] | t->tf_run)
            track_filter(t, b, 0, n);                   /* the FILTER (0.3.0): the last stage of the track's chain */
        if ((pf.mute >> (t - trk)) & 1u)
            perf_mute((uint32_t)(t - trk), b, n);       /* perform.c: a black key in the FX layer */
    }
    if (!T_SPREAD(t)) {                                /* (the mix's values read after the calls: none is held over
                                                         * them, no spill in the loop: the ISR's cost as before 1.5) */
        int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN];
#if FELUCCA_PAIR_MORPH
        if (pair_mix_member(t)) {
            pair_apply_gain(t, b, n);
            lvl = LEVEL_Q12[ep.level & 127]; pan = ep.pan;
        }
#endif

        int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                SM(c)[i] += mulq15(xs, c);
            if (d)
                SM(d)[i] += mulq15(xs, d);
            if (r)
                SM(v)[i] += mulq15(xs, r);
            SM(l)[i] += (x * gl) >> 12;
            SM(r)[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
    if (DX_SIDE(t).on && dx_track(t)) drumx_mix(t, n SM_ARG(sm));
    if (pf.thr)
        perf_throw(t, b, SM(d), SM(v), n);              /* perform.c: REV / ECHO THROW (1.5.1), the sends towards full */
    if (MOD(t).on)
        mod_end(t);                                     /* the stored values back */
#if FELUCCA_HOME_PERF
    hp_end(t);
#endif

}

/* the HOME oscilloscope (ui_graph.c graph_scope): every other sample of the left channel. MENU > DISPLAY > SCOPE (1.2,
 * Discussion #165): OUT (0, the default) what goes out, after MASTER (audio.c audio_block, before the click); MIX the
 * mix before MASTER (and before the FX layer, SPEAKER EQ and the limiter), at the level MASTER at its top gives, so its
 * size does not follow the volume. The click is in neither */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static volatile uint8_t scope_mix;
static __attribute__((noinline)) void scope_take_mix(uint32_t n)
{
    uint32_t i;
    for (i = 1; i < n; i += 2u)                         /* (((x >> 2) * MASTER_FULL) >> 10 is x) */
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(mix_l[i] + wet[i], -32767, 32767);
}

/* the master with the FX layer's effects between its level and master_out (perform.c) */
static __attribute__((noinline)) void perf_master(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    perf_block(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

static void mix_part(track_t *t, uint32_t n SM_DECL)
{
#if FELUCCA_PERF_HW
    pd_stamp_t start;if(pd_active)start=pd_stamp();
#endif
    mix_part_inner(t,n
#if FELUCCA_CPU2
        ,sm
#endif
    );
#if FELUCCA_PERF_HW
    if(pd_active)pd_track_done((uint32_t)(t-trk),start);
#endif
}

/* ---- the second core (hal/fm1_cpu1.h; after Jangada's, github.com/zednaked/jangada, GPL-3.0) renders some of
 * the parts of each block into its own sums (mx[1]) while this core renders the others; then this core adds them in
 * and goes on (the LFOs' blocks, the FX layer, the buses, the master) as with one core. The sums are integers, so the
 * order they add up in changes nothing: the output is bit for bit one core's. A part's render touches its own state
 * only (its track_t with its SPREAD, ENV SYNC and LFO phase, its voices, its matrix: mod.c MOD, its SPREAD buffer,
 * its engine's memory) and what nobody writes meanwhile (song, the tables); what parts share is done before the
 * split (events_block: the notes, voice stealing; the FX layer's perf_begin) or after it (the LFOs' S&H draw from the
 * shared generator, part by part: voice.c track_lfo_end). tests/dualcore_test.c checks it, under ThreadSanitizer too.
 * Without a second core (the host, the browser, MENU > 2ND CORE OFF, one that did not answer or stopped) C1_RUN takes
 * nothing and this core renders them all */
#if FELUCCA_CPU2
#ifndef C1_RUN
#define C1_ON() 0                                       /* a second core is answering */
#define C1_RUN(fn, arg) 0                               /* hand it fn(arg): 1 = taken */
#define C1_WAIT() 0                                     /* wait for it: 0 = done */
#endif
#ifndef C1_SPLIT
#define C1_SPLIT() mix_split()
#endif
static uint32_t c1_blocks;                              /* blocks the second core took parts of (console) */

static void c1_parts(uint32_t arg)                      /* arg: the parts (bit k = part k) | n << 8 */
{
    uint32_t i, n = arg >> 8;
    mixsum_t *sm = &mx[1];
    for (i = 0; i < n; i++)
        sm->l[i] = sm->r[i] = sm->c[i] = sm->d[i] = sm->v[i] = 0;
    for (i = 0; i < NPART; i++)
        if (arg >> i & 1u)
            mix_part(&trk[i], n, sm);
}

/* which parts go to the second core: the split of the last block's costs that leaves the busier core the least; none
 * when that saves less than handing them over costs (C1_HAND) */
#define C1_HAND 40u
static uint32_t mix_split(void)
{
    uint32_t m, i, best = 0, all = 0, lo;
    for (i = 0; i < NPART; i++)
        all += part_cost[i];
    lo = all;
    for (m = 1; m < (1u << NPART) - 1u; m++) {
        uint32_t a = 0, w;
        for (i = 0; i < NPART; i++)
            if (m >> i & 1u)
                a += part_cost[i];
        w = (a > all - a ? a : all - a) + C1_HAND;
        if (w < lo) {
            lo = w;
            best = m;
        }
    }
    return best;
}
#endif

static void mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
#if FELUCCA_PAIR_MORPH
    pair_mix_begin(n);
#endif
    perf = perf_begin(n);                               /* the FX hold layer at work (perform.c) */
#if FELUCCA_CPU2
    uint32_t m = C1_ON() ? C1_SPLIT() & ((1u << NPART) - 1u) : 0u;
#if FELUCCA_PERF_HW
    pd_worker_mask=m;
#endif
    if (m && !C1_RUN(c1_parts, m | n << 8)) {
        m = 0;
#if FELUCCA_PERF_HW
        pd_worker_mask=0;
#endif
    }
    c1_blocks += m != 0;
    for (i = 0; i < NPART; i++)
        if (!(m >> i & 1u))
            mix_part(&trk[i], n, &mx[0]);
    if (m) {
        const mixsum_t *sm = &mx[1];
        if (C1_WAIT()) {                                /* held: complete on core 0 */
#if FELUCCA_PERF_HW
            pd_worker_mask=0;
#endif
            c1_parts(m | n << 8);
        }
        for (i = 0; i < n; i++) {
            mix_l[i] += sm->l[i];
            mix_r[i] += sm->r[i];
            send_c[i] += sm->c[i];
            send_d[i] += sm->d[i];
            send_r[i] += sm->v[i];
        }
    }
#if FELUCCA_PERF_HW
    pd_worker_mask=0;
    if(pd_active)for(i=0;i<3;i++) {
        pd.engine[i].sum+=pd.worker_engine[i].sum;
        pd.engine[i].calls+=pd.worker_engine[i].calls;
        if(pd.worker_engine[i].max>pd.engine[i].max)pd.engine[i].max=pd.worker_engine[i].max;
        memset(&pd.worker_engine[i],0,sizeof pd.worker_engine[i]);
    }
#endif
    for (i = 0; i < NPART; i++)                         /* the LFOs' blocks, part by part (voice.c) */
        track_lfo_end(&trk[i]);
#else
    for (i = 0; i < NPART; i++) {                       /* one core */
        mix_part(&trk[i], n);
    }
#endif
    if (perf)
        perf_pre(mix_l, mix_r, send_d, send_r, n);
    fx_buses(send_c, send_d, send_r, wet, n);
    if (hl.side)
        rev_side_mix(mix_l, mix_r, n);                  /* HALL: stereo */
    if (scope_mix)
        scope_take_mix(n);
    if (perf) {
        perf_master(out, n);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t l = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        int32_t r = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
