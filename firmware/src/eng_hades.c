/* SPDX-License-Identifier: GPL-3.0-only */
/* Hades RE-inspired architecture, original DSP; not an endorsed or measured
 * circuit emulation. Controls and limits: docs/engines/hades-engine.md.
 * All runtime state fits voice_t; no additional audio buffers or global RNG.
 * s[0,1,4] filter, s[5] bass shelf; packed controls in s[2,3,6,7]. */
static const char *const N_HADES_WAVE[] = {"OFF", "SAW", "PULSE"};
static void hades_note_on(track_t *t, voice_t *v)
{
    /* voice_start clears env_out for a fresh voice. A sounding retrigger must
     * also retain smoothing in slots 2/3, which the host keep mask omits. */
    if (v->env_out) return;
    v->s[0] = v->s[1] = v->s[4] = v->s[5] = 0;
    v->s[2] = va_pair(t->p[P_E2] * 258, t->p[P_E3] * 258);
    v->s[3] = va_pair(t->p[P_E6] * 258, t->p[P_E5] * 258);
    v->s[6] = va_pair(clamp(t->p[P_E1] * 256, 8 * 256, 120 * 256), t->p[P_E7] * 258);
    v->s[7] = va_pair(t->p[P_E4] * 256, t->p[P_E0] * 16384);
}
static void hades_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (!n) return;
    const int16_t *p = t->p;
    int32_t z[3] = {v->s[0], v->s[1], v->s[4]}, bass = v->s[5];
    int32_t a[8] = {va_lo(v->s[2]), va_hi(v->s[2]), va_lo(v->s[3]), va_hi(v->s[3]),
                    va_lo(v->s[6]), va_hi(v->s[6]), va_lo(v->s[7]), va_hi(v->s[7])};
    int32_t b[8] = {p[P_E2] * 258, p[P_E3] * 258, p[P_E6] * 258, p[P_E5] * 258,
                    clamp(p[P_E1] * 256 + m->shape - 64 * 256, 8 * 256, 120 * 256),
                    p[P_E7] * 258, clamp(p[P_E4] * 256 + m->cutoff, 0, 127 * 256), p[P_E0] * 16384};
    for (uint32_t j = 0; j < 8u; j++) b[j] = va_slew(a[j], b[j]);
    va_lp3_t f;
    va_lp3_coef(&f, (a[6] + b[6]) / 2, (a[3] + b[3]) / 2);
    uint32_t phase = v->ph[0], inc = m->inc >> 2, reciprocal = 65536u / n;
    for (uint32_t i = 0; i < n; i++) {
        int32_t wave = va_ramp(a[7], b[7], i, reciprocal), s = 0;
        uint32_t ph = phase << 2;
        if (wave) {
            int32_t saw = osc_saw(ph, inc << 2);
            if (wave <= 16384) s = (saw * wave) >> 14;
            else {
                uint32_t pw = (uint32_t)va_ramp(a[4], b[4], i, reciprocal) << 17;
                int32_t pulse = osc_pulse(ph, inc << 2, pw) * 2;
                s = saw + ((pulse - saw) * (wave - 16384) >> 14);
            }
        }
        if (a[0] || b[0]) s += mulq15(va_divider(phase, inc, 1), va_ramp(a[0], b[0], i, reciprocal));
        if (a[1] || b[1]) s += mulq15(va_divider(phase, inc, 0), va_ramp(a[1], b[1], i, reciprocal));
        phase += inc;
        s = va_lp3(&f, s / 4, z);
        /* 120 Hz one-pole shelf, Q12 coefficient at FS=44100. BODY adds up
         * to 75% of its low band. A deliberate approximation to bass boost. */
        int32_t low = va_pole(s, 35, &bass);
        s += mulq15(low, va_ramp(a[5], b[5], i, reciprocal) * 3 / 4);
        s = va_drive(s, va_ramp(a[2], b[2], i, reciprocal));
        out[i] += voice_amp(soft_knee(s, 16000) * 2, m, i) * 2;
    }
    v->ph[0] = phase;
    v->s[0] = z[0]; v->s[1] = z[1]; v->s[4] = z[2]; v->s[5] = bass;
    v->s[2] = va_pair(b[0], b[1]); v->s[3] = va_pair(b[2], b[3]);
    v->s[6] = va_pair(b[4], b[5]); v->s[7] = va_pair(b[6], b[7]);
}
static const preset_t HADES_PRESETS[] = {
    {"DEEP SUB", {1, 64, 88, 64, 44, 12, 8, 80}, {0, 70, 96, 34}, 18, 1, FX(0, 0, 0, 0), PAT(8)},
    {"DARK SAW", {1, 64, 68, 28, 54, 30, 38, 64}, {0, 68, 64, 28}, 36, 1, FX(0, 0, 0, 0), PAT(2)},
    {"PULSE BAS", {2, 48, 76, 34, 48, 42, 28, 48}, {0, 62, 72, 24}, 32, 1, FX(0, 0, 0, 0), PAT(2)},
    {"SUB ONLY", {0, 64, 100, 68, 72, 8, 0, 64}, {0, 74, 112, 38}, 0, 1, FX(0, 0, 0, 0), PAT(8)},
    {"DRIVEN", {1, 64, 90, 44, 70, 46, 100, 40}, {0, 58, 86, 24}, 22, 1, FX(0, 0, 0, 0), PAT(1)},
    {"RESO BASS", {1, 64, 54, 20, 42, 108, 22, 72}, {0, 74, 24, 32}, 52, 1, FX(0, 0, 0, 0), PAT(1)},
    {"DARK PLK", {2, 32, 52, 18, 62, 58, 44, 24}, {0, 58, 0, 30}, 42, 0, FX(0, 0, 0, 0), PAT(3)},
    {"RAW LEAD", {2, 80, 28, 0, 88, 58, 62, 0}, {2, 66, 104, 36}, 16, 1, FX(0, 0, 0, 0), PAT(4)},
};
static const engine_t ENG_HADES = {
    .name = "HADES", .page_title = {"OSC SUB", "FLT DRV"},
    .edit = {
        {"WAVE", F_ENUM, 0, 2, 1, N_HADES_WAVE, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0},
        {"SUB1", F_PCT, 0, 127, 72, 0, 0},
        {"SUB2", F_PCT, 0, 127, 40, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 64, 0, 0},
        {"RES", F_PCT, 0, 127, 24, 0, 0},
        {"DRV", F_PCT, 0, 127, 20, 0, 0},
        {"BODY", F_PCT, 0, 127, 48, 0, 0},
    },
    .presets = HADES_PRESETS, .npresets = NELEM(HADES_PRESETS),
    .knob = {P_E4, P_E5, P_E6, P_E7}, .note_on = hades_note_on, .render = hades_render,
    .keep = 0xFF, /* divider phases, filter and smoothing survive a sounding retrigger */
};
