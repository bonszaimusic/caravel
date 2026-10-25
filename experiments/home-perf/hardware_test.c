/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define WEB_FLASH_TEST 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static hp_model_t model_a,model_b;
static hp_pair_store_t pair_a,pair_b;
static uint8_t packed[HP_HW_META],packed2[HP_HW_META],flash_copy[0x100000];
static void taps(uint32_t b){web_buttons(1u<<b);run_ms(70);web_buttons(0);run_ms(160);}
static void turn(uint32_t en,int s){web_enc(en,s);run_ms(60);}
static void page_is(const char*s){CHECK(!ui.home&&str_eq(cur_page()->title,s));}
#ifndef HARDWARE_TEST_MAIN
#define HARDWARE_TEST_MAIN main
#endif
int HARDWARE_TEST_MAIN(void){
 web_nor_erase();web_boot();run_ms(1400);CHECK(sizeof nor==0x100000);CHECK(HP_HW_META==2934);CHECK(HP_HW_DATA==6774);
 /* Maximum layouts, all ref fields, signed macro amounts and every event; overlap expansion. */
 for(unsigned n=0;n<80;n++){
  memset(&model_a,0,sizeof model_a);memset(&pair_a,0,sizeof pair_a);
  for(unsigned t=0;t<4;t++){
   model_a.track[t].count=32;model_a.track[t].active=(n+t)%32;
   for(unsigned row=0;row<32;row++)for(unsigned k=0;k<4;k++)model_a.track[t].cell[row][k]=(hp_ref_t){(n+row*9+k)%2048,((n+row)%2)?0xFFFF:(n+k)%NPAGES,(n+k)%8,((row+k)%5==4)?255:(row+k)%4,((row+n)%18==17)?255:(row+n)%17,(row+k)%16};
   for(unsigned m=0;m<4;m++){model_a.macro[t][m].base=(n+t+m)%128;for(unsigned j=0;j<4;j++)model_a.macro[t][m].route[j]=(hp_route_t){model_a.track[t].cell[m][j],(int)((n+m*9+j)%128)-64,0};}
  }
  model_a.events=128;for(unsigned i=0;i<128;i++)model_a.event[i]=(hp_event_t){i%4,i%64,(i%6<4)?i%6:6+i%2,i%2,(i+n)%128,0};
  model_a.pair_base=n*397;model_a.pair_motion=n%2;model_a.pair_valid=1;pair_a.on=1;pair_a.a=0;pair_a.b=3;pair_a.base=n*397;for(unsigned i=0;i<64;i++)pair_a.lane[i]=(i%3)?i*400:-1;
  hp_meta_pack(packed,&model_a,&pair_a);memset(&hp_io,0xAA,sizeof hp_io);memcpy(&hp_io.model,packed,sizeof packed);hp_meta_unpack(&hp_io.model,&hp_io.pair,(const uint8_t*)&hp_io.model);
  CHECK(!memcmp(&model_a,&hp_io.model,sizeof model_a));CHECK(!memcmp(&pair_a,&hp_io.pair,sizeof pair_a));hp_meta_pack(packed2,&hp_io.model,&hp_io.pair);CHECK(!memcmp(packed,packed2,sizeof packed));
 }
 /* Fixed navigation rows display their actual behavior and retain legacy preferences. */
 uint32_t pref=ui_prefs,pref2=ui_prefs2,view=ui_home_view;menu_put(MI_BPMLOCK,1);menu_put(MI_SELECT,3);menu_put(MI_HOME,2);CHECK(ui_prefs==pref&&ui_prefs2==pref2&&ui_home_view==view);CHECK(menu_n(MI_BPMLOCK)==1&&str_eq(menu_vname(MI_BPMLOCK,0),"GLO+SEL"));
 /* Actual button workflow; saved legacy SELECT preferences cannot hijack navigation. */
 hp_apply(0,2);go_home();for(unsigned i=0;i<4;i++){ui_prefs2=(ui_prefs2&~0x50)|(i&1?0x10:0)|(i&2?0x40:0);int bpm=song.g[G_BPM];turn(EN_SELECT,1);CHECK(ui.home&&hp.track[0].active==1&&song.g[G_BPM]==bpm);taps(B_HOME);CHECK(ui.home);taps(B_HOME);CHECK(ui.home);turn(EN_SELECT,-1);CHECK(hp.track[0].active==0);}
 taps(B_GLO);page_is("MIXER");turn(EN_SELECT,1);page_is("CLOCK");turn(EN_SELECT,1);page_is("SONG");turn(EN_SELECT,8);page_is("SONG");turn(EN_SELECT,-8);page_is("MIXER");taps(B_HOME);CHECK(ui.home);
 int bpm=song.g[G_BPM];web_buttons(1u<<B_GLO);run_ms(80);turn(EN_SELECT,2);web_buttons(0);run_ms(180);CHECK(song.g[G_BPM]>bpm&&ui.home);
 web_buttons(1u<<B_HOME);run_ms(900);web_buttons(0);run_ms(160);CHECK(ui.menu);turn(EN_SELECT,1);CHECK(ui.menu);taps(B_HOME);CHECK(!ui.menu&&ui.home);
 hp_open_editor();hp_apply(0,2);turn(EN_SELECT,1);CHECK(cur_page()->graph==GR_HOMEEDIT&&hp_edit_row==1);turn(EN_SELECT,99);CHECK(cur_page()->graph==GR_HOMEEDIT&&hp_edit_row==1);go_home();
 /* A-D are complete sound + layout snapshots on the real one MiB address space. */
 hp_default=2;hp_autosave_poll();for(unsigned slot=0;slot<4;slot++){hp_apply(0,slot%3);trk[0].p[P_FILT]=-10-(int)slot;hp.macro[0][0].base=slot*25;CHECK(!hp_project_save(slot,0));CHECK(!hp_pending_mask);}
 hp_pending_mask=0;hp_init();hp_autosave_boot(0);CHECK(hp_default==2);for(unsigned slot=0;slot<4;slot++){CHECK(hp_project_load(slot));CHECK(hp.track[0].count==(slot%3==2?2:1));CHECK(trk[0].p[P_FILT]==-10-(int)slot);CHECK(hp.macro[0][0].base==(int)slot*25);}
 CHECK(web_home_export(0)==10000);memcpy(&model_b,&hp_io.model,sizeof model_b);CHECK(web_home_import(10000));CHECK(!memcmp(&hp,&model_b,sizeof hp));
 /* Renaming changes only the stored name; the working sound is independent. */
 CHECK(hp_project_load(1));int oldfilter=trk[0].p[P_FILT];CHECK(!project_rename(0,"V4 RENAMED"));CHECK(trk[0].p[P_FILT]==oldfilter);CHECK(hp_project_load(0));CHECK(str_eq(proj_name,"V4 RENAMED")&&hp.macro[0][0].base==0);
 /* No metadata crosses the protected updater record tails or reserved per-copy gap. */
 for(unsigned sec=0xE8000;sec<0xFC000;sec+=4096){for(unsigned i=0xF00;i<0x1000;i++)CHECK(nor[sec+i]==255);}CHECK(nor[0xFC000]!=HP_HW_MAGIC%256);
 /* Every failed erase/program before commit leaves the previous complete copy readable. */
 CHECK(hp_project_load(0));hp_apply(0,2);hp.macro[0][0].base=111;hp_capture(&hp_io);hp_meta_pack(packed,&hp_io.model,&hp_io.pair);project_store_t native=hp_io.native;memcpy(flash_copy,nor,sizeof nor);
 for(int cut=0;cut<33;cut++){memcpy(nor,flash_copy,sizeof nor);web_flash_fail_after=cut;int ok=hp_hw_commit(0,packed,&native,0);web_flash_fail_after=-1;const hp_store_t*p=hp_slot_disk(0);CHECK(p);CHECK(p->model.macro[0][0].base==(ok?111:0));CHECK(p->model.track[0].count==(ok?2:1));}
 memcpy(nor,flash_copy,sizeof nor);CHECK(hp_hw_commit(0,packed,&native,0));hp_hw_header_t h;int c=hp_hw_current(0,&h);nor[hp_hw_addr(0,c)+hp_hw_piece(4000)]^=1;CHECK(hp_slot_disk(0)->model.macro[0][0].base==0);
 /* A torn header at any byte is either the old copy or the new complete copy. */
 memcpy(nor,flash_copy,sizeof nor);CHECK(hp_hw_commit(0,packed,&native,0));hp_hw_header_t complete;int newest=hp_hw_current(0,&complete);uint32_t headaddr=hp_hw_addr(0,newest);uint8_t completehead[32];memcpy(completehead,nor+headaddr,32);
 for(unsigned bytes=0;bytes<=32;bytes++){memset(nor+headaddr,255,32);memcpy(nor+headaddr,completehead,bytes);const hp_store_t*p=hp_slot_disk(0);CHECK(p);CHECK(p->model.macro[0][0].base==(bytes==32?111:0));}
 /* Native slots remain renameable; extension tombstones do not block them. */
 CHECK(!hp_forget(3));CHECK(!project_rename(3,"LEGACY"));char named[PROJ_NAME_LEN+1];CHECK(project_info(3,named,0,0,0)&&str_eq(named,"LEGACY"));
 /* Live save is acknowledged as RAM only until STOP; failure retains the retry snapshot. */
 CHECK(hp_project_load(1));song.playing=1;CHECK(!hp_project_save(2,0));CHECK(hp_pending_mask==4);song.playing=0;web_flash_fail_after=0;hp_autosave_poll();CHECK(hp_pending_mask==4);web_flash_fail_after=-1;fm1_ms+=1100;hp_autosave_poll();CHECK(!hp_pending_mask);CHECK(hp_project_load(2)&&hp.macro[0][0].base==25);
 CHECK(!hp_forget(2));CHECK(!hp_slot_disk(2));hp_hw_header_t tomb;CHECK(hp_hw_current(2,&tomb)>=0&&tomb.flags==1);
 /* Full autosave including pair, macros, slots and hardware preferences. */
 web_pair_panic();run_ms(900);CHECK(web_pair_prepare(0,2,ENGI_SEM,ENGI_DRONE));run_ms(30);hp_apply(3,2);hp.macro[3][2].base=73;hp_event_put(3,7,2,99,1);hp_dirty=1;hp_as_saved=0;hp_as_time=0;hp_as_seen=hp_signature();as.t=0;fm1_ms+=12000;hp_autosave_poll();CHECK(hp_slot_disk(4));pair_disable();hp_init();CHECK(hp_autosave_boot(1));CHECK(ep.on&&ep.a==0&&ep.b==2&&hp.track[3].count==2&&hp.macro[3][2].base==73&&hp.events==1);
 /* Freestanding formatter matches the forms the performance pages use. */
 char actual[64],expected[64];perf_format(actual,sizeof actual,"%s %u %+d %%","MAC",32u,-64);snprintf(expected,sizeof expected,"%s %u %+d %%","MAC",32u,-64);CHECK(str_eq(actual,expected));char small[5];CHECK(perf_format(small,sizeof small,"%s","long label")==10&&str_eq(small,"long"));
 printf("HARDWARE BACKEND: %u checks passed: 80 full in-place codec roundtrips, navigation buttons, A-D/reload, HPM1, protected tails, 33 power-cut positions, CRC fallback, RAM/STOP retry and tombstone.\n",checks);return 0;
}
