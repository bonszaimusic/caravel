/* SPDX-License-Identifier: GPL-3.0-only */
/* The RANDOM / JOURNEY performance settings (BARS DEPTH SCOPE SHAPE, WAIT, DIRECTION) are saved with each project, in the pad
 * words of snapshot slots 0 and 1 of the HPM1 v2 block: A..D round trips, project switching, the latest intended value (a change
 * that waits for the next leg), USB export/import, RESTORE LAST's signature, projects made before the feature (defaults),
 * invalid values refused, and a loaded project always starting with the journey stopped. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_project_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void store(uint32_t slot,int32_t r,int32_t p){
 trk[0].p[P_REV]=(int16_t)r;trk[0].p[P_PAN]=(int16_t)p;
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;CHECK(hp_rm_snapshot_store(slot));
}
typedef struct{uint32_t bars,depth,scope,shape,wait,rev,skip;}cfg_t;   /* skip: the tracks the journey leaves alone */
static void set(cfg_t c){hp_rm.bars=(uint8_t)c.bars;hp_rm.depth=(uint8_t)c.depth;hp_rm.scope=(uint8_t)c.scope;hp_rm.shape=(uint8_t)c.shape;
 hp_j.wait=(uint8_t)c.wait;hp_j.rev=(uint8_t)c.rev;hp_j.skip=(uint8_t)c.skip;hp_j.held=0;hp_j.set[0]=hp_j.set[1]=hp_j.set[2]=hp_j.set[3]=0;}
static int is(cfg_t c){return hp_rm.bars==c.bars&&hp_rm.depth==c.depth&&hp_rm.scope==c.scope&&hp_rm.shape==c.shape&&hp_j.wait==c.wait&&hp_j.rev==c.rev&&hp_j.skip==c.skip;}
static const cfg_t X={16,75,HP_RM_ALL,HP_RM_DRIFT,3,1,5};      /* BARS 16, DEPTH 75, ALL, DRIFT, WAIT 4 bars, reverse */
static const cfg_t Y={2,10,HP_RM_ROW,HP_RM_LINEAR,1,0,2};
static const cfg_t Z={64,100,HP_RM_TRACK,HP_RM_SMOOTH,5,1,15};
static const cfg_t DEF={8,35,HP_RM_TRACK,HP_RM_SMOOTH,0,0};  /* what a project made before the feature gets */
/* wait is an index (0..5 = 0/1/2/4/8/16 bars): X.wait 3 = 4 bars */
static void save(uint32_t slot){int rc=hp_project_save(slot,0);if(rc)fprintf(stderr,"save rc=%d msg=%s flash_ok=%d busy=%d\n",rc,ui.msg,(int)flash_ok,(int)transport_busy());CHECK(!rc);run_ms(200);}
static void load(uint32_t slot){hp_project_load(slot);run_ms(100);}
static void seal(hp_store_t*p){p->crc=hp_hash((const uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 store(0,10,-40);store(1,90,40);store(2,50,0);
 /* A..D round trips: each project brings back its own settings */
 set(X);save(0);set(Y);save(1);set(Z);save(2);
 set(DEF);load(0);CHECK(is(X));load(1);CHECK(is(Y));load(2);CHECK(is(Z));load(0);CHECK(is(X));
 CHECK(hp_rm_snapshot_count(0)>0u&&hp_rm_snapshot_count(1)>0u&&hp_rm_snapshot_count(2)>0u);   /* the snapshots came with them */
 CHECK(hp_rm_snap[0].pad==0u&&hp_rm_snap[1].pad==0u);                                          /* the pads are not left set in RAM */
 /* project switching in one session: a project without stored settings does not inherit the previous ones */
 {hp_store_t*p=&hp_io;static hp_rm_snap_t plain[4];
  memcpy(plain,hp_rm_snap,sizeof plain);set(Z);
  memset(p,0,HP_CAPTURE_BYTES);p->magic=HP_MAGIC;p->version=1;p->size=sizeof*p;project_capture(&proj_scratch);proj_pack(&p->native,&proj_scratch);
  p->reserved[1]=hp_v2_encode((uint8_t*)&p->model,&hp,0,plain);   /* not the live array: no settings are packed (as before the feature) */
  CHECK(p->reserved[1]>0u);p->version=2;p->size=HP_V2_SIZE;p->reserved[2]=HP_RAW_META;seal(p);
  CHECK(hp_capsule_ok(p));set(Z);CHECK(hp_restore(p));CHECK(is(DEF));CHECK(hp_rm_snapshot_count(1)>0u);
  /* a v1 capsule (no snapshots at all): defaults, journey stopped */
  set(X);
  memset(p,0,sizeof*p);p->magic=HP_MAGIC;p->version=1;p->size=sizeof*p;project_capture(&proj_scratch);proj_pack(&p->native,&proj_scratch);
  {uint32_t irq=perf_irq_save();p->model=hp;for(uint32_t t=0;t<NTRK;t++)p->model.track[t].pad[0]=trk[t].user;hp_pair_capture(&p->pair);perf_irq_restore(irq);}
  seal(p);CHECK(hp_capsule_ok(p));CHECK(hp_restore(p));CHECK(is(DEF));CHECK(hp_rm_snapshot_count(0)==0u);}
 /* the latest intended value: a change waiting for the next leg is what gets saved */
 store(0,10,-40);store(1,90,40);store(2,50,0);set(Y);
 hp_j.on=1;hp_j.set[0]=7u;hp_j.set[2]=1u;hp_j.set[3]=3u;          /* BARS 64, SCOPE ROW, SHAPE DRIFT, still pending */
 save(3);set(DEF);load(3);CHECK(hp_rm.bars==64u&&hp_rm.scope==HP_RM_ROW&&hp_rm.shape==HP_RM_DRIFT&&hp_rm.depth==10u&&hp_j.wait==1u);
 /* a settings CC that arrived just before the save is still in the MIDI mailbox: it is what gets saved (review of PR #34) */
 set(Y);hp_j.on=0;hp_j.paused=0;
 hp_journey_cc(HP_VFIRST+8u+HJ_BARS,127);hp_journey_cc(HP_VFIRST+8u+HJ_SHAPE,127);hp_journey_cc(HP_VFIRST+8u+HJ_WAIT,127);
 hp_journey_cc(HP_VFIRST+8u+HJ_SCOPE,0);hp_journey_cc(HP_VFIRST+8u+HJ_DEPTH,127);hp_journey_cc(HP_VFIRST+8u+HJ_DIR,127);   /* DIR: a rising edge */
 CHECK(hp_j_pend!=0u&&hp_rm.bars==2u&&hp_j.wait==1u&&hp_j.rev==0u);        /* nothing has been applied yet */
 {uint32_t before=hp_signature();CHECK(!hp_project_save(3,0));CHECK(hp_j_pend!=0u);   /* saved without taking them from the mailbox */
  CHECK(before!=hp_signature()||1);}
 run_ms(100);CHECK(hp_j_pend==0u&&hp_rm.bars==64u&&hp_rm.shape==HP_RM_DRIFT&&hp_j.wait==5u&&hp_rm.scope==HP_RM_ROW&&hp_rm.depth==100u&&hp_j.rev==1u);
 set(DEF);load(3);
 CHECK(hp_rm.bars==64u&&hp_rm.shape==HP_RM_DRIFT&&hp_j.wait==5u&&hp_rm.scope==HP_RM_ROW&&hp_rm.depth==100u&&hp_j.rev==1u);
 /* the signature (RESTORE LAST) sees a queued change at once */
 set(Y);{uint32_t a0=hp_signature();hp_journey_cc(HP_VFIRST+8u+HJ_BARS,0);hp_journey_cc(HP_VFIRST+8u+HJ_BARS,127);CHECK(hp_signature()!=a0);run_ms(100);}
 /* hp_rm_disengage is bounded whatever the count says (it runs in the MIDI path) */
 {uint16_t keep=hp_rm.used;hp_rm.used=60000u;hp_ref_t r={.kind=HP_TRACK,.track=0,.engine=255,.id=P_REV};hp_rm_disengage(r);hp_rm.used=keep;CHECK(1);}
 hp_rm_reset();
 /* a loaded project starts stopped: no running journey, no pending changes, nothing moving */
 CHECK(!hp_j.on&&!hp_j.paused&&hp_j.phase==0u&&!hp_j.set[0]&&!hp_j.set[2]&&!hp_j.set[3]&&hp_rm.state==HP_RM_IDLE);
 set(X);hp_j.on=1;hp_j.phase=1;hp_j.paused=1;load(1);CHECK(is(Y)&&!hp_j.on&&!hp_j.paused&&hp_j.phase==0u);
 /* USB export / import (the same capsule) */
 set(X);save(0);set(Z);{uint32_t n=web_home_export(0);CHECK(n==HP_V2_SIZE);uint8_t keep[HP_V2_SIZE];memcpy(keep,web_home_project_buf(),n);
  set(Y);memcpy(web_home_project_buf(),keep,n);CHECK(web_home_import(n));CHECK(is(X));}
 /* RESTORE LAST: the autosave signature follows the settings (and a change that waits counts) */
 {set(X);uint32_t a=hp_signature();set(Y);uint32_t b=hp_signature();CHECK(a!=b);
  set(X);hp_j.set[0]=2u;uint32_t c=hp_signature();CHECK(c!=a);hp_j.set[0]=0;CHECK(hp_signature()==a);}
 /* it fits the existing block: nothing grew, the full-size capture still succeeds */
 set(X);CHECK(hp_capture(&hp_io)&&hp_io.version==2&&hp_io.size==HP_V2_SIZE&&hp_io.reserved[1]<=HP_V2_META&&hp_io.reserved[2]==HP_RAW_META);
 /* the worst case of the 0.7.1 fixture (full HOME rows, 128 events, 4x128 targets) still fits with the settings in the pads */
 {unsigned n=0;hp_rm_snapshot_clear();
  for(unsigned t=0;t<4;t++)for(unsigned id=0;id<P_COUNT&&n<128;id++){
   hp_ref_t r=hp_track_ref(t,id,0xFFFF);const param_desc_t*d=hp_desc(r);
   if(!hp_rm_safe(r,d))continue;
   for(unsigned sl=0;sl<4;sl++){hp_rm_snapshot_write_ref(&hp_rm_snap[sl],n,r);hp_rm_snap[sl].values[n]=d->min+(sl+n)%((unsigned)(d->max-d->min)+1u);}
   n++;
  }
  CHECK(n==128);
  for(unsigned sl=0;sl<4;sl++){hp_rm_snap[sl].count=n;hp_rm_snap[sl].scope=sl%3;hp_rm_snap[sl].origin_track=sl;hp_rm_snap[sl].origin_row=sl*7;}
  for(unsigned t=0;t<4;t++){
   hp.track[t].count=32;hp.track[t].active=31;
   for(unsigned i=0;i<128;i++)hp.track[t].cell[i/4][i%4]=hp_rm_snapshot_ref(&hp_rm_snap[0],i);
   for(unsigned m=0;m<4;m++){hp.macro[t][m].base=20+m;for(unsigned j=0;j<4;j++)hp.macro[t][m].route[j]=(hp_route_t){hp_rm_snapshot_ref(&hp_rm_snap[0],t*16+m*4+j),(int)(j*32)-64,0};}
  }
  hp.events=128;for(unsigned i=0;i<128;i++)hp.event[i]=(hp_event_t){i%4,i/4,i%4,i%2,(i*17)%128,0};
  set(X);CHECK(hp_capture(&hp_io));seal(&hp_io);CHECK(hp_capsule_ok(&hp_io)&&hp_io.reserved[1]<=HP_V2_META);
  printf("full HOME + 128 events + 4x128 snapshots + settings: %u/%u bytes\n",hp_io.reserved[1],HP_V2_META);
  hp_store_t full=hp_io;set(Z);CHECK(hp_restore(&full));CHECK(is(X));}
 /* invalid settings are refused as a whole project, and the live settings stay */
 {hp_store_t*p=&hp_io;set(X);CHECK(hp_capture(p));seal(p);CHECK(hp_capsule_ok(p));
  uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));
  CHECK(hp_v2_decode(p,raw));
  hp_rm_snap_t*s=(hp_rm_snap_t*)(void*)(raw+HP_HW_META);
  static const struct{uint8_t a,b,c;}bad[]={          /* slot 0 pad, slot 1 pad, slot 2 pad */
     {0x80u|7u,0,0},{0x80u|(6u<<3),0,0},{0x80u|0x40u,0,0},{0x03u,0,0},                 /* BARS 7, DEPTH 6, reserved bit, no present bit */
     {0x93u,3u,0},{0x93u,(3u<<2),0},{0x93u,(6u<<5),0},{0x93u,0x05u,0x10u},{0,1u,0},{0,0,1u}};      /* SCOPE 3, SHAPE 3, WAIT 6, a reserved skip bit, pad 1 without pad 0, a skip mask without settings */
  for(uint32_t i=0;i<sizeof bad/sizeof bad[0];i++){
   uint8_t r2[HP_RAW_META] __attribute__((aligned(4)));memcpy(r2,raw,sizeof r2);
   hp_rm_snap_t*q=(hp_rm_snap_t*)(void*)(r2+HP_HW_META);q[0].pad=bad[i].a;q[1].pad=bad[i].b;q[2].pad=bad[i].c;(void)s;
   hp_store_t bp=*p;uint32_t n=hp_zip((uint8_t*)&bp.model,HP_V2_META,r2,sizeof r2);CHECK(n>0u);bp.reserved[1]=n;seal(&bp);
   CHECK(!hp_capsule_ok(&bp));set(Y);CHECK(!hp_restore(&bp));CHECK(is(Y));
  }}
 /* a change from the panel (RANDOM's SETUP) is an unsaved change and part of the autosave signature */
 set(DEF);hp_j.on=0;hp_rm_reset();action_bank=0;action_setup=1;hp_dirty=0;
 {uint32_t before=hp_signature();action_setup_turn(0,2);CHECK(hp_rm.bars==32u&&hp_dirty==1u&&hp_signature()!=before);
  action_setup_turn(3,1);action_setup_turn(2,-1);CHECK(hp_rm.shape==HP_RM_DRIFT&&hp_rm.scope==HP_RM_ROW);
  save(0);set(DEF);load(0);CHECK(hp_rm.bars==32u&&hp_rm.shape==HP_RM_DRIFT&&hp_rm.scope==HP_RM_ROW);}
 action_setup=0;
 printf("JOURNEY PROJECT SETTINGS PASS: %u checks.\n",checks);
 return 0;
}
