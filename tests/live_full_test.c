/* SPDX-License-Identifier: GPL-3.0-only */
/* Full live scene loading: real snapshots, UI, engines and external clock. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void full_boundary(void)
{
    trk[0].seq_idx = trk[0].p[P_SLEN] - 1u;
    trk[0].seq_pos = step_samples(&trk[0], div_samples(trk[0].p[P_SDIV]), trk[0].seq_idx) - CTL;
    project_pattern_tick(CTL);
}
static void full_pattern(uint32_t note)
{
    for (uint32_t k = 0; k < NTRK; k++) {
        memset(trk[k].step, 0, sizeof trk[k].step);
        trk[k].p[P_SLEN] = 8;
        trk[k].step[0] = (step_t){.note = {note + k}, .n = 1, .time = ST_NOTE, .vel = 96};
    }
}
int main(void)
{
    int bad = 0;
    uint8_t patch[FP_SIZE + 1u];
    int16_t expect[NTRK][P_COUNT];
    dx_config_t kit;
    ui_power_on();
    bad += check("FULL: PATTERN remains default; AUDIO setting names", menu_get(MI_LIVELOAD) == 0 &&
        !MI_PROJECT(MI_LIVELOAD) && MI_TAB[MI_LIVELOAD] == MTAB_AUDIO &&
        str_eq(menu_vname(MI_LIVELOAD, 0), "PATTERN") && str_eq(menu_vname(MI_LIVELOAD, 1), "FULL"));
    menu_put(MI_LIVELOAD, 1);
    { persist_t p; settings_export(&p); ui_prefs2 = 0; settings_import(&p, sizeof p);
      bad += check("FULL: preference survives settings round-trip", menu_get(MI_LIVELOAD) == 1); }
    set_engine_of(&trk[0], ENGI_FM6);
    fm6_unpack(FM6_INIT, patch); patch[FP_FB] = 5; patch[FP_NAME] = 'Z';
    fm6_set_patch(0, patch); fm6_adopt(0);
    set_engine_of(&trk[1], ENGI_DRUM); trk[1].p[P_E0] = DX_KIT_ID;
    dx_set(1, DX_SOUND_FIRST, 33); dx_set(1, DX_FIRST + DX_IMAGE, 91);
    kit = *dx_of(1);
    for (uint32_t k=0;k<NTRK;k++) lfo2_set(k,L2_FIRST,17+k);
    full_pattern(60);
    trk[0].p[P_REV] = 81; trk[0].p[P_ITYPE] = 2; trk[0].p[P_IMIX] = 70;
    trk[2].p[P_MORPH] = 44;
    song.g[G_BPM] = 90; song.g[G_CLOCK] = 0; song.g[G_ROUTE] = 1; song.mch[0] = 7;
    song.g[G_DTIME] = 4; song.g[G_DFDBK] = 75; song.g[G_RTYPE] = 1; song.g[G_RSIZE] = 42;
    motion.on = 1; motion.count = 1; motion.event[0] = (motion_event_t){0, P_PAN | MOTION_LOCK, 50};
    motion_fx_set_track(2);
    for (uint32_t k = 0; k < NTRK; k++) memcpy(expect[k], trk[k].p, sizeof expect[k]);
    seq_start();
    bad += check("FULL: running SAVE B captures sounds and FX in RAM", project_save(1) == 0 && (project_ram_mask() & 2u));
    seq_stop();
    set_engine_of(&trk[0], 0u); set_engine_of(&trk[1], 2u);
    for (uint32_t k=0;k<NTRK;k++) lfo2_set(k,L2_FIRST,3);
    full_pattern(40); memset(&motion, 0, sizeof motion);
    song.g[G_BPM] = 137; song.g[G_SWING] = 17; song.g[G_CLOCK] = 2; song.g[G_ROUTE] = 5; song.mch[0] = 3;
    song.g[G_DTIME] = 0; song.g[G_DFDBK] = 20; song.g[G_RTYPE] = 0; song.g[G_RSIZE] = 10;
    seq_start();
    bad += check("FULL: accepts different engines without applying early", project_pattern_queue(1) == 0 &&
        chain.live_full && trk[0].eng_req == 0u && trk[0].step[0].note[0] == 40 && song.g[G_RSIZE] == 10);
    /* A later SAVE reuses project scratch and replaces B; the queued snapshot must stay frozen. */
    bad += check("FULL: another RAM save cannot overwrite staged sound", project_save(1) == 0);
    menu_put(MI_LIVELOAD, 0); /* mode belongs to the queued request, not to launch time */
    midi_clock.pos = 1234; midi_clock.rendered = 1200; midi_clock.have_pulse = 1;
    uint8_t clock_before[sizeof midi_clock]; memcpy(clock_before, &midi_clock, sizeof clock_before);
    full_boundary();
    for (uint32_t k=0;k<NTRK;k++) bad += check("FULL: the queued LFO2 settings restore at the boundary", lfo2_get(k,L2_FIRST)==(int32_t)(17+k));
    int ok = song.playing && !chain.live_pending && chain.live_applied && trk[0].eng_req == ENGI_FM6 &&
        trk[1].eng_req == ENGI_DRUM && trk[0].step[0].note[0] == 60 && trk[3].step[0].note[0] == 63;
    for (uint32_t k = 0; k < NTRK; k++) ok &= !memcmp(trk[k].p, expect[k], sizeof expect[k]);
    bad += check("FULL: frozen RAM snapshot launches all parameters and engines", ok);
    bad += check("FULL: saved FM6 patch and DRUM-X kit, no factory reload", !memcmp(fm6_patch[0], patch, FP_SIZE) &&
        !memcmp(dx_of(1), &kit, sizeof kit) && trk[0].p[P_E7] == FM6_OWN);
    fm6_poll();
    bad += check("FULL: FM6 main-loop poll keeps loaded OWN voice", !memcmp(fm6_patch[0], patch, FP_SIZE));
    bad += check("FULL: FX buses restored; master clock and MIDI setup preserved", song.g[G_DTIME] == 4 &&
        song.g[G_DFDBK] == 75 && song.g[G_RTYPE] == 1 && song.g[G_RSIZE] == 42 &&
        song.g[G_BPM] == 137 && song.g[G_SWING] == 17 && song.g[G_CLOCK] == 2 && song.g[G_ROUTE] == 5 && song.mch[0] == 3 &&
        !memcmp(&midi_clock, clock_before, sizeof midi_clock));
    bad += check("FULL: FX owner and automation restored", motion_fx_track() == 2 && motion.count == 1);
    for (uint32_t k = 0; k < NTRK; k++) seq_tick(&trk[k], CTL);
    chain.live_applied = 0;
    bad += check("FULL: first-step parameter lock uses new scene base", trk[0].p[P_PAN] == 50);
    motion_end();
    bad += check("FULL: ending motion restores destination, not source base", trk[0].p[P_PAN] == expect[0][P_PAN]);
    /* Same-engine voices fade; the first new note is queued and starts after that fade. */
    song.g[G_CLOCK] = 0; seq_stop(); seq_start();
    for (uint32_t k = 0; k < NTRK; k++) engine_block(&trk[k]);
    trk_note_on(&trk[0], 67, 100);
    bad += check("FULL: save a scene while old voices sound", project_save(0) == 0);
    menu_put(MI_LIVELOAD, 1);
    bad += check("FULL: queue same-engine scene", project_pattern_queue(0) == 0);
    fm6_writing = 1; full_boundary();
    bad += check("FULL: interrupted patch write defers instead of tearing", chain.live_pending == 1);
    fm6_writing = 0; full_boundary();
    bad += check("FULL: sounding voice enters bounded engine fade", trk[0].xf_on && trk[0].xf == XF_BLOCKS);
    trk_note_on(&trk[0], 72, 100);
    int32_t out[CTL];
    for (uint32_t i = 0; i <= XF_BLOCKS; i++) {
        engine_block(&trk[0]); memset(out, 0, sizeof out); track_render(&trk[0], out, CTL);
    }
    ok = 0;
    for (uint32_t i = 0; i < NVOICE; i++) ok |= trk[0].v[i].active && trk[0].v[i].note == 72;
    bad += check("FULL: fade completes and queued destination note starts", !trk[0].xf_on && !trk[0].xp_n && ok);
    project_pattern_queue(0); seq_stop();
    bad += check("FULL: STOP cancels prepared FULL request", !chain.live_pending && !song.playing);
    /* Exercise the actual UART/external-clock path, not only the boundary helper. */
    memset(&midi_clock, 0, sizeof midi_clock); memset(&um, 0, sizeof um); mi_r = mi_w = 0;
    song.g[G_CLOCK] = 2; fm1_ms = 0; events_block(CTL);
    fm1_ms = 2; um_byte(0xFA); events_block(CTL);
    bad += check("FULL: queue under running TRS clock", song.playing && project_pattern_queue(0) == 0);
    uint32_t pulses;
    for (pulses = 0; pulses < 60u && chain.live_pending; pulses++) {
        fm1_ms = 2u + pulses * 125u / 6u; um_byte(0xF8); events_block(CTL);
    }
    bad += check("FULL: TRS launch does not restart or replace external clock", !chain.live_pending && song.playing &&
        song.g[G_CLOCK] == 2 && midi_clock.have_pulse && midi_clock.pos > 0 && midi_clock.rendered > 0);
    um_byte(0xFC); events_block(CTL);
    bad += check("FULL: TRS STOP still works", !song.playing);
    /* Every visible engine pair with sounding voices and real audio blocks:
     * catches union-state ownership, fade completion and wrong-engine values. */
    ui_power_on(); menu_put(MI_LIVELOAD, 1);
    ok = 1;
    for (uint32_t from = 0; from < NENGINES; from++) {
        if (!eng_ok(from)) continue;
        for (uint32_t to = 0; to < NENGINES; to++) {
            if (!eng_ok(to)) continue;
            seq_stop();
            set_engine_of(&trk[0], to); full_pattern(60);
            ok &= project_save(0) == 0;
            set_engine_of(&trk[0], from);
            int32_t audio[CTL * 2];
            for (uint32_t b = 0; b < XF_BLOCKS + 2u; b++) mix_block(audio, CTL);
            trk_note_on(&trk[0], 64, 100);
            mix_block(audio, CTL);
            seq_start();
            ok &= project_pattern_queue(0) == 0;
            full_boundary();
            for (uint32_t b = 0; b < XF_BLOCKS + 4u; b++) mix_block(audio, CTL);
            ok &= trk[0].eng_req == to && trk[0].engine == to && !trk[0].xf_on && !trk[0].xp_n;
        }
    }
    bad += check("FULL: every engine pair renders and completes its sound transition", ok);
    seq_stop();
    return bad ? 1 : 0;
}

