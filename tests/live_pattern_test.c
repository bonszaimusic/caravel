/* SPDX-License-Identifier: GPL-3.0-only */
/* Live project-pattern launcher: real UI, sequencer and project paths. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void pattern(uint32_t note, uint32_t len)
{
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = len;
        t->p[P_SDIV] = 2;
        t->p[P_SQNT] = 1;
        t->step[0].note[0] = note + k;
        t->step[0].n = 1;
        t->step[0].time = ST_NOTE;
        t->step[0].vel = 96;
    }
}
static void boundary(uint32_t before, uint32_t advance)
{
    trk[0].seq_idx = trk[0].p[P_SLEN] - 1;
    trk[0].seq_pos = step_samples(&trk[0], div_samples(trk[0].p[P_SDIV]), trk[0].seq_idx) - before;
    project_pattern_tick(advance);
}
int main(void)
{
    int bad = 0;
    ui_power_on();
    memset(&motion, 0, sizeof motion);
    pattern(60, 8);
    motion.on = 1;
    motion.count = 2;
    motion.event[0] = (motion_event_t){0, P_PAN, 20};
    motion.event[1] = (motion_event_t){1, P_PAN | MOTION_LOCK, 50};
    bad += check("live: save project A", project_save(0) == 0);
    motion_store_t target = motion;
    pattern(70, 12);
    bad += check("live: save project B", project_save(1) == 0);
    pattern(40, 16);
    memset(&motion, 0, sizeof motion);
    seq_start();
    int16_t bpm = song.g[G_BPM];
    uint8_t engine = trk[0].eng_req;
    int16_t level = trk[0].p[P_LEVEL];
    bad += check("live: refuse empty slot without touching playback", project_pattern_queue(3) == 1 && song.playing);
    bad += check("live: queue A, keep old working pattern", project_pattern_queue(0) == 0 && trk[0].step[0].note[0] == 40);
    seq_tick(&trk[0], 64);
    song.rec = 1;
    boundary(128, 64);
    bad += check("live: wait until end of track 1", chain.live_pending == 1 && trk[0].p[P_SLEN] == 16);
    boundary(64, 64);
    bad += check("live: launch all tracks and automation at exact boundary", song.playing && !chain.live_pending && chain.live_applied &&
        !song.rec && !trk[0].seq_n && trk[0].p[P_SLEN] == 8 && trk[3].step[0].note[0] == 63 && memcmp(&motion, &target, sizeof motion) == 0);
    for (uint32_t k = 0; k < NTRK; k++) seq_tick(&trk[k], 64);
    chain.live_applied = 0;
    bad += check("live: first step applies automation, retains sound/tempo", trk[0].seq_idx == 0 && trk[0].seq_pos == 0 &&
        trk[0].p[P_PAN] == 20 && trk[0].eng_req == engine && trk[0].p[P_LEVEL] == level && song.g[G_BPM] == bpm);
    motion_step(&trk[0], 1, &motion, 1);
    bad += check("live: imported parameter lock executes", trk[0].p[P_PAN] == 50);
    bad += check("live: queue B, last valid request A replaces it", project_pattern_queue(1) == 0 && project_pattern_queue(0) == 0 && chain.live_pending == 1);
    bad += check("live: invalid request retains earlier valid queue", project_pattern_queue(3) == 1 && chain.live_pending == 1);
    project_pattern_queue(1);
    boundary(32, 64);
    for (uint32_t k = 0; k < NTRK; k++) seq_tick(&trk[k], 64);
    chain.live_applied = 0;
    bad += check("live: overshoot retained for every track", trk[0].p[P_SLEN] == 12 && trk[0].seq_pos == 32 && trk[3].seq_pos == 32);
    project_pattern_queue(0);
    seq_stop();
    bad += check("live: stop cancels queued launch and notes", !chain.live_pending && !song.playing && !trk[0].seq_n);
    seq_start();
    project_pattern_queue(0);
    trk[0].eng_req = (engine + 1u) % NENGINES;
    boundary(64, 64);
    bad += check("live: engine change after queue safely refuses", !chain.live_pending && chain.live_notice == 5 && trk[0].p[P_SLEN] == 12);
    trk[0].eng_req = engine;
    chain.running = 1;
    bad += check("live: SONG cannot share live staging buffer", project_pattern_queue(0) == 2);
    chain.running = 0;
    seq_stop();
    project_load(0);
    bad += check("live: stopped project LOAD retains legacy behaviour", !song.playing && trk[0].p[P_SLEN] == 8);
    seq_start();
    go_page(GR_SLOTS);
    song.g[G_SLOT] = 2;
    ui.act = 3; /* Knob 3 LOAD */
    act_do();
    bad += check("live: PROJECT LOAD UI queues while playing", song.playing && chain.live_pending == 2);
    seq_stop();
    trk[0].step[0].note[0] = 66;
    bad += check("live: launched pattern stays editable and saveable", project_save(2) == 0);
    pattern(40, 16);
    project_load(2);
    bad += check("live: saved edits and imported motion round-trip", trk[0].step[0].note[0] == 66 && memcmp(&motion, &target, sizeof motion) == 0);
    /* Real UART clock -> event block -> launch, without restarting the master clock. */
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um);
    mi_r = mi_w = 0;
    song.g[G_CLOCK] = 2;
    fm1_ms = 0;
    events_block(CTL);
    pattern(40, 8);
    fm1_ms = 2; um_byte(0xFA); events_block(CTL);
    bad += check("live: TRS Start begins playback", song.playing && trk[0].seq_idx == 0);
    bad += check("live: queue from running TRS clock", project_pattern_queue(1) == 0);
    for (uint32_t pulse = 0; pulse < 60u && chain.live_pending; pulse++) {
        fm1_ms = 2u + pulse * 125u / 6u;
        um_byte(0xF8); events_block(CTL);
    }
    bad += check("live: UART clock launches B through event block without reset", song.playing &&
        !chain.live_pending && trk[0].p[P_SLEN] == 12 && trk[0].step[0].note[0] == 70 &&
        midi_clock.pos > 0 && midi_clock.rendered > 0 && midi_clock.have_pulse);
    project_pattern_queue(0);
    um_byte(0xFC); events_block(CTL);
    bad += check("live: TRS Stop cancels queued pattern", !song.playing && !chain.live_pending);
    return bad ? 1 : 0;
}
