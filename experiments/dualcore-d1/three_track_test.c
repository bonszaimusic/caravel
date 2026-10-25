/* SPDX-License-Identifier: GPL-3.0-only */
/* Representative workload: SEM controls, notes and globals are assumptions. */
#include "../home-perf/captured_controls.h"
int main(void) {
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 apply(&trk[0],&captured[0]);apply(&trk[1],&captured[1]);
 host_preset(&trk[2],ENGI_SEM,0);song.g[G_RTYPE]=1;song.g[G_BPM]=120;
 trk[3].p[P_LEVEL]=0;
 hp.macro[2][0].base=80;hp.macro[2][0].route[0].ref=hp_track_ref(2,P_FILT,0xffff);hp.macro[2][0].route[0].amount=25;
 uint64_t hash=1469598103934665603ull;int32_t o[CTL*2];
 for(unsigned b=0;b<12u*FS/CTL;b++) {
  unsigned ms=(uint64_t)b*CTL*1000/FS;
  if(b==0){trk_note_on(&trk[0],36,110);trk_note_on(&trk[1],48,90);trk_note_on(&trk[2],60,100);}
  if(b==FS*3/CTL)pd_active=1;
  if(b==FS*5/CTL){trk_note_off(&trk[2],60);trk_note_on(&trk[2],67,90);}
  if(b==FS*6/CTL){web_pair_panic();run_ms(800);CHECK(web_pair_prepare(0,2,ENGI_HADES,ENGI_SEM));ep.motion=1;trk_note_on(&trk[0],48,110);}
  if(b>FS*6/CTL&&b<FS*9/CTL)ep.base=(ms*31)%32768;
  if(b==FS*9/CTL){pair_disable();for(unsigned t=0;t<3;t++)trk_all_off(&trk[t]);}
  fm1_ms=ms;mix_block(o,CTL);
  for(unsigned i=0;i<CTL*2;i++)hash=(hash^(uint32_t)o[i])*1099511628211ull;
 }
 printf("three-track HALL macro Pair releases: %016llx\n",(unsigned long long)hash);
#if FELUCCA_CPU2
 if(!c1h_off)CHECK(c1h_taken>0);
#endif
 return 0;
}
