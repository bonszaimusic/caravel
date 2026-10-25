/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap_hp(uint32_t b){web_buttons(1u<<b);run_ms(80);web_buttons(0);run_ms(180);}
static void shot(const char*name){char p[200];snprintf(p,sizeof p,"build/home-perf/%s.ppm",name);FILE*f=fopen(p,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");for(uint32_t i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);uint8_t b[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(b,1,3,f);}fclose(f);}
int main(void)
{
 web_nor_erase();web_boot();run_ms(1400);CHECK(hp.track[0].count==1);CHECK(hp_default==1);CHECK(P_COUNT==114);CHECK(sizeof(hp_store_t)<HP_COPY);
 for(uint32_t tab=0;tab<MTAB_COUNT;tab++){uint32_t first=mtab_first(tab),n=mtab_rows(tab);CHECK(n<=MTAB_ROWS);for(uint32_t j=0;j<n;j++)CHECK(MI_TAB[first+j]==tab);}
 CHECK(menu_valued(MI_HPDEFAULT)&&!menu_valued(MI_HPAPPLY)&&!menu_valued(MI_HPEDIT));menu_put(MI_HPDEFAULT,2);CHECK(hp_default==2);menu_put(MI_HPDEFAULT,1);
 /* Actual source-page -> GLO+HOME capture, with no accidental menu/song/level change. */
 hp_apply(0,0);tap_hp(B_ENV);CHECK(cur_page()->fam==FAM_ENV&&!ui.home);
 int16_t before=trk[0].p[P_ATK],level=trk[0].p[P_LEVEL];web_enc(EN_K1,3);run_ms(60);CHECK(trk[0].p[P_ATK]>before);int16_t capture=trk[0].p[P_ATK];
 web_buttons(1u<<B_GLO);run_ms(70);web_buttons((1u<<B_GLO)|(1u<<B_HOME));run_ms(60);web_buttons(0);run_ms(350);
 CHECK(!ui.home&&!ui.menu&&!ui.layer);CHECK(hp.track[0].cell[0][0].kind==HP_TRACK);CHECK(hp.track[0].cell[0][0].id==P_ATK);CHECK(trk[0].p[P_ATK]==capture&&trk[0].p[P_LEVEL]==level);
 tap_hp(B_HOME);CHECK(ui.home);web_enc(EN_K1,2);run_ms(50);CHECK(trk[0].p[P_ATK]>capture);
 /* Per-track independence and retained row; all 32 rows navigable. */
 for(uint32_t r=1;r<32;r++)CHECK(web_home_assign(0,r,0,HP_TRACK,0,P_FILT));
 web_enc(EN_SELECT,31);run_ms(50);CHECK(hp.track[0].active==31);web_enc(EN_ALGO,1);run_ms(50);CHECK(song.sel==1&&hp.track[1].active==0);web_enc(EN_ALGO,-1);run_ms(50);CHECK(song.sel==0&&hp.track[0].active==31);
 CHECK(!web_home_assign(0,32,0,HP_TRACK,0,P_LEVEL));hp_apply(0,2);go_home();ui.msg_t=0;run_ms(60);shot("home-two-rows");
 /* Fixed foreign target; ordinary capture must not become motion on the wrong track. */
 CHECK(web_home_assign(0,0,0,HP_TRACK,2,P_LEVEL));hp.track[0].active=0;int16_t own=trk[0].p[P_LEVEL],other=trk[2].p[P_LEVEL];web_enc(EN_K1,-1);run_ms(60);CHECK(trk[0].p[P_LEVEL]==own&&trk[2].p[P_LEVEL]<other);CHECK(hp_lock_id(0)==255);
 /* Engine guard prevents E-slot reinterpretation. */
 CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_E4));hp_ref_t guarded=hp.track[0].cell[0][1];CHECK(hp_desc(guarded));uint8_t e=trk[0].eng_req;trk[0].eng_req=ENGI_SEM;CHECK(!hp_desc(guarded));trk[0].eng_req=e;
 /* Structural edits have undo; deleting a cell does not alter the sound. */
 hp_open_editor();hp_edit_row=0;hp_edit_col=0;hp_edit_action=1;own=trk[2].p[P_LEVEL];hp_editor_apply();CHECK(!hp.track[0].cell[0][0].kind&&trk[2].p[P_LEVEL]==own);hp_edit_action=8;hp_editor_apply();CHECK(hp.track[0].cell[0][0].kind==HP_TRACK);
 hp_edit_action=6;hp_editor_apply();CHECK(hp.track[0].count==3);hp_edit_action=7;hp_editor_apply();CHECK(hp.track[0].count==2);
 /* PAIR is in ENV; MACRO and map have non-HOME source pages. */
 hp_open_graph(GR_PAIR);CHECK(cur_page()->fam==FAM_ENV);hp_open_graph(GR_MACRO);CHECK(cur_page()->fam==FAM_ENV);hp_page_knob(0,32);CHECK(hp.macro[0][0].base==32);
 CHECK(web_home_macro_route(0,0,0,P_FILT,32));int16_t base=trk[0].p[P_FILT];hp_begin(&trk[0]);CHECK(trk[0].p[P_FILT]>base);hp_end(&trk[0]);CHECK(trk[0].p[P_FILT]==base);
 /* Two macros sum once; repeated blocks never bake offsets into the patch. */
 CHECK(web_home_macro_route(0,1,0,P_FILT,-16));web_home_macro_set(0,1,64);for(uint32_t n=0;n<50;n++){int32_t b[64];mix_block(b,32);CHECK(trk[0].p[P_FILT]==base);}
 /* MIDI Learn targets the actual macro, including persistence-safe virtual ID. */
 CHECK(ml_learn(22,0,HP_VFIRST)==1);midi(0xB0,22,100);CHECK(hp.macro[0][0].base==100);
 /* Macro automation and locks have their own event namespace. */
 hp_event_put(0,0,0,17,0);hp_event_put(0,2,0,99,1);hp_motion_step(&trk[0],2);CHECK(hp_macro_live[0][0]==99);hp_motion_step(&trk[0],3);CHECK(hp_macro_live[0][0]==17);hp_motion_stop();CHECK(hp_macro_live[0][0]==100);
 /* Complete A-D capsule: different layouts, macros and sound; reload and checksum validation. */
 CHECK(!hp_project_save(0,0));hp_apply(0,0);hp.macro[0][0].base=0;trk[0].p[P_FILT]=-20;CHECK(!hp_project_save(1,0));CHECK(hp_project_load(0));CHECK(hp.track[0].count==2&&hp.macro[0][0].base==100&&trk[0].p[P_FILT]==base);CHECK(hp_project_load(1));CHECK(!hp.track[0].cell[0][0].kind&&trk[0].p[P_FILT]==-20);
 CHECK(hp_project_load(0));uint32_t size=web_home_export(0);CHECK(size==sizeof hp_io);hp_io.model.track[0].count=255;CHECK(!web_home_import(size));CHECK(hp.track[0].count==2);
 CHECK(hp_project_load(0));CHECK(!hp_project_save(0,0));const hp_store_t*latest=hp_slot(0);CHECK(latest&&latest->seq==2);((uint8_t*)latest)[80]^=1;CHECK(hp_slot(0)&&hp_slot(0)->seq==1);CHECK(hp_project_load(0));
 /* Pair state is saved alongside HOME and restored without held notes. */
 prepare(ENGI_DRONE);hp_apply(0,0);CHECK(web_home_assign(0,0,0,HP_PAIR,0,2));ep.base=12345;ep.motion=1;CHECK(!hp_project_save(2,0));pair_disable();run_ms(80);hp_apply(0,2);CHECK(hp_project_load(2));CHECK(ep.on&&ep.base==12345&&ep.motion&&hp.track[0].cell[0][0].kind==HP_PAIR);CHECK(!gates(0)&&!gates(2));go_home();ui.msg_t=0;run_ms(80);shot("home-pair-cell");
 hp.pair_base=12345;hp.pair_motion=1;hp_event_put(ep.a,0,6,20,0);hp_event_put(ep.a,2,6,100,1);hp_motion_step(&trk[ep.a],2);CHECK(ep.base==100*32767/127);hp_motion_step(&trk[ep.a],3);CHECK(ep.base==20*32767/127);hp_motion_stop();CHECK(ep.base==12345);hp.events=0;
 CHECK(!hp_project_save(3,0));CHECK(hp_slot(3));CHECK(!project_erase(3));CHECK(!hp_slot(3));
 /* Autosave and boot preserve metadata, default template and pair. */
 hp_default=2;hp.track[0].active=0;hp_as_seen=0;hp_as_saved=0;run_ms(12000);CHECK(hp_slot(4));hp_apply(0,0);pair_disable();CHECK(hp_autosave_boot(1));CHECK(ep.on&&hp_default==2&&hp.track[0].cell[0][0].kind==HP_PAIR);
 hp_open_graph(GR_PAIR);ui.msg_t=0;run_ms(80);shot("pair-source");hp_open_graph(GR_MACRO);run_ms(80);shot("macro-source");
 for(uint32_t n=0;n<1200;n++){uint32_t t=n%4;track_select(t);hp_open_editor();hp_edit_row=n%hp.track[t].count;hp_edit_col=n%4;hp_edit_action=1+n%8;hp_editor_apply();CHECK(hp.track[t].count>=1&&hp.track[t].count<=HP_ROWS&&hp.track[t].active<hp.track[t].count);CHECK(hp_model_valid(&hp));}
 printf("HOME: %u checks passed; real combo, 32 rows/track, target guards, macros, MIDI Learn, locks, A-D, CRC fallback, autosave and pair restore. Capsule %zu bytes.\n",checks,sizeof(hp_store_t));
 return 0;
}
