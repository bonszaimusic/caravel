/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS 2.0: per-track DRY (P_DRY, retired id 80) scales only the direct bus.
 * Sends (CHORUS/DELAY/REV) are taken before it and must not move. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <unistd.h>
typedef struct { uint64_t dry, snd; } sums_t;
static sums_t run(unsigned dry, unsigned spread, unsigned level)
{
 host_tracks_init();track_t*t=&trk[0];host_preset(t,0,0);
 t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_SPRD]=spread;t->p[P_LEVEL]=level;
 t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=40;t->p[P_DRY]=dry;
 input_on(t,48,100);input_on(t,55,100);
 sums_t s={0,0};
 for(unsigned b=0;b<600;b++){
  events_block(CTL);perf_begin(CTL);
  for(unsigned i=0;i<CTL;i++)mix_l[i]=mix_r[i]=send_c[i]=send_d[i]=send_r[i]=0;
  mix_part(&trk[0],CTL SM_ARG(&mx[0]));
#if FELUCCA_CPU2
  track_lfo_end(&trk[0]);
#endif
  if(b<100)continue; /* attack settle */
  for(unsigned i=0;i<CTL;i++){
   s.dry+=(uint64_t)abs(mix_l[i])+(uint64_t)abs(mix_r[i]);
   s.snd+=(uint64_t)abs(send_c[i])+(uint64_t)abs(send_d[i])+(uint64_t)abs(send_r[i]);
  }
 }
 return s;
}
static sums_t child(unsigned dry, unsigned spread, unsigned level)
{
 int fd[2];assert(!pipe(fd));pid_t p=fork();assert(p>=0);
 if(!p){sums_t s=run(dry,spread,level);assert(write(fd[1],&s,sizeof s)==(ssize_t)sizeof s);_exit(0);}
 sums_t s;assert(read(fd[0],&s,sizeof s)==(ssize_t)sizeof s);
 int st=0;assert(waitpid(p,&st,0)==p&&WIFEXITED(st)&&!WEXITSTATUS(st));
 close(fd[0]);close(fd[1]);return s;
}
int main(void)
{
 const unsigned spreads[2]={0,100};
 for(unsigned k=0;k<2;k++){
  unsigned sp=spreads[k];
  sums_t full=child(127,sp,100), half=child(64,sp,100), q=child(32,sp,100), none=child(0,sp,100);
  assert(full.dry>0 && full.snd>0);
  assert(none.dry==0);                                     /* DRY 0: direct bus silent */
  assert(none.snd==full.snd && half.snd==full.snd && q.snd==full.snd); /* sends independent of DRY */
  assert(half.dry<full.dry && q.dry<half.dry && none.dry<q.dry);      /* monotonic */
  /* 64/127 of the direct bus, within rounding of the per-block gain */
  uint64_t want=full.dry*64u/127u;
  assert(half.dry+full.dry/100u>=want && half.dry<=want+full.dry/100u);
  /* LVL still scales both: DRY does not replace the track level */
  sums_t lo=child(127,sp,50);
  assert(lo.dry<full.dry && lo.snd<full.snd);
 }
 puts("stasis mix DRY: 0/32/64/127 direct bus scales, sends untouched, mono and SPREAD paths passed");
 return 0;
}
