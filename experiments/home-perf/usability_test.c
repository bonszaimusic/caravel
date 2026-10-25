/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t b){web_buttons(1u<<b);run_ms(60);web_buttons(0);run_ms(140);}
static void frame(const char*name){char p[200];snprintf(p,sizeof p,"build/home-perf/%s.ppm",name);FILE*f=fopen(p,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");for(uint32_t i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);uint8_t b[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(b,1,3,f);}fclose(f);}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);
 /* Last-played macro opens its map; return does not transpose. */
 hp_open_graph(GR_MACRO);web_enc(EN_K3,20);run_ms(60);CHECK(hp_macro_sel==2);int octave=song.octave;tap(B_OCTUP);CHECK(cur_page()->graph==GR_MACROMAP&&hp_macro_sel==2);CHECK(song.octave==octave);
 CHECK(!hp_routes_active(0,2));web_enc(EN_K3,1);run_ms(60);CHECK(hp.macro[0][2].route[0].ref.kind==HP_TRACK);CHECK(!hp_routes_active(0,2));web_enc(EN_K4,20);run_ms(60);CHECK(hp_routes_active(0,2)==1);
 /* Slots stay on the same macro and track; other tracks remain untouched. */
 web_enc(EN_K2,1);run_ms(60);web_enc(EN_K3,1);run_ms(60);web_enc(EN_K4,-20);run_ms(60);CHECK(hp_routes_active(0,2)==2);CHECK(!hp_routes_active(1,2));CHECK(hp.macro[0][2].route[1].ref.track==0);
 ui.msg_t=0;run_ms(40);frame("usability-macro-map");tap(B_OCTDN);CHECK(cur_page()->graph==GR_MACRO);CHECK(song.octave==octave);ui.msg_t=0;run_ms(40);frame("usability-macros");
 /* Editor actions execute on release. Undo clamps focus. */
 hp_apply(0,2);hp_open_editor();hp_edit_action=6;tap(B_OCTUP);CHECK(hp.track[0].count==3&&hp_edit_row==2);hp_edit_action=8;tap(B_OCTUP);CHECK(hp.track[0].count==2&&hp_edit_row<2);CHECK(song.octave==octave);tap(B_OCTUP);CHECK(hp.track[0].count==3&&hp_edit_row==2);
 hp_apply(1,0);track_select(1);CHECK(hp_edit_row==0&&hp_edit_action==0);hp_edit_action=6;tap(B_OCTUP);CHECK(hp.track[1].count==2&&hp.track[0].count==3);ui.msg_t=0;run_ms(40);frame("usability-home-edit");tap(B_OCTDN);CHECK(ui.home&&song.octave==octave);
 /* Templates and owner assignments are reversible. */
 track_select(0);hp_apply_template(0,0);CHECK(hp.track[0].count==1);hp_open_editor();hp_edit_action=8;tap(B_OCTUP);CHECK(hp.track[0].count==3);
 hp_edit_row=0;hp_edit_col=0;hp_page_knob(3,1);CHECK(hp.track[0].cell[hp_edit_row][0].track==1);hp_edit_action=8;tap(B_OCTUP);CHECK(hp.track[0].cell[hp_edit_row][0].track==0);
 /* Adjacent cells swap across row boundaries; deleting a binding leaves the sound intact. */
 CHECK(web_home_assign(0,0,3,HP_TRACK,0,P_LEVEL));CHECK(web_home_assign(0,1,0,HP_TRACK,0,P_PAN));hp_open_editor();hp_edit_row=0;hp_edit_col=3;hp_edit_action=3;tap(B_OCTUP);CHECK(hp.track[0].cell[0][3].id==P_PAN&&hp.track[0].cell[1][0].id==P_LEVEL);CHECK(hp_edit_row==1&&hp_edit_col==0);int sound=trk[0].p[P_LEVEL];hp_edit_action=1;tap(B_OCTUP);CHECK(!hp.track[0].cell[1][0].kind&&trk[0].p[P_LEVEL]==sound);tap(B_OCTUP);CHECK(!hp.track[0].cell[1][0].kind);hp_edit_action=8;tap(B_OCTUP);CHECK(hp.track[0].cell[1][0].id==P_LEVEL);
 /* Confirmation cancels on OCT- and navigation, and applies on second tap. */
 ui.menu=1;ui.menu_sel=MI_HPAPPLY;hp_default=0;ui.force=1;tap(B_OCTUP);CHECK(hp_apply_armed&&hp.track[0].count==3);ui.msg_t=0;run_ms(40);frame("usability-template-confirm");tap(B_OCTDN);CHECK(!hp_apply_armed&&hp.track[0].count==3);tap(B_OCTUP);CHECK(hp_apply_armed);web_enc(EN_ALGO,1);run_ms(60);CHECK(!hp_apply_armed);
 ui.menu_sel=MI_HPAPPLY;tap(B_OCTUP);tap(B_OCTUP);CHECK(!hp_apply_armed&&hp.track[0].count==1);web_home_open(1);CHECK(!ui.menu&&!ui.layer&&cur_page()->graph==GR_HOMEEDIT);hp_edit_action=8;tap(B_OCTUP);CHECK(hp.track[0].count==3);
 /* Source opens a real sound page. */
 hp_edit_row=0;hp_edit_col=0;hp_edit_action=0;hp_source();CHECK(!ui.home&&cur_page()->scope==SC_ENGINE);
 ui_prefs|=PREF_LARGE;hp_open_graph(GR_MACROMAP);CHECK(large_kind()==LK_LABEL&&graph_h()==H_GRAPH);run_ms(50);frame("usability-map-large");ui_prefs&=~PREF_LARGE;
 hp_ref_t partner={.kind=HP_PAIR,.track=1,.engine=255,.id=0};pair_ui_partner[1]=0;hp_set(partner,2,0);CHECK(pair_ui_partner[1]==2);
 CHECK(!hp_project_save(0,0));CHECK(web_home_export(0)==10000);hp_apply(0,0);CHECK(web_home_import(10000));CHECK(hp.track[0].count==3);
 printf("USABILITY: %u checks passed; panel shortcuts, macro slots, editor focus/undo, confirmation/cancel, source navigation, LARGE and HPM1 compatibility.\n",checks);return 0;
}
