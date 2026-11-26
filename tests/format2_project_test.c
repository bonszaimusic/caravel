/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
#include "../firmware/src/project.c"
static void rehash(project_store_t *s) {
    uint32_t h=proj_hash(s->raw,sizeof *s-4u);memcpy(s->raw+sizeof *s-4u,&h,4);
}
int main(void) {
    project_t p={0},q;project_store_t wire,bad;
    p.magic=PROJ_MAGIC_WIRE;p.size=sizeof p;p.parts=NPART;p.phys=PROJ_PHYS;
    proj_harmony_off();
    for(uint32_t t=0;t<NTRK;t++) {
        p.t[t].engine=eng_unstore(t);p.t[t].preset=0;
        for(uint32_t k=0;k<P_COUNT;k++)p.t[t].p[k]=param_desc_of(p.t[t].engine,k)->def;
        for(uint32_t k=0;k<NSTEP;k++) {
            step_t *s=&p.t[t].step[k];s->note[0]=48+k;s->n=1;s->vel=87;
            s->probability=100;step_set_ratchet(s,k%4+1);step_set_nudge(s,(int)(k%16)-8);
        }
        proj_harmony.root[t]=t*3;proj_harmony.dur[t]=t;proj_harmony.link_src[t]=t;
        proj_harmony.link_retrig[t]=t;proj_harmony.follow[t]=t%2;
        p.lfo2[t].p[L2_RATE]=17+t;
    }
    proj_harmony.on=1;harmony_saved_t h=proj_harmony;
    assert(proj_pack(&wire,&p));assert(proj_import(&q,wire.raw,sizeof wire));
    assert(!memcmp(p.t,q.t,sizeof p.t));assert(!memcmp(p.lfo2,q.lfo2,sizeof p.lfo2));
    assert(!memcmp(&h,&proj_harmony,sizeof h));
#if CARAVEL_STASIS2
    /* STASIS 2.0 DRY (retired id 80) is a plain track byte: the default is the old byte, 60 survives pack and import. */
    assert(p.t[2].p[P_DRY]==127);
    p.t[2].p[P_DRY]=60;p.t[3].p[P_DRY]=0;
    assert(proj_pack(&wire,&p));
    assert(proj_import(&q,wire.raw,sizeof wire));assert(q.t[2].p[P_DRY]==60&&q.t[3].p[P_DRY]==0&&q.t[0].p[P_DRY]==127);
    p.t[2].p[P_DRY]=127;p.t[3].p[P_DRY]=127;
    assert(proj_pack(&wire,&p));
#if !FELUCCA_FM4
    /* Old FOLLOW migrates once; new absolute maximum is unambiguous. */
    {
        project_t saved=p;
        const uint8_t revs[]={0,1,40,126,127};
        for(uint32_t k=0;k<NELEM(revs);k++) {
            for(uint32_t t=0;t<NTRK;t++) {
                assert(p.t[t].p[P_SSEND]==0);
                p.t[t].p[P_REV]=revs[(k+t)%NELEM(revs)];
            }
            assert(proj_pack(&wire,&p));
            for(uint32_t t=0;t<NTRK;t++) {
                uint32_t off=68u+t*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE)+P_SSEND;
                wire.raw[off]=191u;
            }
            rehash(&wire);
            assert(proj_import(&q,wire.raw,sizeof wire));
            for(uint32_t t=0;t<NTRK;t++) {
                assert(q.t[t].p[P_SSEND]==p.t[t].p[P_REV]);
                q.t[t].p[P_REV]=127-p.t[t].p[P_REV];
            }
            assert(proj_pack(&bad,&q));
            assert(proj_import(&q,bad.raw,sizeof bad));
            for(uint32_t t=0;t<NTRK;t++) assert(q.t[t].p[P_SSEND]==p.t[t].p[P_REV]);
        }
        p=saved;
        p.t[0].p[P_SSEND]=127;p.t[0].p[P_REV]=0;
        p.t[1].p[P_SSEND]=126;p.t[2].p[P_SSEND]=40;p.t[3].p[P_SSEND]=0;
        assert(proj_pack(&wire,&p));assert(wire.raw[68u+P_SSEND]==192u);
        assert(proj_import(&q,wire.raw,sizeof wire));
        assert(!memcmp(p.t,q.t,sizeof p.t));
        bad=wire;bad.raw[68u+P_SSEND]=193u;rehash(&bad);
        assert(!proj_import(&q,bad.raw,sizeof bad));
        bad=wire;bad.raw[68u+P_REV]=192u;rehash(&bad);
        assert(!proj_import(&q,bad.raw,sizeof bad));
        p=saved;
        assert(proj_pack(&wire,&p)&&proj_import(&q,wire.raw,sizeof wire));
        /* Motion carries raw signed bytes, separately from track parameters.
         * An old FOLLOW event/lock takes the saved REV base once. */
        for(uint32_t value=0;value<=127;value++) {
            p=saved;p.t[0].p[P_REV]=40;
            p.motion.count=1;p.motion.on=1;
            p.motion.event[0]=(motion_event_t){.place=0,.param=P_SSEND|MOTION_LOCK,.value=(int8_t)value};
            assert(proj_pack(&wire,&p));
            assert(proj_import(&q,wire.raw,sizeof wire));
            assert(q.motion.event[0].value==(int)value);
            /* Simulate the public 1.2.0 raw motion layout, not today's writer. */
            uint32_t mo=68u+NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE)+PROJ_HARMONY_SIZE;
            motion_store_t old=p.motion;
            memcpy(wire.raw+mo,&old,sizeof old);rehash(&wire);bad=wire;
            assert(proj_import(&q,wire.raw,sizeof wire));
            assert(q.motion.event[0].value==(int)(value==127?40:value));
            assert(!memcmp(&bad,&wire,sizeof wire));
            q.t[0].p[P_REV]=0;
            assert(proj_pack(&wire,&q)&&proj_import(&q,wire.raw,sizeof wire));
            assert(q.motion.event[0].value==(int)(value==127?40:value));
        }
        p=saved;assert(proj_pack(&wire,&p)&&proj_import(&q,wire.raw,sizeof wire));
    }
#endif
#endif
    /* Legacy FUNW has zero marker/padding and restores safe FX defaults. */
    assert(wire.raw[65u] == 0u && wire.raw[67u] == 0u);
    for (uint32_t t2 = 0; t2 < NTRK; t2++) assert(wire.raw[68u + NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE) + PROJ_HARM_DUR_OFF + t2] == t2);   /* default FX: bytes exactly as 1.0 wrote them */
    assert(q.af_on == 0u && q.af_time == 30u && q.af_amt == 32u &&
           q.af_damp == 70u && q.st_tone == 64u);
    /* Nondefault AFTER and TONE stay attached to the saved PROJECT.
     * No project object-size increase and no change to the native checksum. */
    p.af_on = 1u; p.af_time = 47u; p.af_amt = 99u;
    p.af_damp = 51u; p.st_tone = 105u;
    assert(proj_pack(&wire,&p));
    assert(wire.raw[65u] == PROJ_FX_EXT_MARK);
    assert(wire.raw[67u] == (0x80u | 47u));
    {   /* AMT/DAMP/TONE ride in the unused top six bits of the four HARMONY DURATION bytes; the low two bits stay the duration */
        const uint8_t *hd = wire.raw + 68u + NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE) + PROJ_HARM_DUR_OFF;
        uint32_t v = 0, t2;
        for (t2 = 0; t2 < NTRK; t2++) { assert((hd[t2] & 3u) == t2); v |= (uint32_t)(hd[t2] >> 2) << (6u * t2); }
        assert((v & 127u) == 99u && (v >> 7 & 127u) == 51u && (v >> 14 & 127u) == 105u);
    }
    assert(proj_import(&q,wire.raw,sizeof wire));
    assert(q.af_on == 1u && q.af_time == 47u && q.af_amt == 99u &&
           q.af_damp == 51u && q.st_tone == 105u);
    bad = wire; bad.raw[67u] = (uint8_t)(0x80u | 61u); rehash(&bad);
    assert(!proj_import(&q,bad.raw,sizeof bad)); /* invalid AFTER TIME */
    bad = wire; bad.raw[65u] = 0x55u; rehash(&bad);
    assert(!proj_import(&q,bad.raw,sizeof bad)); /* unknown extension */
    assert(!memcmp(&h,&proj_harmony,sizeof h));   /* the harmony survived the shared bytes */
    /* 1.0 strictness is kept: without the marker, a DURATION above 3 is still rejected */
    {   p.af_on = p.af_time = p.af_amt = p.af_damp = p.st_tone = 0u;
        assert(proj_pack(&wire,&p) && wire.raw[65u] == 0u);
        bad = wire; bad.raw[68u + NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE) + PROJ_HARM_DUR_OFF] |= 4u; rehash(&bad);
        assert(!proj_import(&q,bad.raw,sizeof bad)); }
    p.af_on = 1u; p.af_time = 47u; p.af_amt = 99u; p.af_damp = 51u; p.st_tone = 105u;
    assert(proj_pack(&wire,&p) && proj_import(&q,wire.raw,sizeof wire));   /* (a failed import above leaves q cleared) */
    /* A valid new project must re-encode through rename without losing
     * the saved FX state; defaults still use the legacy zero-wire layout. */
    assert(proj_pack(&wire,&q) && wire.raw[65u] == PROJ_FX_EXT_MARK);
#if CARAVEL_STASIS2
    /* New A6 marker owns only unused root/duration high bits. The old
     * G_SYNC/INFO/ENGGO global bytes remain opaque and exactly preserved. */
    {
        project_store_t again;
        p.st_region_present=1u;
        p.st_region=(stasis_region_saved_t){127u,1u,2u,2u,1u};
        p.g[G_SYNC]=1;p.g[G_INFO]=117;p.g[G_ENGGO]=29;
        assert(proj_pack(&wire,&p));
        assert(wire.raw[65u]==PROJ_STREG_MARK);
        assert(wire.raw[67u]==(0x80u|47u));
        assert(proj_import(&q,wire.raw,sizeof wire));
        assert(q.st_region_present &&
               streg_wire_bits(q.st_region)==streg_wire_bits(p.st_region));
        assert(q.g[G_SYNC]==1 && q.g[G_INFO]==117 && q.g[G_ENGGO]==29);
        assert(!memcmp(&h,&proj_harmony,sizeof h));
        assert(proj_pack(&again,&q));
        assert(again.raw[65u]==PROJ_STREG_MARK);
        assert(proj_import(&q,again.raw,sizeof again));
        assert(streg_wire_bits(q.st_region)==streg_wire_bits(p.st_region));
        bad=wire;bad.raw[65]=0xA7;rehash(&bad);
        assert(!proj_import(&q,bad.raw,sizeof bad));
        /* GRAIN now uses all four values of its two bits (23/46/93/186 ms). The
         * two-bit QUALITY field (bits 16..17, hb[7] bits 5..6) has one invalid
         * value, 3, which must be rejected. */
        bad=wire;
        uint32_t ho=68u+NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE);
        bad.raw[ho+7u]|=0x60u;rehash(&bad);
        assert(!proj_import(&q,bad.raw,sizeof bad));
        p.st_region_present=0u;
        p.g[G_SYNC]=p.g[G_INFO]=p.g[G_ENGGO]=0;
    }
#endif
    p.af_on = p.af_time = p.af_amt = p.af_damp = p.st_tone = 0u;
    assert(proj_pack(&wire,&p) && wire.raw[65u] == 0u);
    /* Legacy zero marker preserves T1-linked mode, but new projects can
     * encode an independent global ROOT/SCALE in the same 24-byte region. */
    uint32_t ko=68u+NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE);
    assert(wire.raw[ko+21u]==0u && wire.raw[ko+22u]==0u && wire.raw[ko+23u]==0u);
    proj_harmony.key_mode=1u;proj_harmony.key_root=9u;proj_harmony.key_scale=2u;
    proj_harmony.local_shape[0]=CH_DIA3;
    proj_harmony.local_shape[1]=CH_DIA7;
    proj_harmony.local_shape[2]=CH_DOM7;
    proj_harmony.local_shape[3]=CH_POW;
    assert(proj_pack(&wire,&p));
    assert(wire.raw[ko+21u]==0x2au); /* scale 2, root 9+1 */
    assert(wire.raw[ko+22u]==0x21u); /* local shapes 1,2 */
    assert(wire.raw[ko+23u]==0x95u); /* local shapes 5,9 */
    assert(proj_import(&q,wire.raw,sizeof wire));
    assert(proj_harmony.key_mode==1u && proj_harmony.key_root==9u && proj_harmony.key_scale==2u);
    assert(proj_harmony.local_shape[0]==CH_DIA3 && proj_harmony.local_shape[1]==CH_DIA7);
    assert(proj_harmony.local_shape[2]==CH_DOM7 && proj_harmony.local_shape[3]==CH_POW);
    bad=wire;bad.raw[ko+21u]=0x20u;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad)); /* no root marker */
    bad=wire;bad.raw[ko+21u]=0x2du;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad)); /* root 12 invalid */
    bad=wire;bad.raw[ko+22u]=0xa1u;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad)); /* shape BUS invalid */
    bad=wire;bad.raw[ko+21u]=0;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad)); /* legacy reserved dirty */
    proj_harmony=h;
    for(uint32_t e=0;e<6;e++) {
        p.t[0].engine=eng_unstore(e);assert(proj_pack(&wire,&p));
        assert(wire.raw[68+P_COUNT]==e);assert(proj_import(&q,wire.raw,sizeof wire));
        assert(q.t[0].engine==p.t[0].engine);
    }
    bad=wire;bad.raw[68+P_COUNT]=6;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    bad=wire;bad.raw[69+P_COUNT]=255;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    for(uint32_t m=0x46554e31u;m<PROJ_MAGIC_WIRE;m++) {
        bad=wire;memcpy(bad.raw,&m,4);rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    }
    assert(!proj_import(&q,wire.raw,sizeof wire-1));assert(!proj_import(&q,wire.raw,sizeof wire+1));
    bad=wire;bad.raw[70]^=1;assert(!proj_import(&q,bad.raw,sizeof bad));
    uint32_t off=68+NTRK*(P_COUNT+2+NSTEP*PROJ_STEP_SIZE);
    bad=wire;bad.raw[off]=12;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    assert(!memcmp(&h,&proj_harmony,sizeof h));
    printf("FORMAT 2 project: six engines, step/LFO2/Harmony round-trip, old/corrupt inputs rejected (%zu bytes)\n",sizeof wire);
}
