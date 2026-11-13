/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.2 opt-in, real firmware/UI/audio-bracket integration test.
 * Not a physical device/dual-core or real-time deadline measurement. */
#define CARAVEL_ELASTIC_PLAYABLE 1
#define CARAVEL_ELASTIC_P1 1
#define CARAVEL_ACTION_BANKS 1
#define FELUCCA_RANDOM_MORPH 1
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN elastic_p12_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"

static void setup_p12(void)
{
    web_nor_erase();web_boot();run_ms(1000);
    web_test_drone(0);song.sel=0;hp_apply(0,1);go_home();
    action_enabled=1;action_bank=ACT_BANK_ELASTIC;action_setup=0;
    trk[0].p[P_E2]=48;trk[0].p[P_E4]=68;
    trk[0].p[P_E5]=26;trk[0].p[P_E7]=76;
    hp.track[0].cell[0][0]=hp_track_ref(0,P_E2,0xFFFFu);
    hp.track[0].cell[0][1]=hp_track_ref(0,P_E4,0xFFFFu);
    hp.track[0].cell[0][2]=hp_track_ref(0,P_E5,0xFFFFu);
    hp.track[0].cell[0][3]=hp_track_ref(0,P_E7,0xFFFFu);
    hp.track[0].active=0;hp.track[0].count=1;
    hp_elastic_p1_clear();hp_el_scope=0;action_refresh();
}
int main(void)
{
    setup_p12();
    /* P1.3 adds 1..1000 BOUNCE, REFLECT, COUPLE, SEED per track;
     * footprint is measured on the exact target (do not guess linker size).
     * 4x cfg(4) + 4x times(8) + 4x scope(1) = 52 static bytes. */
    CHECK(sizeof(hp_el_cfg_track)+sizeof(hp_el_times_track)+
          sizeof(hp_el_scope_track)==52u);
    CHECK(sizeof(hp_et_cfg_t)==8u);
    for(uint32_t t=0;t<NTRK;t++){
        CHECK(hp_el_scope_track[t]==0u);
        CHECK(hp_el_times_track[t].pull==HP_ET_DEFAULT.pull);
        CHECK(hp_el_times_track[t].back==HP_ET_DEFAULT.back);
        CHECK(hp_el_cfg_track[t].range==HP_ELASTIC_DEFAULT.range);
    }
    /* Clear CELL pick: A1 itself never writes the preset base. */
    int16_t old=trk[0].p[P_E7];
    hp_elastic_p1_action(0);
    CHECK(hp_el1.on && hp_el_pick && hp_el1.slot==255u);
    CHECK(trk[0].p[P_E7]==old && hp_el1_audio[0]==0u);
    CHECK(hp_elastic_p1_turn(hp_current(3),3,5));
    CHECK(!hp_el_pick && hp_el1.id==P_E7 && hp_el1.slot==3u);
    CHECK(hp_elastic_value(&hp_el1.spring)==old); /* PULL starts at stored anchor */
    run_ms(220);CHECK(hp_elastic_value(&hp_el1.spring)!=old);
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E7]!=old);
    hp_elastic_p1_audio_end(&trk[0]);CHECK(trk[0].p[P_E7]==old);
    hp_elastic_p1_clear();

    /* ROW can excite four independent visible timbre targets. */
    hp_elastic_p1_setup_turn(0,1);CHECK(hp_el_scope==1u);
    /* Independent PULL and RETURN 16ms..45s settings. */
    char title[16],time[24];
    hp_elastic_p1_setup_turn(1,-100);CHECK(hp_el_times.pull==0u);
    hp_elastic_p1_setup_caption(1,title,time);
    CHECK(!strcmp(title,"PULL") && !strcmp(time,"16ms"));
    hp_elastic_p1_setup_turn(1,100);CHECK(hp_el_times.pull==100u);
    hp_elastic_p1_setup_caption(1,title,time);CHECK(!strcmp(time,"45s"));
    hp_elastic_p1_setup_turn(2,-100);CHECK(hp_el_times.back==0u);
    hp_elastic_p1_setup_caption(2,title,time);
    CHECK(!strcmp(title,"RETURN") && !strcmp(time,"16ms"));
    hp_elastic_p1_setup_turn(2,100);CHECK(hp_el_times.back==100u);
    hp_elastic_p1_setup_caption(2,title,time);CHECK(!strcmp(time,"5m"));
    hp_elastic_p1_setup_turn(2,-10);CHECK(hp_el_times.back==90u);
    hp_elastic_p1_setup_caption(2,title,time);CHECK(!strcmp(time,"45s"));
    hp_elastic_p1_setup_turn(2,4);CHECK(hp_el_times.back==94u);
    hp_elastic_p1_setup_caption(2,title,time);CHECK(!strcmp(time,"1m30"));
    hp_elastic_p1_setup_turn(2,-24);CHECK(hp_el_times.back==70u);
    hp_elastic_p1_setup_turn(1,-70);
    CHECK(hp_el_times.pull==30u && hp_el_times.back==70u);
    hp_elastic_p1_action(0);CHECK(hp_el1.on && hp_er_count(&hp_er)==4u);
    CHECK(hp_er.valid==0x0Fu);
    int16_t old2=trk[0].p[P_E2],old4=trk[0].p[P_E4];
    CHECK(hp_elastic_p1_turn(hp_current(0),0,4));
    CHECK(hp_elastic_p1_turn(hp_current(1),1,-4));
    CHECK(hp_er.last==1u && hp_er_value(&hp_er,0)==old2 &&
          hp_er_value(&hp_er,1)==old4); /* both are still at start of PULL */
    run_ms(220);
    CHECK(hp_er.last==1u && hp_er_value(&hp_er,0)!=old2 &&
          hp_er_value(&hp_er,1)!=old4);
    CHECK(hp_er_value(&hp_er,2)==trk[0].p[P_E5] &&
          hp_er_value(&hp_er,3)==trk[0].p[P_E7]);
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E2]!=old2 && trk[0].p[P_E4]!=old4);
    hp_elastic_p1_audio_end(&trk[0]);
    CHECK(trk[0].p[P_E2]==old2 && trk[0].p[P_E4]==old4);
    CHECK(!hp_er_frame[0][0].applied && !hp_er_frame[0][1].applied);
    /* Step locks and motion have first priority in PLAY. ELASTIC may keep
     * moving internally, but cannot overwrite a current MOTION parameter.
     * Its adjacent unowned spring must still be audible in that block. */
    /* Use the real MOTION lock lifecycle, not a synthetic flag bit. */
    int16_t lock_value=old2+5;
    CHECK(motion_set_lock(&trk[0],0u,P_E2,lock_value)==0);
    motion_set_enabled(&trk[0],1u);
    motion_begin();
    song.playing=1;
    motion_step(&trk[0],0u,&motion,1u);
    CHECK(trk[0].p[P_E2]==lock_value);
    CHECK(hp_elastic_p12_motion_owns(0,P_E2));
    CHECK(!hp_elastic_p12_motion_owns(0,P_E4));
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E2]==lock_value);
    CHECK(trk[0].p[P_E4]!=old4);
    hp_elastic_p1_audio_end(&trk[0]);
    CHECK(trk[0].p[P_E2]==lock_value && trk[0].p[P_E4]==old4);
    song.playing=0;
    motion_end(); /* restores actual patch base */
    motion_clear(&trk[0]);
    CHECK(trk[0].p[P_E2]==old2);
    CHECK(!hp_elastic_p12_motion_owns(0,P_E2));
    /* Resume after MOTION is block-faded: first block must be at base, then
     * the spring ramps in instead of stepping to the full delta. */
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E2]==old2);
    hp_elastic_p1_audio_end(&trk[0]);
    int resumed=0;
    for(uint32_t b=0;b<5u;b++){
        hp_elastic_p1_audio_begin(&trk[0]);
        if(trk[0].p[P_E2]!=old2)resumed=1;
        hp_elastic_p1_audio_end(&trk[0]);
    }
    CHECK(resumed);
    CHECK(trk[0].p[P_E2]==old2);
    /* Single-thread SAVE after render bracket must capture stable project
     * params, never a temporary ELASTIC offset. This is NOT a CPU2 race test. */
    project_capture(&proj_scratch);
    CHECK(proj_scratch.t[0].p[P_E2]==old2);
    CHECK(proj_scratch.t[0].p[P_E4]==old4);
    int32_t t0=hp_el_row_time[0].goal_q8,t1=hp_el_row_time[1].goal_q8;
    hp_elastic_p1_action(1); /* A2 pulses only the last-touched K2 */
    CHECK(hp_el_row_time[0].goal_q8==t0 && hp_el_row_time[1].goal_q8!=t1);
    hp_elastic_p1_action(2);CHECK(hp_el_row_held);
    int16_t hold0=hp_er_value(&hp_er,0),hold1=hp_er_value(&hp_er,1);
    run_ms(400);CHECK(hp_er_value(&hp_er,0)==hold0 &&
                      hp_er_value(&hp_er,1)==hold1);
    hp_elastic_p1_action(2);CHECK(!hp_el_row_held);
    hp_elastic_p1_action(3);CHECK(hp_er_value(&hp_er,0)==old2 &&
                                  hp_er_value(&hp_er,1)==old4);
    /* A targeted manual write cancels only that one spring. */
    hp_elastic_p1_manual(0,P_E2);
    CHECK(!(hp_er.valid&1u) && hp_er_count(&hp_er)==3u && hp_el1.on);
    hp_elastic_p1_clear();

    /* P1.2: a track switch keeps the arm on its original track; springs keep
     * running there and the audio bracket still restores the base. */
    hp_elastic_p1_action(0);
    CHECK(hp_el1.on && hp_er.track==0u && hp_er_count(&hp_er)==4u);
    int16_t switch_base=trk[0].p[P_E2];
    CHECK(hp_elastic_p1_turn(hp_current(0),0,4));
    run_ms(500);
    song.sel=1;
    hp_elastic_p1_poll(fm1_ms+16u);
    CHECK(hp_el1_track[0].on && hp_er_track[0].track==0u &&
          hp_er_count(&hp_er_track[0])==4u);
    CHECK(hp_elastic_p1_armed_mask()==1u); /* header HUD: only T1 */
    /* The offscreen spring must continue but must not force an LCD redraw. */
    int32_t offscreen_q8=hp_er_track[0].spring[0].x;
    ui.force=0;
    hp_elastic_p1_poll(fm1_ms+32u);
    CHECK(hp_er_track[0].spring[0].x!=offscreen_q8 && !ui.force);
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E2]!=switch_base);
    hp_elastic_p1_audio_end(&trk[0]);
    CHECK(trk[0].p[P_E2]==switch_base);

    /* A second track can be armed in parallel; A1 OFF there leaves T1 armed.
     * SCOPE is per track now, so make T2 ROW explicitly first. */
    host_preset(&trk[1],ENGI_DRONE,0);hp_apply(1,1);hp.track[1].active=0;hp.track[1].count=1;
    hp_elastic_p1_setup_turn(0,1);
    hp_elastic_p1_action(0);
    CHECK(hp_el1_track[0].on && hp_el1_track[1].on &&
          hp_er_track[1].track==1u && hp_er_count(&hp_er_track[1])==4u);
    CHECK(hp_elastic_p1_armed_mask()==3u); /* T1 + T2 */

    hp_elastic_p1_action(0);
    CHECK(!hp_el1_track[1].on && hp_el1_track[0].on);
    CHECK(hp_elastic_p1_armed_mask()==1u); /* A1 OFF isolates T2 */

    song.sel=0;
    hp_elastic_p1_poll(fm1_ms+32u);
    CHECK(hp_el1_track[0].on && hp_er_track[0].track==0u);
    hp_elastic_p1_action(0);
    CHECK(!hp_el1_track[0].on);

    /* All four tracks can hold an independent arm. */
    for(uint32_t tr=0;tr<NTRK;tr++){
        host_preset(&trk[tr],ENGI_DRONE,0);hp_apply(tr,1);
        hp.track[tr].active=0;hp.track[tr].count=1;
        song.sel=tr;
        if(!hp_el_scope_track[tr])hp_elastic_p1_setup_turn(0,1);
        hp_elastic_p1_action(0);
        CHECK(hp_el1_track[tr].on && hp_er_track[tr].track==tr);
        CHECK(hp_elastic_p1_armed_mask()==((1u<<(tr+1u))-1u));
    }
    /* Each track has its own session-only SCOPE, PULL, RETURN and RANGE.
     * Switching T2 from ROW to CELL cannot silence T1/T3/T4, and editing
     * T2 setup cannot change their trajectory, parameters or published words. */
    CHECK(hp_el_scope_track[0]==1u && hp_el_scope_track[1]==1u);
    hp_et_cfg_t t0_settings=hp_el_times_track[0];
    uint8_t t0_range=hp_el_cfg_track[0].range;
    int32_t t0_position=hp_er_track[0].spring[0].x;
    uint32_t t0_publication=hp_er_audio[0][0];
    uint8_t t0_phase=hp_el_row_time_track[0][0].phase;
    song.sel=1;
    hp_elastic_p1_setup_turn(0,-1);
    CHECK(hp_el_scope_track[1]==0u && hp_el_scope_track[0]==1u);
    CHECK(!hp_el1_track[1].on && hp_el1_track[0].on &&
          hp_el1_track[2].on && hp_el1_track[3].on);
    CHECK(hp_elastic_p1_armed_mask()==13u); /* 1101, T2 only OFF */
    hp_elastic_p1_setup_turn(1,70); /* T2 PULL 45 seconds */
    hp_elastic_p1_setup_turn(2,30); /* T2 RETURN 5 minutes */
    hp_elastic_p1_setup_turn(3,20); /* only T2 RANGE */
    CHECK(hp_el_times_track[1].pull==100u &&
          hp_el_times_track[1].back==100u);
    CHECK(hp_el_cfg_track[1].range==38u);
    hp_elastic_p1_setup_caption(0,title,time);
    CHECK(!strcmp(title,"SCOPE") && !strcmp(time,"CELL"));
    hp_elastic_p1_setup_caption(2,title,time);
    CHECK(!strcmp(title,"RETURN") && !strcmp(time,"5m"));
    CHECK(hp_el_times_track[0].pull==t0_settings.pull &&
          hp_el_times_track[0].back==t0_settings.back);
    CHECK(hp_el_cfg_track[0].range==t0_range);
    CHECK(hp_er_track[0].spring[0].x==t0_position);
    CHECK(hp_er_audio[0][0]==t0_publication &&
          hp_el_row_time_track[0][0].phase==t0_phase);
    CHECK(hp_el_scope_track[2]==1u && hp_el_scope_track[3]==1u);
    /* T2 may play a CELL with its own times while T1/T3/T4 keep ROW. */
    hp_elastic_p1_action(0);
    CHECK(hp_el1_track[1].on && hp_el_pick_track[1] &&
          hp_el_scope_track[1]==0u);
    CHECK(hp_elastic_p1_turn(hp_current(3),3,5));
    CHECK(!hp_el_pick_track[1] && hp_el1_track[0].on &&
          hp_el1_track[2].on && hp_el1_track[3].on);
    CHECK(hp_el_cell_time_track[1].pull_duration_ms==45000u);
    CHECK(hp_el_cell_time_track[1].return_duration_ms==300000u);
    song.sel=0;
    hp_elastic_p1_setup_caption(0,title,time);
    CHECK(!strcmp(time,"ROW"));
    CHECK(hp_el_times.pull==t0_settings.pull &&
          hp_el_times.back==t0_settings.back);
    song.sel=1;
    hp_elastic_p1_action(0); /* OFF T2, settings survive */
    CHECK(!hp_el1_track[1].on && hp_el_times.back==100u);
    hp_elastic_p1_setup_turn(0,1); /* ROW T2, other ROWs unaffected */
    CHECK(hp_el_scope_track[1]==1u && hp_el_scope_track[0]==1u);
    hp_elastic_p1_clear();
    for(uint32_t tr=0;tr<NTRK;tr++)CHECK(!hp_el1_track[tr].on);
    CHECK(hp_elastic_p1_armed_mask()==0u);
    CHECK(hp_el_times_track[1].back==100u); /* disarm is NOT settings reset */
    song.sel=0;

    /* Audio's saved restore frames must remain authoritative if A1 OFF
     * invalidates a spring after audio_begin but before audio_end. This
     * deliberately interleaves the two functions in one host thread; it is
     * a functional invariant, NOT evidence of CPU2 atomicity or TSAN. */
    hp_elastic_p1_action(0);
    int16_t frame_base=trk[0].p[P_E2];
    CHECK(hp_elastic_p1_turn(hp_current(0),0,5));
    run_ms(220);
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E2]!=frame_base);
    hp_elastic_p1_clear_track(0);
    hp_elastic_p1_audio_end(&trk[0]);
    CHECK(trk[0].p[P_E2]==frame_base);
    CHECK(!hp_er_frame[0][0].applied);
    /* Same parameter twice must not produce two overlapping restore frames. */
    hp.track[0].cell[0][1]=hp_track_ref(0,P_E2,0xFFFFu);
    hp_elastic_p1_action(0);
    CHECK(hp_er_count(&hp_er)==3u && (hp_er.valid&2u)==0u);
    hp_elastic_p1_clear();
    hp.track[0].cell[0][1]=hp_track_ref(0,P_E4,0xFFFFu);

    /* Row changes keep the arm on its original row; the cells stay valid. */
    hp_elastic_p1_action(0);CHECK(hp_el1.on);
    hp.track[0].count=2;
    hp_row_turn(1);CHECK(hp_el1.on && hp_er.row==0u);
    hp_row_turn(-1);CHECK(hp_el1.on);
    hp.track[0].count=1;
    hp_elastic_p1_clear();

    /* Non-DRONE engines are eligible when their HOME cells are safe. */
    trk[2].engine=trk[2].eng_req=0u;
    hp.track[2].cell[0][0]=hp_track_ref(2,P_E7,0xFFFFu);
    hp.track[2].active=0;hp.track[2].count=1;song.sel=2;
    hp_elastic_p1_action(0);
    CHECK(hp_el1_track[2].on && hp_er_track[2].track==2u);
    hp_elastic_p1_clear();song.sel=0;

    /* Tier-1 parameter ownership: timing, gain, note/pitch, voice settings,
     * discrete selectors and matrix configuration are excluded even when the
     * UI descriptor happens to be continuous. A filter/FX mix remains valid. */
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_LEVEL));
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_SDIV));
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_TRANS));
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_ARATE));
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_VOICE));
    CHECK(!hp_elastic_p12_safe_id(&trk[0],P_M1AMT));
    CHECK(hp_elastic_p12_safe_id(&trk[0],P_FILT));
    CHECK(hp_elastic_p12_safe_id(&trk[0],P_REV));
    /* Enumerate every selectable engine's descriptor table. A format can
     * never bypass the semantic common-id exclusion, nor can a hidden
     * engine E parameter of a discrete format become eligible. */
    uint8_t saved_engine=trk[2].engine,saved_request=trk[2].eng_req;
    for(uint32_t engine=0;engine<NENGINES;engine++){
        if(!eng_ok(engine))continue;
        trk[2].engine=trk[2].eng_req=(uint8_t)engine;
        CHECK(!hp_elastic_p12_safe_id(&trk[2],P_LEVEL));
        CHECK(!hp_elastic_p12_safe_id(&trk[2],P_ARATE));
        CHECK(hp_elastic_p12_safe_id(&trk[2],P_FILT));
        for(uint32_t id=P_E0;id<P_COUNT;id++){
            if(!hp_elastic_p12_safe_id(&trk[2],id))continue;
            const param_desc_t *d=track_desc(&trk[2],id);
            CHECK(d && (d->fmt==F_PCT||d->fmt==F_BIPCT||
                        d->fmt==F_CUTOFF||d->fmt==F_TIME||
                        d->fmt==F_LFOHZ));
        }
    }
    /* Explicit exclusions derived from real per-engine descriptor tables:
     * DRUM TUNE and FM6 DTUN use F_PCT but change pitch; GRAIN POS uses
     * F_PCT but changes sample position. Format-only policy was unsafe. */
    trk[2].engine=trk[2].eng_req=ENGI_DRUM;
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E1)); /* TUNE */
    CHECK(hp_elastic_p12_safe_id(&trk[2],P_E2));  /* TONE */
    trk[2].engine=trk[2].eng_req=ENGI_FM6;
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E6)); /* DTUN */
    CHECK(hp_elastic_p12_safe_id(&trk[2],P_E2));  /* MLVL */
    trk[2].engine=trk[2].eng_req=8u; /* GRAIN */
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E1)); /* POS */
    CHECK(hp_elastic_p12_safe_id(&trk[2],P_E7));  /* TONE */
    trk[2].engine=trk[2].eng_req=ENGI_DRONE;
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E0)); /* SPREAD: not pilot */
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E1)); /* DETUNE */
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E3)); /* TOPO */
    CHECK(!hp_elastic_p12_safe_id(&trk[2],P_E6)); /* RATE */
    for(uint32_t id=P_E2;id<=P_E7;id++)
        if(id!=P_E3 && id!=P_E6)CHECK(hp_elastic_p12_safe_id(&trk[2],id));
    trk[2].engine=saved_engine;trk[2].eng_req=saved_request;
    /* Unsupported card must not silently edit the underlying preset. P_FILT is
     * a safe BIPCT and is now eligible on every engine; P_E3 is the DRONE
     * topology selector and must stay out. */
    hp.track[0].cell[0][1]=hp_track_ref(0,P_E3,0xFFFFu);
    hp_elastic_p1_action(0);
    CHECK(hp_el1.on && hp_er_count(&hp_er)==3u);
    int16_t topo=trk[0].p[P_E3];
    CHECK(hp_elastic_p1_turn(hp_current(1),1,4));
    CHECK(trk[0].p[P_E3]==topo);
    hp_elastic_p1_clear();

    /* Transport priority: P1.2 can arm and keep its springs while PLAY runs. */
    hp.track[0].cell[0][1]=hp_track_ref(0,P_E4,0xFFFFu);
    hp_elastic_p1_action(0);CHECK(hp_el1.on);
    song.playing=1;hp_elastic_p1_poll(fm1_ms+16u);CHECK(hp_el1.on);
    hp_elastic_p1_action(0);CHECK(!hp_el1.on);
    hp_elastic_p1_action(0);CHECK(hp_el1.on); /* arm during transport */
    hp_elastic_p1_clear();song.playing=0;

    /* P1.3 second page: four persistent options are independently mutable,
     * SEED changes are barred while coupled springs are sounding. */
    setup_p12();
    /* A single excited K1 can move dormant K2 through a deterministic
     * seed link. Switching COUPLE=0 immediately removes all cross-talk,
     * restoring bit-identical independent audio publication. */
    hp_el_scope=1u;hp_elastic_p1_action(0u);
    CHECK(hp_er_count(&hp_er)==4u);
    uint32_t coupled_seed=0u;
    for(;coupled_seed<256u;coupled_seed++)
        if(hp_ec_weight(coupled_seed,0u,1u)>80)break;
    CHECK(coupled_seed<256u);
    hp_el_times.seed=(uint8_t)coupled_seed;
    hp_el_times.couple=100u;hp_el_couple_live[0]=100u;
    hp_er.spring[0].active=1u;
    hp_er.spring[0].x=hp_elastic_limit(&hp_er.spring[0],hp_el1_cfg)*256;
    int16_t uncoupled_k2=hp_er_value(&hp_er,1u);
    int16_t coupled_k2=hp_elastic_p13_row_value(0u,1u);
    CHECK(coupled_k2!=uncoupled_k2);
    hp_elastic_p1_publish_track(0u);
    CHECK((hp_er_audio[0][1]&HP_EL1_VALID)!=0u);
    hp_el_couple_live[0]=0u;hp_el_times.couple=0u;
    /* Default-OFF fast path must keep every spring's original Q8 state. */
    CHECK(hp_elastic_p13_row_x(0u,0u)==hp_er.spring[0].x);
    CHECK(hp_elastic_p13_row_x(0u,1u)==hp_er.spring[1].x);
    hp_elastic_p1_publish_track(0u);
    CHECK(hp_er_audio[0][1]==0u);
    hp_elastic_p1_clear();
    hp_el_scope=0u;

    CHECK(hp_el_times_track[0].bounce==HP_ES_DEFAULT &&
          hp_el_times_track[0].reflect==70u &&
          hp_el_times_track[0].couple==0u);
    hp_elastic_p1_setup_page_toggle();
    CHECK(hp_el_setup_page==1u);
    hp_elastic_p1_setup_turn(0,200); /* clamp at 64-bar division */
    hp_elastic_p1_setup_turn(1,-20);
    hp_elastic_p1_setup_turn(2,60);
    hp_el_times.seed=0u; /* detents are relative; start from a known seed */
    hp_elastic_p1_setup_turn(3,42);
    CHECK(hp_el_times_track[0].bounce==(HP_ES_SYNC_FLAG|11u));
    CHECK(hp_el_times_track[0].reflect==50u);
    CHECK(hp_el_times_track[0].couple==60u);
    CHECK(hp_el_times_track[0].seed==42u);
    hp_elastic_p1_setup_caption(0,title,time);
    CHECK(!strcmp(title,"BOUNCE") && !strcmp(time,"64 BAR"));
    hp_elastic_p1_setup_caption(2,title,time);
    CHECK(!strcmp(title,"COUPLE") && !strcmp(time,"60%"));
    hp_elastic_p1_setup_page_toggle();
    CHECK(hp_el_setup_page==0u);
    hp_el_times_track[0].back=HP_ES_RETURN_INF; /* infinity survives v5 SAVE/LOAD */
    song.sel=1;
    CHECK(hp_el_times_track[1].bounce==HP_ES_DEFAULT &&
          hp_el_times_track[1].couple==0u);
    hp_el_scope_track[1]=1u;
    hp_el_times_track[1].back=100u;
    hp_el_times_track[1].bounce=432u;
    hp_el_times_track[1].seed=191u;
    /* Preserve RANDOM snapshot slots: v5 extends v4's nine-byte record
     * without modifying the native project or removing older snapshots. */
    song.sel=0;
    CHECK(hp_rm_snapshot_store(0u)==1);
    uint32_t prior_snap_count=hp_rm_snapshot_count(0u,0u);
    CHECK(prior_snap_count>=1u);
    song.sel=1;
    /* P1.4 writes HPM1 v5; tagged periods are distinct from historical v4
     * bounce counts; exact nine-byte record and CRC are exercised. */
    /* A v5 writer must reject malformed runtime settings BEFORE flash.
     * Validating only the v3 prefix would save capsules that reject LOAD. */
    hp_el_times_track[1].bounce=0u;
    CHECK(!hp_capture(&hp_io));
    hp_el_times_track[1].bounce=432u;
    hp_el_times_track[1].reflect=101u;
    CHECK(!hp_capture(&hp_io));
    hp_el_times_track[1].reflect=HP_EC_REFLECT_DEFAULT;
    CHECK(hp_capture(&hp_io));
    CHECK(hp_io.version==5u && hp_io.reserved[2]==HP_RAW_META);
    hp_io.crc=hp_hash((const uint8_t*)&hp_io+32,hp_io.size-32);
    hp_io.reserved[0]=hp_hash(&hp_io,20u);
    CHECK(hp_capsule_ok(&hp_io));
    uint8_t packed_raw[HP_RAW_META] __attribute__((aligned(4)));
    CHECK(hp_v2_decode(&hp_io,packed_raw));
    CHECK(hp_el_persist_valid(packed_raw+HP_RAW_META_V3,5u));
    uint8_t corrupt[HP_EL_PERSIST_BYTES];
    memcpy(corrupt,packed_raw+HP_RAW_META_V3,sizeof corrupt);
    corrupt[4]=0u;corrupt[5]=0u; /* zero bounces invalid */
    CHECK(!hp_el_persist_valid(corrupt,5u));
    hp_elastic_p1_init();
    CHECK(hp_el_times_track[0].bounce==HP_ES_DEFAULT &&
          hp_el_times_track[1].back==70u);
    CHECK(hp_restore(&hp_io));
    CHECK(hp_el_times_track[0].bounce==(HP_ES_SYNC_FLAG|11u) &&
          hp_el_times_track[0].back==HP_ES_RETURN_INF &&
          hp_el_times_track[0].reflect==50u &&
          hp_el_times_track[0].couple==60u &&
          hp_el_times_track[0].seed==42u);
    CHECK(hp_el_times_track[1].bounce==432u &&
          hp_el_times_track[1].back==100u &&
          hp_el_times_track[1].seed==191u);
    CHECK(hp_el_scope_track[1]==1u);
    CHECK(hp_elastic_p1_armed_mask()==0u && hp_el_setup_page==0u);
    CHECK(hp_rm_snapshot_count(0u,0u)==prior_snap_count);
    /* Decode a complete synthetic v4 capsule: nine-byte v4 BOUNCE was
     * an untagged 1..1000 count. It must not become a tempo index. */
    uint8_t v4_raw[HP_RAW_META] __attribute__((aligned(4)));
    memcpy(v4_raw,packed_raw,sizeof v4_raw);
    for(uint32_t tr=0u;tr<NTRK;tr++){
        uint8_t *rec=v4_raw+HP_RAW_META_V3+tr*9u;
        rec[2]=70u;rec[4]=2u;rec[5]=0u;
    }
    uint32_t n4=hp_zip((uint8_t*)&hp_io.model,HP_V2_META,v4_raw,HP_RAW_META);
    CHECK(n4>0u && n4<=HP_V2_META);
    hp_io.version=4u;hp_io.reserved[1]=n4;
    hp_io.reserved[2]=HP_RAW_META;
    hp_io.crc=hp_hash((const uint8_t*)&hp_io+32,hp_io.size-32);
    hp_io.reserved[0]=hp_hash(&hp_io,20u);
    CHECK(hp_capsule_ok(&hp_io));
    CHECK(hp_restore(&hp_io));
    for(uint32_t tr=0;tr<NTRK;tr++){
        CHECK(hp_el_times_track[tr].bounce==2u);
        CHECK(hp_el_times_track[tr].back==70u);
    }
    /* In a legacy v3 project the prefix is identical and the new
     * settings must initialize to OFF/default, not read garbage. */
    uint32_t n3=hp_zip((uint8_t*)&hp_io.model,HP_V2_META,
                       packed_raw,HP_RAW_META_V3);
    CHECK(n3>0 && n3<=HP_V2_META);
    hp_io.version=3u;hp_io.reserved[1]=n3;
    hp_io.reserved[2]=HP_RAW_META_V3;
    hp_io.crc=hp_hash((const uint8_t*)&hp_io+32,hp_io.size-32);
    hp_io.reserved[0]=hp_hash(&hp_io,20u);
    CHECK(hp_capsule_ok(&hp_io));
    CHECK(hp_restore(&hp_io));
    for(uint32_t tr=0;tr<NTRK;tr++){
        CHECK(hp_el_scope_track[tr]==0u);
        CHECK(hp_el_times_track[tr].bounce==HP_ES_DEFAULT);
        CHECK(hp_el_times_track[tr].reflect==70u);
        CHECK(hp_el_times_track[tr].couple==0u);
    }
    puts("elastic_p12_test: PASS (P1.4 musical bounce, HPM1 v5 SAVE/LOAD, v3 defaults, MOTION/ROW/CELL)");
    return 0;
}
