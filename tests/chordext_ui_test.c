/* SPDX-License-Identifier: GPL-3.0-only */
/* T17: BUILD data/input contract. Ring drawing belongs to the UI integration. */
#include <stdint.h>
#include <string.h>
#include <assert.h>
static int cx_hook(uint32_t f,void *out);
#define EXT_INFO_HOOK(f,out) cx_hook(f,out)
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
static ext_info_t cx_info;
static int cx_mock;
static int cx_hook(uint32_t f,void *out) {
 (void)f;if(!cx_mock)return 0;memcpy(out,&cx_info,sizeof cx_info);return 1;
}
static void chord_page(void) {
 for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].graph==GR_CHORD){ui.page=i;ui.home=0;page_entered();return;}
 assert(0);
}
int main(void) {
 ui_power_on();chord_page();harmony_on=0;
 assert(!chord_ext_row);scl_page_turn(1);assert(cur_page()->graph==GR_CHORD&&chord_ext_row);
 scl_page_turn(-1);assert(!chord_ext_row);assert(page_tap(B_SCL)&&chord_ext_row);
 uint32_t bottom=0;const dual_t *dd=dual_of(cur_page(),&bottom);
 assert(dd&&dd->kind==DK_CHEXT&&bottom==1&&str_eq(dd->hb,"CHORD EXT"));
 TSEL->p[P_CSRC]=1;TSEL->p[P_CEXT]=2;TSEL->p[P_COCT]=2;
 cx_mock=1;cx_info=(ext_info_t){.state=EXT_ST_OK,.src=1,.npc=3,.want=3,.pc={11,2,9}};
 int32_t v;const param_desc_t *d=dual_card(dd,0,1,0,&v);assert(str_eq(d->label,"SOURCE")&&v==1&&d->max==5);
 d=dual_card(dd,0,1,1,&v);assert(str_eq(d->label,"EXT")&&v==2&&str_eq(d->names[v],"7+9+13"));
 d=dual_card(dd,0,1,2,&v);assert(str_eq(d->label,"OCT")&&v==2);
 d=dual_card(dd,0,1,3,&v);assert(str_eq(d->label,"NOTES")&&v==EXT_ST_OK);
 chext_view_t view;chext_view(3,&view);assert(view.readonly&&!strcmp(view.value,"B D A")&&!strcmp(view.detail,"3 OF 3"));
 struct {uint8_t st,why,mono,n,w;const char *value,*detail;} cases[]={
  {EXT_ST_OFF,0,0,0,0,"--",""},
  {EXT_ST_OK,0,0,3,3,"B D A","3 OF 3"},
  {EXT_ST_WAIT,EXT_WHY_PLAY,0,0,0,"WAIT","PLAY"},
  {EXT_ST_WAITBUS,EXT_WHY_BUSOFF,0,0,0,"WAIT BUS","BUS OFF"},
  {EXT_ST_AMBIG,EXT_WHY_KEPT,0,0,0,"AMBIG","KEPT"},
  {EXT_ST_EMPTY,EXT_WHY_INSRC,0,0,3,"EMPTY","IN SRC"},
  {EXT_ST_NA,EXT_WHY_PAIR,0,0,0,"N/A","PAIR"},
  {EXT_ST_OK,0,1,3,3,"B D A","1 OF 3"},
  {EXT_ST_OK,0,0,2,3,"B D","2 OF 3"}};
 for(uint32_t i=0;i<NELEM(cases);i++) {
  cx_info.state=cases[i].st;cx_info.why=cases[i].why;cx_info.mono=cases[i].mono;cx_info.npc=cases[i].n;cx_info.want=cases[i].w;
  chext_view(3,&view);assert(view.readonly&&!strcmp(view.value,cases[i].value)&&!strcmp(view.detail,cases[i].detail));
 }
 cx_info.state=EXT_ST_WAIT;cx_info.why=EXT_WHY_PLAY;cx_info.src=2;
 chext_view(3,&view);assert(!strcmp(view.value,"WAIT")&&!strcmp(view.detail,"PLAY T1"));
 cx_info.src=5;chext_view(3,&view);assert(!strcmp(view.detail,"PLAY T4"));
 cx_info.src=1;
 cx_info.state=EXT_ST_OFF;TSEL->p[P_CSRC]=0;TSEL->p[P_CEXT]=0;TSEL->p[P_COCT]=0;
 int16_t ch=TSEL->p[P_CHRD],vo=TSEL->p[P_VOIC],la=TSEL->p[P_LATCH];
 ui.ml.on=1;edit_param(0,1);assert(TSEL->p[P_CSRC]==1&&ui.ml.pick&&ui.ml.id==P_CSRC);
 edit_param(1,-1);assert(TSEL->p[P_CEXT]==1&&ui.ml.id==P_CEXT);
 edit_param(1,1);assert(TSEL->p[P_CEXT]==0);
 edit_param(1,1);assert(TSEL->p[P_CEXT]==2);
 edit_param(2,-20);assert(TSEL->p[P_COCT]==-2&&ui.ml.id==P_COCT);
 ui.ml.pick=0;edit_param(3,100);assert(!ui.ml.pick&&TSEL->p[P_COCT]==-2);
 cx_info.state=EXT_ST_NA;edit_param(1,-1);edit_param(2,1);assert(TSEL->p[P_CEXT]==2&&TSEL->p[P_COCT]==-2);
 edit_param(0,-1);assert(TSEL->p[P_CSRC]==0);
 assert(TSEL->p[P_CHRD]==ch&&TSEL->p[P_VOIC]==vo&&TSEL->p[P_LATCH]==la);
 chord_page();assert(!chord_ext_row);
 puts("CHORD EXT UI data: nine states, actual notes/counts, dual_card row, SCL navigation, K1-K3, read-only K4, N/A and MIDI Learn passed");
}
