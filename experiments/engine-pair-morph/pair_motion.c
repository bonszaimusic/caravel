/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional lightweight morph character: no extra engine, buffer, FFT or audio-rate coupling. */
static int32_t pair_power_gain(int32_t position)
{
    /* sin(pi*x/2), Q15, 33 values / 66 bytes. Integer interpolation, no runtime trig. */
    static const int16_t gain[] = {
        0,1608,3212,4808,6393,7962,9512,11039,12539,14010,15446,16846,18204,19519,
        20787,22005,23170,24279,25329,26319,27245,28105,28898,29621,30273,30852,
        31356,31785,32137,32412,32609,32728,32767
    };
    if (position >= 32767) return 32767;
    uint32_t at = (uint32_t)position >> 10, frac = (uint32_t)position & 1023u;
    return gain[at] + ((gain[at+1] - gain[at]) * (int32_t)frac >> 10);
}
static void pair_motion_weights(uint32_t i)
{
    int32_t target = ep.motion ? 32767 : 0;
    ep.motion_mix += clamp(target - ep.motion_mix, -75, 75);
    if (!ep.motion_mix) return;
    /* Normalised curve: retains the expressive shape without a +3 dB centre boost.
     * sin(x)/(sin(x)+cos(x)); LUT keeps division out of the sample loop. */
    static const int16_t curve[] = {
        0,1534,2938,4233,5436,6564,7626,8635,9597,10521,11414,12280,13125,13953,14770,15579,16384,17188,17997,18814,19642,20487,21353,22246,23170,24132,25141,26203,27331,28534,29829,31233,32767
    };
    int32_t linear=ep.weight[1][i], curved;
    if(linear>=32767) curved=32767;
    else {uint32_t k=(uint32_t)linear>>10,f=(uint32_t)linear&1023u;
        curved=curve[k]+((curve[k+1]-curve[k])*(int32_t)f>>10);}
    ep.weight[1][i]=ep.motion_mix==32767 ? curved : linear+((curved-linear)*ep.motion_mix>>15);
    ep.weight[0][i]=32767-ep.weight[1][i];
}
static uint32_t pair_tone_begin(track_t *t, int16_t *keep)
{
    if (!ep.motion_mix || !pair_member(t)) return 0;
    uint32_t id;
    if (t->engine == ENGI_SEM) {
        if(t->p[P_E5]>96) return 0;                /* no extra filter motion at high resonance */
        id = P_E4;
    }
    else if (t->engine == ENGI_DRONE) id = P_E7;   /* TONE only; never increase feedback/coupling */
    else return 0;                                 /* other supported engines get the curve only */
    int32_t m = ep.effective;
    int32_t bell = (m * (32767 - m)) >> 13;          /* 4*m*(1-m), 0 at both endpoints */
    int32_t wet = (bell * ep.motion_mix) >> 15;
    int32_t offset = (wet * 12 + 16383) >> 15;
    if(t->engine==ENGI_DRONE) offset=-offset;        /* a slightly warmer centre, no extra excitation */
    if (!offset) return 0;
    const param_desc_t *pd = &ENGINES[t->engine]->edit[id - P_E0];
    *keep = t->p[id];                               /* includes current normal MOD contributions */
    t->p[id] = clamp(*keep + offset, pd->min, pd->max);
    return id;
}
