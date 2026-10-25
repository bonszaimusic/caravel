/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproduce captured per-track controls, not an unavailable full project/global setup. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
typedef struct {int eng,pre,mode,fx[4],it,im,filter,l1[4],l2[4],ld[4],edit[8],env[4],mod[12];} settings_t;
static const settings_t captured[2]={
{15,3,2,{54,55,26,51},1,127,-16,{18,0,0,0},{0,0,0,0},{0,0,0,0},{0,64,100,68,72,8,0,64},{0,74,112,38},{1,25,19,0,0,0,0,0,0,0,0,0}},
{16,9,2,{0,0,0,0},0,127,14,{0,0,0,0},{2,0,0,0},{0,0,0,0},{78,87,24,5,0,23,127,59},{105,90,120,115},{1,13,12,11,14,8,0,0,0,0,0,0}}
};
static void apply(track_t *t,const settings_t *s){
 host_preset(t,s->eng,s->pre);t->p[P_VOICE]=s->mode;
 const int fx[]={P_DIST,P_CHOR,P_DLY,P_REV};for(unsigned j=0;j<4;j++)t->p[fx[j]]=s->fx[j];
 t->p[P_ITYPE]=s->it;t->p[P_IMIX]=s->im;t->p[P_FILT]=s->filter;
 const int l1[]={P_LRATE,P_LWAVE,P_LFADE,P_LPOL},ld[]={P_LD_PIT,P_LD_FLT,P_LD_SHP,P_LD_AMP};
 for(unsigned j=0;j<4;j++){t->p[l1[j]]=s->l1[j];t->p[ld[j]]=s->ld[j];lfo2_config[t-trk].p[j]=s->l2[j];}
 for(unsigned j=0;j<8;j++)t->p[P_E0+j]=s->edit[j];
 const int en[]={P_ATK,P_DEC,P_SUS,P_REL};for(unsigned j=0;j<4;j++)t->p[en[j]]=s->env[j];
 for(unsigned j=0;j<12;j++)t->p[P_M1SRC+j]=s->mod[j];
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);unsigned cases=0;
 for(unsigned rev=0;rev<2;rev++)for(unsigned variant=0;variant<4;variant++)for(unsigned capture=0;capture<2;capture++){
  normal_off();CHECK(web_pair_prepare(0,1,captured[rev].eng,captured[1-rev].eng));
  apply(&trk[0],&captured[rev]);apply(&trk[1],&captured[1-rev]);ep.base=14706;ep.motion=0;
  if(variant==1)trk[0].p[P_FILT]=trk[1].p[P_FILT]=0;
  if(variant==2){for(unsigned t=0;t<2;t++){trk[t].p[P_ITYPE]=0;trk[t].p[P_DIST]=0;}}
  run_ms(20);pd_active=capture;pd.seconds=60;pd.start_ms=fm1_ms;
  web_keys(1u);uint64_t h=1469598103934665603ull;
  for(unsigned ms=0;ms<2400;ms++){
   if(ms==400)ep.base=0;if(ms==800)ep.base=32767;if(ms==1200)ep.base=16384;
   if(variant==3&&ms==500)trk_note_on(&trk[2],60,100);
   if(variant==3&&ms==1500)trk_note_off(&trk[2],60);
   if(ms==1600)web_keys(0);
   if(ms==1900){trk[0].p[P_FILT]=0;trk[1].p[P_FILT]=0;trk[0].p[P_ITYPE]=0;trk[1].p[P_ITYPE]=0;}
   web_render(44);
   for(unsigned j=0;j<44;j++){uint32_t l,r;CHECK(isfinite(out_l[j])&&isfinite(out_r[j]));memcpy(&l,&out_l[j],4);memcpy(&r,&out_r[j],4);h=(h^l)*1099511628211ull;h=(h^r)*1099511628211ull;}
  }
  pd_active=0;printf("reverse=%u variant=%u capture=%u hash=%016llx\n",rev,variant,capture,(unsigned long long)h);cases++;
 }
 fprintf(stderr,"CAPTURED CONTROL AUDIO: %u cases; globals/default routing not captured in USB log.\n",cases);return 0;
}
