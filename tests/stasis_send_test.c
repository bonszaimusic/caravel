/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS 2.x: the track's STASIS send (P_SSEND, retired id 78) feeds its own bus send_s, the ring's input.
 *   0..127 = independent ring gain, REV 0 + send > 0 = STASIS only.
 *   Not STASIS (HALL): the ring bus stays silent. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <unistd.h>
typedef struct { uint64_t rev, st; unsigned same; } sums_t;
static unsigned test_chor,test_delay,test_level=100,test_voices=2;
static sums_t run(unsigned rtype, unsigned rev, unsigned ss, unsigned spread)
{
 host_tracks_init();track_t*t=&trk[0];host_preset(t,0,0);
 song.g[G_RTYPE]=rtype;
 t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_SPRD]=spread;t->p[P_LEVEL]=test_level;
 t->p[P_CHOR]=test_chor;t->p[P_DLY]=test_delay;t->p[P_REV]=rev;t->p[P_SSEND]=ss;
 for(unsigned i=0;i<test_voices;i++)input_on(t,36+7*i,127);
 sums_t s={0,0,1};
 for(unsigned b=0;b<600;b++){
  events_block(CTL);perf_begin(CTL);
  for(unsigned i=0;i<CTL;i++)mix_l[i]=mix_r[i]=send_c[i]=send_d[i]=send_r[i]=send_s[i]=0;
  mix_part(&trk[0],CTL SM_ARG(&mx[0]));
#if FELUCCA_CPU2
  track_lfo_end(&trk[0]);
#endif
  if(b<100)continue;
  for(unsigned i=0;i<CTL;i++){
   s.rev+=(uint64_t)abs(send_r[i]);s.st+=(uint64_t)abs(send_s[i]);
   if(send_s[i]!=send_r[i])s.same=0;
  }
 }
 return s;
}
static sums_t child(unsigned rtype, unsigned rev, unsigned ss, unsigned spread)
{
 int fd[2];assert(!pipe(fd));pid_t p=fork();assert(p>=0);
 if(!p){sums_t s=run(rtype,rev,ss,spread);assert(write(fd[1],&s,sizeof s)==(ssize_t)sizeof s);_exit(0);}
 sums_t s;assert(read(fd[0],&s,sizeof s)==(ssize_t)sizeof s);
 int st=0;assert(waitpid(p,&st,0)==p&&WIFEXITED(st)&&!WEXITSTATUS(st));
 close(fd[0]);close(fd[1]);return s;
}
/* Both REV THROW variants must leave the ring send independent. */
static void perf_reset(void)                          /* as tests/perform_test.c: the FX layer's state at rest */
{
 memset(&pf,0,sizeof pf);
 pf.src=pf.next=PF_N;
 pf.lc=PF_TOP;
 pf.mg[0]=pf.mg[1]=pf.mg[2]=pf.mg[3]=32768;
 perf_held=perf_act=0;
 perf_latched=0;
 perf_kill=0;
 memcpy((void *)perf_map,PF_DEF,sizeof PF_DEF);
 memset((void *)perf_k,0,sizeof perf_k);
 sl_lent=0;
 memset(sl,0,sizeof sl);
}
typedef struct { unsigned same, ring_null, throw_seen, gthrow_seen; uint64_t st; } mb_t;
static mb_t run_mb(unsigned ss0, unsigned ss1, unsigned global)
{
 host_tracks_init();perf_reset();song.g[G_RTYPE]=3;
 for(unsigned k=0;k<2;k++){
  track_t*t=&trk[k];host_preset(t,0,0);
  t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_LEVEL]=100;t->p[P_CHOR]=t->p[P_DLY]=0;t->p[P_REV]=30;t->p[P_SSEND]=k?ss1:ss0;
  input_on(t,48+12*k,100);
 }
 perf_press(PF_RTHR,1);
 if(global)perf_k[2]=100;
 mb_t m={1,1,0,0,0};
 int32_t out[2*CTL];
 for(unsigned b=0;b<400;b++){
  mix_block(out,CTL);
  if(b<100)continue;
  if(st_ring_src)m.ring_null=0;
  if(pf.thr)m.throw_seen=1;
  if(pf.td)m.gthrow_seen=1;
  for(unsigned i=0;i<CTL;i++){
   if(send_s[i]!=send_r[i])m.same=0;
   m.st+=(uint64_t)abs(send_s[i]);
  }
 }
 return m;
}
static mb_t child_mb(unsigned a,unsigned b,unsigned g)
{
 int fd[2];assert(!pipe(fd));pid_t p=fork();assert(p>=0);
 if(!p){mb_t s=run_mb(a,b,g);assert(write(fd[1],&s,sizeof s)==(ssize_t)sizeof s);_exit(0);}
 mb_t s;assert(read(fd[0],&s,sizeof s)==(ssize_t)sizeof s);
 int st=0;assert(waitpid(p,&st,0)==p&&WIFEXITED(st)&&!WEXITSTATUS(st));
 close(fd[0]);close(fd[1]);return s;
}
/* a loud eight-voice chord at LEVEL 127, REV 0 and the send at 127: the limit of its own keeps the Q15 product inside 32 bit (run with -fsanitize=undefined) */
static void loud(void)
{
 host_tracks_init();song.g[G_RTYPE]=3;
 track_t*t=&trk[0];host_preset(t,0,0);
 t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_LEVEL]=127;t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;t->p[P_SSEND]=127;
 for(unsigned n=0;n<8;n++)input_on(t,36+7*n,127);
 int64_t peak=0;
 for(unsigned b=0;b<300;b++){
  events_block(CTL);perf_begin(CTL);
  for(unsigned i=0;i<CTL;i++)mix_l[i]=mix_r[i]=send_c[i]=send_d[i]=send_r[i]=send_s[i]=0;
  mix_part(&trk[0],CTL SM_ARG(&mx[0]));
  for(unsigned i=0;i<CTL;i++){int64_t a=send_s[i]<0?-(int64_t)send_s[i]:send_s[i];if(a>peak)peak=a;}
 }
 assert(peak>0&&peak<0x7FFFFFFF);
}
#if FELUCCA_CPU2
/* the second core's sums are cleared each block, the STASIS bus included (a stale one would be added into send_s again and again) */
static void worker_clear(void)
{
 host_tracks_init();
 for(unsigned i=0;i<CTL;i++)mx[1].s[i]=12345;
 c1_parts((uint32_t)CTL<<8);                          /* no part to render: only the clear */
 for(unsigned i=0;i<CTL;i++)assert(mx[1].s[i]==0);
}
#endif
int main(void)
{
 const unsigned spreads[2]={0,100};
 for(unsigned k=0;k<2;k++){
  unsigned sp=spreads[k];
  sums_t fol=child(3,40,127,sp), maxonly=child(3,0,127,sp), off=child(3,40,0,sp), half=child(3,40,63,sp), only=child(3,0,63,sp), hall=child(2,40,127,sp), hall2=child(2,40,63,sp);
  assert(fol.rev>0 && fol.st>0);
  assert(maxonly.rev==0 && maxonly.st==fol.st && fol.st>fol.rev); /* full send is independent */
  assert(off.st==0 && off.rev==fol.rev);              /* 0: ring silent, REV (the hall) untouched */
  assert(half.rev==fol.rev && half.st>0);             /* REV does not move with the STASIS send */
  assert(only.rev==0 && only.st>0);                   /* REV 0: STASIS only */
  assert(only.st==half.st);                           /* independent of REV */
  /* 63 of 127 of REV 40's own gain scale: send 63 is 63*258, REV 40 is 40*258, so the bus is 63/40 of the REV bus */
  uint64_t want=fol.rev*63u/40u;
  assert(half.st+fol.rev/50u>=want && half.st<=want+fol.rev/50u);
  assert(hall.st==0 && hall2.st==0 && hall.rev==fol.rev);   /* not STASIS: no ring bus */
 }
 /* Equal ss/rev must still have its OWN clamp when the other sends change.
  * Exercise the real eight-voice mixer at the low/equal and full gains. */
 test_voices=8;test_level=127;
 for(unsigned sp=0;sp<=100;sp+=100)for(unsigned gain=1;gain<=127;gain+=126) {
  test_chor=test_delay=0;sums_t a=child(3,gain,gain,sp);
  test_chor=test_delay=127;sums_t b=child(3,gain,gain,sp),c=child(3,0,gain,sp);
  assert(a.st>0&&a.st==b.st&&b.st==c.st);
 }
 test_chor=test_delay=0;test_voices=2;test_level=100;
 {
  mb_t all=child_mb(127,127,0), allg=child_mb(127,127,1), man=child_mb(0,0,0), mang=child_mb(0,0,1), man2=child_mb(63,63,0);
  assert(all.ring_null && all.throw_seen && all.st>0);
  assert(allg.gthrow_seen && allg.st==all.st);
  assert(man.throw_seen && mang.gthrow_seen && man.st==0 && mang.st==0); /* neither THROW feeds ring */
  assert(man2.st>0 && man2.ring_null);
 }
 loud();
#if FELUCCA_CPU2
 worker_clear();
#endif
 puts("stasis send: 0..127 independent, THROW isolated, HALL ring silent");
 return 0;
}
