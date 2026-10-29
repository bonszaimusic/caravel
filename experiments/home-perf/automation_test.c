/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN automation_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void select_event(uint32_t step,uint32_t control)
{
 int h=hp_auto_find(song.sel,step,control);CHECK(h>=0);ev_follow(EVC(EVK_PERF,h));
 uint16_t rw[EV_ROWS];CHECK(ev_cur(rw,0)==EVC(EVK_PERF,h));
}
static void shot(const char *name)
{
 char path[160];snprintf(path,sizeof path,"build/home-perf/%s.ppm",name);ui.msg_t=0;ui.force=1;ui_draw();
 FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");
 for(uint32_t i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);uint8_t b[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(b,1,3,f);}fclose(f);
}
int main(void)
{
 web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(2,0,ENGI_HADES,ENGI_SEM));CHECK(ep.a==2&&ep.b==0);track_select(0);
 hp.pair_base=12345;hp.pair_motion=0;hp.pair_valid=1;ep.base=12345;
 /* Existing project with hidden performance records and PLAY OFF: no longer moves. */
 hp_event_put(2,0,6,20,0);hp_event_put(2,2,6,100,0);motion.on=0;
 CHECK(motion_count(&trk[2])==0);motion_step(&trk[2],2,&motion,1);CHECK(ep.base==12345);
 motion_set_enabled(&trk[2],1);motion_step(&trk[2],2,&motion,1);CHECK(ep.base==100*32767/127);
 motion_set_enabled(&trk[2],0);CHECK(ep.base==12345&&hp.events==2);
 motion_step(&trk[2],0,&motion,1);CHECK(ep.base==12345);
 motion_set_enabled(&trk[2],1);motion_step(&trk[2],0,&motion,1);CHECK(ep.base==20*32767/127);
 /* Actual capture from B writes A, enables A PLAY and keeps the unrecorded baseline. */
 song.playing=1;song.rec=1;trk[2].seq_idx=4;hp_special_set(0,6,80,1);song.playing=0;song.rec=0;
 CHECK(hp_auto_find(2,4,6)>=0&&hp_auto_find(0,4,6)<0);CHECK(motion_enabled(&trk[2]));CHECK(hp.pair_base==12345);
 hp_open_graph(GR_PAIR);shot("pair-automation-owner");CHECK(hp_pair_auto_count()==3);
 CHECK(ev_count(&trk[0],0)==0);track_select(2);hp_open_graph(GR_EVENTS);ev_enter();
 CHECK(ev_count(TSEL,0)==3&&ev_count(TSEL,1)==0&&motion_count(TSEL)==0);
 uint16_t rw[EV_ROWS];CHECK(ev_list(rw)==6);CHECK(ev_act_ready());shot("pair-automation-list");
 select_event(2,6);CHECK(!strcmp(ev_act_name(),"LOCK"));uint32_t sig=ev_sig();
 ev_knob(3,-1);CHECK(hp.event[hp_auto_find(2,2,6)].value<100);CHECK(ev_sig()!=sig);
 undo_swap();CHECK(hp.event[hp_auto_find(2,2,6)].value==100);CHECK(hp.events==3);
 undo_swap();CHECK(hp.event[hp_auto_find(2,2,6)].value<100);
 select_event(2,6);ev_knob(1,1);CHECK(hp_auto_find(2,2,6)<0&&hp_auto_find(2,3,6)>=0);
 undo_swap();CHECK(hp_auto_find(2,2,6)>=0&&hp_auto_find(2,3,6)<0);
 select_event(2,6);ev_oct();CHECK(hp.event[hp_auto_find(2,2,6)].lock);CHECK(ev_count(TSEL,1)==1);
 motion_step(TSEL,2,&motion,0);CHECK(ep.base==20*32767/127); /* failed chance: no lock */
 motion_step(TSEL,2,&motion,1);CHECK(ep.base!=20*32767/127);
 motion_step(TSEL,3,&motion,1);CHECK(ep.base==20*32767/127); /* lock released */
 shot("pair-automation-lock");select_event(2,6);ev_delete();CHECK(hp_auto_find(2,2,6)<0);
 undo_swap();CHECK(hp_auto_find(2,2,6)>=0);undo_swap();CHECK(hp_auto_find(2,2,6)<0);
 /* Macro row and ordinary motion coexist; normal undo must preserve HP events. */
 hp.macro[2][0].base=11;hp_event_put(2,1,0,99,0);select_event(1,0);ev_knob(3,-1);
 CHECK(hp.event[hp_auto_find(2,1,0)].value<99);undo_swap();CHECK(hp.event[hp_auto_find(2,1,0)].value==99);
 uint32_t before=hp_auto_sig(TSEL);motion_undo_take(TSEL,0);CHECK(!motion_set_lock(TSEL,1,P_FILT,12));motion_undo_done(TSEL);
 undo_swap();CHECK(hp_auto_sig(TSEL)==before&&motion_count(TSEL)==0);undo_swap();CHECK(hp_auto_sig(TSEL)==before&&motion_count(TSEL)==1);
 motion_step(TSEL,1,&motion,1);CHECK(hp_macro_live[2][0]==99);motion_set_enabled(TSEL,0);CHECK(hp_macro_live[2][0]==11);
 /* Clear via the actual confirmation action, undo/redo both namespaces, retain other tracks. */
 hp_event_put(1,0,0,44,0);uint32_t count=hp_auto_count(TSEL,0);before=hp_auto_sig(TSEL);
 confirm_open(CF_CLEAR_MOTION,2);web_buttons(1u<<B_OCTUP);run_ms(80);web_buttons(0);run_ms(180);CHECK(!ui.confirm);
 CHECK(!hp_auto_count(TSEL,0)&&!motion_count(TSEL));CHECK(hp_auto_count(&trk[1],0)==1);
 undo_swap();CHECK(hp_auto_count(TSEL,0)==count&&hp_auto_sig(TSEL)==before&&motion_count(TSEL)==1);
 undo_swap();CHECK(!hp_auto_count(TSEL,0)&&!motion_count(TSEL));undo_swap();CHECK(hp_auto_sig(TSEL)==before);
 /* No eviction if another track uses the space needed by an undo. */
 perf_undo_take(TSEL,0);motion_clear(TSEL);motion_undo_done(TSEL);
 for(uint32_t c=0;c<2;c++)for(uint32_t st=0;st<64;st++)hp_event_put(1,st,c,33,0);
 CHECK(hp.events==HP_EVENTS);uint32_t other=hp_auto_sig(&trk[1]);undo_swap();CHECK(hp.events==HP_EVENTS&&hp_auto_sig(&trk[1])==other&&!hp_auto_count(TSEL,0));
 /* Shared HOME edit buffer invalidates an older performance undo safely. */
 motion_clear(&trk[1]);hp_event_put(2,0,0,25,0);select_event(0,0);ev_delete();hp_backup_take(2);
 CHECK(!hp_auto_undo_ready(TSEL));undo_swap();CHECK(!hp_auto_count(TSEL,0));
 /* A Pair lock entered from B is also stored on canonical owner A. */
 track_select(0);hp_apply(0,0);CHECK(web_home_assign(0,0,0,HP_PAIR,0,2));hp_lock_turn(0,1,1ull<<7);
 CHECK(hp_auto_find(2,7,6)>=0&&hp_auto_find(0,7,6)<0);
 CHECK(hp_model_valid(&hp));printf("Performance automation PASS: %u checks; owner, PLAY, visible rows, value/step/kind, locks, undo/redo, clear, capacity, HOME history.\n",checks);return 0;
}
