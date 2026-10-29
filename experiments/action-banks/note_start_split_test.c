/* SPDX-License-Identifier: GPL-3.0-only */
/* Scheduling must see newly-started/released voices before rendering them. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_CPU2 1
#define PAIR_TEST_MAIN split_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static uint32_t estimated_weight(uint32_t mask){
 uint32_t a=0,b=0;
 for(uint32_t t=0;t<NPART;t++){
  uint32_t voices=0;
  for(uint32_t v=0;v<NVOICE;v++)voices+=trk[t].v[v].active!=0;
  uint32_t cost=voices*ENG_VCOST[trk[t].engine]+(voices||trk[t].tail?60u:0u);
  if(mask&(1u<<t))a+=cost;else b+=cost;
 }
 return (a>b?a:b)+C1_HAND;
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(0,2,ENGI_SEM,ENGI_SEM));
 host_preset(&trk[1],ENGI_DRONE,9);run_ms(100);
 for(uint32_t t=0;t<3;t++)trk[t].p[P_SUS]=127;
 trk_note_on(&trk[0],48,100);run_ms(100);
 uint32_t before=mix_split();
 trk_note_on(&trk[1],36,100); /* no track render between note-on and split */
 uint32_t after=mix_split();
 printf("new DRONE: old split=%u first-note split=%u estimated cost=%u vs cold=%u\n",before,after,estimated_weight(after),estimated_weight(before));
 CHECK(estimated_weight(after)<estimated_weight(before));
 uint32_t best=estimated_weight(after);
 for(uint32_t mask=1;mask<15;mask++)CHECK(best<=estimated_weight(mask));
 CHECK(gates(0)==1 && gates(1)==1 && gates(2)==1);
 printf("NOTE START split PASS: %u checks; new heavy voice balanced before its first rendered block.\n",checks);
 return 0;
}
