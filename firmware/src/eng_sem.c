/* SPDX-License-Identifier: GPL-3.0-only */
/* SEM-inspired virtual analogue; original DSP, not a circuit-identical model.
 * Two independently selected BLEP oscillators and a two-pole TPT state-variable
 * filter. STATE blends LP -> notch -> HP; TYPE also selects BP or fixed HP.
 * All state is in the allocated voice, including the slow local random drift:
 * no new audio buffers, shared RNG or cross-core writable scratch. */
static const char *const N_SEM_WAVE[] = {"SAW", "PULSE", "TRI", "PWM"};
static const char *const N_SEM_TYPE[] = {"STATE", "BP", "HP"};
static const param_desc_t SEM_TYPE = {"TYPE", F_ENUM, 0, 2, 0, N_SEM_TYPE, 0};
static int32_t sem_state(int32_t v)
{
    return v <= 64 ? v * 256 : 16384 + (v - 64) * 16384 / 63;
}

static void sem_note_on(track_t *t, voice_t *v)
{
    uint32_t lane = (uint32_t)(v - t->v), part = (uint32_t)(t - trk);
    uint32_t seed = 0x6D2B79F5u ^ ((part * NVOICE + lane + 1u) * 0x9E3779B9u);
    v->ph[1] = v->ph[0] + 0x40000000u;
    v->s[0] = v->s[1] = v->s[4] = 0;
    v->s[2] = (int32_t)(seed ^ (v->age * 0x85EBCA6Bu));
    if (!v->s[2]) v->s[2] = 1;
    v->s[3] = 0;
    v->s[5] = (int32_t)(noise32(&v->s[2]) >> 16) - 32768;
    v->s[6] = sem_state(t->p[P_E6]);
    v->s[7] = (int32_t)(seed >> 24) - 128;  /* fixed calibration of this physical voice slot */
}

static inline __attribute__((always_inline)) int32_t sem_osc(uint32_t wave, uint32_t ph, uint32_t inc, uint32_t pw)
{
    if (wave == 2u) return osc_tri(ph);
    if (wave == 1u || wave == 3u)
        return osc_pulse(ph, inc, wave == 3u ? pw : 0x80000000u);
    return osc_saw(ph, inc);
}

/* Small tuning errors as a relative Q20 increment correction. At full VINT,
 * fixed calibration is about +/-2 cents and slow drift about +/-2 cents.
 * The multiply is split so even MIDI 127 stays inside signed 32-bit range. */
static uint32_t sem_tune(uint32_t inc, int32_t q20)
{
    return inc + (uint32_t)((int32_t)(inc >> 11) * q20 / 512);
}

static void sem_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t vint = p[P_E7], drift, fixed, cut, k;
    int32_t state0 = v->s[6], state1, delta;
    int32_t ic1 = v->s[0], ic2 = v->s[1];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], i, ft = (uint32_t)p[P_FTYPE];
    uint32_t w0 = (uint32_t)p[P_E0], w1 = (uint32_t)p[P_E1];
    uint32_t pw = (uint32_t)clamp(m->shape, 8 * 256, 120 * 256) << 17;
    int32_t mix = p[P_E3] * 258, gain0 = 32767 - mix;
    tsvf_t f;
    if (++v->s[3] >= 1024) {
        v->s[3] = 0;
        v->s[5] = (int32_t)(noise32(&v->s[2]) >> 16) - 32768;
    }
    v->s[4] += (v->s[5] - v->s[4]) / 256;
    drift = (v->s[4] * vint / 127) / 27;
    fixed = v->s[7] * vint * 10 / 127;
    uint32_t inc0 = sem_tune(m->inc, fixed + drift);
    uint32_t inc1 = sem_tune(cents_inc(m->pitch16, p[P_E2], m->fine), -fixed + drift / 2);
    cut = p[P_E4] * 256 + m->cutoff + (v->s[7] * vint / 64);
    k = 8192 - p[P_E5] * 6500 / 127;  /* finite resonance: no self oscillation */
    tsvf_coef_k(&f, cut, k);
    /* Bound a live STATE jump to three knob steps per block and ramp each sample. */
    state1 = state0 + clamp(sem_state(p[P_E6]) - state0, -768, 768);
    v->s[6] = state1;
    delta = (state1 - state0) / (int32_t)(n ? n : 1u);
    for (i = 0; i < n; i++) {
        int32_t a = sem_osc(w0, ph0, inc0, pw), b = sem_osc(w1, ph1, inc1, pw);
        int32_t in = (mulq15(a, gain0) + mulq15(b, mix)) >> 1;
        int32_t v3 = in - ic2;
        int32_t bp = (f.a1 * ic1 + f.a2 * v3) >> 13;
        int32_t lp = ic2 + ((f.a2 * ic1 + f.a3 * v3) >> 13);
        int32_t hp = in - ((k * bp) >> 12) - lp, s;
        ic1 = clamp(2 * bp - ic1, -150000, 150000);
        ic2 = clamp(2 * lp - ic2, -150000, 150000);
        ph0 += inc0; ph1 += inc1;
        if (ft == 1u) s = bp;
        else if (ft == 2u) s = hp;
        else {
            int32_t state = state0 + delta * (int32_t)(i + 1u);
            /* Q14: exact LP at 0, LP+HP notch at 64, exact HP at 127. */
            s = state <= 16384 ? lp + ((hp * state) >> 14)
                              : hp + ((lp * (32768 - state)) >> 14);
        }
        out[i] += voice_amp(soft_knee(s, 16000) * 2, m, i) * 2;
    }
    v->ph[0] = ph0; v->ph[1] = ph1;
    v->s[0] = ic1; v->s[1] = ic2;
}

static const preset_t SEM_PRESETS[] = {
    {"SEM BRASS", {0, 0, 8, 64, 48, 24, 0, 28}, {12, 76, 88, 48}, 36, 0, FX(0, 12, 12, 28), PAT(6)},
    {"WARM PAD", {0, 0, 14, 60, 64, 14, 0, 44}, {76, 90, 108, 94}, 12, 0, FX(0, 38, 16, 56), PAT(5)},
    {"OB STRING", {3, 0, 10, 64, 82, 12, 0, 38}, {58, 88, 112, 84}, 8, 0, FX(0, 44, 12, 46), PAT(5)},
    {"ROUND BAS", {1, 0, 3, 38, 38, 34, 0, 18}, {0, 70, 58, 34}, 42, 1, FX(0, 0, 0, 10), PAT(2)},
    {"NOTCH PAD", {0, 3, 12, 64, 70, 32, 64, 32}, {72, 90, 110, 96}, 14, 0, FX(0, 32, 22, 54), PAT(5)},
    {"SEM LEAD", {0, 1, 6, 58, 76, 38, 0, 24}, {2, 72, 100, 44}, 22, 1, FX(0, 10, 34, 24), PAT(4)},
    {"HIGH AIR", {0, 0, 18, 64, 60, 20, 127, 48}, {82, 92, 112, 98}, -10, 0, FX(0, 36, 18, 64), PAT(5)},
    {"SOFT KEY", {2, 0, 5, 34, 72, 16, 0, 16}, {2, 76, 38, 62}, 18, 0, FX(0, 14, 24, 26), PAT(6)},
};

static const engine_t ENG_SEM = {
    .name = "SEM", .page_title = {"OSC", "SEM FLT"},
    .edit = {
        {"OSC1", F_ENUM, 0, 3, 0, N_SEM_WAVE, 0},
        {"OSC2", F_ENUM, 0, 3, 0, N_SEM_WAVE, 0},
        {"DTN", F_INT, 0, 50, 8, 0, "ct"},
        {"MIX", F_PCT, 0, 127, 64, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 80, 0, 0},
        {"RES", F_PCT, 0, 127, 24, 0, 0},
        {"STATE", F_PCT, 0, 127, 0, 0, 0},
        {"VINT", F_PCT, 0, 127, 24, 0, 0},
    },
    .presets = SEM_PRESETS, .npresets = NELEM(SEM_PRESETS),
    .knob = {P_E4, P_E5, P_E6, P_E7},
    .note_on = sem_note_on, .render = sem_render, .keep = 0xF3,
};
_Static_assert(NELEM(SEM_PRESETS) <= 8u, "SEM favorites occupy one reserved byte");
