/* SPDX-License-Identifier: GPL-3.0-only */
#include "captured_controls.h"
#include "drone_v9_reference.h"
#include "filter_v9_reference.h"
#include "insert_v9_reference.h"
static uint32_t random_state=271828183u;
static uint32_t random32(void){random_state=random_state*1664525u+1013904223u;return random_state;}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(0,1,15,16));apply(&trk[0],&captured[0]);apply(&trk[1],&captured[1]);
 trk_note_on(&trk[1],48,100);run_ms(100);
 track_t *t=&trk[1];drone_part_t *s=drone_state(t),initial,expected;
 int32_t a[128],b[128];uint64_t samples=0;
 for(unsigned i=0;i<12000;i++){
  unsigned n=1+random32()%128;
  if(i%31==0){for(unsigned k=0;k<8;k++)t->p[P_E0+k]=random32()%128;t->p[P_E3]=random32()%6;}
  if(i%13==0){unsigned note=random32()%128;s->held[note>>5]^=1u<<(note&31);}
  vmod_t m={.inc=pitch_inc(48*16),.pitch16=48*16,.amp0=32767,.amp1=32767,.shape=64*256};
  if(i%7==0){m.cutoff=(int32_t)(random32()%8193)-4096;m.shape=64*256+(int32_t)(random32()%8193)-4096;}
  if(i%5==0)m.fine=(int32_t)(random32()%8191)-4095;
  initial=*s;memset(a,0,sizeof a);memset(b,0,sizeof b);
  drone_cell_v9_reference(t,&t->v[0],a,n,&m);expected=*s;*s=initial;
  drone_cell_render(t,&t->v[0],b,n,&m);
  CHECK(!memcmp(a,b,n*sizeof *a));CHECK(!memcmp(s,&expected,sizeof *s));samples+=n;
 }
 printf("DRONE oracle: 12000 blocks, %llu samples, exact output and all 352 state bytes match V9\n",(unsigned long long)samples);
 tfilt_t before[NTRK],after[NTRK];int32_t x[128],y[128],side[128],side_ref[128];
 for(unsigned i=0;i<16000;i++){
  unsigned n=1+random32()%128,stereo=i%2;track_t *f=&trk[i%NTRK];
  if(i%29==0)f->p[P_FILT]=(int)(random32()%128)-64;
  if(i%997==0)tf_rest(f);
  for(unsigned j=0;j<n;j++){x[j]=y[j]=(int32_t)(random32()%524287)-262143;side[j]=side_ref[j]=(int32_t)(random32()%262143)-131071;}
  memcpy(before,tf,sizeof tf);unsigned run=f->tf_run;
  track_filter_v9_reference(f,x,stereo?side_ref:0,n);memcpy(after,tf,sizeof tf);unsigned expected_run=f->tf_run;
  memcpy(tf,before,sizeof tf);f->tf_run=run;
  track_filter(f,y,stereo?side:0,n);
  CHECK(!memcmp(x,y,n*sizeof *x));CHECK(!memcmp(side,side_ref,n*sizeof *side));
  CHECK(!memcmp(after,tf,sizeof tf));CHECK(f->tf_run==expected_run);
 }
 printf("FILTER oracle: 16000 blocks, mono/stereo, sweeps/bypass/LP-HP transitions; output and all state bytes match V9\n");
 ins_t ibefore,iafter;track_t *it=&trk[0];ins_t *is=&ins[0];memset(is,0,sizeof *is);
 for(unsigned i=0;i<12000;i++){
  unsigned n=1+random32()%128;
  if(i%17==0){it->p[P_IA]=random32()%128;it->p[P_IB]=random32()%128;it->p[P_IC]=random32()%128;}
  if(i%53==0)is->type=1+random32()%5;
  int32_t tw=i%151<120?32768:(i%151<140?0:random32()%32769);
  for(unsigned j=0;j<n;j++)x[j]=y[j]=(int32_t)(random32()%262143)-131071;
  ibefore=*is;ins_drive_v9_reference(is,it,x,n,tw);iafter=*is;*is=ibefore;
  ins_drive(is,it,y,n,tw);CHECK(!memcmp(x,y,n*sizeof *x));CHECK(!memcmp(is,&iafter,sizeof *is));
 }
 printf("INSERT oracle: 12000 blocks, SOFT/HARD/FOLD/FUZZ/CRUSH, wet/dry transitions; output and state match V9\n");
 return 0;
}
