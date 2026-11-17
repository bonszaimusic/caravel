/* SPDX-License-Identifier: GPL-3.0-only */
/* FORMAT 2 settings/persistence successor to the legacy CHORD settings fixture.
 * No legacy project, removed-engine or Song/Chain import is supported. */
#define FORMAT2_EDITOR_MAIN chordset_editor_fixture_main
#include "format2_editor_test.c"
static void cs_settings(track_t *t,int s,int e,int o) { t->p[P_CSRC]=s;t->p[P_CEXT]=e;t->p[P_COCT]=o; }
static int same(const track_t *t,int s,int e,int o) { return t->p[P_CSRC]==s&&t->p[P_CEXT]==e&&t->p[P_COCT]==o; }
int main(void) {
 web_nor_erase();web_boot();for(uint32_t i=0;i<1200;i++)web_render(44);
 assert(P_CSRC==76&&P_CEXT==77&&P_COCT==79&&P_COUNT==114&&PROJ_STORE_SIZE==2784);
 assert(UP_BANK_MAGIC==0x32425055u&&HP_HW_MAGIC==0x32504649u&&HP_V2_SIZE==6144);
 assert(TP[P_CSRC].min==0&&TP[P_CSRC].max==5&&TP[P_CSRC].def==0);
 assert(TP[P_CEXT].max==2&&TP[P_CEXT].def==0&&TP[P_COCT].min==-2&&TP[P_COCT].max==2);
 assert(page_desc(&UXL_LY_SCL,0,0)==page_desc(&UXL_LY_SCL,0,&(int16_t *){0}));
 const uint32_t ids[]={P_CSRC,P_CEXT,P_COCT};
 for(uint32_t t=0;t<NTRK;t++)assert(same(&trk[t],0,0,0));
 for(uint32_t i=0;i<3;i++) {
  assert(param_kept(ids[i])&&!motion_param(ids[i])&&!motion_engine_id(ids[i]));
  assert(!motion_capture(TSEL,ids[i],1));
  assert(ml_code(ids[i])==ids[i]+1u);
 }
 assert(param_fit(&TP[P_CSRC],-1)==0&&param_fit(&TP[P_CSRC],6)==0&&param_fit(&TP[P_CSRC],127)==0);
 assert(param_fit(&TP[P_COCT],-64)==-2&&param_fit(&TP[P_COCT],127)==2);
 uint32_t presets=0;
 for(uint32_t e=0;e<6;e++) {
  cs_settings(TSEL,1,2,-2);set_engine_of(TSEL,eng_unstore(e));
  assert(same(TSEL,1,2,-2));
  for(uint32_t p=0;p<ENGINES[TSEL->eng_req]->npresets;p++) {
   apply_preset_to(TSEL,p);assert(same(TSEL,1,2,-2));presets++;
  }
  uint8_t a[]={e,0};request(ED_PRESET,a,sizeof a);assert(reply_n&&same(TSEL,1,2,-2));
 }
 assert(presets==67);
 assert(!up_store(0,"EXT TEST"));int16_t values[P_COUNT];up_values(up_rec(0),values);
 assert(values[P_CSRC]==1&&values[P_CEXT]==2&&values[P_COCT]==-2);
 cs_settings(TSEL,0,1,2);assert(!up_load(0)&&same(TSEL,0,1,2));
 static up_bank_t bank;assert(st_load(OBJ_UPRESET0,&bank,sizeof bank)==sizeof bank);
 ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof bank;memcpy(ED_BK_RAW,&bank,sizeof bank);ed_bk_crc=st_crc32(&bank,sizeof bank);
 memset(up_bank,0,sizeof up_bank);assert(!ed_bk_commit()&&up_used(0));
 for(uint32_t t=0;t<NTRK;t++)cs_settings(&trk[t],1,2,-2);
 static project_t p,q;static project_store_t wire;
 link_retrig[0]=3;link_retrig[1]=1;link_retrig[2]=2;link_retrig[3]=0;
 project_capture(&p);assert(proj_pack(&wire,&p)&&proj_import(&q,wire.raw,sizeof wire));
 assert(!memcmp(p.t,q.t,sizeof p.t));
 assert(proj_harmony.link_retrig[0]==3&&proj_harmony.link_retrig[1]==1&&proj_harmony.link_retrig[2]==2);
 static hp_store_t cap;assert(hp_capture(&cap));seal(&cap);assert(hp_capsule_ok(&cap));
 for(uint32_t t=0;t<NTRK;t++)cs_settings(&trk[t],0,0,0);
 assert(hp_restore(&cap));for(uint32_t t=0;t<NTRK;t++)assert(same(&trk[t],1,2,-2));
 assert(!hp_project_save(0,0));for(uint32_t t=0;t<NTRK;t++)cs_settings(&trk[t],0,0,0);
 assert(hp_project_load(0));for(uint32_t t=0;t<NTRK;t++)assert(same(&trk[t],1,2,-2));
 /* The backup handler restores the complete container, not just a native block. */
 for(uint32_t t=0;t<NTRK;t++)cs_settings(&trk[t],0,0,0);
 ed_bk_id=0;ed_bk_len=ed_bk_pos=cap.size;memcpy(ED_BK_RAW,&cap,cap.size);ed_bk_crc=st_crc32(&cap,cap.size);
 assert(!ed_bk_commit());for(uint32_t t=0;t<NTRK;t++)assert(same(&trk[t],1,2,-2));
 project_init_runtime();for(uint32_t t=0;t<NTRK;t++)assert(same(&trk[t],0,0,0));
 puts("CHORD settings FORMAT 2: ids, fresh defaults, 67 preset keeps, editor, MIDI Learn codes, motion exclusion, FUNW/UPB2/IFP2 and complete backup passed");
}
