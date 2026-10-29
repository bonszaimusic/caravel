/* SPDX-License-Identifier: GPL-3.0-only */
/* Captured engine/filter controls; BPM, ARP rate and STASIS window are
 * representative assumptions because diagnostic schema 1 omits them. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN arp_load_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
int main(int argc,char **argv){
 uint32_t held=argc>1?(uint32_t)atoi(argv[1]):2u;
 CHECK(held==1u || held==2u);
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(0,2,ENGI_SEM,ENGI_HADES));
 host_preset(&trk[1],ENGI_DRONE,9);run_ms(100);
 const int16_t edit[3][8]={{0,3,50,87,127,70,67,71},{78,62,58,5,93,17,102,69},{1,64,88,64,33,0,62,74}};
 const int16_t env[3][4]={{72,90,110,96},{105,90,120,115},{0,70,96,34}};
 const uint32_t eid[]={P_ATK,P_DEC,P_SUS,P_REL};
 const int16_t filter[3]={-48,-11,-56},rev[3]={83,78,92};
 for(uint32_t t=0;t<3;t++){
  for(uint32_t k=0;k<8;k++)trk[t].p[P_E0+k]=edit[t][k];
  for(uint32_t k=0;k<4;k++)trk[t].p[eid[k]]=env[t][k];
  trk[t].p[P_VOICE]=V_LEGATO;trk[t].p[P_FILT]=filter[t];
  trk[t].p[P_REV]=rev[t];trk[t].p[P_CHOR]=trk[t].p[P_DLY]=0;
 }
 trk[2].p[P_ITYPE]=1;trk[2].p[P_IMIX]=127;
 trk[1].p[P_M1SRC]=1;trk[1].p[P_M1DST]=13;trk[1].p[P_M1AMT]=12;
 trk[1].p[P_M2SRC]=11;trk[1].p[P_M2DST]=14;trk[1].p[P_M2AMT]=8;
 song.g[G_RTYPE]=3;song.g[G_RSIZE]=100;song.g[G_BPM]=120;
 song.g[G_DRLVL]=0;st_expand.quality=0;ep.base=0;
 song.g[G_ROUTE]=ROUTE_TRACK;run_ms(100);midi(0x90,48,100);run_ms(1800);
 song.g[G_DRLVL]=1;run_ms(100);CHECK(st.auto_hold);uint32_t writer=st.w;
 trk[1].p[P_AMODE]=1;trk[1].p[P_ARATE]=2;trk[1].p[P_AGATE]=64;
 midi(0x91,36,100);if(held==2u)midi(0x91,43,100);CHECK(trk[1].nheld==held);
 uint64_t hash=1469598103934665603ull;uint32_t changes=0,last=trk[1].arp_note;
 for(uint32_t ms=0;ms<6000;ms++){
  web_render(44);
  if(trk[1].arp_note!=last){changes++;last=trk[1].arp_note;}
  for(uint32_t i=0;i<44;i++){
   CHECK(isfinite(out_l[i]) && isfinite(out_r[i]));
   uint32_t l,r;memcpy(&l,out_l+i,4);memcpy(&r,out_r+i,4);
   hash=(hash^l)*1099511628211ull;hash=(hash^r)*1099511628211ull;
  }
 }
 CHECK(changes>10u && trk[1].nheld==held && st.w==writer);
 printf("SEM/HADES pair + TIDE internal ARP, %u held keys: changes=%u hash=%016llx\n",held,changes,(unsigned long long)hash);
 fprintf(stderr,"ARP LOAD PASS: %u checks. Host PCM identity is not a hardware CPU/deadline measurement.\n",checks);
 return 0;
}
