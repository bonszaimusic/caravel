/* SPDX-License-Identifier: GPL-3.0-only */
#include "captured_controls.h"
static uint64_t hash=1469598103934665603ull;
static void put(const void*p,unsigned n){const unsigned char*b=p;while(n--)hash=(hash^*b++)*1099511628211ull;}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(unsigned e=0;e<NENGINES;e++)if(eng_ok(e)&&ENGINES[e]->npresets){
  set_engine_of(&trk[0],e);
  for(unsigned p=0;p<ENGINES[e]->npresets;p++){
   apply_preset_to(&trk[0],p);hash=1469598103934665603ull;
   put(trk[0].p,sizeof trk[0].p);put(&lfo2_config[0],sizeof lfo2_config[0]);put(fm6_patch[0],FP_SIZE);
   trk_note_on(&trk[0],48,100);for(unsigned b=0;b<32;b++){int32_t o[CTL*2];mix_block(o,CTL);put(o,sizeof o);}trk_all_off(&trk[0]);
   for(unsigned b=0;b<64;b++){int32_t o[CTL*2];mix_block(o,CTL);put(o,sizeof o);}
   printf("engine=%u preset=%u %016llx\n",e,p,(unsigned long long)hash);
  }
 }
 return 0;
}
