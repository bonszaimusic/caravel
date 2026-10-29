/* SPDX-License-Identifier: GPL-3.0-only */
/* Report audio identity and voice ownership for SEM pair T1/T3 plus DRONE T2.
 * Host timing is deliberately not presented as FM-1 CPU load. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN three_track_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static uint64_t hash;
static uint32_t peak,jump,curve;
static int32_t previous[2],slope[2];
static void render_blocks(uint32_t blocks){
 int32_t out[CTL*2];
 while(blocks--){
  mix_block(out,CTL);
  for(uint32_t i=0;i<CTL*2;i++){
   CHECK(out[i]>=-32768 && out[i]<=32767);
   hash=(hash^(uint32_t)out[i])*1099511628211ull;
   uint32_t ch=i&1u;int32_t d=out[i]-previous[ch];
   uint32_t a=abs(out[i]),b=abs(d),c=abs(d-slope[ch]);
   if(a>peak)peak=a;if(b>jump)jump=b;if(c>curve)curve=c;
   previous[ch]=out[i];slope[ch]=d;
  }
 }
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);
 const uint32_t presets[]={0,5,9};
 for(uint32_t quality=0;quality<3;quality++)for(uint32_t p=0;p<3;p++){
  normal_off();CHECK(web_pair_prepare(0,2,ENGI_SEM,ENGI_SEM));
  host_preset(&trk[0],ENGI_SEM,1);host_preset(&trk[2],ENGI_SEM,4);
  host_preset(&trk[1],ENGI_DRONE,presets[p]);run_ms(80);
  for(uint32_t t=0;t<3;t++){
   trk[t].p[P_VOICE]=V_MONO;trk[t].p[P_LEVEL]=88;
   trk[t].p[P_CHOR]=trk[t].p[P_DLY]=trk[t].p[P_DIST]=0;
   trk[t].p[P_REV]=70;
  }
  song.g[G_RTYPE]=3;song.g[G_RSIZE]=100;song.g[G_DRLVL]=0;
  st_expand.quality=quality;st_expand.grain=1;ep.base=16384;
  hash=1469598103934665603ull;peak=jump=curve=0;
  memset(previous,0,sizeof previous);memset(slope,0,sizeof slope);
  trk_note_on(&trk[0],48,100);render_blocks(2u*FS/CTL);
  CHECK(gates(0)==1u && gates(2)==1u);
  song.g[G_DRLVL]=1;render_blocks(FS/4u/CTL);CHECK(st.auto_hold);
  uint32_t writer=st.w;
  trk_note_on(&trk[1],36,100);render_blocks(FS/CTL);
  CHECK(gates(0)==1u && gates(1)==1u && gates(2)==1u);CHECK(st.w==writer);
  CHECK(voices_busy()==3u);
  trk_note_off(&trk[1],36);render_blocks(FS/4u/CTL);
  trk_note_on(&trk[1],43,110);render_blocks(FS/CTL);
  CHECK(gates(1)==1u && st.w==writer && st.auto_hold);
  printf("SEM pair T1/T3 + DRONE T2 preset=%u window=%u: hash=%016llx peak=%u jump=%u curve=%u\n",presets[p],quality,(unsigned long long)hash,peak,jump,curve);
 }
#if FELUCCA_CPU2
 if(!c1h_off)CHECK(c1h_taken>0);
#endif
 fprintf(stderr,"THREE TRACK PASS: %u sample/ownership checks; all three voices playable over held STASIS. Host rendering cannot establish MCU deadlines.\n",checks);
 return 0;
}
