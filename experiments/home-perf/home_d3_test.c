/* SPDX-License-Identifier: GPL-3.0-only */
#include "captured_controls.h"
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 unsigned presets=0;
 for(unsigned e=0;e<NENGINES;e++)if(eng_ok(e)&&ENGINES[e]->npresets){
  set_engine_of(&trk[0],e);
  for(unsigned p=0;p<ENGINES[e]->npresets;p++){
   apply_preset_to(&trk[0],p);
   if(hp_sound_changed(&trk[0])){fprintf(stderr,"fresh preset marked: %u %s %u\n",e,ENGINES[e]->name,p);return 1;}
   int16_t keep=trk[0].p[P_ATK];trk[0].p[P_ATK]=keep?keep-1:1;CHECK(hp_sound_changed(&trk[0]));trk[0].p[P_ATK]=keep;CHECK(!hp_sound_changed(&trk[0]));
   trk[0].p[P_LEVEL]=11;trk[0].p[P_PAN]=-20;trk[0].p[P_MUTE]=1;trk[0].step[0].note[0]=55;
   trk[0].lfo_ph++;lfo2_state[0].ph++;CHECK(!hp_sound_changed(&trk[0]));trk[0].p[P_MUTE]=0;
   uint8_t rate=lfo2_config[0].p[L2_RATE];lfo2_config[0].p[L2_RATE]=(rate+1)%128;CHECK(hp_sound_changed(&trk[0]));lfo2_config[0].p[L2_RATE]=rate;
   presets++;
  }
 }
 for(unsigned e=0;e<3;e++){
  set_engine_of(&trk[0],e==0?ENGI_HADES:e==1?ENGI_FM6:ENGI_DRUM);if(e==2)apply_preset_to(&trk[0],4);
  CHECK(up_store(0,"MY SOUND")!=2);CHECK(!up_load(0));CHECK(!hp_sound_changed(&trk[0]));
  trk[0].p[P_FILT]=21;CHECK(hp_sound_changed(&trk[0]));CHECK(!hp_project_save(e,0));trk[0].p[P_FILT]=0;CHECK(hp_project_load(e));CHECK(hp_sound_changed(&trk[0]));
  CHECK(up_store(0,"EDITED")!=2);CHECK(!hp_sound_changed(&trk[0]));
 }
 set_engine_of(&trk[0],ENGI_FM6);apply_preset_to(&trk[0],0);uint8_t before=fm6_patch[0][FP_OL];fm6_patch[0][FP_OL]=before?before-1:1;CHECK(hp_sound_changed(&trk[0]));fm6_patch[0][FP_OL]=before;CHECK(!hp_sound_changed(&trk[0]));
 hp_apply(0,2);CHECK(!strcmp(hp_row_name(&hp.track[0],0),"SOUND"));CHECK(!strcmp(hp_row_name(&hp.track[0],1),"MIX"));
 hp_ref_t v=hp.track[0].cell[0][0];hp.track[0].cell[0][0]=(hp_ref_t){.kind=HP_GLOBAL,.id=G_BPM,.track=0};go_home();ui.msg_t=0;run_ms(30);hp.track[0].active=1;run_ms(30);hp.track[0].cell[0][0]=v;
 hp_model_t model={0},decoded={0};hp_pair_store_t ps={0},ps2={0};uint8_t packed[HP_HW_META];
 for(unsigned t=0;t<NTRK;t++){model.track[t].count=32;model.track[t].active=31;model.track[t].pad[0]=t?UP_SLOTS:0;}
 hp_meta_pack(packed,&model,&ps);hp_meta_unpack(&decoded,&ps2,packed);CHECK(!memcmp(&model,&decoded,sizeof model));
 CHECK(hp_model_valid(&decoded));decoded.track[1].pad[0]=63;CHECK(!hp_model_valid(&decoded));
 printf("D3: %u factory presets clean; edits/revert, LFO-state exclusion, user saves, HPM restore, FM6 patch, foreign/global cards passed (%u checks).\n",presets,checks);
 return 0;
}
