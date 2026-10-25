/* SPDX-License-Identifier: GPL-3.0-only */
/* Same harness on V7/V8; opt-in measurement and snapshot reads must preserve audio. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
#ifdef PD_WORDS
static void ed_b(uint32_t x){(void)x;}
static void ed_bk_u32(uint32_t x){(void)x;}
#include "editor_diag.c"
#endif
int main(int argc,char **argv){
 int capture=argc>1;web_nor_erase();web_boot();run_ms(1400);unsigned cases=0;
 for(unsigned rev=0;rev<2;rev++)for(unsigned chord=0;chord<2;chord++)for(unsigned changes=0;changes<2;changes++){
  normal_off();CHECK(web_pair_prepare(0,2,rev?ENGI_HADES:ENGI_DRONE,rev?ENGI_DRONE:ENGI_HADES));
  unsigned drone=rev?2:0,hades=rev?0:2;
  host_preset(&trk[drone],ENGI_DRONE,5);host_preset(&trk[hades],ENGI_HADES,0);
  trk[0].p[P_VOICE]=chord?V_POLY:V_MONO;run_ms(20);
  if(changes){trk[0].p[P_DLY]=25;trk[2].p[P_REV]=32;trk[drone].p[P_LRATE]=29;trk[drone].p[P_LD_SHP]=12;trk[hades].p[P_LD_FLT]=18;}
#ifdef PD_WORDS
  if(capture){uint8_t a[3]={1,42,2};ed_diag_handle(91,a,3);}
#endif
  run_ms(5);web_keys(chord?((1u<<0)|(1u<<4)|(1u<<7)):1u);
  uint64_t digest=1469598103934665603ull;
  for(unsigned ms=0;ms<2200;ms++){
   if(ms==450)ep.base=0;if(ms==900)ep.base=32767;if(ms==1350)ep.base=16384;
   if(ms==1800)web_keys(0);
#ifdef PD_WORDS
   if(capture&&ms%97==0){uint8_t a[2]={0,42};ed_diag_handle(91,a,2);}
   if(capture&&ms==1700){uint8_t a[2]={2,42};ed_diag_handle(91,a,2);}
#endif
   web_render(44);
   for(unsigned i=0;i<44;i++){uint32_t l,r;CHECK(isfinite(out_l[i])&&isfinite(out_r[i]));memcpy(&l,&out_l[i],4);memcpy(&r,&out_r[i],4);digest=(digest^l)*1099511628211ull;digest=(digest^r)*1099511628211ull;}
  }
  printf("role=%u chord=%u changes=%u hash=%016llx\n",rev,chord,changes,(unsigned long long)digest);cases++;
 }
 fprintf(stderr,"DIAG AUDIO: %u CELL AIR / DEEP SUB cases, %u checks; capture=%d.\n",cases,checks,capture);return 0;
}
