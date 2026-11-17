/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Native integration probes, real sequencer/input/voice code on ui_test's HAL.
 * Build with the UX/profile flags used by the emulator. Expected failures are
 * deliberately assertions of the required behaviour, not golden bugs. */
#define FELUCCA_CPU2 0
#define CARAVEL_UX_CAPTURE 1
#define UI_TEST_NO_MAIN 1
#include <stdint.h>
static void action_refresh(void);
static void action_dispatch(uint32_t keys);
#include "ui_harness.h"
#if CARAVEL_ACTION_BANKS
#include "../experiments/action-banks/action_control.c"
#endif
#include "chordext_sim.h"

static unsigned failures;
#define REQUIRE(s, expr) do { int pass = !!(expr); printf("capture: %s: %s\n", s, pass ? "PASS" : "FAIL"); failures += !pass; } while (0)
static void fresh(void)
{
    unsigned i;
    ui_power_on(); hp_init();
    memset(&ep, 0, sizeof ep);
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(&uxl, 0, sizeof uxl);
    memset(cap_rn, 0, sizeof cap_rn); memset(cap_rh, 0, sizeof cap_rh);
    memset(cap_ln, 0, sizeof cap_ln); memset(cap_st, 0, sizeof cap_st);
    /* Newer implementations may add capacity feedback; clearing via cap_wipe
     * keeps this probe valid on the original prototype too. */
    for (i=0;i<NTRK;i++) { cap_wipe(i); cap_li[i] = 1u; }
    memset(cap_hn, 0, sizeof cap_hn); memset(cap_held, 0, sizeof cap_held);
    memset(cap_open, 0, sizeof cap_open); memset(cap_tt, 0, sizeof cap_tt);
    memset(lat_stay, 0, sizeof lat_stay); memset(lat_eat, 0, sizeof lat_eat);
    memset(cap_serial,0,sizeof cap_serial);memset(cap_taken_serial,0,sizeof cap_taken_serial);
    cap_clock_prev=0;cap_clock_wrapped=0;
    perf_bars_q16 = perf_bars_rem = 0; harmony_phase = harmony_base_bar = 0;
    memset(midi_notes,0,sizeof midi_notes); memset(midi_ch,0,sizeof midi_ch);
    memset(midi_owners,0,sizeof midi_owners); memset(mchord,0,sizeof mchord);
    memset(&fm1_in,0,sizeof fm1_in); ce_reset();
    harmony_on=0; memset(harmony_follow,0,sizeof harmony_follow);
    fm1_ms = 1000; cap_replay = cap_skip = 0; in_key = 0;
    for (i = 0; i < NTRK; i++) {
        set_engine_of(&trk[i], 0); apply_preset_to(&trk[i], 0);
        trk[i].engine = trk[i].eng_req; trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_CSRC]=0; trk[i].p[P_CHRD]=CH_OFF; trk[i].p[P_QUANT]=QN_OFF;
        song.mch[i]=(uint8_t)(i+1u);
        trk[i].p[P_LATCH] = LT_OFF; trk[i].p[P_AMODE] = 0;
        memset(trk[i].step, 0, sizeof trk[i].step);
    }
    song.g[G_CLOCK] = 0; song.g[G_ROUTE] = 0;
    song.playing = 1; kb_boot_hold = 0;
}
static unsigned probe_gates(unsigned t)
{
    unsigned j, n = 0;
    for (j = 0; j < NVOICE; j++) n += trk[t].v[j].active && trk[t].v[j].gate;
    return n;
}
static unsigned has(unsigned t, unsigned note)
{
    unsigned j;
    for (j = 0; j < NVOICE; j++)
        if (trk[t].v[j].active && trk[t].v[j].gate && trk[t].v[j].note == note) return 1;
    return 0;
}
static void notes(unsigned t)
{
    unsigned j; printf("played T%u:", t + 1);
    for (j = 0; j < NVOICE; j++)
        if (trk[t].v[j].active && trk[t].v[j].gate) printf(" %u", trk[t].v[j].note);
    puts("");
}
static void loop(unsigned t, unsigned note)
{
    cap_loop[t][0] = (cap_ev_t){0, note, 100};
    cap_loop[t][1] = (cap_ev_t){1024, note, 0};
    cap_ln[t] = 2; cap_len[t] = 65536; cap_o[t] = perf_bars_q16;
    cap_st[t] = 1; cap_last[t] = -1;
}
static void midi_capture(void)
{
    for(unsigned trs=0;trs<2;trs++)for(unsigned track=0;track<NTRK;track++) {
        fresh(); song.sel=(uint8_t)((track+1u)%NTRK);
        ce_ev_t ev[]={
            {0,trs?CE_TRS:CE_USB,(uint8_t)(0x90u|track),60,93},
            {12000,trs?CE_TRS_RS:CE_USB,(uint8_t)(0x90u|track),64,72},
            {24000,trs?CE_TRS:CE_USB,(uint8_t)(0xB0u|track),64,127},
            {36000,trs?CE_TRS:CE_USB,(uint8_t)(0x80u|track),60,0},
            {48000,trs?CE_TRS:CE_USB,(uint8_t)(0x90u|track),64,0},
        };
        ce_sched(ev,sizeof ev/sizeof *ev);ce_run_until(60000);
        REQUIRE("USB/TRS, channel track, running status and velocity-zero off captured",cap_rn[track]==4 &&
            cap_ring_at(track,0)->note==60 && cap_ring_at(track,0)->vel==93 &&
            cap_ring_at(track,1)->note==64 && cap_ring_at(track,1)->vel==72 &&
            cap_ring_at(track,2)->note==60 && !cap_ring_at(track,2)->vel &&
            cap_ring_at(track,3)->note==64 && !cap_ring_at(track,3)->vel);
        REQUIRE("physical MIDI offs end capture while sustain still sounds",!cap_open[track][1] && !cap_open[track][2] && has(track,60));
        unsigned count=cap_rn[track],over; midi_event(0xB0,track,64,0);
        REQUIRE("pedal up releases sound without a duplicate captured off",!has(track,60)&&cap_rn[track]==count);
        REQUIRE("MIDI notes can be taken into a loop",cap_take(track,1,&over)==2&&cap_ln[track]==4);
        cap_release(track);unsigned recorded=cap_rn[track];
        perf_bars_q16=cap_o[track]+cap_len[track];cap_last[track]=-1;cap_tick();
        REQUIRE("loop playback never records itself",cap_rn[track]==recorded);
    }
}
int main(void)
{
    unsigned i, over;
    midi_capture();
    fresh(); loop(0, 60); trk[0].p[P_AMODE] = AM_UP;
    cap_tick(); REQUIRE("ARP receives loop pitch", trk[0].nheld == 1 && trk[0].held[0] == 60);
    arp_step(&trk[0], 0, 0); notes(0);
    REQUIRE("ARP emits a gated voice", probe_gates(0) > 0);
    perf_bars_q16 = 17000; cap_tick();
    REQUIRE("ARP loop release removes physical input", trk[0].arp_phys == 0);

    for (i = LT_NEW; i <= LT_STACK; i++) {
        fresh(); trk[0].p[P_LATCH] = i; loop(0, 60); cap_tick();
        perf_bars_q16 = 17000; cap_tick();
        REQUIRE("LATCH sustains first loop note after off", has(0, 60));
        cap_play_on(0, 67, 100); notes(0);
        REQUIRE(i == LT_NEW ? "LATCH NEW replaces old set" : "LATCH STACK loop follows E8 NEW", !has(0, 60) && has(0, 67));
    }

    fresh(); harmony_on = harmony_follow[0] = 1;
    trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 1;
    for (i = 0; i < 4; i++) { hroot_set(i, i == 1 ? 7 : 0); hdur_set(i, 0); }
    harmony_bar = 0; loop(0, 60); cap_tick(); notes(0);
    REQUIRE("HARMONY C chord emits C4", has(0, 60)); cap_release(0);
    harmony_bar = 1; cap_last[0] = -1; cap_tick(); notes(0);
    REQUIRE("same raw loop follows current G chord (B3)", has(0, 59) && !has(0, 60)); cap_release(0);
    trk[0].p[P_CHRD] = CH_MAJ; cap_last[0] = -1; cap_tick(); notes(0);
    REQUIRE("CHRD expands snapped B3 into 59,63,66", has(0,59) && has(0,63) && has(0,66)); cap_release(0);
    trk[0].p[P_CHRD] = CH_BUS; cap_last[0] = -1; cap_tick(); notes(0);
    REQUIRE("CHRD BUS emits G triad at 55,59,62", has(0,55) && has(0,59) && has(0,62));

    fresh(); harmony_on = harmony_follow[0] = 1;
    trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 1;
    for (i=0;i<4;i++) { hroot_set(i, i==1 ? 7 : 0); hdur_set(i,0); }
    perf_bars_q16 = 65535; harmony_bar = 0; harmony_phase = harmony_base_bar = 0;
    loop(0,60); cap_o[0] = 65536; cap_last[0] = 65000;
    events_block(128); notes(0);
    REQUIRE("loop on bar boundary uses newly effective harmony", harmony_bar == 1 && has(0,59) && !has(0,60));

    fresh(); harmony_on=harmony_follow[0]=1;trk[0].p[P_ROOT]=0;trk[0].p[P_SCALE]=1;
    hroot_set(0,7); harmony_bar=0; midi_event(0x90,0,60,100);
    REQUIRE("MIDI loop stores raw note before HARMONY", cap_rn[0]==1 && cap_ring_at(0,0)->note==60);

    fresh();
    for (i = 0; i < 4; i++) {
        song.mch[i] = i + 1; midi_event(0x90, i, 60+i, 100);
        perf_bars_q16 += 5000; midi_event(0x80, i, 60+i, 0);
        REQUIRE("MIDI channel recorded on matching track", cap_rn[i] == 2 && cap_ring_at(i,0)->note == 60+i);
    }
    for (i=0;i<4;i++) { cap_take(i,1,&over); cap_release(i); }
    song.sel = 3; perf_bars_q16 += 65536; cap_tick();
    REQUIRE("all four loops survive selected-track change", cap_st[0] == 1 && cap_st[3] == 1);
    { unsigned seen[4]={0},k; for(k=0;k<21;k++) {
        cap_tick();for(i=0;i<4;i++)seen[i]|=has(i,60+i);perf_bars_q16+=1000;
      }
      REQUIRE("all four MIDI loops emit their actual pitches after track change", seen[0]&&seen[1]&&seen[2]&&seen[3]); }


    fresh(); loop(0,60); song.g[G_CLOCK] = 1;
    events_block(128); cap_idle();
    { unsigned sounded=0;
      midi_enqueue(0x0Fu | (0xFAu<<8),1);events_block(128);sounded|=probe_gates(0)>0;
      for (i=0;i<30;i++) { fm1_ms += 21; midi_enqueue(0x0Fu | (0xF8u<<8),1); events_block(128);sounded|=probe_gates(0)>0; }
      cap_idle();
      REQUIRE("external MIDI START automatically resumes waiting loop", cap_st[0] == 1 && sounded);
    }
    cap_resume(0); midi_enqueue(0x0Fu | (0xFCu<<8),1);events_block(128); cap_idle();
    REQUIRE("external STOP pauses loop and releases voices", cap_st[0] == 2 && probe_gates(0) == 0);
    midi_enqueue(0x0Fu | (0xFAu<<8),1);events_block(128); cap_resume(0);
    fm1_ms += 501; events_block(128); cap_idle();
    REQUIRE("clock loss stops transport and releases loop", !song.playing && cap_st[0] == 2 && probe_gates(0) == 0);

    fresh(); for (i=0;i<80;i++) { cap_put(0,48+i%24,100); perf_bars_q16 += 100; cap_put(0,48+i%24,0); perf_bars_q16 += 100; }
    REQUIRE("LIVE ring bounded at 128", cap_rn[0] == 128);
    cap_take(0,1,&over);
    { unsigned n=cap_ln[0], on=0, off=0; for(i=0;i<n;i++) { on += !!cap_loop[0][i].vel; off += !cap_loop[0][i].vel; }
      printf("full ring: loop events=%u on=%u off=%u\n",n,on,off);
      REQUIRE("full-ring capture keeps complete note pairs", n == 128 && on == 64 && off == 64); }

    fresh(); for(i=0;i<9;i++) cap_play_on(0,60+i,100);
    for(i=0;i<9;i++) cap_play_off(0,60+i);
    REQUIRE("nine simultaneous raw notes retain release ownership", probe_gates(0) == 0);
    fresh(); cap_play_on(0,60,100); kb_note[0]=60; kb_trk[0]=0; kb_vel=100;
    key_on(0,&trk[0]); cap_play_off(0,60);
    REQUIRE("loop off preserves same live pitch", has(0,60));

    fresh(); perf_bars_q16=0xFFFFF000u; cap_put(0,60,100);
    perf_bars_q16=0x00001000u; cap_put(0,60,0); cap_take(0,1,&over);
    REQUIRE("capture across Q16 clock wrap retains both ends", cap_ln[0] == 2 && cap_loop[0][0].t < 4096 && cap_loop[0][1].t < 4096 && ((cap_loop[0][0].vel ? cap_loop[0][1].t-cap_loop[0][0].t : cap_loop[0][0].t-cap_loop[0][1].t)&4095u) == 512);
    fresh();
    for (i=0;i<65;i++) { cap_put(0,40+i,100); perf_bars_q16 += 10; }
    for (i=0;i<65;i++) { cap_put(0,40+i,0); perf_bars_q16 += 10; }
    cap_take(0,1,&over);
    { unsigned active[128]={0}, balanced=1; for(i=0;i<cap_ln[0];i++) { cap_ev_t *e=&cap_loop[0][i]; active[e->note]=!!e->vel; }
      for(i=0;i<128;i++) balanced &= !active[i];
      REQUIRE("dense full-ring capture closes every admitted note", balanced); }
    fresh(); song.playing=0; REQUIRE("PAIR can be enabled", pair_enable(0,1)); song.playing=1;
    song.sel=1; kb_vel=100; kb_note[0]=60; kb_trk[0]=0; key_on(0,pair_owner(TSEL));
    perf_bars_q16=8000; key_off(0,pair_owner(TSEL));
    REQUIRE("PAIR raw capture belongs to owner", cap_rn[0]==2 && cap_rn[1]==0);
    uxl_cap_rec(); cap_idle(); uxl_cap_poll();
    REQUIRE("REC on PAIR follower captures owner's notes", cap_ln[0]>0);
    fresh();
    for(i=0;i<65;i++) { cap_put(0,40+i,100); perf_bars_q16+=10; }
    REQUIRE("65 held notes reject the whole take instead of losing offs", cap_take(0,1,&over)==0 && cap_ln[0]==0);
    fresh(); loop(0,60);
    for(i=0;i<128;i++) cap_loop[0][i]=(cap_ev_t){i*100u,(uint8_t)(48+i/2),i%2?0:100};
    cap_ln[0]=128;
    { cap_ev_t keep[128]; uint32_t old_o=cap_o[0],old_tt=cap_tt[0]; memcpy(keep,cap_loop[0],sizeof keep);
      perf_bars_q16=9000;cap_put(0,90,100);perf_bars_q16=10000;cap_put(0,90,0);
      REQUIRE("overflow overdub keeps the previous loop byte for byte", cap_take(0,1,&over)==0 &&
          !memcmp(keep,cap_loop[0],sizeof keep) && cap_o[0]==old_o && cap_tt[0]==old_tt); }
    fresh(); cap_play_on(0,60,100);kb_note[0]=60;kb_trk[0]=0;kb_vel=100;key_on(0,&trk[0]);
    key_off(0,&trk[0]); REQUIRE("live key-up preserves the loop's shared pitch",has(0,60));
    cap_play_off(0,60);REQUIRE("last shared owner releases the voice",!has(0,60));
    fresh();harmony_on=harmony_follow[0]=1;trk[0].p[P_ROOT]=0;trk[0].p[P_SCALE]=1;hroot_set(0,0);
    cap_play_on(0,59,100);cap_play_on(0,60,100);cap_play_off(0,59);
    REQUIRE("two snapped raw inputs share one pitch until both end",has(0,60));
    cap_play_off(0,60);REQUIRE("last mapped loop owner releases shared pitch",!has(0,60));
    fresh();for(i=0;i<CAP_N/2;i++)cap_play_on(0,40+i,100);
    for(i=0;i<CAP_N/2;i++)cap_play_off(0,40+i);
    REQUIRE("all 64 admitted simultaneous raw inputs retain their releases",probe_gates(0)==0);
    fresh();trk[0].p[P_LATCH]=LT_STACK;cap_play_on(0,60,100);cap_play_off(0,60);
    cap_release(0);REQUIRE("pausing clears the loop's released latched voice",!has(0,60)&&!lat_get(lat_stay[0],60));
    fresh();trk[0].p[P_LATCH]=LT_NEW;cap_play_on(0,60,100);cap_play_off(0,60);
    kb_note[0]=60;kb_trk[0]=0;kb_vel=100;key_on(0,&trk[0]);key_off(0,&trk[0]);cap_release(0);
    REQUIRE("pause preserves a new live latch owner of the same pitch",has(0,60));
    fresh();loop(0,60);cap_tick();cap_pause(0);song.playing=0;cap_idle();song.playing=1;cap_idle();
    REQUIRE("transport restart preserves a manual SEQ pause",cap_st[0]==2u);
    { cap_ev_t expected[CAP_N]; unsigned count;
      fresh();perf_bars_q16=0x1F000u;cap_put(0,60,100);
      perf_bars_q16=0x21000u;cap_put(0,60,0);perf_bars_q16=0x22000u;cap_take(0,1,&over);
      count=cap_ln[0];memcpy(expected,cap_loop[0],sizeof expected);
      fresh();perf_bars_q16=0xFFFFF000u;cap_put(0,60,100);
      perf_bars_q16=0x1000u;cap_put(0,60,0);perf_bars_q16=0x2000u;cap_take(0,1,&over);
      REQUIRE("wrapped capture equals the same window before wrap",cap_ln[0]==count && !memcmp(expected,cap_loop[0],count*sizeof expected[0]));
    }
    fresh();perf_bars_q16=0xFFFFD000u;loop(0,60);cap_tt[0]=perf_bars_q16;cap_tick();
    perf_bars_q16=0xFFFFF000u;cap_put(0,65,100);perf_bars_q16=0x1000u;cap_put(0,65,0);
    REQUIRE("overdub crosses clock wrap at the original phase",cap_take(0,1,&over)==1 && over && cap_ln[0]==4 && cap_loop[0][1].note==65 && cap_loop[0][1].t==512 && cap_loop[0][3].note==65 && cap_loop[0][3].t==1024);
    REQUIRE("repeated REC after wrap adds no second copy",cap_take(0,1,&over)==0 && cap_ln[0]==4);
    fresh();perf_bars_q16=0xFFFFF000u;loop(0,60);cap_tick();perf_bars_q16=0x1000u;cap_pause(0);
    REQUIRE("pause stores the phase across clock wrap",cap_pph[0]==8192);
    perf_bars_q16=0x2000u;cap_resume(0);
    REQUIRE("resume preserves the wrapped phase",(perf_bars_q16-cap_o[0])%cap_len[0]==8192);
    fresh();loop(0,60);cap_loop[0][0]=(cap_ev_t){0,60,0};cap_loop[0][1]=(cap_ev_t){3750,60,100};cap_last[0]=59000;
    perf_bars_q16=66000;cap_tick();
    REQUIRE("a wrapped block dispatches the tail before the head",!has(0,60) && !cap_note_held(&trk[0],60));
    fresh();cap_put(0,60,100);perf_bars_q16=100;cap_put(0,60,0);cap_take(0,1,&over);
    cap_put(0,62,100);cap_put(0,62,0);
    REQUIRE("new events at the previous REC timestamp are not lost",cap_take(0,1,&over)==1 && cap_ln[0]==4);
    /* Extended CAPTURE: next-take LENGTH is independent for each track. */
    fresh();
    {   static const unsigned li[4] = {0, 2, 4, 5};
        for(i=0;i<NTRK;i++) cap_li[i]=(uint8_t)li[i];
        song.sel=0; cap_publish(); uxl_cap_knob(0,1);
        REQUIRE("REC K1 changes only selected track LENGTH",cap_li[0]==1 && cap_li[1]==2 && cap_li[2]==4 && cap_li[3]==5);
        cap_li[0]=0;
        perf_bars_q16 = 31u << 16;
        for(i=0;i<NTRK;i++) cap_put(i,60u+i,100);
        perf_bars_q16 += 1000u;
        for(i=0;i<NTRK;i++) cap_put(i,60u+i,0);
        for(i=0;i<NTRK;i++) {
            cap_view_t cv;
            song.sel=(uint8_t)i;
            uxl_cap_rec(); cap_idle(); uxl_cap_poll();
            cap_view(i,&cv);
            REQUIRE("REC selects each track's own 1/4/16/32 bars",cap_len[i]==((uint32_t)CAP_BARS[li[i]]<<16) && cv.bars==CAP_BARS[li[i]] && cv.ln==2);
        }
        REQUIRE("other track LENGTH selections survive REC and track switches",cap_li[0]==0 && cap_li[1]==2 && cap_li[2]==4 && cap_li[3]==5);
    }
    fresh();perf_bars_q16 = 17u << 16;cap_put(0,60,100);
    perf_bars_q16 += 1024u;cap_put(0,60,0);
    perf_bars_q16 = 32u << 16;
    REQUIRE("32-bar loop captures events past old 16-bit time wrap",cap_take(0,32,&over)==1 && cap_len[0]==(32u<<16) &&
        cap_ln[0]==2 && cap_loop[0][0].t >= (17u<<12) && cap_loop[0][1].t > cap_loop[0][0].t);
    fresh();perf_bars_q16=0xFFF00000u;cap_put(0,70,100);
    perf_bars_q16=0xFFF01000u;cap_put(0,70,0);
    perf_bars_q16=0x00010000u;
    REQUIRE("32-bar capture survives Q16 wrap with 18-bit timestamps",cap_take(0,32,&over)==1 && cap_ln[0]==2 &&
        cap_loop[0][1].t > cap_loop[0][0].t);
    fresh();
    for(i=0;i<128u;i++)cap_loop[0][i]=(cap_ev_t){i*10u,(uint8_t)(40u+i/2u),(uint8_t)(i%2u?0u:100u)};
    cap_ln[0]=128;cap_len[0]=32u<<16;cap_st[0]=1;cap_publish();
    { cap_view_t cv;cap_view(0,&cv);
      REQUIRE("LCD snapshot stores all 128 events and 32-bar length",cv.ln==128 && cv.bars==32);
    }
    printf("memory: rings=%zu held-maps=%zu total test failures=%u\n",sizeof cap_ring+sizeof cap_loop,
           sizeof cap_hn+sizeof cap_held+sizeof cap_open,failures);
    return failures ? 1 : 0;
}
