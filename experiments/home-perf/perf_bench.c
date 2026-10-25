/* SPDX-License-Identifier: GPL-3.0-only */
/* Deterministic offline workload using actual emulator/DSP sources.
 * Not a replay of missing full project data, and not target cycle timing. */
#include "captured_controls.h"
static uint64_t bench_ns(void) { struct timespec t; 
#ifdef __EMSCRIPTEN__
 clock_gettime(CLOCK_MONOTONIC,&t);
#else
 clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);
#endif
return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec; }
int main(int argc,char **argv) {
 unsigned variant=argc>1?atoi(argv[1]):0, seconds=argc>2?atoi(argv[2]):30;
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(0,1,15,16));apply(&trk[0],&captured[0]);apply(&trk[1],&captured[1]);
 trk[1].p[P_FILT]=35;trk[1].p[P_E4]=58;trk[1].p[P_E5]=87;
 ep.base=14964;ep.motion=0;song.g[G_CLOCK]=0;song.g[G_BPM]=120;
 if(variant==2) trk[0].p[P_FILT]=trk[1].p[P_FILT]=0;
 if(variant==3) {trk[0].p[P_ITYPE]=0;trk[0].p[P_DIST]=0;}
 if(variant==4) {trk[0].p[P_ITYPE]=0;trk[0].p[P_DIST]=0;trk[0].p[P_FILT]=trk[1].p[P_FILT]=0;}
 run_ms(30);
 if(variant!=0) {web_pair_sequence();for(unsigned i=0;i<16;i++){uint8_t note=48+(i%8)*2;put_step(&trk[0],i,1,&note,ST_NOTE,0);ep.lane[i]=-1;}seq_start();}
 else trk_note_on(&trk[0],48,100);
 /* Audio mix only: includes events/seq/Pair, tracks, buses, master. UI outside
  * this timer; real MCU timer/panel/MIDI IRQ costs are not simulated here. */
 int32_t audio[CTL*2];uint64_t hash=1469598103934665603ull, elapsed=0;
 unsigned blocks=(uint64_t)seconds*FS/CTL;
 for(unsigned batch=0;batch<blocks;batch+=256) {
   unsigned end=batch+256<blocks?batch+256:blocks;
   if(variant==5) {
      unsigned step=batch/256;
      ep.base=(step*1237u)&32767u;
      trk[1].p[P_FILT]=(int32_t)(step%128)-64;
      trk[0].p[P_IMIX]=(step%7)?127:32;
      trk[0].p[P_ITYPE]=(step/23)%6;
   }
   uint64_t begin=bench_ns();
   for(unsigned b=batch;b<end;b++){fm1_ms=(uint64_t)b*CTL*1000/FS;mix_block(audio,CTL);}
   elapsed+=bench_ns()-begin;
   /* Batch endpoint check; full-output regression is a separate run. */
   for(unsigned j=0;j<CTL*2;j++)hash=(hash^(uint32_t)audio[j])*1099511628211ull;
 }
 printf("{\"variant\":%u,\"seconds\":%u,\"blocks\":%u,\"cpu_ns\":%llu,\"hash\":\"%016llx\"}\n",variant,seconds,blocks,(unsigned long long)elapsed,(unsigned long long)hash);
 return 0;
}
