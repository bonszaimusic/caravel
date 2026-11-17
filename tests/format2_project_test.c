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
    /* Legacy zero marker preserves T1-linked mode, but new projects can
     * encode an independent global ROOT/SCALE in the same 24-byte region. */
    uint32_t ko=68u+NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE);
    assert(wire.raw[ko+21u]==0u && wire.raw[ko+22u]==0u && wire.raw[ko+23u]==0u);
    proj_harmony.key_mode=1u;proj_harmony.key_root=9u;proj_harmony.key_scale=2u;
    assert(proj_pack(&wire,&p));
    assert(wire.raw[ko+21u]==1u && wire.raw[ko+22u]==9u && wire.raw[ko+23u]==2u);
    assert(proj_import(&q,wire.raw,sizeof wire));
    assert(proj_harmony.key_mode==1u && proj_harmony.key_root==9u && proj_harmony.key_scale==2u);
    bad=wire;bad.raw[ko+21u]=2u;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    bad=wire;bad.raw[ko+22u]=12u;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
    bad=wire;bad.raw[ko+23u]=16u;rehash(&bad);assert(!proj_import(&q,bad.raw,sizeof bad));
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
