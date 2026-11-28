/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * A project stays saveable after RANDOM / JOURNEY / ELASTIC. Their target sets are written from the PLAY groups
 * (uxl_hp_sync); everything written there must be something the project codec accepts. Up to 1.2.1 an engine control
 * (and STASIS MOTN) was written as it stands on screen, and the project could never be saved again. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define FELUCCA_PAIR_MORPH 1
#define FELUCCA_CPU2 0
#define CARAVEL_ACTION_BANKS 1
#define FELUCCA_RANDOM_MORPH 1
#define CARAVEL_SLIM 1
#define CARAVEL_STASIS_EXPAND 1
#define CARAVEL_REVERB_MINIMAL 1
#define CARAVEL_FORMAT2 1
#define CARAVEL_UX_LAYERS 1
#define CARAVEL_UX_EVOLVE 1
#define CARAVEL_ELASTIC_P1 1
#define CARAVEL_ELASTIC_PLAYABLE 1
#define CARAVEL_UX_CAPTURE 1
#define CARAVEL_STASIS2 1
#define CARAVEL_STASIS_SCOPE_V6 1
#define CARAVEL_UX_BUILD_FX_PLAY_STYLE 1
#include "../web/emu/felucca_web.c"
#include <assert.h>
#include <stdio.h>
static void seal(hp_store_t *p){p->crc=hp_hash((const uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);}
static void render(uint32_t n){while(n--)web_render(44);}
static unsigned bad_cells(void)
{
    unsigned n=0;
    for(uint32_t t=0;t<NTRK;t++)for(uint32_t r=0;r<HP_ROWS;r++)for(uint32_t k=0;k<4u;k++)n+=!hp_ref_valid(hp.track[t].cell[r][k]);
    return n;
}
static void must_save(const char *what,uint32_t e)
{
    static hp_store_t s;
    unsigned bad=bad_cells();
    int ok=hp_capture(&s);
    if(bad||!ok){printf("FAIL %s engine %u: %u invalid target cells, capture=%d\n",what,(unsigned)e,bad,ok);exit(1);}
    seal(&s);assert(hp_capsule_ok(&s));
}
static int set_has(uint32_t t,uint32_t id)      /* is the parameter in the track's target set */
{
    for(uint32_t r=0;r<hp.track[t].count;r++)for(uint32_t k=0;k<4u;k++){
        hp_ref_t x=hp.track[t].cell[r][k];
        if(x.kind==HP_TRACK&&hp_tid(x)==id&&hp_desc(x))return 1;
    }
    return 0;
}
int main(void)
{
    web_nor_erase();web_boot();render(1200);
    static hp_store_t s0;assert(hp_capture(&s0));

    /* every engine on every track, with and without STASIS */
    unsigned engines=0;
    for(uint32_t e=0;e<NENGINES;e++){
        if(!eng_ok(e))continue;
        engines++;
        for(uint32_t t=0;t<NTRK;t++){trk[t].eng_req=(uint8_t)e;trk[t].preset=0;}
        render(200);
        for(uint32_t rt=2u;rt<=3u;rt++){
            song.g[G_RTYPE]=(int16_t)rt;uxl.uset=0;
            uxl_hp_sync();
            must_save(rt==3u?"sync, STASIS":"sync, HALL",e);
        }
    }
    assert(engines>=6u);

    /* the engine controls stay targets (they are stored with their engine), MOTN does not */
    uint32_t e=trk[0].eng_req;
    uxl.uset=0;uxl_hp_sync();
    {
        uint32_t found=0;
        for(uint32_t i=0;i<8u;i++)found+=set_has(0,P_E0+i);
        assert(found>0u);
        for(uint32_t t=0;t<NTRK;t++)for(uint32_t r=0;r<HP_ROWS;r++)for(uint32_t k=0;k<4u;k++){
            hp_ref_t x=hp.track[t].cell[r][k];
            assert(!(x.kind==HP_GLOBAL&&x.id>G_CDEPTH&&x.id!=G_RTYPE));
        }
        hp_ref_t motn={0};motn.kind=HP_GLOBAL;motn.id=G_DRREV;motn.track=0;motn.engine=255;
        assert(!uxl_morph_has(motn));
    }

    /* a snapshot taken from that set is saved too, with its engine values */
    uxl.uset=0;uxl_hp_sync();
    assert(hp_rm_snapshot_store(0)!=0);
    assert(hp_rm_snap[song.sel][0].count>0u);
    must_save("snapshot",e);

    /* the real keys: RANDOM, JOURNEY, ELASTIC verbs, then SAVE and LOAD */
    for(uint32_t ev=0;ev<UXL_EVN-1u;ev++){
        uxl.ev=(uint8_t)ev;uxl.uset=0;
        uxl_ev_a4(1);uxl_ev_a4(0);uxl_ev_verb(0);render(50);
        must_save("verb",ev);
    }
    int16_t keep=trk[0].p[P_E0];
    assert(!hp_project_save(0,0));
    trk[0].p[P_E0]=(int16_t)(keep?0:1);
    assert(hp_project_load(0));
    assert(trk[0].p[P_E0]==keep);
    must_save("after load",e);

    /* data the codec cannot hold is reported as such, not as a size problem */
    hp.track[0].cell[0][0]=(hp_ref_t){0};
    hp.track[0].cell[0][0].kind=HP_GLOBAL;hp.track[0].cell[0][0].id=G_DRREV;
    assert(hp_project_save(0,0)==2);
    if(!str_eq(ui.msg,"BAD DATA / NOT SAVED")){printf("FAIL message: %s\n",ui.msg);return 1;}
    puts("evolve_save: PASS");
    return 0;
}
