/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Uses the existing *actual* capture.c and audio-clock/voice/UI fixture.
 * Run with tests/capture_post_quant.sh (generated build/gen required).
 * Not an FM-1 target or CPU/ISR timing measurement. */
#define main capture_baseline_fixture_main
#include "ux_capture_test.c"
#undef main
#include <assert.h>

static void cq_fresh(void)
{
    fresh();
    cap_qfailed = 0u;
    cap_qowner = NTRK;
    cap_qstage = cap_qpos = 0u;
    cap_cq_w = cap_cq_r = cap_rq_w = cap_rq_r = 0u;
}
static void cq_seed(unsigned tr, unsigned len_bars, cap_ev_t *ev, unsigned n)
{
    cq_fresh();
    memcpy(cap_loop[tr], ev, n * sizeof *ev);
    cap_ln[tr] = (uint8_t)n;
    cap_len[tr] = len_bars << 16;
    cap_o[tr] = 0u;
    cap_last[tr] = -1;
    cap_st[tr] = 1u;
    cap_publish();
}
static void cq_build(unsigned tr)
{
    for (unsigned j = 0; j < 200u && !cap_qready[tr]; j++)
        cap_quant_work(); /* at most four source events per audio block */
    REQUIRE("bounded quant preparation completed", cap_qready[tr]);
}
static unsigned cq_pair_duration(const cap_ev_t *e, unsigned n, unsigned pitch, unsigned len)
{
    for (unsigned i=0;i<n;i++)
        if (e[i].vel && e[i].note==pitch)
            for (unsigned j=0;j<n;j++)
                if (!e[j].vel && e[j].note==pitch)
                    return (e[j].t + len - e[i].t) % len;
    return 0xffffffffu;
}
static void test_grid_and_restore(void)
{
    cap_ev_t ev[]={{301,60,101},{902,60,0},{1601,67,85},{2103,67,0}};
    cq_seed(0,1,ev,4);
    cap_ev_t original[4];memcpy(original,cap_loop[0],sizeof original);
    REQUIRE("first quant 1/16 command accepted",cap_quant_request(0,1u));
    cq_build(0);
    REQUIRE("active playback stays original until a loop boundary", cap_qactive[0]==0u && !memcmp(cap_loop[0],original,sizeof original));
    cap_pause(0); /* pausing releases owned notes; quant is applied consistently */
    cap_quant_apply(0);
    REQUIRE("1/16 rounds note-on to nearest grid",
            cap_qactive[0]==1u && cap_qn[0]==4u &&
            cap_qloop[0][0].t==256u && cap_qloop[0][2].t==1536u);
    REQUIRE("velocity and duration retained",cq_pair_duration(cap_qloop[0],4,60,4096)==601u &&
            cq_pair_duration(cap_qloop[0],4,67,4096)==502u);
    for(unsigned grid=2;grid<=5;grid++){
        REQUIRE("changing grid posts new request",cap_quant_request(0,grid));
        cq_build(0);cap_quant_apply(0);
        REQUIRE("new grid always computed from ORIGINAL",
                !memcmp(cap_loop[0],original,sizeof original) &&
                cap_qactive[0]==grid &&
                cq_pair_duration(cap_qloop[0],4,60,4096)==601u);
    }
    REQUIRE("OFF request accepted",cap_quant_request(0,0u));
    cap_quant_apply(0);
    REQUIRE("OFF plays exact byte-for-byte original",!cap_qactive[0] && !memcmp(cap_loop[0],original,sizeof original));
}
static void test_wrap_short_simultaneous(void)
{
    /* Make chronological 1/4096-bar order for the actual loop parser. */
    cap_ev_t sorted[]={{1,70,127},{2,70,0},{100,60,0},{3900,60,60}};
    cq_seed(0,1,sorted,4);
    REQUIRE("wrap and 1-unit short note quant command",cap_quant_request(0,5));
    cq_build(0);cap_pause(0);cap_quant_apply(0);
    REQUIRE("wrap note moves to phase zero and release follows it",
            cap_qactive[0]==5u && cq_pair_duration(cap_qloop[0],4,60,4096)==296u);
    REQUIRE("1-unit short note stays one unit",
            cq_pair_duration(cap_qloop[0],4,70,4096)==1u);
    cap_ev_t together[]={{300,60,91},{340,64,75},{800,60,0},{850,64,0}};
    cq_seed(0,1,together,4);
    REQUIRE("different pitches may share a quantized onset",cap_quant_request(0,1u));
    cq_build(0);cap_pause(0);cap_quant_apply(0);
    REQUIRE("chord onsets can coincide while lengths survive",
            cap_qactive[0]==1u &&
            cap_qloop[0][0].t==256u && cap_qloop[0][1].t==256u &&
            cq_pair_duration(cap_qloop[0],4,60,4096)==500u &&
            cq_pair_duration(cap_qloop[0],4,64,4096)==510u);
}
static void test_same_pitch_touch_and_long_loop(void)
{
    cap_ev_t touching[]={{128,60,100},{384,60,0},{400,60,90},{600,60,0}};
    cq_seed(0,1,touching,4);
    cap_quant_request(0,1u);cq_build(0);cap_pause(0);cap_quant_apply(0);
    REQUIRE("adjacent same pitch keeps both notes and both releases",
            cap_qactive[0]==1u && cap_qn[0]==4u &&
            cap_qloop[0][0].t==256u && cap_qloop[0][1].t==512u &&
            !cap_qloop[0][1].vel && cap_qloop[0][2].vel &&
            cap_qloop[0][2].t==512u && cap_qloop[0][3].t==712u);
    cap_ev_t long_note[]={{17u*4096u+290u,69,73},{17u*4096u+800u,69,0}};
    cq_seed(0,32,long_note,2);
    cap_quant_request(0,5u);cq_build(0);cap_pause(0);cap_quant_apply(0);
    REQUIRE("1/1 uses a full four-beat bar across the 32-bar loop",
            cap_qactive[0]==5u && cap_qloop[0][0].t==17u*4096u &&
            cap_qloop[0][1].t==17u*4096u+510u &&
            !memcmp(cap_loop[0],long_note,sizeof long_note));
}
static void test_no_prequant(void)
{
    cq_fresh();
    REQUIRE("no-loop quantization refused",!cap_quant_request(0,2u));
    REQUIRE("queued quantization also reports NO LOOP",
            cap_post(CAP_OP_QUANT,0u,2u));
    cap_commands();
    REQUIRE("no queued/pre-armed quant after missing loop",
            cap_result()==CAPR_NO_LOOP &&
            !cap_qactive[0] && !cap_qwant[0] && !cap_qready[0]);
}
static void test_same_pitch_collision(void)
{
    cap_ev_t ev[]={{100,60,90},{700,60,0},{800,60,95},{900,60,0}};
    cq_seed(0,1,ev,4);
    cap_ev_t original[4];memcpy(original,cap_loop[0],sizeof original);
    REQUIRE("coarse grid request accepted before checking",cap_quant_request(0,5u));
    for(unsigned i=0;i<200;i++)cap_quant_work();
    REQUIRE("same-pitch collision is rejected as a whole",
            cap_qwant[0]==0 && cap_qready[0] && cap_qfailed &&
            !memcmp(cap_loop[0],original,sizeof original));
    cap_pause(0);cap_quant_apply(0);
    REQUIRE("collision keeps raw playback, no partial quant or orphan OFF",
            cap_qactive[0]==0 && !memcmp(cap_loop[0],original,sizeof original));
    cap_ev_t zero[]={{100,60,90},{100,60,0}};
    cq_seed(0,1,zero,2);
    cap_quant_request(0,1);for(unsigned j=0;j<200;j++)cap_quant_work();
    REQUIRE("zero-length same-tick note rejected to avoid hanging gates",cap_qfailed && !cap_qwant[0]);
}
static void test_boundary_overdub_pause_clear(void)
{
    cap_ev_t original[]={{300,60,83},{900,60,0}};
    cq_seed(0,1,original,2);
    cap_quant_request(0,2);cq_build(0);
    REQUIRE("ready grid still waits during playback",!cap_qactive[0] && cap_qready[0]);
    cap_last[0]=3900;
    perf_bars_q16=65536u+400u; /* next cycle */
    cap_tick();
    REQUIRE("grid commits exactly at the next loop boundary",cap_qactive[0]==2u && cap_qn[0]==2u && !cap_qready[0]);
    cap_ev_t extra[]={{1200,64,67},{1600,64,0}};
    memcpy(&cap_loop[0][2],extra,sizeof extra);
    cap_ln[0]=4;
    cap_quant_dirty(0);
    REQUIRE("overdub changes ORIGINAL but preserves previous playback count",
            cap_qn[0]==2u && cap_qactive[0]==2u && cap_qdirty[0]);
    cq_build(0);
    REQUIRE("overdub gets re-quantized from new raw source",cap_qready[0] && cap_qn[0]==2u);
    cap_pause(0);cap_quant_apply(0);
    REQUIRE("paused loop accepts new grid without moving its saved phase",
            cap_qn[0]==4u && cap_qactive[0]==2u && cap_pph[0] == (65536u+400u) % 65536u);
    cap_resume(0);
    REQUIRE("resume retains the loop position",cap_st[0]==1u &&
            (perf_bars_q16-cap_o[0])%cap_len[0]==cap_pph[0]);
    cap_wipe(0);
    REQUIRE("CLEAR resets quant to OFF and does not prequant next capture",
            !cap_ln[0] && !cap_qwant[0] && !cap_qactive[0] &&
            !cap_quant_request(0,1u));
}
static void test_pair_and_rec_gesture(void)
{
#if FELUCCA_PAIR_MORPH
    cq_fresh();song.playing=0;
    REQUIRE("PAIR is enabled",pair_enable(0,1));
    song.playing=1;
    song.sel=1;
    cap_ev_t e[]={{300,60,100},{700,60,0}};
    memcpy(cap_loop[0],e,sizeof e);cap_ln[0]=2;cap_len[0]=65536u;cap_st[0]=2u;
    cap_publish();
    REQUIRE("PAIR follower quant posts to the CAPTURE owner",cap_selected()==0u);
    uxl.play=1;fm1_in.buttons|=1u<<panel.btn[B_REC];
    uxl_cap_knob(1,1);
    REQUIRE("REC held + K2 posts quant, never creates loop on follower",
            cap_cq_w!=cap_cq_r && cap_ln[1]==0u && uxl.rec_knob_used);
    fm1_in.buttons=0;cap_idle();cap_view_t v;cap_view(0,&v);
    REQUIRE("PAIR owner has pending quant and follower is unaffected",v.requested==1u && !cap_qwant[1]);
    cap_wipe(0);
#endif
    cq_fresh();uxl.play=1;
    perf_bars_q16=8000;cap_put(0,60,98);
    perf_bars_q16=9000;cap_put(0,60,0);
    btn_down(B_REC);frame();
    turn(EN_K2,1);             /* before the 350ms view opens */
    btn_up(B_REC);frame();cap_idle();
    REQUIRE("REC+K2 does not create CAPTURE or overdub on release",cap_ln[0]==0u &&
            !cap_qactive[0] && !cap_qwant[0]);
}
int main(void)
{
    test_grid_and_restore();
    test_wrap_short_simultaneous();
    test_same_pitch_touch_and_long_loop();
    test_same_pitch_collision();
    test_no_prequant();
    test_boundary_overdub_pause_clear();
    test_pair_and_rec_gesture();
    printf("post-CAPTURE QUANT: %u failed assertions\n",failures);
    return failures!=0;
}
