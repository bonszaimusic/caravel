/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY under stress, in simulated time (the morph and the scheduler are polled with a synthetic clock; no audio is rendered):
 *  - worst case: sixteen snapshots (four per track) of 32 targets each, thousands of chained legs on T1's set, every arrival checked against its snapshot;
 *  - chaos: random buttons and absolute CCs, tempo changes (BPM 20..300), hand edits, FREEZE / RESUME, RANDOM morphs, project
 *    capture / restore in the middle, and the state-machine invariants after every poll;
 *  - long runs: hours of simulated time with long legs and rests;
 *  - cost on the host for the functions that run in the MIDI path and per frame (the target's instruction counts are read from the
 *    disassembly by the CI notes; these host numbers say nothing about the FM-1's speed).
 * Not simulated: external MIDI clock, audio, the real CPU. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_stress_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <time.h>
static uint32_t st_seed=0x1234567u;
static uint32_t st_rnd(void){st_seed^=st_seed<<13;st_seed^=st_seed>>17;st_seed^=st_seed<<5;return st_seed;}
static double st_now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
static void seal(hp_store_t*p){p->crc=hp_hash((uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);}
static void cc(uint32_t j,uint32_t v){hp_journey_cc(HP_VFIRST+8u+j,v);}
static void press(uint32_t j){cc(j,0);cc(j,127);}
static uint32_t arrivals,starts_seen,stops_seen;
/* invariants that must hold after every poll */
static void invariants(void){
 for(uint32_t t=0;t<NTRK;t++){
  const hp_rm_state_t*e=&hp_eng[t];const hp_jn_t*j=&hp_jn[t];
  CHECK(e->used<=HP_RM_MAX&&e->count<=e->used);
  CHECK(j->cur<4u&&j->tgt<4u&&j->wait<6u&&j->trk==t);
  if(j->on&&!j->paused&&!j->held){
   if(j->phase==0u)CHECK(e->state==HP_RM_RECALL||e->state==HP_RM_FROZEN);
   else CHECK(e->state==HP_RM_IDLE);
  }
  if(!j->on)CHECK(j->phase==0u);
 }
}
static void params_in_range(void){
 for(uint32_t t=0;t<NTRK;t++)for(uint32_t slot=0;slot<4u;slot++){const hp_rm_snap_t*s=&hp_rm_snap[t][slot];
  for(uint32_t i=0;i<s->count;i++){hp_ref_t r=hp_rm_snapshot_ref(s,i);const param_desc_t*d=hp_desc(r);int32_t v=hp_get(r);CHECK(v>=d->min&&v<=d->max);}}
}
static uint32_t T;                                         /* the synthetic clock, ms */
static void step(uint32_t ms){T+=ms;hp_rm_poll(T);hp_journey_poll(T);invariants();}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 song.sel=0;go_home();action_enabled=1;action_bank=ACT_BANK_JOURNEY;action_refresh();
 /* worst case: 4 tracks x 4 snapshots x 32 targets, each track's set holds that track's own controls */
 {hp_rm_snapshot_clear();
  for(unsigned t=0;t<4;t++){unsigned n=0;
   for(unsigned id=0;id<P_COUNT&&n<HP_RM_SNAP_MAX;id++){
    hp_ref_t r=hp_track_ref(t,id,0xFFFF);const param_desc_t*d=hp_desc(r);
    if(!hp_rm_safe(r,d))continue;
    if(hp_rm_fm6_patch(r))continue;                   /* FM6's patch selector is never captured (issue #35): the fixture enumerates what a capture would take */
    for(unsigned sl=0;sl<4;sl++){hp_rm_snapshot_write_ref(&hp_rm_snap[t][sl],n,r);hp_rm_snap[t][sl].values[n]=d->min+(sl*37+n*11+t*5)%((unsigned)(d->max-d->min)+1u);}
    n++;
   }
   CHECK(n==HP_RM_SNAP_MAX);
   for(unsigned sl=0;sl<4;sl++){hp_rm_snap[t][sl].count=n;hp_rm_snap[t][sl].scope=HP_RM_TRACK;}
  }
  CHECK(hp_snapshots_valid(hp_rm_snap));}
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.shape=HP_RM_SMOOTH;
 /* 1. clean run: 600 legs of 32 targets at 120 BPM, 1 bar each; every arrival reaches its snapshot exactly */
 {hp_rm.bars=1;song.g[G_BPM]=240;hp_j.wait=1;hp_j.rev=0;action_snap_slot=0;
  for(unsigned t=0;t<4;t++)trk[t].p[P_REV]=77;
  cc(HJ_PLAY,0);cc(HJ_PLAY,127);step(10);CHECK(hp_j.on);
  uint8_t expect=0;uint32_t legs=0,guard=0;
  while(legs<600u&&guard++<4000000u){
   uint8_t was=hp_j.phase;step(10);
   if(was==0u&&hp_j.phase==1u){                           /* arrival at hp_j.cur */
    const hp_rm_snap_t*s=&hp_rm_snap[hp_j.trk][hp_j.cur];int ok=1;
    for(uint32_t i=0;i<s->count;i++){hp_ref_t r=hp_rm_snapshot_ref(s,i);if(hp_get(r)!=s->values[i]){ok=0;const param_desc_t*d=hp_desc(r);fprintf(stderr,"arrival miss slot %u target %u: track %u kind %u id %u aux %u engine %u want %d got %d range %d..%d fmt %d\n",(unsigned)hp_j.cur,(unsigned)i,r.track,r.kind,r.id,r.aux,r.engine,(int)s->values[i],(int)hp_get(r),d?d->min:0,d?d->max:0,d?(int)d->fmt:-1);}}
    CHECK(ok);CHECK(hp_j.cur==expect%3u||hp_j.cur==expect%4u);
    legs++;arrivals++;expect=(uint8_t)((hp_j.cur+1u)%4u);
   }
  }
  CHECK(legs==600u);}
 /* 1b. four journeys at once, each on its own track's set, engine and settings; every arrival matches that track's snapshot */
 {for(uint32_t t=0;t<NTRK;t++){hp_eng[t].bars=(uint8_t)(1u<<(t&1u));hp_eng[t].scope=HP_RM_TRACK;hp_eng[t].shape=HP_RM_SMOOTH;hp_jn[t].wait=(uint8_t)(1u+t);hp_jn[t].rev=(uint8_t)(t>>1);}
  hp_j.on=hp_j.paused=0;hp_rm_reset_all();song.g[G_BPM]=240;
  for(uint32_t t=0;t<NTRK;t++){song.sel=t;action_snap_slot=0;cc(HJ_PLAY,0);cc(HJ_PLAY,127);step(10);CHECK(hp_jn[t].on);}
  song.sel=0;
  uint32_t legs[NTRK]={0,0,0,0},guard=0;uint8_t was[NTRK];
  while((legs[0]<120u||legs[1]<120u||legs[2]<60u||legs[3]<60u)&&guard++<2000000u){
   for(uint32_t t=0;t<NTRK;t++)was[t]=hp_jn[t].phase;
   step(10);
   for(uint32_t t=0;t<NTRK;t++)if(was[t]==0u&&hp_jn[t].phase==1u){
    const hp_rm_snap_t*q=&hp_rm_snap[t][hp_jn[t].cur];int ok=1;
    for(uint32_t i=0;i<q->count;i++)if(hp_get(hp_rm_snapshot_ref(q,i))!=q->values[i])ok=0;
    CHECK(ok);legs[t]++;arrivals++;
   }
  }
  CHECK(legs[0]>=120u&&legs[1]>=120u&&legs[2]>=60u&&legs[3]>=60u);
  printf("four journeys: legs %u / %u / %u / %u\n",legs[0],legs[1],legs[2],legs[3]);
  for(uint32_t t=0;t<NTRK;t++){hp_jn[t].on=hp_jn[t].paused=hp_jn[t].held=0;hp_jn[t].phase=0;hp_jn[t].wait=1;hp_jn[t].rev=0;hp_eng[t].bars=1;}
  hp_rm_reset_all();}
 /* 2. chaos: random controls, tempo changes, hand edits, freezes, RANDOM morphs, project capture/restore */
 {uint32_t iter,restores=0,hand=0,tempo=0,presses=0;
  for(iter=0;iter<300000u;iter++){
   uint32_t r=st_rnd(),ev=r%400u;
   if(ev==0){press(HJ_PLAY);presses++;}
   else if(ev==1){press(HJ_DIR);presses++;}
   else if(ev==2||ev==3){press(HJ_NEXT);presses++;}
   else if(ev==4)cc(HJ_WAIT,st_rnd()%128u);
   else if(ev==5)cc(HJ_WAIT,st_rnd()%128u);
   else if(ev==6)cc(HJ_BARS,st_rnd()%128u);
   else if(ev==7)cc(HJ_DEPTH,st_rnd()%128u);
   else if(ev==8)cc(HJ_SCOPE,st_rnd()%128u);
   else if(ev==9)cc(HJ_SHAPE,st_rnd()%128u);
   else if(ev==10){song.g[G_BPM]=(int16_t)(20+st_rnd()%281u);tempo++;}          /* tempo change in the middle of anything */
   else if(ev==11){hp_rm_freeze();}
   else if(ev==12){hp_rm_resume(T);}
   else if(ev==13&&hp_rm_snap[hp_j.trk][0].count){                                     /* a knob or CC on one of the targets */
    uint32_t i=st_rnd()%hp_rm_snap[hp_j.trk][0].count;hp_ref_t rf=hp_rm_snapshot_ref(&hp_rm_snap[hp_j.trk][0],i);const param_desc_t*d=hp_desc(rf);
    hp_set(rf,d->min+(int32_t)(st_rnd()%(unsigned)(d->max-d->min+1)),1);hand++;
   }
   else if(ev==16)song.sel=st_rnd()&3u;                                       /* the buttons and CCs below act on another track's journey */
   else if(ev==14&&(st_rnd()&7u)==0)hp_rm_start(T);                             /* RANDOM by hand ends the journey */
   else if(ev==15&&(st_rnd()&15u)==0){                                          /* a project saved and loaded in the middle */
    CHECK(hp_capture(&hp_io));seal(&hp_io);CHECK(hp_restore(&hp_io));restores++;
    for(uint32_t t=0;t<NTRK;t++)CHECK(!hp_jn[t].on&&hp_eng[t].state==HP_RM_IDLE&&!hp_jn[t].set[0]&&!hp_jn[t].set[1]&&!hp_jn[t].set[2]&&!hp_jn[t].set[3]);   /* always stopped after a load */
    for(uint32_t t=0;t<NTRK;t++)for(uint32_t sl=0;sl<4;sl++)CHECK(hp_rm_snap[t][sl].count==HP_RM_SNAP_MAX&&(t+sl==0||(hp_rm_snap[t][sl].pad[0]==0u&&hp_rm_snap[t][sl].pad[1]==0u)));
   }
   uint32_t dt=1+st_rnd()%40u;step(dt);
   if(!hp_j.on&&(st_rnd()&63u)==0){press(HJ_PLAY);}
   if((iter&1023u)==0)params_in_range();
   if(hp_j.on)starts_seen++;else stops_seen++;
  }
  printf("chaos: %u iterations, %u hand edits, %u tempo changes, %u button presses, %u project capture/restores, journey on %u / off %u polls\n",
         iter,hand,tempo,presses,restores,starts_seen,stops_seen);
  params_in_range();CHECK(hand>100u&&tempo>100u&&restores>3u);}
 /* 3. long runs: hours of simulated time, long legs and rests, slow and fast tempi */
 {song.sel=0;for(uint32_t t=0;t<NTRK;t++){hp_jn[t].on=hp_jn[t].paused=hp_jn[t].held=0;hp_jn[t].phase=0;}hp_rm_reset_all();hp_rm.bars=64;hp_rm.shape=HP_RM_DRIFT;hp_j.wait=5;hp_j.rev=0;hp_j.set[0]=hp_j.set[1]=hp_j.set[2]=hp_j.set[3]=0;
  action_snap_slot=0;song.g[G_BPM]=60;press(HJ_PLAY);step(10);CHECK(hp_j.on);
  uint32_t legs=0;uint8_t was=0;uint32_t t0=T;
  while(T-t0<6u*3600u*1000u){                                              /* six simulated hours */
   if(((T-t0)/600000u)%2u==0)song.g[G_BPM]=60;else song.g[G_BPM]=200;      /* the tempo changes every ten minutes */
   was=hp_j.phase;step(50);if(was==0u&&hp_j.phase==1u)legs++;
  }
  printf("long run: %u simulated hours, %u legs of 64 bars at 60/200 BPM, journey still %s\n",6u,legs,hp_j.on?"running":"stopped");
  CHECK(hp_j.on&&legs>10u);params_in_range();}
 /* 4. cost on the host (instruction counts for the target come from its disassembly, see the handoff) */
 {double t;uint32_t N;song.sel=0;hp_rm_reset_all();for(uint32_t u=0;u<NTRK;u++)hp_jn[u].on=0;
  hp_ref_t ref0=hp_rm_snapshot_ref(&hp_rm_snap[0][0],0);
  CHECK(hp_rm_snapshot_recall(0,1,T));
  N=200000;t=st_now_ns();for(uint32_t i=0;i<N;i++)hp_rm_disengage(ref0);t=(st_now_ns()-t)/N;
  printf("host cost: hp_rm_disengage over 32 cells %.0f ns\n",t);
  hp_rm.used=HP_RM_MAX;for(uint32_t i=0;i<HP_RM_MAX;i++)hp_rm.cell[i].ref=ref0;                 /* the unrealistic worst case: every cell matches */
  N=200000;t=st_now_ns();for(uint32_t i=0;i<N;i++)hp_rm_disengage(ref0);t=(st_now_ns()-t)/N;
  printf("host cost: hp_rm_disengage, all 32 cells match %.0f ns\n",t);
  hp_rm_reset();
  N=2000;t=st_now_ns();for(uint32_t i=0;i<N;i++){hp_rm_reset();hp_rm_snapshot_recall(i&3u,i&3u,T);}t=(st_now_ns()-t)/N;
  printf("host cost: recall of 32 targets (validate + build) %.0f ns\n",t);
  N=20000;t=st_now_ns();for(uint32_t i=0;i<N;i++){T+=10;hp_rm_poll(T);}t=(st_now_ns()-t)/N;
  printf("host cost: hp_rm_poll, 4 engines %.0f ns\n",t);
  N=20000;t=st_now_ns();for(uint32_t i=0;i<N;i++){hp_journey_poll(T);}t=(st_now_ns()-t)/N;
  printf("host cost: hp_journey_poll (idle) %.0f ns\n",t);
  N=500;t=st_now_ns();for(uint32_t i=0;i<N;i++)CHECK(hp_capture(&hp_io));t=(st_now_ns()-t)/N;
  printf("host cost: project capture incl. compression %.0f ns\n",t);}
 printf("JOURNEY STRESS PASS: %u checks, %u arrivals verified against their snapshots.\n",checks,arrivals);
 return 0;
}
