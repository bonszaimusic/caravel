/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions adapted from JIANT (juanjiant-bit, GPL-3.0-only); changes since the fork: Copyright (C) 2026 bonszaimusic */
/* Chip voice: pulse/triangle/saw/noise/wave RAM, quantised amplitude and a held
 * (downsampled) output; SWEEP bends the pitch down after note-on.
 *
 * WAVE = WRAM plays a 32-sample, 4-bit wave table, read step-wise (no interpolation);
 * DUTY then picks one of 16 built-in tables (DUTY / 8, shown as "WAV#" with its name).
 * CHIP = STEP: a 4-bit stepped volume envelope with a frame-clock divider replaces the
 * ADSR curve (lofi_amp), the triangle becomes a 32-step 4-bit staircase, the pulse
 * duties are 12.5 / 25 / 50 / 75 %; CRSH then sets the envelope (shown as "DCY"). */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT", "STEP"};
static const char *const N_RWAVE[] = {"PLS", "TRI", "SAW", "NOIS", "WRAM", "BYTE", "FLOAT"};
static const char *const N_RARP[] = {"OFF", "OCT", "MAJ", "MIN"};
enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT, CHIP_STEP };
enum { RW_PLS, RW_TRI, RW_SAW, RW_NOIS, RW_WRAM, RW_BYTE, RW_FLOAT };
#define LOFI_FORMULA_STATE 0xB17E0000u

/* wave RAM: 16 tables x 32 samples x 4 bits, two samples a byte (high nibble first);
 * our own shapes, from simple formulas (squares, sine, triangle, saws, harmonic sums,
 * one formant peak for the vowels). DC is taken out per table in lofi_render. */
static const char *const N_WRAM[] = {"SQ50", "SQ25", "SQ12", "SINE", "TRI", "SAW", "SAW2", "BAS1",
                                     "BAS2", "ORGN", "HOLW", "VOXA", "VOXO", "VOXE", "BUZZ", "STAIR", 0};
static const uint8_t WRAM[16][16] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ50 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ25 */
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ12 */
    {0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56},   /* SINE */
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},   /* TRI */
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},   /* SAW */
    {0x01, 0x12, 0x33, 0x45, 0x56, 0x77, 0x89, 0x9A, 0x56, 0x67, 0x88, 0x9A, 0xAB, 0xCC, 0xDE, 0xEF},   /* SAW2: + its octave */
    {0xEE, 0xEF, 0xFF, 0xFF, 0xFF, 0xEE, 0xED, 0xDC, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x12, 0x23},   /* BAS1: sloped square */
    {0xCE, 0xFF, 0xED, 0xBA, 0x9A, 0xBD, 0xEF, 0xFE, 0xC9, 0x64, 0x21, 0x00, 0x00, 0x01, 0x24, 0x69},   /* BAS2: 1 + 2 + 3 */
    {0x8B, 0xEF, 0xED, 0xBA, 0x9A, 0xAA, 0x98, 0x77, 0x88, 0x87, 0x65, 0x55, 0x65, 0x42, 0x10, 0x14},   /* ORGN: 1..4 */
    {0x8D, 0xFE, 0xDD, 0xDD, 0xCD, 0xDD, 0xDE, 0xFD, 0x82, 0x01, 0x22, 0x22, 0x32, 0x22, 0x21, 0x02},   /* HOLW: odd */
    {0x8F, 0xFC, 0xA7, 0x67, 0x89, 0x88, 0x88, 0x87, 0x88, 0x77, 0x77, 0x76, 0x78, 0x98, 0x53, 0x00},   /* VOXA: "ah" */
    {0x8F, 0xEE, 0xFD, 0xDB, 0x98, 0x66, 0x66, 0x77, 0x88, 0x89, 0x99, 0x97, 0x64, 0x22, 0x01, 0x10},   /* VOXO: "oh" */
    {0x7F, 0x96, 0xCF, 0xBB, 0xCC, 0xBB, 0xAA, 0x98, 0x77, 0x65, 0x54, 0x43, 0x34, 0x40, 0x39, 0x60},   /* VOXE: "ee" */
    {0xF3, 0x00, 0x12, 0x22, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},   /* BUZZ: spike + ring */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00},   /* STAIR: 4 levels */
};
#define Q4 2184                  /* one 4-bit step (2 * 2184 * 7.5 = 32760) */

/* CHIP = STEP: CRSH as the envelope, CRSH / 4 = b: 0 constant volume, 1..16 a one-shot decay
 * (divider period 0..15), 17..31 a looping one (period 1..15) */
static const char *const N_DCY[] = {"CONST", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10",
                                    "D11", "D12", "D13", "D14", "D15", "L1", "L2", "L3", "L4", "L5", "L6", "L7",
                                    "L8", "L9", "L10", "L11", "L12", "L13", "L14", "L15", 0};
static const param_desc_t LOFI_WAVNUM = {"WAV#", F_INT, 0, 127, 64, N_WRAM, 0};   /* = edit[2] but the names */
static const param_desc_t LOFI_DCY = {"DCY", F_INT, 0, 127, 0, N_DCY, 0};        /* = edit[3] but the names */

/* (JIANT 0.4) WAVE = BYTE: bytebeat, an 8-bit formula of the time t played as the wave (after viznut's 2011
 * one-liners: the music is in the formula). DUTY picks one of 32 (ALGO, DUTY / 4), CRSH is the formulas' variable
 * a (VAR: 1 .. 16); t runs with the note played (C4: ~8 kHz, the classic rate; an octave up twice as fast), restarts
 * at each note-on; the hold (4B/2), the bits (CHIP), TONE and the rest as for the other waves. Our own picks and
 * variations; each a few operations a sample */
static const char *const N_BYTE[] = {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11",
                                     "B12", "B13", "B14", "B15", "B16", "B17", "B18", "B19", "B20", "B21", "B22",
                                     "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32", 0};
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 127, 64, N_BYTE, 0};    /* = edit[2] (BYTE) */
static const param_desc_t LOFI_VAR = {"VAR", F_PCT, 0, 127, 0, 0, 0};            /* = edit[3] (BYTE) */
/* (JIANT 0.5) BYTE's own: RES the filter's resonance (VIB's slot), LOOP (MASK's slot) t held in a window of 2^17 ..
 * 2^5 steps: the formula's long evolution (0: free) folded into a short cycle repeating at the note: a pitched tone */
static const param_desc_t LOFI_LOOP = {"LOOP", F_PCT, 0, 127, 0, 0, 0};           /* = edit[7] (BYTE) */
/* (JIANT 0.5.1) each formula's start, in 512 steps of t: where it is already moving (some sit silent for up to
 * ~2 s from t = 0); a note starts there, LOOP's window too. The first 17 were rewritten in 0.5.1, away from the
 * well-worn one-liners (t * (42 & t >> 10), t & t >> 8 ..): rhythms and arpeggios after Tejeez, xpansive's
 * "lost in space", a melody of t >> 9 modulo 13, textures of t % 24 and t % 19 */
static const uint8_t BYTE_T0[32] = {0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0, 16, 0, 7, 16, 0, 1, 8, 15, 8, 0, 15, 23, 8, 0};
static __attribute__((noinline)) uint32_t lofi_byte(uint32_t f, uint32_t t, uint32_t a)
{
    switch (f & 31u) {
    case 0: return (~t >> 2) * ((127u & t * (7u & t >> 10)) < (245u & t * (2u + (5u & t >> 14))));   /* (Tejeez) */
    case 1: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (5u + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (Tejeez) */
    case 2: return ((-t & 4095u) * (255u & t * (t & t >> 13)) >> 12) + (127u & t * (234u & t >> 8 & t >> 3) >> (3u & t >> 14));   /* (xpansive) */
    case 3: return t * (((t >> 9) ^ ((t >> 9) - 1u) ^ 1u) % 13u);
    case 4: return ((t >> 13 | t % 24u) & (t >> 7 | t % 19u)) << 3;
    case 5: return t * (t ^ (t + (t >> 15 | 1u)) ^ (((t - 1280u) ^ t) >> 10));
    case 6: return (t * (0xCA98u >> (t >> 9 & 14u) & 15u)) | t >> 8;
    case 7: return (t * (t >> 8 | t >> 9) & 46u & t >> 8) ^ ((t & t >> 13) | t >> 6);
    case 8: return (t >> 4) * (13u & 0x8898A989u >> (t >> 11 & 30u));
    case 9: return t * (t >> ((8u * ((t >> 15) | (t >> 8))) & 31u) & (20u | (((t >> 19) * 5u) >> (t & 31u)) | t >> 3));
    case 10: return (t & t % 255u) - (t * 3u & t >> 13 & t >> 6);
    case 11: return (t * (t >> 9 | t >> 13) & (t >> 6 | t >> 11) & 127u) + ((t >> 4) * (3u + (t >> 12 & 3u)) & 64u);
    case 12: return t + (t & (t ^ t >> 6)) - t * ((t >> 9) & (t % 16u ? 2u : 6u) & t >> 9);
    case 13: return t * (t >> 11 & t >> 8 & 123u & t >> 3);
    case 14: return (t * 9u & t >> 4) | (t * 5u & t >> 7) | (t * 3u & t >> 10);
    case 15: return (t * ((t & 4096u ? (t % 65536u < 59392u ? 7u : t & 7u) : 16u) + (1u & t >> 14)) >> (3u & -t >> (t & 2048u ? 2 : 10))) | t >> (t & 16384u ? (t & 4096u ? 10 : 3) : 2);   /* (Tejeez) */
    case 16: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (a + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (a: VAR) */
    case 17: return t * ((t >> (a & 15u) | t >> 8) & 63u & t >> 4);
    case 18: return (t * a & t >> 7) | (t * 3u & t >> 10);
    case 19: return (t >> a | t) * (t >> 9 & 7u);
    case 20: return t * (t >> 8 & a) ^ t >> 4;
    case 21: return (t * a & t >> 8) * (t >> 12 & 3u);
    case 22: return t * (0x9AD5u >> (t >> (8u + (a & 3u)) & 14u) & 15u);
    case 23: return (t ^ t >> a) * (t >> 10 & 5u);
    case 24: return t >> 2 & (t * a >> 6 | t >> 9);
    case 25: return (t | t >> a | t >> 7) * (t >> 13 & 7u);
    case 26: return t * ((t >> 11 & 3u) + a) & t >> 6;
    case 27: return (t * (a | 1u) % (257u - (t >> 10 & 127u))) & 255u;
    case 28: return t * (t >> 10 & 7u & a) | t >> 5;
    case 29: return (t >> 5 & t >> 7) * a + (t >> 9 & 32u);
    case 30: return t * (t >> 12 & a ? 3u : 2u) & t >> 6;
    default: return (t * a >> 4 | t >> 3) ^ (t >> 7 & t >> 11);
    }
}
/* (JIANT 0.5.1; 0.5.6 a floatbeat generator) WAVE = FLOAT: floatbeat, bytebeat's sibling in floats. Each ALGO is a
 * formula of t, as BYTE's, but made of sines: sin(t x (t >> 11 ^ t >> 13 & 7)), FM whose index walks with t's bits,
 * melodies read from nibble tables, kicks and noise hats inside the formula, folds and powers of sines .. rhythm,
 * melody and timbre at once. t runs 32 steps a cycle of the note (the formula in tune with the key, its rhythm faster
 * up the keyboard, as bytebeat's), with 8 bits of fraction for the sines (x = t in Q8; S(x) = sin(2 pi x / 32)).
 * The same t as BYTE's: MOTN runs its slow bits at their own speed, VAR mutates it (lofi_mut), BEND folds it, LOOP
 * holds it in a window; FBK (E5, 0.6.4) feeds the output back into t, from a shimmer to chaos; CHIP the bits, RES the
 * filter's */
static const char *const N_FLOAT[] = {"F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10", "F11",
                                      "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22",
                                      "F23", "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31", "F32", 0};
static const param_desc_t LOFI_FALGO = {"ALGO", F_INT, 0, 127, 64, N_FLOAT, 0};   /* = edit[2] (FLOAT) */
/* FLOAT CHIP: 4 bits, held 4 bits, FULL, 1 bit, or the existing STEP envelope. */
static const char *const N_FCHIP[] = {"4BIT", "4B/2", "FULL", "1BIT", "STEP"};
static const param_desc_t LOFI_FCHIP = {"CHIP", F_ENUM, 0, 4, 2, N_FCHIP, 0};    /* = edit[0] (FLOAT; 0.5.5: no STEP) */
/* (0.5.4) BYTE's and FLOAT's own second page: MOTN (E4, where CUT was: the time's speed, 64 as written), GRIT (BYTE,
 * E5: the samples held longer, a rate crusher), FBK (FLOAT, E5: the output back into the formula's time) */
static const param_desc_t LOFI_MOTN = {"MOTION", F_INT, 0, 127, 64, 0, 0};          /* = edit[4] (BYTE, FLOAT) */
static const param_desc_t LOFI_GRIT = {"GRIT", F_PCT, 0, 127, 0, 0, 0};           /* = edit[5] (BYTE) */
static const param_desc_t LOFI_FDCY = {"FEEDBK", F_PCT, 0, 127, 0, 0, 0};            /* = edit[5] (FLOAT, 0.6.4: was DRV, a
                                                         * fold that silenced it and doubled the FILTER page's DIST) */
/* MOTN 0..127 -> the time's speed, Q8: x1/16 .. x1 (64) .. x15.5, exponential */
static uint32_t lofi_motion(int32_t m)
{
    uint32_t e = (uint32_t)clamp(m, 0, 127) + 64u;
    return ((16u + (e & 15u)) << (e >> 4)) >> 4;
}
/* (0.5.4) VAR as a mutation of the time (BYTE's t, FLOAT's step count): v >> 4 picks the way, v & 15 how much. 0..15
 * the time as written; then stairs that transpose as it goes, xor and or with itself shifted, a bit dropped (holes in
 * the rhythm), the low bits reversed (the phrase backwards), multiplied (faster, higher), scrambled */
static uint32_t lofi_mut(uint32_t t, uint32_t v)
{
    uint32_t k = v & 15u, m;
    switch ((v >> 4) & 7u) {
    case 0: return t;
    case 1: return t + (t >> (9u - k / 2u)) * (k + 1u);
    case 2: return t ^ (t >> (2u + k / 2u));
    case 3: return t | (t >> (1u + k / 2u));
    case 4: return t & ~(1u << (2u + k / 2u));
    case 5: m = (1u << (3u + k / 2u)) - 1u; return (t & ~m) | (~t & m);
    case 6: return t * (3u + k);
    default: return t ^ ((t >> 3) * ((k + 1u) * 0x9E3779B1u >> 24));
    }
}
#define S(x) sine_i((uint32_t)(x) << 19)                 /* sin of x (t in Q8): a turn every 32 t */
#define PM(y, k) ((uint32_t)(((int32_t)(y) * (int32_t)(k)) >> 6))   /* a sine (Q15) times k: k / 16 of a turn */
/* the floatbeat formula f at t (its integer) and x (t in Q8); aux: a formula's own state (a kick's phase) */
static __attribute__((noinline)) int32_t lofi_float(uint32_t f, uint32_t t, uint32_t x, int32_t *aux)
{
    static const uint8_t R[8] = {16, 18, 20, 21, 24, 27, 30, 32};   /* just ratios, Q4: 1 9/8 5/4 4/3 3/2 5/3 15/8 2 */
    uint32_t st = t >> 11, fr = t & 2047u, h;
    int32_t E = 32767 - (int32_t)(fr << 4), a, b;          /* (a step's envelope: 1 .. 0) */
    switch (f & 31u) {
    case 0: return S(x * (((t >> 11) ^ (t >> 13)) & 7u) + x) * E >> 15;                       /* sin(t (t>>11 ^ t>>13 & 7)) */
    case 1: return S(x + PM(S(x * 3u), (int32_t)((t >> 10) & 15u) * 3));                      /* FM, its index walking */
    case 2: return S((x >> 4) * R[(0x36364689u >> ((st & 7u) * 4u)) & 7u]) * E >> 15;         /* a melody, a nibble table */
    case 3: return S(x * (1u + ((t >> 12) & 3u))) * S(x * (((t >> 9) & 7u) + 1u)) >> 15;      /* a ring, rhythmic */
    case 4: {                                                                                  /* a kick, a bass */
        uint32_t k = fr < 1024u ? (1024u - fr) * (1024u - fr) >> 6 : 0u;
        *aux = (int32_t)((uint32_t)*aux + k + 40u);
        a = (st & 1u) ? 0 : sine_i((uint32_t)*aux << 13) * (int32_t)(1024u - (fr < 1024u ? fr : 1024u)) >> 10;
        return (a + (S(x >> 1) >> 1)) >> 1;
    }
    case 5: return S(x * ((((t >> 9) & (t >> 11)) & 7u) | 1u)) * E >> 15;                     /* Sierpinski partials */
    case 6: return softclip(S(x * (1u + (st & 1u))) * (1 + (int32_t)((t >> 11) & 7u) * 3) >> 1);   /* drive, rhythmic */
    case 7: return S((x >> 4) * R[(st * 3u + (st >> 2)) & 7u] * (1u + (st & 1u)));            /* fifths and octaves */
    case 8: {                                                                                  /* two arpeggios */
        static const uint8_t H[8] = {2, 3, 4, 5, 6, 8, 5, 3};
        return (S(x * H[(t >> 9) & 7u] >> 1) + S(x * H[(t >> 11) & 7u] >> 2)) >> 1;
    }
    case 9: h = (t * 2654435761u) >> 17;                                                       /* a snare, a tone */
        a = (st & 1u) ? (int32_t)h - 16384 : 0;
        return (mulq15(a, E) + S(x)) >> 1;
    case 10: return S(x + PM(S(x * (((t >> 12) & 7u) + 1u)), E >> 10));                       /* a pluck of FM */
    case 11: return S(x + PM(S(t << 2), 6));                                                  /* a vibrato, deep (4 Hz) */
    case 12: return S((uint32_t)(S(x) * (2 + (int32_t)((t >> 11) & 7u))) >> 2);               /* folded, rhythmic */
    case 13: a = S(x * (1u + (st & 1u))); b = S((x >> 4) * (24u + (st & 6u) * 4u));           /* rectified, leaping */
             return (a < 0 ? -a : a) - (b < 0 ? -b : b);
    case 14: return S(x & ~((1u << (4u + (st & 7u))) - 1u));                                  /* a staircase sine */
    case 15: return S(x * (((t >> 8) % 7u) + 1u)) * E >> 15;                                  /* partials mod 7 */
    case 16: {                                                                                 /* three against four */
        int32_t e3 = 32767 - (int32_t)((t % 3072u) * 10u), e4 = E;
        return (mulq15(S(x * (1u + (st & 1u))), e4) + mulq15(S((x >> 4) * (24u + (st & 2u) * 8u)), e3 > 0 ? e3 : 0)) >> 1;
    }
    case 17: return S((x >> 6) * (64u + ((t >> 6) & 255u))) * E >> 15;                        /* a riser each bar */
    case 18: return mulq15(S(x * (1u + st % 3u)) * ((t >> 10) & 1u ? 1 : 0) + S(x + (t << 2)), 16384);   /* beating, gated */
    case 19: {                                                                                 /* odd partials, built up */
        uint32_t k, n = 1u + ((t >> 11) & 3u);
        for (a = 0, k = 0; k < n; k++)
            a += S(x * (2u * k + 1u)) / (int32_t)(2u * k + 1u);
        return a * 3 >> 2;
    }
    case 20: return S(x * (2u + ((st * 5u) & 7u)) >> 1);                                      /* leaps */
    case 21: {                                                                                 /* a drum loop */
        uint32_t k = fr < 768u ? (768u - fr) * (768u - fr) >> 6 : 0u;
        *aux = (int32_t)((uint32_t)*aux + k + 30u);
        a = (st & 3u) == 0u ? sine_i((uint32_t)*aux << 13) * (int32_t)(768u - (fr < 768u ? fr : 768u)) / 768 : 0;
        b = (t >> (8u + (st & 3u))) & 1u ? (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(1024u - (t & 1023u)) >> 12 : 0;
        return (a + b + (S(x >> 1) >> 2)) >> 1;
    }
    case 22: return S(x * (1u + (((t >> 9) & (t >> 7)) & 7u)) + (((t >> 9) & (t >> 7) & 255u) << 9));   /* bytebeat in it */
    case 23: return S(x * 2u + PM(S(x * 5u), (int32_t)((t >> 11) & 3u) * 8));                 /* FM 5:2, stepped */
    case 24: a = S(x * (1u + (st & 3u))); return (int32_t)((int64_t)a * (a < 0 ? -a : a) >> 15) * E >> 15;   /* squared */
    case 25: return S(x & ~(1u << (8u + (st & 7u)))) * E >> 15;                              /* a bit of t dropped: jumps */
    case 26: return S((x >> 4) * (34u - ((t >> 11) & 15u)) >> 1) * E >> 15;                   /* a descending line */
    case 27: return (S(x) + mulq15(S(x * (((t >> 13) & 3u) + 2u)), E)) >> 1;                   /* a partial, struck */
    case 28: return S(x + PM(S(x * 2u), S(t) >> 10));                                         /* FM, its index a 1 Hz sine */
    case 29: return S((x >> 4) * (16u + ((t >> 7) & 63u)));                                   /* a glide up partials */
    case 30: h = ((st + 1u) * 2654435761u) >> 29; return S((x >> 4) * R[h]) * E >> 15;        /* a random line */
    default: a = ((st * 5u) & 7u) < 5u ? E : 0;                                               /* euclid hats, a chord */
        b = (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(512u - (t & 511u)) >> 11;
        h = (x >> 4) * R[(st >> 1) & 7u] >> 4;                                                 /* (the chord's root walks) */
        return (S(h * 16u) + S(h * 20u) + S(h * 24u)) / 4 + (mulq15(b, a) >> 1);
    }
}
#undef S
#undef PM

/* Ported from juanjiant-bit/JIANT@d5f99ee2d4277adefcdf8f892e34133d33ec27a6,
 * GPL-3.0-only. Formula helpers retain the source's attribution. Felucca's
 * existing CHIP, presets and five legacy wave render paths are preserved. */
static const char *const N_BEND[] = {"OFF", "LIGHT", "MED", "DEEP"};
static const param_desc_t LOFI_BEND = {"BEND", F_ENUM, 0, 3, 0, N_BEND, 0};

/* The new modes have their own path and reuse only their own voice state.
 * E4=MOTION, E5=GRIT/FEEDBK, E6=BEND, E7=LOOP. E6 retains its stored 0..3
 * range; the formula bend uses shifts 13, 8, 3. No universal-filter change. */
static __attribute__((noinline)) void lofi_formula_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E1], chip = (uint32_t)p[P_E0], f = (uint32_t)p[P_E2] >> 2;
    uint32_t tt = (uint32_t)v->s[2], tf = v->ph[0] & 0xFFFFFFu, ts = v->ph[1], inc = pitch_inc(clamp(m->pitch16, 0, 2047));
    uint32_t ti = wave == RW_BYTE ? (inc >> 8) * 31u : inc >> 3, mul = lofi_motion(p[P_E4]);
    uint32_t var = (uint32_t)p[P_E3], lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : ~0u;
    uint32_t bs = p[P_E6] ? 18u - (uint32_t)p[P_E6] * 5u : 0u, t0 = wave == RW_BYTE ? (uint32_t)BYTE_T0[f & 31u] << 9 : 0u;
    int32_t held = v->s[0], cnt = v->s[1], aux = v->s[3], lp = v->s[4], l2 = (int32_t)v->ph[2];
    int32_t bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    int32_t hold = (1 + (chip == CHIP_4B2)) * (1 + (p[P_E5] >> 3));
    int32_t lpk = clamp(24000 + m->cutoff, 1000, 32767);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t te = mul == 256u ? tt : (tt & 255u) | ((ts >> 8) & ~255u);
        uint32_t tl = lofi_mut((te & lm) + t0, var), tw = bs ? tl ^ (tl >> bs) : tl;
        int32_t sample;
        if (wave == RW_BYTE) {
            if (--cnt <= 0) {
                sample = (int32_t)(lofi_byte(f, tw, 1u + (var >> 3)) & 255u) * 256 - 32640;
                if (bits < 8) { int32_t q = 1 << (16 - bits); sample = ((sample + q / 2) >> (16 - bits)) * q; }
                held = sample;
                cnt = hold;
            }
            aux += ((held >> 1) - aux) * 19500 >> 15;
            l2 += (aux - l2) * 19500 >> 15;
            sample = l2 * 2;
        } else {
            int32_t d = (held * p[P_E5]) >> 10;
            sample = lofi_float(f, tw + (uint32_t)(d >> 8), (tw << 8 | tf >> 16) + (uint32_t)d, &aux);
            if (chip != CHIP_8BIT) {
                uint32_t shift = chip == CHIP_1BIT ? 15u : 12u;
                int32_t q = 1 << shift;
                sample = ((sample + q / 2) >> shift) * q;
            }
            if (chip == CHIP_4B2 && (tt & 1u)) sample = held;
            held = sample;
        }
        lp += mulq15(soft_knee(sample, 20000) - lp, lpk);
        out[i] += voice_amp(lp, m, i);
        tf += ti; tt += tf >> 24; tf &= 0xFFFFFFu;
        ts += ((ti >> 8) * mul) >> 16;
    }
    v->s[0] = held; v->s[1] = cnt; v->s[2] = (int32_t)tt;
    v->s[3] = aux; v->s[4] = lp; v->ph[0] = tf; v->ph[1] = ts; v->ph[2] = (uint32_t)l2;
}

static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if (t->p[P_E1] >= RW_BYTE) {
        if (k == 0u && t->p[P_E1] == RW_FLOAT) return &LOFI_FCHIP;
        if (k == 2u) return t->p[P_E1] == RW_BYTE ? &LOFI_ALGO : &LOFI_FALGO;
        if (k == 3u) return &LOFI_VAR;
        if (k == 4u) return &LOFI_MOTN;
        if (k == 5u) return t->p[P_E1] == RW_BYTE ? &LOFI_GRIT : &LOFI_FDCY;
        if (k == 6u) return &LOFI_BEND;
        if (k == 7u) return &LOFI_LOOP;
    }
    if (k == 2u && t->p[P_E1] == RW_WRAM)
        return &LOFI_WAVNUM;
    if (k == 3u && t->p[P_E0] == CHIP_STEP)
        return &LOFI_DCY;
    return 0;
}

/* the stepped envelope: volume 15..0 in v->s[6] bits 0..3, its divider in bits 4..7, a 240 Hz
 * frame clock (Q16 phase in v->s[7]); each frame the divider counts down and, at 0, reloads the
 * period and takes one step off the volume (a looping one starts again at 15 after 0) */
#define FRAME_Q16 ((240u * CTL * 65536u) / FS)       /* frames per control tick, Q16 */
static int32_t step_period(int32_t b) { return b <= 16 ? b - 1 : b - 16; }

static void lofi_note_on(track_t *t, voice_t *v)
{
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = t->p[P_E1] >= RW_BYTE ? 0 : 0x7FFF;            /* LFSR */
    v->s[3] = 0;                 /* sweep, 1/16 st */
    v->s[4] = 0;                 /* 1-pole lp state */
    v->s[5] = t->p[P_E1] >= RW_BYTE ? (int32_t)(LOFI_FORMULA_STATE | 0x100u | (uint32_t)t->p[P_E1]) : 0; /* mode/retrigger marker, else arp counter */
    v->s[6] = 15 | (step_period(t->p[P_E3] >> 2) & 15) << 4;   /* stepped env: every note-on restarts it */
    v->s[7] = 0;
    if (t->p[P_E1] >= RW_BYTE) { v->ph[0] = v->ph[1] = v->ph[2] = 0; }
}

/* CHIP = STEP: the voice amplitude is the stepped envelope, not the ADSR curve. The ADSR only
 * gates: held, it is kept at full; after note-off the staircase keeps running (as the chip's
 * envelope never sees a note-off) and the voice is cut when the ADSR release has fallen to 1 %
 * (about the REL time; REL 0 cuts at once) or a one-shot decay has reached 0. */
static int32_t lofi_amp(track_t *t, voice_t *v, int32_t adsr)
{
    int32_t b = t->p[P_E3] >> 2, vol = v->s[6] & 15, div = (v->s[6] >> 4) & 15;
    if (t->p[P_E0] != CHIP_STEP)
        return adsr;
    if (!v->active)                                     /* the ADSR ended it (or the voice was given up) */
        return 0;
    if (v->stage == 3u) {
        if (v->env < (1 << 24) / 100 || (b >= 1 && b <= 16 && !vol)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
            return 0;
        }
    } else {
        v->env = 1 << 24;                               /* the release starts from full */
    }
    if (!b)
        return 15 * Q4;
    v->s[7] += (int32_t)FRAME_Q16;
    while (v->s[7] >= 65536) {
        v->s[7] -= 65536;
        if (div) {
            div--;
        } else {
            div = step_period(b);
            if (vol)
                vol--;
            else if (b > 16)
                vol = 15;
        }
    }
    v->s[6] = vol | div << 4;
    return vol * Q4;
}

/* Wave blend adapted from JIANT 0.6.45/0.6.50; legacy chip semantics retained. */
static inline int32_t lofi_morph_wave(uint32_t w, uint32_t ph, uint32_t duty, const uint8_t *wr,
                                      int32_t dc, uint32_t lfsr, uint32_t step, int32_t bits)
{
    switch (w) {
    case RW_TRI: {
        uint32_t k = ph >> 27;
        return step ? (int32_t)(k < 16u ? 15u - k : k - 16u) * (2 * Q4) - 15 * Q4 : osc_tri(ph);
    }
    case RW_SAW: return (int32_t)(ph >> 16) - 32768;
    case RW_NOIS: return (lfsr & 1u) ? 32767 : -32768;
    case RW_WRAM: {
        uint32_t k = ph >> 27, b = wr[k >> 1]; b = k & 1u ? b & 15u : b >> 4;
        return (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - dc;
    }
    default: return (ph < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
    }
}
static __attribute__((noinline)) void lofi_morph_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    static const uint8_t STEP_DUTY[4] = {1, 2, 4, 6};   /* eighths: 12.5 / 25 / 50 / 75 % */
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], wave = (uint32_t)p[P_E1], i;
    uint32_t stepc = chip == CHIP_STEP;
    uint32_t duty = stepc ? 0x20000000u * STEP_DUTY[(p[P_E2] / 32) & 3]
                          : 0x20000000u * (1u + (uint32_t)p[P_E2] / 32u);   /* 12.5 / 25 / 37.5 / 50 % */
    int32_t crush = stepc ? 0 : p[P_E3];                /* STEP: CRSH is the envelope */
    int32_t bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    int32_t hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    int32_t lpk = 3000 + (p[P_E7] << 8) + (m->cutoff > 0 ? m->cutoff : 0);
    static const int8_t ARPS[4][3] = {{0, 0, 0}, {0, 12, 0}, {0, 4, 7}, {0, 3, 7}};
    int32_t ar = p[P_E6];
    int32_t pitch, held = v->s[0], cnt = v->s[1], lp = v->s[4];   /* state in locals: out[] may alias v->s[] */
    uint32_t inc, ph0 = v->ph[0], lfsr = (uint32_t)v->s[2];
    const uint8_t *wr = WRAM[((uint32_t)p[P_E2] >> 3) & 15u];
    int32_t wdc = 0;
    if (wave == RW_WRAM) {                              /* the table's mean: no DC from lopsided shapes */
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;   /* (2 v - 15) Q4, or +-32767 at 1 bit */
    }
    /* sweep down, vibrato (uses the track LFO through m->pitch16 already) */
    if (v->s[3] < 4096)                                 /* past 2047 + an arp step the pitch is 0 anyway */
        v->s[3] += p[P_E4] / 8;
    v->s[5]++;
    pitch = m->pitch16 - v->s[3] + ARPS[ar & 3][(v->s[5] / 28) % 3] * 16;   /* ~50 Hz, chip-style */
    inc = pitch_inc(clamp(pitch, 0, 2047));
    if (p[P_E5])
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((osc_sine(v->ph[1]) >> 8) * p[P_E5]) >> 4)) >> 4);   /* no overflow */
    v->ph[1] += 0x01000000u;
    if (lpk > 32767)
        lpk = 32767;
    int32_t sh = m->shape - 64 * 256;
    int32_t ma = clamp((int32_t)t->mo_prev + sh, 0, 127 * 256) * 3 / 127;
    int32_t mb = clamp((int32_t)t->mo_cur + sh, 0, 127 * 256) * 3 / 127;
    int32_t mdq = (mb - ma) * 256 / (int32_t)(n ? n : 1u);
    /* WRAM can be a target from any legacy wave; always remove its mean. */
    if (wave != RW_WRAM) {
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            uint32_t q = bits < 16;                     /* quantise (the 4-bit tables are already) */
            cnt = hold;
            int32_t pos = clamp((int32_t)wave * 256 + ma + ((mdq * (int32_t)i) >> 8), 0, RW_WRAM * 256);
            uint32_t w = (uint32_t)pos >> 8, frac = (uint32_t)pos & 255u;
            if ((w == RW_NOIS || (frac && w + 1u == RW_NOIS)) && ((ph0 + inc * (uint32_t)hold) < ph0 || chip == 3u))
                lfsr = (lfsr >> 1) | (((lfsr ^ (lfsr >> 1)) & 1u) << 14);
            switch (w) {
            case RW_TRI:
                if (stepc) {                            /* 32 steps, 4 bits: 15..0, 0..15 */
                    uint32_t k = ph0 >> 27;
                    s = (int32_t)(k < 16u ? 15u - k : k - 16u) * (2 * Q4) - 15 * Q4;
                    q = 0;
                } else {
                    s = osc_tri(ph0);
                }
                break;
            case RW_SAW:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case RW_NOIS: {
                uint32_t l = lfsr;

                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            case RW_WRAM: {                             /* step-wise, no interpolation */
                uint32_t k = ph0 >> 27, b = wr[k >> 1];
                b = (k & 1u) ? b & 15u : b >> 4;
                s = (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - wdc;
                q = 0;
                break;
            }
            default:                                    /* minus its mean: no DC for narrow pulses */
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (frac) {
                int32_t b = lofi_morph_wave(w + 1u, ph0, duty, wr, wdc, lfsr, stepc, bits);
                s += (b - s) * (int32_t)frac >> 8;
                q = 1; /* quantise the blend with the selected chip resolution */
            }
            if (q)                                      /* rounded, not floored (floor = DC) */
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            if (crush > 64)
                s = (s >> 12) << 12;
            held = s;
        }
        ph0 += inc;
        lp += mulq15(held - lp, lpk);
        out[i] += voice_amp(lp, m, i);
    }
    v->ph[0] = ph0;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = lp;
}


static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t marker = (uint32_t)v->s[5], wave_now = (uint32_t)t->p[P_E1];
    if (wave_now >= RW_BYTE) {
        if ((marker & ~0x100u) != (LOFI_FORMULA_STATE | wave_now)) lofi_note_on(t, v);
        if ((uint32_t)v->s[5] & 0x100u) { /* voice.c keeps phases on retrigger; formulas restart their own clock */
            v->ph[0] = v->ph[1] = v->ph[2] = 0;
            v->s[5] = (int32_t)(LOFI_FORMULA_STATE | wave_now);
        }
        lofi_formula_render(t, v, out, n, m); return;
    }
    if ((marker & 0xFFFF0000u) == LOFI_FORMULA_STATE) { lofi_note_on(t, v); v->ph[0] = 0; }
    if (t->p[P_E1] <= RW_WRAM && (t->mo_cur || t->mo_prev)) {
        lofi_morph_render(t, v, out, n, m);
        return;
    }


    static const uint8_t STEP_DUTY[4] = {1, 2, 4, 6};   /* eighths: 12.5 / 25 / 50 / 75 % */
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], wave = (uint32_t)p[P_E1], i;
    uint32_t stepc = chip == CHIP_STEP;
    uint32_t duty = stepc ? 0x20000000u * STEP_DUTY[(p[P_E2] / 32) & 3]
                          : 0x20000000u * (1u + (uint32_t)p[P_E2] / 32u);   /* 12.5 / 25 / 37.5 / 50 % */
    int32_t crush = stepc ? 0 : p[P_E3];                /* STEP: CRSH is the envelope */
    int32_t bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    int32_t hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    int32_t lpk = 3000 + (p[P_E7] << 8) + (m->cutoff > 0 ? m->cutoff : 0);
    static const int8_t ARPS[4][3] = {{0, 0, 0}, {0, 12, 0}, {0, 4, 7}, {0, 3, 7}};
    int32_t ar = p[P_E6];
    int32_t pitch, held = v->s[0], cnt = v->s[1], lp = v->s[4];   /* state in locals: out[] may alias v->s[] */
    uint32_t inc, ph0 = v->ph[0], lfsr = (uint32_t)v->s[2];
    const uint8_t *wr = WRAM[((uint32_t)p[P_E2] >> 3) & 15u];
    int32_t wdc = 0;
    if (wave == RW_WRAM) {                              /* the table's mean: no DC from lopsided shapes */
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;   /* (2 v - 15) Q4, or +-32767 at 1 bit */
    }
    /* sweep down, vibrato (uses the track LFO through m->pitch16 already) */
    if (v->s[3] < 4096)                                 /* past 2047 + an arp step the pitch is 0 anyway */
        v->s[3] += p[P_E4] / 8;
    v->s[5]++;
    pitch = m->pitch16 - v->s[3] + ARPS[ar & 3][(v->s[5] / 28) % 3] * 16;   /* ~50 Hz, chip-style */
    inc = pitch_inc(clamp(pitch, 0, 2047));
    if (p[P_E5])
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((osc_sine(v->ph[1]) >> 8) * p[P_E5]) >> 4)) >> 4);   /* no overflow */
    v->ph[1] += 0x01000000u;
    if (lpk > 32767)
        lpk = 32767;
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            uint32_t q = bits < 16;                     /* quantise (the 4-bit tables are already) */
            cnt = hold;
            switch (wave) {
            case RW_TRI:
                if (stepc) {                            /* 32 steps, 4 bits: 15..0, 0..15 */
                    uint32_t k = ph0 >> 27;
                    s = (int32_t)(k < 16u ? 15u - k : k - 16u) * (2 * Q4) - 15 * Q4;
                    q = 0;
                } else {
                    s = osc_tri(ph0);
                }
                break;
            case RW_SAW:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case RW_NOIS: {
                uint32_t l = lfsr;
                if ((ph0 + inc * (uint32_t)hold) < ph0 || chip == 3u)
                    l = (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14);
                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            case RW_WRAM: {                             /* step-wise, no interpolation */
                uint32_t k = ph0 >> 27, b = wr[k >> 1];
                b = (k & 1u) ? b & 15u : b >> 4;
                s = (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - wdc;
                q = 0;
                break;
            }
            default:                                    /* minus its mean: no DC for narrow pulses */
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (q)                                      /* rounded, not floored (floor = DC) */
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            if (crush > 64)
                s = (s >> 12) << 12;
            held = s;
        }
        ph0 += inc;
        lp += mulq15(held - lp, lpk);
        out[i] += voice_amp(lp, m, i);
    }
    v->ph[0] = ph0;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = lp;
}

static const preset_t LOFI_PRESETS[] = {
    {"PULSE LD", {0, 0, 32, 0, 0, 20, 0, 127}, {0, 60, 90, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"WAVE BASS", {1, 1, 0, 0, 0, 0, 0, 90}, {0, 50, 70, 20}, 0, 1, FX(0, 0, 10, 0), PAT(2)},
    {"ARP 8BIT", {2, 0, 96, 0, 0, 0, 2, 110}, {0, 60, 80, 40}, 0, 0, FX(0, 0, 30, 20), PAT(13)},
    /* wave RAM VOXA (DUTY 92 / 8 = 11), a little vibrato */
    {"WAVE LEAD", {0, 4, 92, 0, 0, 18, 0, 110}, {0, 70, 90, 30}, 0, 1, FX(0, 0, 40, 25), PAT(3)},
    /* STEP: 25 % pulse, DCY 34 = D7 (a 15-step decay over 0.5 s), REL ~54 ms of staircase after note-off */
    {"STEP LEAD", {4, 0, 40, 34, 0, 16, 0, 127}, {0, 64, 127, 55}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"BYTEBEAT", {2, 5, 4, 0, 64, 0, 0, 0}, {0, 80, 110, 40}, 0, 1, FX(0, 0, 25, 20), PAT(4)},
    {"FLOAT FM", {2, 6, 4, 0, 64, 0, 0, 0}, {0, 60, 90, 40}, 0, 1, FX(0, 15, 25, 25), PAT(4)},
};

static const engine_t ENG_LOFI = {
    .name = "LOFI",
    .page_title = {"CHIP", "MOTN"},
    .edit = {
        {"CHIP", F_ENUM, 0, 4, 0, N_CHIP, 0},
        {"WAVE", F_ENUM, 0, 6, 0, N_RWAVE, 0},
        {"DUTY", F_INT, 0, 127, 64, 0, 0},              /* WRAM: the table, DUTY / 8 (lofi_desc) */
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},               /* STEP: the envelope (lofi_desc) */
        {"SWP", F_PCT, 0, 127, 0, 0, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"ARP", F_ENUM, 0, 3, 0, N_RARP, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
    },
    .presets = LOFI_PRESETS,
    .npresets = NELEM(LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x11,                /* the held sample and the low-pass */
    .amp = lofi_amp,
    .desc = lofi_desc,
};
