/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* FORMAT 2 snapshot persistence and USB import. The old HPM1/7200-byte fixture
 * did not build on the six-engine target; rejected old formats stay rejected. */
#define WEB_FLASH_TEST 1
#define FORMAT2_EDITOR_MAIN snapshot_editor_fixture_main
#include "../../tests/format2_editor_test.c"
static hp_store_t capsule,bad;
static hp_rm_snap_t expected[NTRK][HP_RM_SNAP_SLOTS];
static uint8_t flash_before[sizeof nor];
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){printf("snapshot FAIL line %u: %s\n",__LINE__,#x);exit(1);} } while(0)
static void raw_seal(hp_store_t *p,const uint8_t *raw) {
 p->reserved[1]=hp_zip((uint8_t*)&p->model,HP_V2_META,raw,HP_RAW_META);
 CHECK(p->reserved[1]);seal(p);
}
static void boot(void) {
 web_flash_fail_after=-1;web_nor_erase();web_boot();for(unsigned i=0;i<1200;i++)web_render(44);
 seq_stop();pair_disable();for(unsigned i=0;i<100;i++)web_render(128);
}
static void fill(void) {
 hp_rm_snapshot_clear();
 for(unsigned t=0;t<NTRK;t++)for(unsigned slot=0;slot<4;slot++) {
  hp_rm_snap_t *s=&hp_rm_snap[t][slot];s->scope=HP_RM_TRACK;
  for(unsigned id=0;id<P_COUNT&&s->count<HP_RM_SNAP_MAX;id++) {
   hp_ref_t r=hp_track_ref(t,id,0xFFFF);const param_desc_t *d=hp_desc(r);
   if(!hp_rm_safe(r,d)||hp_rm_fm6_patch(r)||!hp_rm_snapshot_packable(r))continue;
   unsigned i=s->count++;hp_rm_snapshot_write_ref(s,i,r);s->values[i]=d->min;
  }
  CHECK(s->count==HP_RM_SNAP_MAX);
 }
 /* Runtime refs use engine ids; hp_capture validates their stored FORMAT 2 ids. */
 memcpy(expected,hp_rm_snap,sizeof expected);
}
static void reject(const hp_store_t *p) {
 uint32_t writes=web_flash_writes;CHECK(!hp_restore(p));CHECK(!hp_project_import(1,p));CHECK(web_flash_writes==writes);
}
int main(void) {
 boot();fill();CHECK(hp_capture(&capsule));seal(&capsule);
 CHECK(capsule.version==6&&capsule.size==HP_V2_SIZE&&hp_capsule_ok(&capsule));
 hp_rm_snapshot_clear();CHECK(hp_restore(&capsule));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 for(unsigned s=0;s<4;s++){CHECK(!hp_project_save(s,0));hp_rm_snapshot_clear();CHECK(hp_project_load(s));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));}
 hp_init();CHECK(hp_project_load(0));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Valid Pair with stored SPREAD on both members: direct load and USB slot import agree. */
 pair_disable();CHECK(pair_enable(0,1));CHECK(hp_capture(&capsule));seal(&capsule);
 CHECK(proj_import(&proj_scratch,&capsule.native,sizeof capsule.native));
 proj_scratch.t[0].p[P_SPRD]=100;proj_scratch.t[1].p[P_SPRD]=90;
 CHECK(proj_pack(&capsule.native,&proj_scratch));seal(&capsule);
 CHECK(hp_capsule_ok(&capsule));CHECK(hp_restore(&capsule));
 CHECK(ep.on&&trk[0].p[P_SPRD]==0&&trk[1].p[P_SPRD]==0);
 /* The USB path validates, writes a slot, then its load normalises the same two tracks. */
 CHECK(hp_project_import(1,&capsule));pair_disable();CHECK(hp_project_load(1));
 CHECK(ep.on&&trk[0].p[P_SPRD]==0&&trk[1].p[P_SPRD]==0);
 CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Keep malformed-capsule rejection, including a structurally invalid Pair, before any NOR write. */
 bad=capsule;bad.crc^=1;reject(&bad);
 bad=capsule;bad.reserved[1]=HP_V2_META+1;seal(&bad);reject(&bad);
 uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));
 CHECK(hp_v2_decode(&capsule,raw));hp_rm_snap_t(*sn)[HP_RM_SNAP_SLOTS]=(void*)(raw+HP_HW_META);
 sn[0][0].count=33;bad=capsule;raw_seal(&bad,raw);reject(&bad);
 CHECK(hp_v2_decode(&capsule,raw));sn[0][0].scope=3;bad=capsule;raw_seal(&bad,raw);reject(&bad);
 CHECK(hp_v2_decode(&capsule,raw));sn[0][0].values[0]=32767;bad=capsule;raw_seal(&bad,raw);reject(&bad);
 CHECK(hp_v2_decode(&capsule,raw));sn[0][0].refs[0][2]|=128;bad=capsule;raw_seal(&bad,raw);reject(&bad);
 CHECK(hp_v2_decode(&capsule,raw));hp_pair_store_t *pair=(void*)(raw+HP_HW_PAIR);pair->a=pair->b;
 bad=capsule;raw_seal(&bad,raw);reject(&bad);
 bad=capsule;bad.magic=0x314D5048u;bad.version=3;seal(&bad);reject(&bad);
 /* Every interrupted slot import keeps either the old complete copy or the complete new one. */
 CHECK(hp_restore(&capsule));CHECK(!hp_project_save(0,0));memcpy(flash_before,nor,sizeof nor);
 unsigned successes=0;
 for(int cut=0;cut<40;cut++) {
  memcpy(nor,flash_before,sizeof nor);web_flash_fail_after=cut;
  int ok=hp_project_import(0,&capsule);web_flash_fail_after=-1;
  hp_pending_mask=0;const hp_store_t *p=hp_slot_disk(0);CHECK(p&&hp_restore(p));
  CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));successes+=ok;
 }
 CHECK(successes>0);
 printf("SNAPSHOT PERSISTENCE: %u checks PASS; FORMAT 2, 4x4x32, A-D, reboot, Pair SPREAD direct/USB, malformed, interrupted imports.\n",checks);
 return 0;
}
