/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
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
#include "../web/emu/felucca_web.c"
#include <assert.h>
static void seal(hp_store_t *p) {
 p->crc=hp_hash((const uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);
}
#ifndef FORMAT2_PERFORMANCE_MAIN
#define FORMAT2_PERFORMANCE_MAIN main
#endif
int FORMAT2_PERFORMANCE_MAIN(void) {
 web_nor_erase();web_boot();for(uint32_t i=0;i<1200;i++)web_render(44);
 hp_store_t saved;
 uxl.uset=0;assert(hp_capture(&saved));seal(&saved);assert(hp_capsule_ok(&saved));
 assert(saved.version==6 && saved.magic==HP_MAGIC);
 /* An explicit empty own set must not be replaced by PLAY defaults. */
 memset(hp.track[0].cell,0,sizeof hp.track[0].cell);hp.track[0].count=1;hp.track[0].active=0;uxl.uset=1;
 assert(!hp_project_save(2,0));uxl.uset=0;assert(hp_project_load(2));
 assert(uxl.uset==1 && !uxl_set_count(0));
 for(uint32_t t=0;t<NTRK;t++) {
  hp.track[t].count=1;hp.track[t].active=0;memset(hp.track[t].cell,0,sizeof hp.track[t].cell);
  hp_ref_t r=hp_track_ref(t,P_E0,0);hp.track[t].cell[0][0]=r;
  for(uint32_t k=0;k<4;k++) {
   hp_rm_snap_t *s=&hp_rm_snap[t][k];memset(s,0,sizeof *s);
   s->count=1;s->scope=HP_RM_TRACK;hp_rm_snapshot_write_ref(s,0,r);
   s->values[0]=param_desc_of(trk[t].eng_req,P_E0)->def;
  }
 }
 uxl.uset=15;assert(hp_capture(&saved));seal(&saved);assert(hp_capsule_ok(&saved));
 assert(!hp_project_save(0,0));
 uxl.uset=0;memset(hp_rm_snap,0,sizeof hp_rm_snap);hp_apply(0,0);
 assert(hp_project_load(0));assert(uxl.uset==15);
 for(uint32_t t=0;t<NTRK;t++)for(uint32_t k=0;k<4;k++) {
  assert(hp_rm_snap[t][k].count==1);
  hp_ref_t r=hp_rm_snapshot_ref(&hp_rm_snap[t][k],0);assert(r.engine==trk[t].eng_req);
 }
 /* A different saved set replaces the previous project rather than inheriting it. */
 uxl.uset=0;assert(!hp_project_save(1,0));assert(hp_project_load(0));assert(uxl.uset==15);
 assert(hp_project_load(1));assert(!uxl.uset);
 /* Saving an active journey captures its settings/slots without resuming it on LOAD. */
 hp_eng_ovr=0;hp_j.on=1;assert(hp_capture(&saved));assert(hp_j.on);seal(&saved);
 assert(hp_restore(&saved));hp_eng_ovr=0;assert(!hp_j.on);hp_eng_ovr=255;
 /* A missing engine target stays guarded; the reference is never reinterpreted. */
 hp_ref_t r=hp_rm_snapshot_ref(&hp_rm_snap[0][0],0);assert(hp_desc(r));
 trk[0].eng_req=ENGI_PHYS;assert(!hp_desc(r));trk[0].eng_req=r.engine;
 /* All 16 slots at their full 32-target capacity coexist with own sets. */
 uxl.uset=15;
 for(uint32_t t=0;t<NTRK;t++) {
  uint32_t count=0;hp.track[t].count=8;hp.track[t].active=0;memset(hp.track[t].cell,0,sizeof hp.track[t].cell);
  for(uint32_t id=0;id<P_COUNT && count<HP_RM_SNAP_MAX;id++) {
   hp_ref_t ref=hp_track_ref(t,id,0xFFFF);const param_desc_t *d=hp_desc(ref);
   if(!hp_rm_safe(ref,d))continue;
   hp.track[t].cell[count/4][count%4]=ref;
   for(uint32_t k=0;k<4;k++) {
    hp_rm_snap_t *slot=&hp_rm_snap[t][k];
    hp_rm_snapshot_write_ref(slot,count,ref);
    slot->values[count]=d->min+(int32_t)((count*7+k*13+t)%(uint32_t)(d->max-d->min+1));
   }
   count++;
  }
  assert(count==32);
  for(uint32_t k=0;k<4;k++){hp_rm_snap[t][k].count=32;hp_rm_snap[t][k].scope=HP_RM_TRACK;}
 }
 hp_rm_snap_t expected[NTRK][4];memcpy(expected,hp_rm_snap,sizeof expected);
 assert(!hp_project_save(3,0));memset(hp_rm_snap,0,sizeof hp_rm_snap);
 assert(hp_project_load(3));assert(!memcmp(expected,hp_rm_snap,sizeof expected));
 assert(uxl.uset==15);for(uint32_t t=0;t<NTRK;t++)assert(uxl_set_count(t)==32);
 uint8_t noise[HP_RAW_META],packed[HP_V2_META];uint32_t seed=0xDEADBEEFu;
 for(uint32_t i=0;i<sizeof noise;i++){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;noise[i]=(uint8_t)seed;}
 assert(!hp_zip(packed,sizeof packed,noise,sizeof noise));
 const uint8_t malformed[]={128,0,0};assert(!hp_unzip(noise,sizeof noise,malformed,sizeof malformed));
 /* Neither old capsule versions nor a malformed own-set section are accepted. */
 assert(hp_capture(&saved));seal(&saved);saved.version=5;seal(&saved);assert(!hp_capsule_ok(&saved));
 uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));
 assert(hp_capture(&saved));seal(&saved);assert(hp_v2_decode(&saved,raw));
 raw[HP_RAW_META-HP_F2_SET_BYTES+5]=16;assert(!hp_meta_valid(raw,6));
 puts("FORMAT 2 complete project: empty/own/different sets, 16 full 32-target snapshots, missing target and running JOURNEY passed");
}
