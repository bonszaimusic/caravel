/* SPDX-License-Identifier: GPL-3.0-only */
/* Real panel, sequencer, storage and editor paths for master FX automation. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void fx_init(void)
{
    ui_power_on();
    song.rec = 1;
    trk[0].p[P_SLEN] = 4;
    trk[0].p[P_SDIV] = 2;
    seq_start();
    seq_tick(&trk[0], CTL);
}
static void at(track_t *t, uint32_t step, uint32_t phase)
{
    t->seq_idx = (uint16_t)step;
    t->seq_pos = (step_samples(t, div_samples(t->p[P_SDIV]), step) * phase + 255u) / 256u;
}
static int capture(void)
{
    int bad = 0;
    fx_init();
    track_t *t = &trk[0];
    at(t, 1, 48); perf_key_press(white(12), 1);
    at(t, 1, 76); perf_key_press(white(12), 0);
    bad += check("FX: short press and release in one step are two fine-timed events", motion.count == 2 &&
        motion.event[0].place == 1 && MOTION_ID(&motion.event[0]) == MOTION_FX_FIRST + PF_RTHR &&
        (uint8_t)motion.event[0].value == 48 && (uint8_t)motion.event[1].value == 76 &&
        (motion.event[1].param & MOTION_LOCK) && motion_valid(&motion));
    bad += check("FX: release is not a parameter lock or sound parameter", !motion_lock_count(t) &&
        !motion_lock_steps(0) && !motion_param(MOTION_FX_FIRST));
    at(t, 1, 48); perf_key_press(white(12), 1);
    at(t, 1, 76); perf_key_press(white(12), 0);
    bad += check("FX: identical overdub reuses both edges", motion.count == 2);
    at(t, 2, 32); perf_press(PF_FRZ, 1);
    song.sel = 1; song.rec = 0;
    at(t, 2, 200); perf_press(PF_FRZ, 0);
    bad += check("FX: release follows its owner after changing selected track or disarming", motion_count(t) == 4 &&
        !motion_count(&trk[1]) && !motion_fx_recording);
    motion_store_t before = motion;
    perf_press(PF_LPF, 1); perf_press(PF_LPF, 0);
    song.rec = 2; step_record_mask = 2;
    perf_press(PF_HPF, 1); perf_press(PF_HPF, 0);
    perf_press(PF_M1, 1); perf_press(PF_M1, 0);
    bad += check("FX: unarmed, STEP REC and black-key mutes remain live only", !memcmp(&before, &motion, sizeof motion));
    seq_stop();
    fx_init();
    uint32_t key = white(4), mo = mo_w;
    fm1_in.buttons = 1u << panel.btn[B_FX];
    fm1_in.notes = 1u << key; events_block(CTL);
    uint32_t pos = trk[0].seq_pos;
    for (uint32_t n = 0; n < 100; n++) events_block(CTL);
    fm1_in.notes = 0; events_block(CTL);
    bad += check("FX: actual FX+key gesture records no note/MIDI and both edges", motion.count == 2 &&
        !trk[0].step[0].n && mo_w == mo && trk[0].seq_pos > pos && !perf_held);
    fm1_in.buttons = 0; seq_stop();
    fx_init(); at(TSEL, 0, 40); perf_press(PF_LPF, 1); perf_press(PF_LPF, 0);
    bad += check("FX: sub-phase tap retains distinct ON/OFF times", motion.count == 2 &&
        (uint8_t)motion.event[1].value == (uint8_t)motion.event[0].value + 1);
    seq_stop();
    return bad;
}
static int latch_and_stop(void)
{
    int bad = 0;
    fx_init(); perf_latch_on = 1;
    at(TSEL, 0, 20); perf_press(PF_ETHR, 1); perf_press(PF_ETHR, 0);
    bad += check("FX: LATCH key release does not record a false OFF", motion.count == 1 && perf_latched == PF_BIT(PF_ETHR));
    at(TSEL, 0, 150); perf_press(PF_ETHR, 1);
    bad += check("FX: second LATCH press records OFF", motion.count == 2 && !perf_latched && !motion_fx_recording);
    at(TSEL, 1, 20); perf_press(PF_OUP, 1);
    at(TSEL, 1, 180); seq_stop();
    bad += check("FX: STOP closes the captured hold and releases sequencer FX", motion.count == 4 &&
        !motion_fx_recording && !perf_motion[0] && (motion.event[3].param & MOTION_LOCK));
    fx_init(); at(TSEL, 0, 10); perf_press(PF_LPF, 1);
    at(TSEL, 0, 90); perf_latched = 0; motion_fx_sync();
    bad += check("FX: ALL OFF/menu latch clearing captures the release", motion.count == 2 && !motion_fx_recording);
    perf_latch_on = 0; seq_stop();
    fx_init(); at(TSEL, 0, 30); perf_press(PF_HPF, 1);
    motion_set_enabled(TSEL, 0); perf_press(PF_HPF, 0);
    bad += check("FX: bypass while recording closes hold and stays disabled on key-up", motion.count == 2 &&
        !motion_enabled(TSEL) && !motion_fx_recording);
    seq_stop();
    return bad;
}
static int playback(void)
{
    int bad = 0;
    fx_init(); song.rec = 0;
    motion_fx_put(TSEL, 0, PF_LPF, 32, 1); motion_fx_put(TSEL, 0, PF_LPF, 64, 0);
    motion_fx_put(TSEL, 0, PF_ETHR, 200, 1); motion_fx_put(TSEL, 1, PF_ETHR, 30, 0);
    seq_stop(); seq_start(); seq_tick(TSEL, CTL);
    bad += check("FX: before short trigger remains off", !perf_motion[0]);
    at(TSEL, 0, 32); seq_tick(TSEL, 0); perf_begin(CTL);
    bad += check("FX: trigger plays without FX layer or live key", perf_motion[0] == PF_BIT(PF_LPF) &&
        (perf_act & PF_BIT(PF_LPF)) && !perf_held && !perf_latched);
    perf_press(PF_LPF, 1);
    at(TSEL, 0, 64); seq_tick(TSEL, 0); perf_begin(CTL);
    bad += check("FX: recorded OFF cannot release a manual hold", !perf_motion[0] && (perf_act & PF_BIT(PF_LPF)));
    perf_press(PF_LPF, 0); perf_begin(CTL);
    at(TSEL, 0, 200); seq_tick(TSEL, 0);
    bad += check("FX: hold crosses step boundary", perf_motion[0] == PF_BIT(PF_ETHR));
    at(TSEL, 1, 0); motion_fx_step(TSEL, &motion);
    at(TSEL, 1, 30); seq_tick(TSEL, 0);
    bad += check("FX: next step OFF closes cross-step hold", !perf_motion[0]);
    motion_fx_put(TSEL, 3, PF_FRZ, 120, 1); motion_fx_put(TSEL, 0, PF_FRZ, 80, 0);
    at(TSEL, 0, 0); motion_fx_step(TSEL, &motion);
    bad += check("FX: loop restart chases a cross-loop hold", perf_motion[0] == PF_BIT(PF_FRZ));
    at(TSEL, 0, 80); seq_tick(TSEL, 0);
    bad += check("FX: cross-loop hold releases at recorded phase", !perf_motion[0]);
    at(TSEL, 0, 0); motion_fx_step(TSEL, &motion); motion_set_enabled(TSEL, 0);
    bad += check("FX: automation bypass releases replay without deleting it", !perf_motion[0] && motion.count == 6);
    motion_set_enabled(TSEL, 1); at(TSEL, 0, 0); motion_fx_step(TSEL, &motion); motion_clear(TSEL);
    bad += check("FX: CLEAR removes edges and replay", !perf_motion[0] && !motion.count && !motion.rsv[0]);
    seq_stop();
    fx_init(); song.rec = 0;
    motion_fx_put(TSEL, 0, PF_LPF, 250, 1); motion_fx_put(TSEL, 1, PF_LPF, 20, 0);
    TSEL->p[P_SSWING] = 30; TSEL->p[P_SQNT] = 0;
    step_set_chance(&TSEL->step[0], 0); step_set_nudge(&TSEL->step[0], 5);
    seq_stop(); seq_start(); seq_tick(TSEL, CTL);
    uint32_t length = step_samples(TSEL, div_samples(TSEL->p[P_SDIV]), 0);
    seq_tick(TSEL, length + 1);
    bad += check("FX: boundary overshoot applies old tail despite chance/nudge/swing", TSEL->seq_idx == 1 &&
        perf_motion[0] == PF_BIT(PF_LPF));
    at(TSEL, 1, 20); seq_tick(TSEL, 0);
    bad += check("FX: swung next-step release uses that step's duration", !perf_motion[0]);
    seq_stop();
    return bad;
}
static int storage_and_capacity(void)
{
    int bad = 0;
    project_t q, restored; project_store_t disk;
    fx_init(); at(TSEL, 2, 40); perf_press(PF_PHS, 1);
    at(TSEL, 2, 170); project_capture(&q);
    bad += check("FX: live SAVE closes only snapshot, leaving live capture active", q.motion.count == 2 &&
        motion.count == 1 && motion_fx_recording && (q.motion.event[1].param & MOTION_LOCK));
    bad += check("FX: fine timing survives fixed 3840-byte project and import", proj_pack(&disk, &q) &&
        proj_import(&restored, &disk, sizeof disk) && !memcmp(&q.motion, &restored.motion, sizeof q.motion));
    seq_stop();
    project_save(0); motion_clear(TSEL); project_load(0);
    bad += check("FX: actual project save/load restores edge pair", motion.count == 2 && motion_valid(&motion));
    motion_store_t snap; motion_snapshot_track(TSEL, &snap); motion_clear(TSEL); motion_replace_track(TSEL, &snap);
    bad += check("FX: track snapshot/undo replacement retains schema", !memcmp(&motion, &snap, sizeof motion));
    bad += check("FX: sound-engine change keeps master FX", !motion_sound_loaded(TSEL, 0, 2) && motion.count == 2);
    motion_clear_locks(TSEL, NSTEP);
    bad += check("FX: clearing locks never deletes releases", motion.count == 2);
    fx_init();
    for (uint32_t i = 0; i < MOTION_MAX - 2; i++)
        motion_set_event(TSEL, i % NSTEP, i < NSTEP ? P_REV : P_DLY, i % NSTEP);
    at(TSEL, 0, 40); perf_press(PF_LPF, 1);
    bad += check("FX: capture reserves the last slot for release", motion.count == MOTION_MAX - 1 &&
        motion_set_event(&trk[1], 0, P_REV, 5) == 2 && motion_fx_recording);
    at(TSEL, 0, 160); perf_press(PF_LPF, 0);
    bad += check("FX: reserved release succeeds at full capacity", motion.count == MOTION_MAX && !motion_fx_recording && motion_valid(&motion));
    motion_store_t before = motion;
    perf_press(PF_HPF, 1); perf_press(PF_HPF, 0);
    bad += check("FX: full pool rejects new pair atomically but live effect works", !memcmp(&motion, &before, sizeof motion) && !motion_fx_recording);
    seq_stop();
    before.rsv[1] = 0;
    bad += check("FX: unknown event schema is rejected", !motion_valid(&before));
    fx_init(); song.rec = 0;
    motion_fx_put(TSEL, 0, PF_FRZ, 40, 1); motion_fx_put(TSEL, 0, PF_FRZ, 80, 0);
    before = motion; motion_event_t tmp = before.event[0]; before.event[0] = before.event[1]; before.event[1] = tmp;
    bad += check("FX: malformed reverse time ordering is rejected", !motion_valid(&before));
    seq_stop();
    return bad;
}
static int ui_and_lanes(void)
{
    int bad = 0;
    fx_init(); song.rec = 0;
    for (uint32_t e = 0; e < PF_NFX; e++) {
        motion_fx_put(TSEL, e / 4, e, 30, 1); motion_fx_put(TSEL, e / 4, e, 200, 0);
    }
    seq_stop(); go_page(GR_EVENTS);
    uint16_t rows[EV_ROWS]; uint32_t n = ev_list(rows);
    bad += check("FX: automation list includes every special edge", n == 3 + 2 * PF_NFX && !motion_lock_count(TSEL));
    for (uint32_t i = 0; i < n; i++) { ui.ev_row = i; ui.force = 1; ui_draw(); }
    ui.ev_row = 2; uint32_t count = motion.count; ev_knob(3, 5); ev_oct();
    bad += check("FX: fine-time rows cannot become parameter locks", motion.count == count && motion_valid(&motion));
    uint32_t c = ev_cur(rows, 0); uint32_t fx = MOTION_ID(&motion.event[c & 0xFFu]) - MOTION_FX_FIRST;
    ev_delete();
    bad += check("FX: EDIT removes the selected pair, leaving other effects", motion.count == count - 2 && motion_valid(&motion));
    motion_fx_put(&trk[1], 0, PF_FRZ, 0, 1); motion_fx_put(&trk[1], 1, PF_FRZ, 0, 0);
    motion_clear(TSEL);
    bad += check("FX: track clear keeps other track's FX", motion.count == 2 && motion.rsv[0] == 2);
    motion_fx_set_track(1);
    seq_start(); seq_tick(&trk[1], CTL);
    bad += check("FX: playback owns the source track independently of selection", perf_motion[1] == PF_BIT(PF_FRZ) && !perf_motion[0]);
    chain.running = 1; chain.lane[1] = CHAIN_SILENT;
    motion_fx_step(&trk[1], chain_motion(&trk[1]));
    bad += check("FX: silent SONG lane releases its automation", !perf_motion[1]);
    chain.running = 0; seq_stop();
    /* Saved live part launch cancels the previous replay and begins new FX. */
    fx_init(); song.rec = 0;
    motion_fx_put(TSEL, 0, PF_LPF, 0, 1); motion_fx_put(TSEL, 1, PF_LPF, 0, 0);
    seq_stop(); project_save(0); motion_clear(TSEL);
    motion_fx_put(TSEL, 0, PF_HPF, 0, 1); motion_fx_put(TSEL, 1, PF_HPF, 0, 0);
    seq_start(); seq_tick(TSEL, CTL);
    project_pattern_queue(0); at(TSEL, 3, 255); project_pattern_tick(CTL * 2);
    seq_tick(TSEL, CTL * 2);
    bad += check("FX: live part launch releases old FX and uses new part's timeline", chain.live_applied &&
        (perf_motion[0] & PF_BIT(PF_LPF)) && !(perf_motion[0] & PF_BIT(PF_HPF)));
    seq_stop();
    /* External TRS transport drives the same timeline, including Stop. */
    fx_init(); seq_stop(); motion_clear(TSEL); song.rec = 0;
    motion_fx_put(TSEL, 0, PF_LPF, 0, 1); motion_fx_put(TSEL, 1, PF_LPF, 0, 0);
    memset(&midi_clock, 0, sizeof midi_clock); memset(&um, 0, sizeof um); mi_r = mi_w = 0;
    song.g[G_CLOCK] = 2; fm1_ms = 0; events_block(CTL);
    fm1_ms = 2; um_byte(0xFA); events_block(CTL);
    bad += check("FX: external TRS Start plays the first edge", song.playing && perf_motion[0] == PF_BIT(PF_LPF));
    for (uint32_t pulse = 0; pulse < 8; pulse++) { fm1_ms = 2 + pulse * 125 / 6; um_byte(0xF8); events_block(CTL); }
    bad += check("FX: external clock reaches release", !perf_motion[0] && TSEL->seq_idx == 1);
    um_byte(0xFC); events_block(CTL);
    bad += check("FX: external Stop releases timeline", !song.playing && !perf_motion[0]);
    (void)fx;
    return bad;
}
static int repeated_releases(void)
{
    int bad = 0;
    fx_init();
    at(TSEL, 0, 20); perf_press(PF_R16, 1);
    at(TSEL, 0, 100); perf_press(PF_R16, 0);
    at(TSEL, 0, 40); perf_press(PF_R16, 1);
    at(TSEL, 0, 140); perf_press(PF_R16, 0);
    bad += check("FX: overlapping takes retain both priority-setting ONs but only the first OFF", motion.count == 3 &&
        !(motion.event[0].param & MOTION_LOCK) && !(motion.event[1].param & MOTION_LOCK) &&
        (motion.event[2].param & MOTION_LOCK) && (uint8_t)motion.event[2].value == 100 && motion_valid(&motion));
    at(TSEL, 0, 120); perf_press(PF_R16, 1);
    at(TSEL, 0, 160); perf_press(PF_R16, 0);
    bad += check("FX: a new ON between releases keeps its necessary OFF", motion.count == 5 && motion_valid(&motion));
    seq_stop();
    return bad;
}
static void fx_trace(const motion_store_t *m, uint32_t *out)
{
    memset(perf_motion, 0, sizeof perf_motion);
    memset(perf_ord, 0, sizeof perf_ord);
    perf_seq = 0;
    for (uint32_t s = 0; s < 4; s++)
        for (uint32_t phase = 0; phase < 256; phase++)
            for (uint32_t k = 0; k < NTRK; k++) {
                trk[k].seq_idx = (uint16_t)s;
                if (!phase) motion_fx_step(&trk[k], m);
                else motion_fx_run(&trk[k], m, phase, phase);
                uint32_t master = 0;
                for (uint32_t t = 0; t < NTRK; t++) master |= perf_motion[t];
                *out++ = perf_motion[k] | perf_pick(master) << 16;
            }
}
static int cleanup_preserves_playback(void)
{
    int bad = 0, equivalent = 1;
    static uint32_t before_trace[4 * 256 * NTRK], after_trace[4 * 256 * NTRK];
    uint32_t seed = 12345;
    for (uint32_t run = 0; run < 32; run++) {
        fx_init(); song.rec = 0;
        for (uint32_t k = 0; k < NTRK; k++) trk[k].p[P_SLEN] = 4;
        motion_set_event(&trk[2], 0, P_REV, 20);
        motion_put(&trk[2], 1, P_DLY, 40, MOTION_LOCK);
        for (uint32_t i = 0; i < 100; i++) {
            seed = seed * 1664525u + 1013904223u;
            motion_fx_put(&trk[(seed >> 8) % NTRK], (seed >> 12) % 4u,
                (seed >> 16) % PF_NFX, (seed >> 20) % 256u, (seed >> 29) & 1u);
        }
        motion_store_t before = motion;
        fx_trace(&before, before_trace);
        motion_fx_compact_off(&motion, (1u << NTRK) - 1u);
        fx_trace(&motion, after_trace);
        equivalent &= !memcmp(before_trace, after_trace, sizeof before_trace) && motion_valid(&motion) &&
            motion_lock_count(&trk[2]) == 1 && motion_find(&trk[2], 0, P_REV) >= 0;
        seq_stop();
    }
    bad += check("FX: cleanup preserves all 256 phases, loop chase, master priority and parameter locks", equivalent);
    fx_init(); song.rec = 0;
    motion_fx_put(TSEL, 3, PF_R16, 40, 1);
    motion_fx_put(TSEL, 0, PF_R16, 30, 0);
    motion_fx_put(TSEL, 0, PF_R16, 80, 0);
    motion_fx_put(&trk[1], 0, PF_R16, 80, 0);
    motion_store_t before = motion;
    motion_fx_compact_off(&motion, 1u);
    bad += check("FX: first OFF closes a cross-loop hold and another track's OFF is retained", motion.count == 3 &&
        motion_count(&trk[1]) == 1 && (uint8_t)motion.event[0].value == 30 && motion_valid(&motion));
    /* Old projects are tidied in RAM on load, without altering the saved slot. */
    motion = before; project_t p; project_capture(&p); p.motion = before;
    proj_pack(&proj_slot[0], &p);
    project_store_t disk = proj_slot[0]; project_load(0);
    bad += check("FX: loading an old part removes redundant OFFs without rewriting its saved bytes", motion.count == 3 &&
        !memcmp(&disk, &proj_slot[0], sizeof disk));
    seq_stop();
    return bad;
}
static int project_controls(void)
{
    int bad = 0, ok;
    static project_t boot, reset, q;
    static project_store_t saved;
    ui_power_on(); fm6_init();
    apply_preset_to(&trk[1], TRK_DEF[1][1]); project_capture(&boot);
    bad += check("AUDIO: FX TRACK defaults T1 and is a project row", menu_get(MI_FXTRACK) == 0 &&
        menu_n(MI_FXTRACK) == 4 && MI_PROJECT(MI_FXTRACK) && MI_TAB[MI_FXTRACK] == MTAB_AUDIO);
    menu_put(MI_FXTRACK, 2);
    song.sel = 1; song.rec = 4; trk[2].p[P_SLEN] = 8; trk[2].p[P_SDIV] = 3;
    seq_start(); seq_tick(&trk[2], CTL); at(&trk[2], 3, 80);
    perf_press(PF_LPF, 1); at(&trk[2], 3, 190); perf_press(PF_LPF, 0);
    bad += check("CONTROL: selected sound T2 records FX on armed T3 at T3 timing", motion.count == 2 &&
        motion_count(&trk[2]) == 2 && !motion_count(&trk[1]) && motion.event[0].place == (2u << 6 | 3u) &&
        (uint8_t)motion.event[0].value == 80);
    song.rec = 2; perf_press(PF_HPF, 1); perf_press(PF_HPF, 0);
    bad += check("CONTROL: arming sound track alone does not arm global FX track", motion.count == 2);
    song.rec = 4; at(&trk[2], 4, 20); perf_press(PF_FRZ, 1);
    perf_motion[2] = PF_BIT(PF_FRZ); menu_put(MI_FXTRACK, 3);
    bad += check("CONTROL: changing FX owner closes previous hold and replay", !motion_fx_recording &&
        !perf_motion[2] && motion_count(&trk[2]) == 4 && motion_fx_track() == 3);
    perf_press(PF_FRZ, 0); seq_stop();
    project_capture(&q); project_store_t wire;
    ok = proj_pack(&wire, &q) && proj_import(&reset, &wire, sizeof wire);
    bad += check("CONTROL: empty T4 owner survives fixed-size project serialization", ok &&
        ((reset.motion.rsv[0] >> 4) & 3u) == 3 && sizeof wire == 3840);
    project_save(0); saved = proj_slot[0]; menu_put(MI_FXTRACK, 0); project_load(0);
    bad += check("CONTROL: PROJECT LOAD restores owner T4", motion_fx_track() == 3);
    motion_clear(&trk[2]);
    bad += check("CONTROL: clearing automation preserves project FX owner", motion_fx_track() == 3);
    motion_fx_put(&trk[0], 0, PF_LPF, 0, 1); motion_fx_put(&trk[3], 0, PF_HPF, 0, 1);
    at(&trk[0], 0, 0); at(&trk[3], 0, 0);
    motion_fx_step(&trk[0], &motion); motion_fx_step(&trk[3], &motion);
    bad += check("CONTROL: only configured track replays global special FX", !perf_motion[0] && perf_motion[3] == PF_BIT(PF_HPF));
    /* Part launch adopts the saved part's owner at the same boundary as its events. */
    motion_clear(&trk[0]); motion_clear(&trk[3]); menu_put(MI_FXTRACK, 2);
    motion_fx_put(&trk[2], 0, PF_LPF, 0, 1); project_save(1);
    menu_put(MI_FXTRACK, 0); seq_start();
    ok = project_pattern_queue(1) == 0;
    at(&trk[0], trk[0].p[P_SLEN] - 1, 255); project_pattern_tick(CTL * 2);
    bad += check("CONTROL: live part boundary adopts the saved FX track", ok && motion_fx_track() == 2);
    seq_stop();
    /* INIT is reachable with KNOB2, supports cancel, and preserves saved banks. */
    uint8_t prefs = ui_prefs, prefs2 = ui_prefs2, recprefs = ui_rec_prefs;
    uint32_t banks = fm6_bank_used;
    ml_learn(22, 0, P_LEVEL); uint32_t learn = ml_get(0);
    project_ram_set(5); ((uint8_t *)&up_bank[0])[0] = 57;
    go_page(GR_SLOTS); turn(EN_K2, 1); press(B_OCTUP);
    bad += check("PROJECT: KNOB2 INIT opens explicit confirmation", ui.confirm == CF_INIT_PROJECT && act_cols() == 14u);
    press(B_OCTDN);
    bad += check("PROJECT: cancel INIT leaves working project and FX track", motion_fx_track() == 2 && motion.count == 1);
    song.rec = 15; step_record_mask = 4; seq_start();
    trk[0].step[7].n = 1; trk[0].step[7].note[0] = 64;
    song.g[G_TUNE] = 35; song.octave = 2;
    perf_latched = PF_BIT(PF_FRZ); perf_motion[2] = PF_BIT(PF_LPF);
    uint32_t key = white(4);
    usb.config = 1; mo_r = mo_w;
    kb_note[key] = 64; kb_trk[key] = 0; key_on(key, &trk[0]);
    uint32_t midi_before = mo_w;
    turn(EN_K2, 1); press(B_OCTUP); press(B_OCTUP);
    project_capture(&reset);
    bad += check("PROJECT: INIT sends held panel note-offs before clearing keys", !kb_chn[key] && mo_w > midi_before);
    bad += check("PROJECT: confirmed INIT stops transport, LIVE/STEP REC and FX", !song.playing && !song.rec &&
        !step_record_mask && !chain_busy() && !perf_latched && !perf_held && !motion_fx_recording && !motion.count && !perf_motion[2]);
    ok = !memcmp(boot.g, reset.g, sizeof boot.g) && !memcmp(boot.t, reset.t, sizeof boot.t) &&
        !memcmp(boot.fm6, reset.fm6, sizeof boot.fm6) && !memcmp(&boot.chain, &reset.chain, sizeof boot.chain);
    bad += check("PROJECT: INIT matches boot sounds, steps, FM6 patches and globals", ok && !motion_fx_track() &&
        !song.sel && !song.octave && !proj_name[0] && proj_cur == PROJ_NO_SLOT && !undo.trk);
    bad += check("PROJECT: INIT keeps saved A-D, pending saves, USER, bank and settings", !memcmp(&saved, &proj_slot[0], sizeof saved) &&
        project_ram_mask() == 5 && ((uint8_t *)&up_bank[0])[0] == 57 && banks == fm6_bank_used &&
        prefs == ui_prefs && prefs2 == ui_prefs2 && recprefs == ui_rec_prefs && ml_get(0) == learn);
    events_block(CTL);
    bad += check("PROJECT: INIT remains usable for fresh FX recording without reboot", !panic_req && !seq_counting());
    song.rec = 1; seq_start(); seq_tick(&trk[0], CTL);
    perf_press(PF_LPF, 1); perf_press(PF_LPF, 0);
    bad += check("PROJECT: fresh FX press/release records after INIT", motion.count == 2 && motion_count(&trk[0]) == 2);
    seq_stop();
    return bad;
}

int main(void)
{
    int bad = capture() + latch_and_stop() + playback() + storage_and_capacity() + ui_and_lanes() +
        repeated_releases() + cleanup_preserves_playback() + project_controls();
    printf("FX special automation: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
