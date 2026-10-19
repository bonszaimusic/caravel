/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise allocation through real note, ARP, drum, FX, UI and settings paths. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static uint32_t reserved_busy(uint32_t p)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}
static void reserve_setup(uint32_t policy)
{
    ui_power_on();
    for (uint32_t p = 0; p < NPART; p++) {
        host_preset(&trk[p], 0, 0);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_REL] = 127;
    }
    voice_policy = policy;
    events_block(CTL);
}
static void reserve_fill(void)
{
    for (uint32_t p = 0; p < NPART; p++)
        for (uint32_t i = 0; i < voice_policy_cap(voice_policy, p); i++)
            trk_note_on(&trk[p], 40u + p * 12u + i, 100);
}
static int reserve_notes(void)
{
    int bad = 0;
    voice_t before[NVOICE];
    reserve_setup(VOICE_POLICY_FIXED | 0x3131u);        /* 1 / 3 / 1 / 3 */
    reserve_fill();
    bad += check("FIXED fills every track's reserved slots, total eight", voices_busy() == 8u &&
                 reserved_busy(0) == 1u && reserved_busy(1) == 3u && reserved_busy(2) == 1u && reserved_busy(3) == 3u);
    int ok = 1;
    for (uint32_t p = 0; p < NPART; p++) {
        uint32_t other = (p + 1u) % NPART;
        memcpy(before, trk[other].v, sizeof before);
        for (uint32_t n = 0; n < 64u; n++) {
            trk_note_on(&trk[p], 30u + n, 100);
            if (n & 1u) trk_note_off(&trk[p], 30u + n);
            ok &= voices_busy() <= 8u && reserved_busy(p) <= voice_policy_cap(voice_policy, p);
        }
        ok &= !memcmp(before, trk[other].v, sizeof before);
    }
    bad += check("held notes and long release tails only replace their own track's voices", ok);
    reserve_setup(VOICE_POLICY_FIXED | 0x3131u);
    trk[0].p[P_VOICE] = V_UNISON;
    trk[1].p[P_VOICE] = V_UNISON;
    trk[2].p[P_VOICE] = V_LEGATO;
    trk[3].p[P_VOICE] = V_MONO;
    for (uint32_t p = 0; p < NPART; p++) trk_note_on(&trk[p], 48u + p, 100);
    bad += check("UNISON obeys the reservation; MONO and LEGATO retain one voice", reserved_busy(0) == 1u &&
                 reserved_busy(1) == 3u && reserved_busy(2) == 1u && reserved_busy(3) == 1u);
    reserve_setup(VOICE_POLICY_FIXED | 0x3131u);
    reserve_fill();
    for (uint32_t p = 0; p < NPART; p++) {
        trk[p].p[P_DIST] = 80; trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 100;
        trk[p].p[P_ITYPE] = 1; trk[p].p[P_IMIX] = 100;
    }
    int32_t out[CTL * 2u];
    for (uint32_t i = 0; i < 128u; i++) mix_block(out, CTL);
    bad += check("inserts and shared FX do not consume reserved synth voices", voices_busy() == 8u &&
                 reserved_busy(0) == 1u && reserved_busy(1) == 3u && reserved_busy(2) == 1u && reserved_busy(3) == 3u);
    trk[0].p[P_AMODE] = AM_UP;
    input_on(&trk[0], 60, 100); input_on(&trk[0], 64, 100); input_on(&trk[0], 67, 100);
    memcpy(before, trk[1].v, sizeof before);
    for (uint32_t i = 0; i < 64u; i++) arp_step(&trk[0], div_samples((uint32_t)trk[0].p[P_ARATE]), CTL);
    bad += check("a fast ARP stays inside its one voice without touching the chord track", reserved_busy(0) == 1u &&
                 !memcmp(before, trk[1].v, sizeof before));
    return bad;
}
static int reserve_drums_and_changes(void)
{
    int bad = 0, ok = 1;
    int32_t out[CTL * 2u];
    for (uint32_t kit = 0; kit < 2u; kit++) {
        reserve_setup(VOICE_POLICY_FIXED | 0x3131u);
        set_engine_of(&trk[3], ENGI_DRUM); apply_preset_to(&trk[3], kit ? 4u : 0u);
        for (uint32_t p = 0; p < 3u; p++) trk_note_on(&trk[p], 48u + p, 100);
        voice_t protected[3][NVOICE];
        for (uint32_t p = 0; p < 3u; p++) memcpy(protected[p], trk[p].v, sizeof protected[p]);
        for (uint32_t i = 0; i < 64u; i++) trk_note_on(&trk[3], 35u + i % 47u, 100);
        ok &= reserved_busy(3) <= 3u;
        for (uint32_t p = 0; p < 3u; p++) ok &= !memcmp(protected[p], trk[p].v, sizeof protected[p]);
        voice_policy = VOICE_POLICY_FIXED | 0x1131u;   /* reduce drums while sounding */
        events_block(CTL);
        for (uint32_t i = 0; i < 64u; i++) {
            trk_note_on(&trk[3], 35u + i % 47u, 100);
            ok &= reserved_busy(3) <= 1u;
        }
        mix_block(out, CTL);
    }
    bad += check("DRUM and DRUM-X respect caps and cannot revive a fading out-of-cap lane", ok);
    reserve_setup(0);
    for (uint32_t i = 0; i < NVOICE; i++) trk_note_on(&trk[0], 48u + i, 100);
    bad += check("AUTO retains the existing shared eight-voice allocation", reserved_busy(0) == 8u);
    voice_policy = VOICE_POLICY_FIXED | 0x3131u;
    events_block(CTL);
    bad += check("AUTO to FIXED trims excess voices at the next control block", reserved_busy(0) == 1u);
    mix_block(out, CTL);
    for (uint32_t i = 1; i < NVOICE; i++) ok &= !trk[0].v[i].active;
    voice_policy &= ~VOICE_POLICY_FIXED;
    events_block(CTL);
    for (uint32_t i = 0; i < NVOICE; i++) trk_note_on(&trk[0], 48u + i, 100);
    bad += check("returning to AUTO restores sharing and retains the prepared caps", reserved_busy(0) == 8u &&
                 voice_policy_cap(voice_policy, 1) == 3u);
    return bad;
}
static int reserve_controls_and_settings(void)
{
    int bad = 0;
    reserve_setup(0);
    hold(B_HOME); ui.menu_sel = MI_VMODE;
    turn(EN_K1, -1); turn(EN_K3, -1); turn(EN_K2, 1); turn(EN_K4, 1);
    int ok = voice_policy == 0x3131u;
    turn(EN_K2, 1); ok &= voice_policy == 0x3131u;
    menu_input(2u); ok &= voice_policy == (VOICE_POLICY_FIXED | 0x3131u);
    menu_input(1u); ok &= voice_policy == 0x3131u;
    bad += check("four knobs edit T1..T4, sum is bounded, OCT+ FIXED / OCT- AUTO", ok);
    persist_t saved = {0};
    voice_policy |= VOICE_POLICY_FIXED;
    ui_prefs3 = 3u; ui_prefs2 = 0x80u;
    assert(ml_learn(20, 0, DX_FIRST + DX_FB) == 1);
    settings_export(&saved);
    reserve_setup(0);
    ok = settings_import(&saved, sizeof saved) == 1 && voice_policy == (VOICE_POLICY_FIXED | 0x3131u) &&
         ui_prefs3 == 3u && ui_prefs2 == 0x80u && ml_cc_of(0, DX_FIRST + DX_FB) == 21u;
    persist_t again = saved; settings_export(&again);
    ok &= !memcmp(&again, &saved, sizeof saved);
    bad += check("PER6 round trip keeps caps, CPU2, EXT START, LIVE LOAD and DRUM-X MIDI Learn", ok);
    saved.magic = 0x50455235u;
    saved.voice_allocation = 0xFFFFFFFFu;             /* not present in PER5 */
    ok = settings_import(&saved, PERSIST_PREVOICE_SIZE) == 2 && voice_policy == 0u &&
         ml_cc_of(0, DX_FIRST + DX_FB) == 21u && ui_prefs3 == 3u && ui_prefs2 == 0x80u;
    bad += check("Custom18 PER5 migrates to AUTO without losing MIDI Learn or other preferences", ok);
    saved.magic = PERSIST_MAGIC; saved.voice_allocation = VOICE_POLICY_FIXED | 0x5555u;
    bad += check("invalid saved allocation falls back to AUTO", settings_import(&saved, sizeof saved) == 1 && !voice_policy);
    voice_policy = VOICE_POLICY_FIXED | 0x3131u;
    project_t project; project_capture(&project);
    project_restore_runtime(&project);
    bad += check("project loads preserve the device's voice allocation", voice_policy == (VOICE_POLICY_FIXED | 0x3131u));
    return bad;
}
int main(void)
{
    int bad = reserve_notes() + reserve_drums_and_changes() + reserve_controls_and_settings();
    if (getenv("VOICE_SCREEN_DIR")) {
        reserve_setup(VOICE_POLICY_FIXED | 0x3131u);
        hold(B_HOME); ui.menu_sel = MI_VMODE;
        ui_prefs = PREF_ANIM_OFF;
        screen_save(getenv("VOICE_SCREEN_DIR"), "voices-fixed");
        ui_prefs |= PREF_LARGE;
        screen_save(getenv("VOICE_SCREEN_DIR"), "voices-fixed-large");
        voice_policy &= ~VOICE_POLICY_FIXED;
        screen_save(getenv("VOICE_SCREEN_DIR"), "voices-auto-large");
    }
    printf("TRACK VOICE RESERVATIONS: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
