/* SPDX-License-Identifier: GPL-3.0-only */
/* ENSEMBLE: a per-track string / choir / dark texture engine with a built-in multi-tap ensemble. Original DSP, inspired by the
 * string-machine idea (sawtooth registers into a chorus-like ensemble); not an emulation of any instrument or circuit.
 *
 * Voice (ens_render, per sounding voice): the registers 16' / 8' / 4' (a triangle an octave down, a band-limited saw, a saw an
 * octave up locked to it), a second detuned oscillator for the dark end, a breath noise for the choir, then ONE trapezoidal
 * state-variable filter whose cutoff, damping and low-pass / band-pass mix follow MORPH: STRINGS (open low-pass, TONE sets
 * the brightness) -> CHOIR (a resonant formant band, TONE moves the vowel) -> DARK (low-pass, rough ring-modulated pulse, harder
 * knee). The voice state is the host's voice_t (ph[0..2], s[0..7]): no engine memory per voice, no shared mutable state.
 *
 * Ensemble (ens_post, per track, after the voices are mixed and BEFORE the DIST / INSERT / FILTER / sends): three delay taps of
 * 6.0 / 8.5 / 11.0 ms read from one ring in the part's engine memory (ENG_MEM, the pool region every part already has),
 * each moved by a slow LFO (taps 120 degrees apart) plus a fast one, with linear interpolation; MIX crosses dry and ensemble.
 * The cost grows with the number of tracks on this engine, not with their voices; the REV send, STASIS and HALL hear the
 * ensembled sound; the global CHORUS stays free. The ring is the part's engine memory: an engine switch zeroes it
 * (eng_mem_claim), and ens_post leaves anything it does not own alone.
 *
 * Stereo (0.11.0): SPRD is the ensemble's width. Six taps read the same ring: taps 0..2 (the three above) are the left channel, taps 3..5 (6.8 / 10.1 / 12.7 ms, their own slow and fast
 * LFO rates, 60 degrees on) the right. While the part is POLY or UNISON with SPRD above 0 (fx.c mix_part_inner, T_SPREAD 2), ens_post writes d = left - right to the part's side buffer
 * and b becomes the mean of the two channels, the track's mono sum (the sends take it): L = gl (b + d SPRD / 256), R = gr (b - d SPRD / 256) in fx.c mix_spread, so every channel is
 * an ensemble of the three-tap level and the mono sum is about 3 dB below the mono ensemble's (the two channels are only partly correlated; this does not depend on SPRD). With SPRD 0, MONO
 * or LEGATO ens_post reads the three left taps only and is the engine of 0.10.1 bit for bit (the right group's LFOs run on, so raising SPRD does not jump); MIX 0 is the dry voices
 * in either case. The ring is the same; ens_t grows by 28 B inside the part's engine memory. */
#define ENS_NT 6u                          /* the taps: 0..2 left, 3..5 right */
#define ENS_RING 1024u                     /* ring length in samples: a power of two; 23.2 ms at 44.1 kHz */
#define ENS_TAIL_BLOCKS 28u                /* blocks (CTL samples) the taps keep running after the last voice: 20 ms > the longest tap */
#define ENS_WET_Q16 26000u                 /* the three taps' mean (1/3) with 1.19 x make-up: the taps' comb takes about 1.5 dB off a sustained chord on average */
#define ENS_DELAY_BASE_0 265               /* the taps' resting delays in samples: 6.0, 8.5, 11.0 ms */
#define ENS_DELAY_BASE_1 375
#define ENS_DELAY_BASE_2 485
#define ENS_MID_Q16 (ENS_WET_Q16 >> 1)     /* stereo: the mean of both channels' three taps (each channel at the level of the mono ensemble) */
static const int32_t ENS_BASE[ENS_NT] = {ENS_DELAY_BASE_0, ENS_DELAY_BASE_1, ENS_DELAY_BASE_2, 300, 445, 560};   /* the right channel's taps: 6.8 10.1 12.7 ms, irregular against the left's */

typedef struct {
    int32_t ring[ENS_RING];                /* the input (the track's voices, mono), 4096 B */
    uint32_t w;                            /* write position */
    uint32_t slow[ENS_NT];                 /* the slow LFO of each tap (phase) */
    uint32_t fast[2];                      /* the fast LFO of each channel (phase) */
    int32_t delay[ENS_NT];                 /* each tap's delay in 1/256 sample, as of the last block */
    uint32_t live;                         /* 1 after the first block (the delays start at their targets) */
    uint32_t seen;                         /* ens_blocks at the last block this part ran: a gap (another engine had the part) clears the ring */
} ens_t;
static uint32_t ens_blocks;                /* counts the mix's blocks (fx.c mix_block): only ens_post reads it */
ENG_MEM_FITS("ENSEMBLE", sizeof(ens_t));

static void ens_note_on(track_t *t, voice_t *v)
{
    uint32_t lane = (uint32_t)(v - t->v), part = (uint32_t)(t - trk);
    uint32_t seed = 0x1B873593u ^ ((part * NVOICE + lane + 1u) * 0x9E3779B9u);
    v->ph[1] = v->ph[0] + 0x55555555u;     /* the dark end's second oscillator, detuned in render */
    v->ph[2] = v->ph[0];
    v->s[0] = v->s[1] = v->s[4] = 0;
    v->s[2] = (int32_t)(seed ^ (v->age * 0x85EBCA6Bu));
    if (!v->s[2]) v->s[2] = 1;
    v->s[3] = 0;
    v->s[5] = (int32_t)(noise32(&v->s[2]) >> 16) - 32768;
    v->s[6] = 0;
    v->s[7] = (int32_t)(seed >> 24) - 128;  /* a fixed calibration of this physical voice slot: a few cents */
}

/* the filter's resting position (an index into the cutoff table, 0..127) for each of the three timbres */
static inline __attribute__((always_inline)) int32_t ens_idx(int32_t base, int32_t tone, int32_t num, int32_t den)
{
    return base + tone * num / den;
}

static CARAVEL_AUDIO_HOT void ens_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t reg = p[P_E0], morph = p[P_E1], tone = p[P_E2], drift = p[P_E3], tex = p[P_E6];
    int32_t wS = morph < 64 ? (64 - morph) * 512 : 0, wD = morph > 64 ? (morph - 64) * 520 : 0;
    int32_t wC = 32768 - (morph > 64 ? morph - 64 : 64 - morph) * 512;
    int32_t g4 = (reg < 64 ? reg : 64) * 160, g16 = reg > 64 ? (reg - 64) * 160 : 0, g8 = 14000;
    int32_t ic1 = v->s[0], ic2 = v->s[1], q20, cut, k, gl, gb, knee, rough, breath;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], i;
    uint32_t pw = (uint32_t)clamp(m->shape, 8 * 256, 120 * 256) << 17;
    uint32_t inc0, inc1, inc2;
    tsvf_t f;
    wS = clamp(wS, 0, 32767); wD = clamp(wD, 0, 32767); wC = clamp(wC, 0, 32767);
    if (++v->s[3] >= 1024) {                /* a new drift target about every 0.7 s, then a one-pole glide to it */
        v->s[3] = 0;
        v->s[5] = (int32_t)(noise32(&v->s[2]) >> 16) - 32768;
    }
    v->s[4] += (v->s[5] - v->s[4]) / 256;
    /* tuning errors as a relative Q20 increment (as SEM): at full DRIFT about +-7 cents of drift, +-1.5 cents of fixed calibration */
    q20 = (v->s[4] * drift / 127) / 8 + v->s[7] * drift / 127 * 3;
    inc0 = m->inc + (uint32_t)((int32_t)(m->inc >> 11) * q20 / 512);
    inc1 = inc0 + (inc0 >> 6);              /* the second oscillator: about 27 cents up */
    inc2 = inc0 >> 1;
    /* one filter, three timbres: cutoff (index << 8) and damping blend by the MORPH weights */
    cut = (mulq15(wS, ens_idx(62, tone, 57, 127) << 8) + mulq15(wC, ens_idx(58, tone, 32, 127) << 8)
           + mulq15(wD, ens_idx(44, tone, 51, 127) << 8)) + m->cutoff;
    cut += mulq15(wS + wD, clamp(m->pitch16 - 768, -768, 768) * 5);   /* a little key tracking for the strings and the dark end */
    k = mulq15(wS, 6000) + mulq15(wC, 1400) + mulq15(wD, 2600);
    k = clamp(k, 900, 8192);
    tsvf_coef_k(&f, cut, k);
    gl = mulq15(wS + wD, 32767) + mulq15(wC, 11000);   /* low-pass share */
    gb = mulq15(wC, 29000);                            /* band-pass share (normalised below) */
    knee = 16000 - tex * 60;
    rough = mulq15(wD, 8192 + tex * 160);
    breath = mulq15(wC, 2200 + tex * 12);
    for (i = 0; i < n; i++) {
        int32_t a = osc_saw(ph0, inc0), b = osc_saw(ph0 << 1, inc0 << 1), c = osc_tri(ph2);
        int32_t x = (a * g8 + b * g4 + c * g16) >> 15, v3, bp, lp, y;
        if (rough)
            x += mulq15(mulq15(a, osc_pulse(ph1, inc1, pw)), rough);
        if (breath)
            x += (((int32_t)(noise32(&v->s[2]) >> 17) - 16384) * breath) >> 15;
        v3 = x - ic2;
        bp = (f.a1 * ic1 + f.a2 * v3) >> 13;
        lp = ic2 + ((f.a2 * ic1 + f.a3 * v3) >> 13);
        ic1 = clamp(2 * bp - ic1, -150000, 150000);
        ic2 = clamp(2 * lp - ic2, -150000, 150000);
        y = mulq15(lp, gl) + mulq15((bp * k) >> 12, gb);
        out[i] += voice_amp(soft_knee(y, knee) * 2, m, i) * 2;
        ph0 += inc0; ph1 += inc1; ph2 += inc2;
    }
    v->ph[0] = ph0; v->ph[1] = ph1; v->ph[2] = ph2;
    v->s[0] = ic1; v->s[1] = ic2;
}

/* the ensemble: b = the track's mono mix of this block (n <= CTL samples), processed in place; called by fx.c mix_part_inner for a part
 * whose engine is ENSEMBLE, also during the tail after its last voice. sd: the part's side buffer when it is stereo (SPRD above 0, POLY / UNISON), else 0 (the mono ensemble) */
static CARAVEL_AUDIO_HOT __attribute__((noinline)) void ens_post(track_t *t, int32_t *b, uint32_t n, int32_t *sd)
{
    uint32_t part = (uint32_t)(t - trk), j, i;
    ens_t *s;
    const int16_t *p = t->p;
    int32_t depth = p[P_E4], mix = p[P_E7] * 516, tgt[ENS_NT], step[ENS_NT];
    uint32_t slow_inc, fast_inc, slow_inc_b, fast_inc_b;
    if (part >= NPART || eng_mem_owner[part] != ENGI_ENSEMBLE + 1u)
        return;
    s = (ens_t *)ENG_MEM(part);
    if (s->seen != ens_blocks && s->seen + 1u != ens_blocks) {   /* not the block after our last one: the engine was away (SEM and the other engines with no
                                                                   * memory leave the region as it is): no old sound from the ring */
        memset(s, 0, sizeof *s);
    }
    s->seen = ens_blocks;
    slow_inc = 19500u + (uint32_t)p[P_E5] * 1075u;      /* 0.2 .. 1.6 Hz */
    fast_inc = 584000u + (uint32_t)p[P_E5] * 2000u;     /* 6 .. 8.6 Hz */
    slow_inc_b = (slow_inc * 165u) >> 7;                /* the right channel's taps: 1.29 x the slow and 1.17 x the fast rate of the left's */
    fast_inc_b = (fast_inc * 150u) >> 7;
    if (!s->live) {                                     /* the slow LFOs start 120 degrees apart (they all started at 0 before 0.10.1 and moved the taps together) */
        for (j = 0; j < ENS_NT; j++)
            s->slow[j] = (j % 3u) * 0x55555555u + (j / 3u) * 0x2AAAAAAAu;   /* (the right channel's group 60 degrees on) */
        s->fast[1] = 0x2AAAAAAAu;
    }
    for (j = 0; j < ENS_NT; j++) {
        int32_t lfo = (sine_i(s->slow[j]) * 3 + sine_i(s->fast[j / 3u] + (j % 3u) * 0x55555555u)) >> 2;   /* +-32767: mostly the slow one */
        s->slow[j] += (j < 3u ? slow_inc : slow_inc_b) * n;
        tgt[j] = (ENS_BASE[j] << 8) + ((lfo * depth) >> 7);    /* 1/256 sample: depth 127 moves a tap +-127 samples (+-2.9 ms) */
    }
    s->fast[0] += fast_inc * n;
    s->fast[1] += fast_inc_b * n;
    if (!s->live) {
        for (j = 0; j < ENS_NT; j++)
            s->delay[j] = tgt[j];
        s->live = 1;
    }
    for (j = 0; j < ENS_NT; j++)
        step[j] = (tgt[j] - s->delay[j]) / (int32_t)(n ? n : 1u);
    for (i = 0; i < n; i++) {
        int32_t dry = b[i], sum = 0;
        s->ring[s->w] = dry;
        for (j = 0; j < 3u; j++) {
            int32_t d = s->delay[j] + step[j] * (int32_t)(i + 1u);
            uint32_t di = (uint32_t)d >> 8, idx0 = (s->w - di) & (ENS_RING - 1u), idx1 = (idx0 - 1u) & (ENS_RING - 1u);
            int32_t r0 = s->ring[idx0], r1 = s->ring[idx1];
            sum += r0 + (((r1 - r0) * (d & 255)) >> 8);
        }
        if (sd) {                                       /* the right channel: three more taps; b = the mean of both, sd = left - right */
            int32_t sumr = 0;
            for (j = 3; j < ENS_NT; j++) {
                int32_t d = s->delay[j] + step[j] * (int32_t)(i + 1u);
                uint32_t di = (uint32_t)d >> 8, idx0 = (s->w - di) & (ENS_RING - 1u), idx1 = (idx0 - 1u) & (ENS_RING - 1u);
                int32_t r0 = s->ring[idx0], r1 = s->ring[idx1];
                sumr += r0 + (((r1 - r0) * (d & 255)) >> 8);
            }
            s->w = (s->w + 1u) & (ENS_RING - 1u);
            b[i] = dry + mulq16(mulq16(sum + sumr, ENS_MID_Q16) - dry, (uint32_t)mix);
            sd[i] = mulq16(mulq16(sum - sumr, ENS_WET_Q16), (uint32_t)mix);
            continue;
        }
        s->w = (s->w + 1u) & (ENS_RING - 1u);
        b[i] = dry + mulq16(mulq16(sum, ENS_WET_Q16) - dry, (uint32_t)mix);   /* dry + (ensemble - dry) * MIX */
    }
    for (j = 0; j < ENS_NT; j++)
        s->delay[j] += step[j] * (int32_t)n;
}

/* The factory presets are design intent: eight controls, the ADSR and the sends chosen for a different job each; tests/ensemble_presets_test measures them
 * (spectrum, level movement, attack, release, line smearing) and keeps every pair a minimum distance apart. Not a verdict on how they sound.
 * Order is the stored order: favorites and projects address a preset by its index, so new ones go to the end. */
/* the width (SPRD) a factory preset loads with (ui.c preset_sound_values; preset_t has no field for it), in the order of ENS_PRESETS below; 0 = mono. The pads (PRESET_CAT 'P') get the widest of 127 /
 * 112 / 96 at which no octave band between 125 Hz and 8 kHz of the chords E2 B2 E3 G3 / E3 G3 B3 E4 is anti-correlated (the lowest band correlation of the two chords above -0.05, measured on the host
 * with the mono sum of the mean of both channels): 127 for seven of the eleven, 112 for STRING MACHINE and DEEP DRONE, 96 for BREATH CHOIR and HUGE ENSEMBLE. Not the FX (ROUGH PULSE), the struck
 * GLASS SHIMMER (anti-correlated at every width above 80: a bell does not need a pad's image) or SOLO DRIFT (a MONO preset: ensemble stereo needs POLY or UNISON). A user preset and a project store
 * their own SPRD, and what they store wins; only a factory load sets this */
static const uint8_t ENS_WIDTH[14] = {127, 127, 127, 127, 127, 112, 96, 112, 0, 127, 0, 0, 96, 127};
static inline int16_t ens_width(uint32_t pi) { return pi < 14u ? ENS_WIDTH[pi] : 0; }
static const preset_t ENS_PRESETS[] = {
    {"VINTAGE STRINGS", {64, 0, 80, 40, 70, 50, 10, 100}, {60, 90, 112, 80}, 0, 0, FX(0, 12, 10, 50), PAT(5)},
    {"GLASS CHOIR", {40, 64, 60, 50, 60, 40, 15, 90}, {80, 90, 115, 95}, 0, 0, FX(0, 10, 20, 60), PAT(5)},
    {"DUSK ORCHESTRA", {100, 40, 70, 60, 85, 35, 30, 100}, {75, 95, 115, 90}, 0, 0, FX(0, 14, 24, 56), PAT(5)},
    {"ABYSS PAD", {80, 110, 40, 90, 75, 20, 90, 95}, {100, 100, 120, 105}, 0, 0, FX(0, 10, 30, 70), PAT(5)},
    /* PROFILE A: the controls were set by measurement to the profile of a reference recording (spectral centroid, four band levels, level movement); the ADSR and the
     * sends are this table's choice. No audio from the recording is stored anywhere. The others, by role: a classic string machine, a breathy choir, a very low
     * drone, a struck glass / bell tone (MORPH DARK with TEXT: the ring-modulated pulse is inharmonic), a swell that opens with ENV > FILTER, a rough short
     * pulse, a MONO solo with full DRIFT, the deepest ensemble, a pad with the fastest RATE */
    {"PROFILE A", {120, 15, 80, 60, 70, 71, 31, 126}, {85, 90, 112, 95}, 0, 0, FX(0, 8, 8, 40), PAT(5)},
    {"STRING MACHINE", {64, 0, 127, 25, 100, 22, 0, 127}, {85, 90, 115, 95}, 0, 0, FX(0, 12, 12, 45), PAT(5)},
    {"BREATH CHOIR", {64, 64, 90, 35, 40, 45, 127, 100}, {90, 95, 115, 100}, 0, 0, FX(0, 6, 20, 65), PAT(5)},
    {"DEEP DRONE", {127, 0, 0, 60, 80, 8, 0, 110}, {105, 100, 120, 115}, 0, 0, FX(0, 8, 24, 75), PAT(5)},
    {"GLASS SHIMMER", {64, 127, 127, 15, 10, 60, 127, 100}, {55, 70, 60, 100}, 0, 0, FX(0, 4, 30, 70), PAT(5)},
    {"SLOW SWELL", {90, 10, 70, 40, 80, 10, 0, 100}, {120, 100, 120, 112}, 40, 0, FX(0, 10, 16, 60), PAT(5)},
    {"ROUGH PULSE", {127, 127, 110, 50, 70, 110, 127, 127}, {50, 70, 70, 60}, 0, 0, FX(10, 6, 10, 30), PAT(5)},
    {"SOLO DRIFT", {70, 20, 95, 127, 25, 60, 0, 100}, {60, 90, 112, 70}, 0, 1, FX(0, 8, 28, 40), PAT(5)},
    {"HUGE ENSEMBLE", {64, 0, 110, 50, 127, 6, 0, 127}, {70, 90, 115, 100}, 0, 0, FX(0, 0, 10, 50), PAT(5)},
    {"RUNNING PAD", {40, 30, 100, 40, 127, 127, 0, 127}, {95, 90, 115, 100}, 0, 0, FX(0, 6, 12, 45), PAT(5)},
};

static const engine_t ENG_ENSEMBLE = {
    .name = "ENSEMB", .page_title = {"TIMBRE", "ENSEMBLE"},
    .edit = {
        {"REG", F_PCT, 0, 127, 64, 0, 0},
        {"MORPH", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 80, 0, 0},
        {"DRIFT", F_PCT, 0, 127, 30, 0, 0},
        {"ENS", F_PCT, 0, 127, 64, 0, 0},
        {"RATE", F_PCT, 0, 127, 50, 0, 0},
        {"TEXT", F_PCT, 0, 127, 10, 0, 0},
        {"MIX", F_PCT, 0, 127, 100, 0, 0},
    },
    .presets = ENS_PRESETS, .npresets = NELEM(ENS_PRESETS),
    .knob = {P_E1, P_E4, P_E2, P_E7},
    .poly = 6,
    .note_on = ens_note_on, .render = ens_render, .keep = 0xF3,
    .mem = sizeof(ens_t),
};
_Static_assert(NELEM(ENS_PRESETS) <= 32u, "ENSEMBLE favorites are one 32-bit word (ENGINE_FAV_PRESETS)");
