/* SPDX-License-Identifier: GPL-3.0-only
 * Custom MIDI resync through the real USB/TRS queues, transport, sequencers,
 * ARPs and panel handlers. MIDI clock has no bar metadata: only Start anchors it. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void rs_packet(uint32_t source, uint32_t status, uint32_t ms)
{
    fm1_ms = ms;
    if (source == 2u) um_byte(status);
    else midi_enqueue(0xFu | status << 8, source);
    events_block(CTL);
}
static void rs_setup(uint32_t source)
{
    ui_power_on();
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um);
    mi_r = mi_w = 0; midi_in_overflow = transport_req = midi_resync_notice = 0;
    midi_beat_samples = 0; fm1_ms = 0;
    song.g[G_CLOCK] = (int16_t)source; song.g[G_BPM] = 120;
    events_block(CTL);
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, 0); apply_preset_to(t, 0); t->engine = t->eng_req;
        t->p[P_SLEN] = (int16_t)(16u + k * 7u); t->p[P_SDIV] = 2;
        t->p[P_SSWING] = 0; t->p[P_SQNT] = 1;
        for (uint32_t i = 0; i < NSTEP; i++)
            t->step[i] = (step_t){{(uint8_t)(30u + i % 16u)}, 1, ST_NOTE, 0, 100};
    }
}
static void rs_clock(uint32_t source, uint32_t i)
{
    rs_packet(source, 0xF8u, 10u + i * 125u / 6u);
}
static void rs_request(uint32_t mode)
{
    transport_req = (uint8_t)(mode + 3u);
    events_block(CTL);
}
static int rs_at_top(void)
{
    for (uint32_t k = 0; k < NTRK; k++)
        if (trk[k].seq_idx || trk[k].seq_pos) return 0;
    return 1;
}
static int rs_bar_test(uint32_t source)
{
    int bad = 0;
    rs_setup(source);
    rs_packet(source, 0xFAu, 10);
    for (uint32_t i = 0; i <= 30; i++) rs_clock(source, i);
    rs_request(1);
    bad += check("bar resync arms without interrupting playback", midi_clock.resync == 1u && song.playing);
    for (uint32_t i = 31; i <= 95; i++) rs_clock(source, i);
    bad += check("first clock is downbeat zero; clocks 1..95 do not trigger", midi_clock.resync == 1u && midi_clock.bar_tick == 95u);
    motion_store_t saved = motion;
    step_t steps[NTRK][NSTEP];
    for (uint32_t k = 0; k < NTRK; k++) memcpy(steps[k], trk[k].step, sizeof steps[k]);
    rs_clock(source, 96);
    bad += check("clock 96 resets all four polymetric tracks together", !midi_clock.resync && rs_at_top() && song.playing && midi_resync_notice == RS_DONE);
    int intact = !memcmp(&saved, &motion, sizeof saved);
    for (uint32_t k = 0; k < NTRK; k++) intact &= !memcmp(steps[k], trk[k].step, sizeof steps[k]);
    bad += check("resync changes no pattern or stored automation", intact);
    rs_request(1); rs_request(1);
    bad += check("repeating the same request cancels it", !midi_clock.resync && midi_resync_notice == RS_CANCEL);
    for (uint32_t i = 97; i <= 192; i++) rs_clock(source, i);
    bad += check("cancelled request never fires later", midi_resync_notice == RS_CANCEL);
    return bad;
}
static int rs_pulse_test(uint32_t source)
{
    int bad = 0;
    rs_setup(source); rs_packet(source, 0xFAu, 10);
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_AMODE] = AM_UP; trk[k].p[P_ARATE] = 2;
        trk[k].p[P_AHOLD] = 1;
        input_on(&trk[k], 60, 100); input_on(&trk[k], 64, 100); input_on(&trk[k], 67, 100);
    }
    for (uint32_t i = 0; i <= 40; i++) rs_clock(source, i);
    uint32_t old_pos = trk[0].seq_pos;
    rs_request(2);
    bad += check("manual request waits for a new pulse", midi_clock.resync == 2u && trk[0].seq_pos == old_pos);
    rs_clock(source, 41);
    int arps = 1;
    for (uint32_t k = 0; k < NTRK; k++) arps &= trk[k].arp_idx == 0u && !trk[k].arp_pos && trk[k].nheld == 3u &&
        trk[k].arp_note == 60u && trk[k].seq_n == 1u && trk[k].seq_notes[0] == 30u;
    bad += check("next pulse restarts sequences and held ARP chords at their first note", rs_at_top() && arps && song.playing);
    int gates_ok = 1;
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t v = 0; v < NVOICE; v++)
            if (trk[k].v[v].active && trk[k].v[v].gate && trk[k].v[v].note != 30u && trk[k].v[v].note != 60u)
                gates_ok = 0;
    bad += check("old sequenced and ARP notes are released at the reset", gates_ok);
    bad += check("manual resync preserves the master bar counter", midi_clock.bar_valid && midi_clock.bar_tick == 41u);
    rs_request(1);
    for (uint32_t i = 42; i <= 95; i++) rs_clock(source, i);
    bad += check("bar request after manual resync still waits for master clock 96", midi_clock.resync == 1u);
    rs_clock(source, 96);
    bad += check("manual phase changes do not move the master's next downbeat", rs_at_top() && !midi_clock.resync);
    rs_setup(source); transport_req = 1; events_block(CTL); rs_clock(source, 0);
    rs_request(1);
    bad += check("local PLAY cannot invent a master bar reference", !midi_clock.bar_valid && !midi_clock.resync && midi_resync_notice == RS_WAIT);
    rs_request(2); rs_request(1);
    bad += check("an unavailable bar replacement does not leave a pulse request armed", !midi_clock.resync && midi_resync_notice == RS_WAIT);
    rs_request(2); rs_clock(source, 1);
    bad += check("manual pulse resync also works without a received MIDI Start", rs_at_top() && !midi_clock.bar_valid && midi_resync_notice == RS_DONE);
    return bad;
}
static int rs_safety_test(void)
{
    int bad = 0; rs_setup(2); rs_packet(2, 0xFA, 10); rs_clock(2, 0); rs_clock(2, 1);
    rs_request(2);
    midi_enqueue(0xFEu << 8 | 0xFu, 2); events_block(CTL);
    bad += check("non-transport realtime cannot cancel resync or invalidate bars", midi_clock.resync == 2u && midi_clock.bar_valid);
    rs_packet(1, 0xFA, 35); rs_packet(1, 0xF8, 36);
    bad += check("wrong-source Start and clock cannot cancel or execute a resync", midi_clock.resync == 2u && midi_clock.bar_tick == 1u);
    rs_packet(2, 0xFC, 40);
    bad += check("MIDI Stop cancels the request and invalidates the bar reference", !song.playing && !midi_clock.resync && !midi_clock.bar_valid);
    rs_packet(2, 0xFB, 50); rs_packet(2, 0xF8, 51); rs_request(1);
    bad += check("Continue preserves position but cannot establish a bar reference", midi_resync_notice == RS_WAIT && !midi_clock.bar_valid);
    rs_request(2); fm1_ms = 552; events_block(CTL);
    bad += check("clock timeout cancels resync and stops safely", !song.playing && !midi_clock.resync && !midi_clock.bar_valid);
    rs_setup(2); rs_packet(2, 0xFA, 10); rs_clock(2, 0); rs_clock(2, 1); rs_request(1);
    song.g[G_CLOCK] = 1; events_block(CTL);
    bad += check("switching clock source discards pending resync and old reference", !midi_clock.resync && !midi_clock.bar_valid && !song.playing);
    rs_setup(2); rs_packet(2, 0xFA, 10); rs_clock(2, 0); rs_request(1);
    midi_in_overflow = 1; events_block(CTL);
    bad += check("queue overflow invalidates counted phase", !midi_clock.bar_valid && !midi_clock.resync);
    rs_setup(2); rs_packet(2, 0xFA, 10); rs_clock(2, 0);
    song.rec = 1; motion_store_t saved = motion; rs_request(2);
    bad += check("LIVE REC is preserved and resync is refused", song.rec == 1u && song.playing && !midi_clock.resync && midi_resync_notice == RS_REC && !memcmp(&saved, &motion, sizeof saved));
    song.rec = 0; rs_request(1); song.rec = 1; events_block(CTL);
    bad += check("recording begun after arming also cancels the request", !midi_clock.resync && song.rec == 1u);
    song.rec = 0; chain.armed = 1; midi_resync_request(2);
    bad += check("SONG is refused without changing its state", !midi_clock.resync && chain.armed && midi_resync_notice == RS_SONG);
    chain.armed = 0;
    rs_request(1); transport_req = 2; events_block(CTL);
    bad += check("local STOP cancels a request", !midi_clock.resync && !song.playing);
    return bad;
}
static int rs_batch_test(void)
{
    int bad = 0; rs_setup(1); rs_packet(1, 0xFA, 10); rs_clock(1, 0); rs_clock(1, 1);
    /* A clock already queued before the button request must not trigger it. */
    fm1_ms = 40; midi_enqueue(0xF8u << 8 | 0xFu, 1); fm1_ms = 45;
    rs_request(2);
    bad += check("older queued clocks cannot trigger a new button request", midi_clock.resync == 2u);
    fm1_ms = 60; midi_enqueue(0xF8u << 8 | 0xFu, 1);
    fm1_ms = 81; midi_enqueue(0xF8u << 8 | 0xFu, 1);
    events_block(CTL);
    bad += check("clocks queued after the reset pulse advance the new pass", !midi_clock.resync && trk[0].seq_idx == 0 && trk[0].seq_pos == beat_samples() / 24u && clk_pos == trk[0].seq_pos);
    rs_setup(1); transport_req = 1; events_block(CTL); rs_clock(1, 0); rs_request(2);
    rs_packet(1, 0xF8, 600);
    bad += check("a long gap cancels an unanchored pulse request before resumed clocks", !midi_clock.resync && midi_resync_notice == RS_CANCEL && !midi_clock.bar_valid);
    rs_setup(1); rs_packet(1, 0xFA, 0xFFFFFFE0u); rs_packet(1, 0xF8, 0xFFFFFFE1u);
    fm1_ms = 0xFFFFFFF0u; rs_request(2); rs_packet(1, 0xF8, 5);
    bad += check("resync request timestamps survive millisecond wrap", !midi_clock.resync && midi_resync_notice == RS_DONE && rs_at_top());
    rs_setup(1); rs_packet(1, 0xFA, 10); rs_clock(1, 0); rs_clock(1, 1);
    midi_clock.pos = midi_clock.rendered = 0xFFFFFC18u;
    uint32_t before = trk[0].seq_pos;
    rs_clock(1, 2);
    bad += check("fixed musical clock counter wrap preserves forward sequencer progress", song.playing &&
                 trk[0].seq_idx == 0u && trk[0].seq_pos > before && midi_clock.pos == 0u);
    return bad;
}
static int rs_ui_test(void)
{
    int bad = 0; rs_setup(2); rs_packet(2, 0xFA, 10); rs_clock(2, 0); rs_clock(2, 1);
    btn_down(B_GLO); frame(); press(B_PLAY); events_block(CTL); ui_notices();
    bad += check("real GLO+PLAY arms bar resync and shows its status", midi_clock.resync == 1u && msg_is("RESYNC: NEXT BAR"));
    press(B_PLAY); events_block(CTL); ui_notices();
    bad += check("real GLO+PLAY again cancels while playback continues", !midi_clock.resync && song.playing && msg_is("RESYNC CANCELLED"));
    key_down(white(7)); frame(); key_up(white(7)); frame(); events_block(CTL); ui_notices();
    bad += check("real GLO+F4 arms manual pulse resync without changing tempo", midi_clock.resync == 2u && song.g[G_BPM] == 120 && msg_is("RESYNC: NEXT PULSE"));
    btn_up(B_GLO); frame();
    fm1_ms += 20; rs_packet(2, 0xF8, fm1_ms); ui_notices();
    bad += check("resync survives releasing GLO and reports completion", midi_resync_notice == 0 && msg_is("RESYNC DONE") && rs_at_top());
    return bad;
}
static int rs_start_test(uint32_t source)
{
    int bad = 0;
    rs_setup(source); rs_packet(source, 0xFA, 10);
    for (uint32_t i = 0; i <= 30; i++) rs_clock(source, i);
    transport_req = 2; events_block(CTL);
    uint16_t old_idx = trk[0].seq_idx;
    rs_request(1);
    bad += check("stopped transport arms a counted bar start while remaining silent", !song.playing && midi_clock.resync_start &&
                 midi_clock.resync == 1u && midi_resync_notice == RS_START_BAR && !gates());
    for (uint32_t i = 31; i <= 95; i++) rs_clock(source, i);
    bad += check("armed start does not advance or sound before the downbeat", !song.playing && !gates() && trk[0].seq_idx == old_idx);
    rs_clock(source, 96);
    bad += check("stopped bar start fires all tracks together at clock 96", song.playing && rs_at_top() && !midi_clock.resync_start && !midi_clock.resync && gates());
    rs_setup(source); rs_packet(source, 0xF8, 10);
    rs_request(1);
    bad += check("joining clock-only while stopped still needs Start for a bar", !song.playing && midi_resync_notice == RS_WAIT);
    rs_request(2);
    bad += check("clock-only input permits a silent next-pulse start", !song.playing && midi_clock.resync_start && midi_resync_notice == RS_START_PULSE);
    rs_clock(source, 1);
    bad += check("first requested pulse starts without a previous master Start", song.playing && rs_at_top() && !midi_clock.bar_valid);
    transport_req = 2; events_block(CTL); rs_clock(source, 2); rs_request(2); rs_request(2);
    rs_clock(source, 3);
    bad += check("repeating a stopped-start request cancels without starting", !song.playing && !midi_clock.resync && !midi_clock.resync_start);
    rs_request(2); fm1_ms += 501; events_block(CTL);
    bad += check("stopped-start request also times out without incoming clock", !song.playing && !midi_clock.resync);
    rs_clock(source, 40);
    bad += check("resumed clocks cannot execute a timed-out start", !song.playing && !gates());
    rs_setup(source); rs_packet(source, 0xFA, 10); rs_clock(source, 0);
    transport_req = 2; events_block(CTL);
    btn_down(B_GLO); frame(); press(B_PLAY); events_block(CTL); ui_notices();
    bad += check("real stopped GLO+PLAY shows START NEXT BAR", !song.playing && midi_clock.resync_start && msg_is("START: NEXT BAR"));
    btn_up(B_GLO); frame();
    rs_packet(source, 0xFC, fm1_ms + 1u); rs_packet(source, 0xF8, fm1_ms + 20u);
    bad += check("MIDI Stop cancels an armed start without later auto-start", !song.playing && !midi_clock.resync_start);
    return bad;
}

static int rs_menu_test(uint32_t source)
{
    int bad = 0;
    rs_setup(source); rs_clock(source, 0);
    bad += check("EXT START defaults to PULSE, CPU2 defaults OFF", menu_get(MI_EXTSTART) == 0u && ui_prefs3 == 0u);
    press(B_PLAY); events_block(CTL);
    bad += check("plain PLAY arms the next pulse and stays silent", !song.playing && midi_clock.resync == 2u && !gates());
    press(B_PLAY); events_block(CTL); rs_packet(source, 0xF8, fm1_ms + 21u);
    bad += check("plain PLAY again cancels the waiting start", !song.playing && !midi_clock.resync && !gates());
    press(B_PLAY); events_block(CTL); rs_packet(source, 0xF8, fm1_ms + 21u);
    bad += check("plain PLAY starts on the requested pulse", song.playing && rs_at_top());
    press(B_PLAY); events_block(CTL);
    bad += check("plain PLAY while running still stops", !song.playing);
    menu_put(MI_EXTSTART, 1); press(B_PLAY); events_block(CTL);
    bad += check("BAR setting requires a master Start reference", !song.playing && midi_resync_notice == RS_WAIT);
    rs_packet(source, 0xFA, fm1_ms + 1u); rs_packet(source, 0xF8, fm1_ms + 1u);
    transport_req = 2; events_block(CTL);
    press(B_PLAY); events_block(CTL);
    bad += check("BAR setting arms plain PLAY at the next bar", !song.playing && midi_clock.resync == 1u);
    for (uint32_t i = 1; i <= 96; i++) rs_packet(source, 0xF8, fm1_ms + 21u);
    bad += check("plain PLAY reaches the counted bar boundary", song.playing && rs_at_top());
    menu_put(MI_LIVELOAD, 1);
    persist_t prefs = {0}; settings_export(&prefs);
    ui_prefs2 = ui_prefs3 = 0; settings_import(&prefs, sizeof prefs);
    bad += check("saved FULL and BAR preferences round trip without enabling CPU2", menu_get(MI_LIVELOAD) == 1u && menu_get(MI_EXTSTART) == 1u && !(ui_prefs3 & 1u));
#if FELUCCA_CPU2
    menu_put(MI_CPU2, 1);
    bad += check("CPU2 is independent of FULL and BAR", menu_get(MI_CPU2) == 1u && menu_get(MI_LIVELOAD) == 1u && menu_get(MI_EXTSTART) == 1u);
#endif
    return bad;
}

int main(void)
{
    int bad = rs_bar_test(1) + rs_bar_test(2) + rs_pulse_test(1) + rs_pulse_test(2) +
        rs_safety_test() + rs_batch_test() + rs_ui_test() + rs_start_test(1) + rs_start_test(2);
    bad += rs_menu_test(1); bad += rs_menu_test(2);
    printf("MIDI RESYNC: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
