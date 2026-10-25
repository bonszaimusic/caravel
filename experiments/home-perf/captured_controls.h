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
