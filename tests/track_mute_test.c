/* SPDX-License-Identifier: GPL-3.0-only */
/* Issue #2: track mute gates sounding voices as well as note triggers. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static void block(int *heard) {
 /* Observe the track contribution before global buses: old FX tails may continue. */
 events_block(CTL);perf_begin(CTL);
 for(unsigned i=0;i<CTL;i++)mix_l[i]=mix_r[i]=send_c[i]=send_d[i]=send_r[i]=0;
 mix_part(&trk[0],CTL SM_ARG(&mx[0]));
#if FELUCCA_CPU2
 track_lfo_end(&trk[0]);
#endif
 for(unsigned i=0;i<CTL;i++)if(mix_l[i]||mix_r[i]||send_c[i]||send_d[i]||send_r[i])*heard=1;
}
static void scene(unsigned latch,unsigned spread,unsigned arp,unsigned slicer) {
 host_tracks_init();track_t*t=&trk[0];host_preset(t,0,0);
 t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_LATCH]=latch;t->p[P_SPRD]=spread;
 t->p[P_AMODE]=arp;t->p[P_SLCR]=slicer;t->p[P_SLDEPTH]=127;
 t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=32;
 input_on(t,48,100);input_on(t,55,100);
 if(latch){input_off(t,48);input_off(t,55);}
 int heard=0;for(unsigned b=0;b<1000;b++)block(&heard);assert(heard);
 uint32_t held[4];memcpy(held,lat_stay[0],sizeof held);unsigned kills=voice_kills;
 t->p[P_MUTE]=1;
 for(unsigned b=0;b<8;b++){heard=0;block(&heard);}
 heard=0;for(unsigned b=0;b<128;b++)block(&heard);
 if(heard){fprintf(stderr,"muted track audible: latch=%u spread=%u arp=%u slicer=%u\n",latch,spread,arp,slicer);exit(1);}
 assert(t->p[P_LATCH]==latch&&!memcmp(held,lat_stay[0],sizeof held)&&voice_kills==kills);
 t->p[P_MUTE]=0;heard=0;for(unsigned b=0;b<1000;b++)block(&heard);assert(heard);
 for(unsigned b=0;b<100;b++){t->p[P_MUTE]=b&1;heard=0;block(&heard);}
 t->p[P_MUTE]=1;for(unsigned b=0;b<8;b++){heard=0;block(&heard);}
 heard=0;for(unsigned b=0;b<16;b++)block(&heard);assert(!heard);
 _exit(0);
}
int main(void) {
 unsigned cases=0;
 for(unsigned ll=0;ll<3;ll++)for(unsigned s=0;s<2;s++)for(unsigned a=0;a<2;a++)for(unsigned z=0;z<3;z++){
  pid_t p=fork();assert(p>=0);if(!p)scene((ll+1)%3,s?100:0,a,z);
  int status=0;assert(waitpid(p,&status,0)==p);if(!WIFEXITED(status)||WEXITSTATUS(status))return 1;cases++;
 }
 host_tracks_init();trk[0].p[P_MUTE]=1;perf_begin(1);
 int32_t previous=65536;
 for(unsigned i=0;i<128;i++){int32_t b=65536;perf_mute(0,&b,1);assert(b<=previous&&previous-b<=512);previous=b;}
 assert(previous==0);perf_kill=1;perf_begin(1);assert(pf.act&PF_BIT(PF_M1));
 trk[0].p[P_MUTE]=0;perf_kill=0;perf_begin(1);
 for(unsigned i=0;i<128;i++){int32_t b=65536;perf_mute(0,&b,1);assert(b>=previous&&b-previous<=512);previous=b;}
 assert(previous==65536);
 puts("track mute: 36 normal/LATCH/ARP/SPREAD/SLICER scenes, dry/sends, resumed voices and rapid toggling passed");
 return cases!=36;
}
