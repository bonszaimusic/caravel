/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* BYTE/FLOAT compatibility, renderer bounds and storage through real firmware.
 * Legacy reference: hugelton/Felucca@1727537624390cc3f9c7dff2456b43b3a8edb005. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static void lofi_legacy_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
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


int main(void)
{
    int bad = 0, ok = 1;
    ui_power_on();
    track_t *t = TSEL;
    set_engine_of(t, 3);
    bad += check("LOFI: append-only waves/presets and unchanged parameter layout", P_COUNT == 114 && P_E0 == 106 && P_FILT == 105 && P_MORPH == 103 && P_PD_FB == 104 &&
        RW_BYTE == 5 && RW_FLOAT == 6 && ENG_LOFI.edit[1].max == 6 && ENG_LOFI.npresets == 7 &&
        !strcmp(ENG_LOFI.presets[0].name, "PULSE LD") && !strcmp(ENG_LOFI.presets[4].name, "STEP LEAD"));
    for (uint32_t wave = 0; wave < 5; wave++) for (uint32_t chip = 0; chip < 5; chip++)
        for (uint32_t corner = 0; corner < 2; corner++) {
            voice_t a = {0}, b = {0};
            vmod_t m = {.pitch16 = (corner ? 96 : 36) * 16, .amp0 = 32767, .amp1 = 32767, .shape = 64 << 8};
            int32_t x[CTL], y[CTL];
            for (uint32_t k = 0; k < 8; k++) t->p[P_E0+k] = corner ? 127 : 0;
            t->p[P_E0] = chip; t->p[P_E1] = wave; t->p[P_E6] = corner ? 3 : 0;
            lofi_note_on(t, &a); b = a;
            for (uint32_t block = 0; block < 32; block++) {
                memset(x, 0, sizeof x); memset(y, 0, sizeof y);
                lofi_render(t, &a, x, CTL, &m); lofi_legacy_render(t, &b, y, CTL, &m);
                ok &= !memcmp(x,y,sizeof x) && !memcmp(&a,&b,sizeof a);
            }
        }
    bad += check("LOFI: all five legacy waves and chip modes match upstream at parameter corners", ok);
    ok = 1;
    for (uint32_t wave = RW_BYTE; wave <= RW_FLOAT; wave++) for (uint32_t formula = 0; formula < 32; formula++)
        for (uint32_t corner = 0; corner < 2; corner++) {
            voice_t v = {0}; int32_t out[CTL];
            vmod_t m = {.pitch16 = (corner ? 96 : 36) * 16, .amp0 = 32767, .amp1 = 32767};
            for (uint32_t k = 0; k < 8; k++) t->p[P_E0+k] = 0;
            t->p[P_E0] = corner ? CHIP_1BIT : CHIP_8BIT; t->p[P_E1] = wave;
            t->p[P_E2] = formula * 4; t->p[P_E3] = corner ? 127 : 0;
            t->p[P_E4] = corner ? 127 : 64; t->p[P_E5] = corner ? 127 : 0;
            t->p[P_E6] = corner ? 3 : 0; t->p[P_E7] = corner ? 127 : 0;
            lofi_note_on(t, &v);
            v.s[2] = corner ? (int32_t)0xFFFFF800u : 0;
            for (uint32_t block = 0; block < 128; block++) {
                memset(out,0,sizeof out); lofi_render(t,&v,out,CTL,&m);
                for (uint32_t i = 0; i < CTL; i++) ok &= out[i] >= -65535 && out[i] <= 65535;
            }
            int32_t aux = 2147483640;
            (void)lofi_float(formula, 0xFFFFF001u, ~0u, &aux);
        }
    bad += check("BYTE/FLOAT: all 64 formulas bounded at control/time wrap corners", ok);
    ok = 1;
    for (uint32_t wave = RW_BYTE; wave <= RW_FLOAT; wave++) {
        static project_t a,b; static project_store_t wire;
        t->p[P_E1]=wave; t->p[P_E2]=124; t->p[P_E3]=127;
        t->p[P_E4]=64; t->p[P_E5]=127; t->p[P_E6]=3; t->p[P_E7]=89;
        project_capture(&a);
        ok &= proj_pack(&wire,&a) && proj_import(&b,&wire,sizeof wire);
        for (uint32_t k=0;k<8;k++) ok &= a.t[0].p[P_E0+k] == b.t[0].p[P_E0+k];
        ok &= !strcmp(track_desc(t,P_E2)->label,"ALGO") && !strcmp(track_desc(t,P_E3)->label,"VAR") &&
              !strcmp(track_desc(t,P_E4)->label,"MOTION") && !strcmp(track_desc(t,P_E7)->label,"LOOP");
        for (uint32_t k=0;k<8;k++) ok &= track_desc(t,P_E0+k)->max == param_desc_of(3,P_E0+k)->max;
        t->p[P_E0] = CHIP_8BIT;
        int16_t before[8]; memcpy(before,t->p+P_E0,sizeof before);
        ok &= up_store(0,"FORMULA") == 3; /* this host UI has no flash: applied in RAM */
        t->p[P_E1]=0; up_load(0);
        ok &= !memcmp(before,t->p+P_E0,sizeof before);
    }
    bad += check("BYTE/FLOAT: project and user preset round trips, dynamic descriptors with stable ranges",ok);
    { /* Mode edits while a voice is sounding must not reuse the other mode's state. */
        voice_t v = {0}; vmod_t m = {.pitch16=60*16,.amp0=32767,.amp1=32767}; int32_t out[CTL];
        uint32_t modes[] = {RW_PLS,RW_BYTE,RW_FLOAT,RW_SAW,RW_BYTE};
        ok=1;
        for (uint32_t j=0;j<NELEM(modes);j++) {
            t->p[P_E1]=modes[j]; memset(out,0,sizeof out); lofi_render(t,&v,out,CTL,&m);
            ok &= modes[j]<RW_BYTE ? ((uint32_t)v.s[5] & 0xFFFF0000u)!=LOFI_FORMULA_STATE :
                (uint32_t)v.s[5] == (LOFI_FORMULA_STATE | modes[j]);
        }
        lofi_note_on(t,&v); v.ph[0]=~0u; v.ph[1]=~0u; /* emulate voice.c phase restoration */
        memset(out,0,sizeof out); lofi_render(t,&v,out,CTL,&m);
        ok &= (uint32_t)v.s[2]<1000u && (uint32_t)v.s[5]==(LOFI_FORMULA_STATE|RW_BYTE);
        bad += check("BYTE/FLOAT: live mode changes initialize their state and retriggers restart formula time",ok);
    }

    printf("BYTE/FLOAT: %s\n",bad?"FAILED":"all checks passed");
    return bad != 0;
}
