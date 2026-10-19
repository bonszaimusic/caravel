/* SPDX-License-Identifier: GPL-3.0-only */
/* Real UI gestures and sequencer: cursor recording independent of playback. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void mode_gesture(void)
{
    btn_down(B_SEQ); frame();
    press(B_REC);
    btn_up(B_SEQ); frame();
}
int main(void)
{
    int bad = 0;
    ui_power_on();
    go_page(GR_ROLL);
    track_t *t = TSEL;
    t->p[P_VOICE] = V_POLY;
    memset(t->step, 0, sizeof t->step);
    t->p[P_SLEN] = 16;
    seq_start();
    events_block(64);
    cursor_set(8);
    mode_gesture();
    bad += check("step live: SEQ + REC selects STEP REC without stopping", song.playing &&
        song.rec == 1 && step_record_mask == 1 && !transport_req && msg_is("STEP REC") && !ui.layer);
    uint32_t head = t->seq_idx;
    step_t old_head = t->step[head];
    key_down(7); frame();
    bad += check("step live: key writes chosen cursor, never playhead", t->step[8].n == 1 &&
        t->step[8].note[0] == kb_map(t,7) && ui.cursor == 8 && !memcmp(&old_head,&t->step[head],sizeof old_head));
    key_down(11); frame();
    bad += check("step live: poly chord accumulates on one step", t->step[8].n == 2 && ui.cursor == 8);
    key_up(7); frame();
    bad += check("step live: cursor waits for every held key", ui.cursor == 8);
    key_up(11); frame();
    bad += check("step live: release advances once with playback still running", ui.cursor == 9 && song.playing);
    uint32_t pos = t->seq_pos;
    events_block(64);
    bad += check("step live: playhead advances independently", t->seq_pos != pos && ui.cursor == 9 && !rec_on(t));
    input_on(t, 80, 100); input_off(t,80);
    bad += check("step live: MIDI input cannot live-write at playhead", !memcmp(&old_head,&t->step[head],sizeof old_head));
    uint32_t count = motion.count;
    motion_capture(t,P_PAN,10);
    bad += check("step live: knob moves cannot accidentally record playhead automation", motion.count == count);
    cursor_set(15);
    key_down(7); frame(); key_up(7); frame();
    bad += check("step live: release wraps inside pattern length", ui.cursor == 0 && t->step[15].n == 1);
    ui_prefs2 |= PREF_CHORD_ADD >> 16;
    cursor_set(10);
    key_down(7); frame(); key_up(7); frame();
    key_down(11); frame(); key_up(11); frame();
    bad += check("step live: ADD keeps cursor for one-finger chords", ui.cursor == 10 && t->step[10].n == 2);
    ui_prefs2 &= ~(PREF_CHORD_ADD >> 16);
    mode_gesture();
    bad += check("step live: same gesture chooses LIVE REC without restarting", song.playing && song.rec == 1 &&
        !step_record_mask && rec_on(t) && !transport_req && msg_is("LIVE REC"));
    cursor_set(12);
    key_down(9); frame(); key_up(9); frame();
    bad += check("step live: LIVE REC leaves cursor untouched and records at playhead", ui.cursor == 12 &&
        !t->step[12].n && t->step[t->seq_idx].n);
    press(B_REC);
    bad += check("step live: normal REC disarms", !song.rec && !step_record_mask && song.playing);
    mode_gesture();
    key_down(7); frame(); mode_gesture();
    bad += check("step live: mode change refuses held keys", step_record_mask == 1 && msg_is("RELEASE KEYS"));
    key_up(7); frame();
    frames(400);
    press(B_REC); press(B_REC);
    bad += check("step live: REC double-tap lock restores STEP mode and arming", ui.lock == LAYER_REC && song.rec == 1 && step_record_mask == 1 && !rec_on(t));
    press(B_REC); frames(400);
    track_select(1); cursor_set(4);
    key_down(7); frame(); key_up(7); frame();
    bad += check("step live: another unarmed track does not receive cursor notes", !trk[1].step[4].n && step_record_mask == 1);
    track_select(0);
    song.rec |= 2;
    bad += check("step live: another armed track still records live", !rec_on(t) && rec_on(&trk[1]));
    song.rec &= (uint8_t)~2u;
    chain.running = 1; mode_gesture();
    bad += check("step live: active SONG refuses mode changes", step_record_mask == 1 && msg_is("STOP SONG FIRST"));
    chain.running = 0;
    cin_bars = 1;
    seq_stop();
    press(B_PLAY); events_block(64);
    bad += check("step live: STEP-only arming skips live-record count-in", song.playing && !seq_counting() && step_record_mask == 1);
    cin_bars = 0;
    seq_stop();
    key_down(7); frame(); key_up(7); frame();
    bad += check("step live: STEP REC continues when transport stopped", t->step[ui.cursor ? ui.cursor-1 : 15].n && !song.playing);
    seq_start();
    bad += check("step live: PLAY retains explicit STEP REC", step_record_mask == 1 && !rec_on(t));
    bad += check("step live: save captures cursor edits without stopping", project_save(0) == 0 && song.playing && project_ram_mask() == 1);
    bad += check("step live: RAM-saved pattern can queue", project_pattern_queue(0) == 0);
    t->seq_idx = t->p[P_SLEN]-1;
    t->seq_pos = step_samples(t,div_samples(t->p[P_SDIV]),t->seq_idx)-64;
    project_pattern_tick(64);
    bad += check("step live: pattern launch disarms both recording paths", !song.rec && !step_record_mask && song.playing);
    puts(bad ? "STEP PLAY FAILED" : "step play test passed");
    return bad != 0;
}
