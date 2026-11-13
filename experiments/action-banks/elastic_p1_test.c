/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1 on the real firmware/UI/DSP path (web emulator build).
 * Covers the ACTION bank dispatch, HOME encoder capture, the audio render
 * bracket, HOLD/RESUME/RESET, MIDI/manual ownership, transport and target
 * invalidation.  Not a hardware, dual-core or listening test. */
#define CARAVEL_ELASTIC_P1 1
#define CARAVEL_ACTION_BANKS 1
#define FELUCCA_RANDOM_MORPH 1
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN elastic_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <string.h>

static void p1_setup(void)
{
    hp_elastic_p1_clear();
    web_nor_erase(); web_boot(); run_ms(1500);
    web_test_drone(0);
    trk[0].p[P_E7] = 76;
    hp_apply(0, 1);
    song.sel = 0; go_home();
    action_enabled = 1; action_bank = ACT_BANK_ELASTIC; action_setup = 0;
    action_refresh();
    CHECK(!hp_el1.on);
}
static void p1_arm(void) { action_key(21u); CHECK(hp_el1.on); }
static void p1_turn(int32_t n) { web_enc(EN_K1 + 3u, n); run_ms(20); }
static void p1_settle(uint32_t ms)
{
    for (uint32_t i = 0; i < ms && hp_el1.spring.active; i += 16u) run_ms(16);
    CHECK(!hp_el1.spring.active);
    CHECK(hp_elastic_value(&hp_el1.spring) == hp_el1.spring.anchor);
}
static uint64_t p1_audio_hash(void)
{
    web_render(1024);
    float *l = web_out_l(), *r = web_out_r();
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < 1024u; i++) {
        union { float f; uint32_t u; } a = {l[i]}, b = {r[i]};
        h = (h ^ a.u) * 1099511628211ull;
        h = (h ^ b.u) * 1099511628211ull;
    }
    return h;
}
int main(void)
{
    /* 1. the seventh ACTION bank arms the DRONE TONE cell and the HOME K4
     *    writes the transient overlay, never the stored base value. */
    p1_setup();
    int16_t base = trk[0].p[P_E7];
    p1_arm();
    CHECK(action_bank == ACT_BANK_ELASTIC && hp_el1.row == 0u && hp_el1.slot == 3u);
    trk_note_on(&trk[0], 60u, 100u);                 /* the DRONE renders only with a voice */
    run_ms(100);
    p1_turn(4);
    CHECK(hp_el1.spring.active);
    CHECK(hp_elastic_value(&hp_el1.spring) != base);
    CHECK(trk[0].p[P_E7] == base);                    /* UI copies the base back */
    CHECK(hp_el1_audio[0] & HP_EL1_VALID);            /* published audio delta */
    int32_t tone0 = drone_state(&trk[0])->ctrl[DR_TONE];
    uint64_t moving = p1_audio_hash();                /* renders the actual bracket, 1024 frames */
    int32_t tone1 = drone_state(&trk[0])->ctrl[DR_TONE];
    CHECK(tone1 > tone0);                             /* the DRONE DSP followed the overlay */
    CHECK(trk[0].p[P_E7] == base);                    /* restored after the block */
    CHECK(hp_el1_frame[0].applied == 0);
    trk_note_off(&trk[0], 60u);
    run_ms(50);
    hp_elastic_p1_clear();

    /* 1b. any safe continuous per-track parameter works, not only TONE. */
    hp_apply(0, 1);
    hp.track[0].cell[1][0] = hp_track_ref(0, P_E4, 0xFFFFu); /* DRONE CHAR, F_PCT */
    hp.track[0].count = 2; hp.track[0].active = 1; hp_last_valid = 0;
    p1_arm();
    CHECK(hp_el1.id == P_E4 && hp_el1.row == 1 && hp_el1.slot == 0);
    int16_t generic_base = trk[0].p[P_E4];
    web_enc(EN_K1, 4); run_ms(20);                    /* the armed cell is K1 here */
    CHECK(hp_el1_audio[0] & HP_EL1_VALID);
    CHECK(HP_EL1_ID(hp_el1_audio[0]) == P_E4);
    hp_elastic_p1_audio_begin(&trk[0]);
    CHECK(trk[0].p[P_E4] != generic_base);
    hp_elastic_p1_audio_end(&trk[0]);
    CHECK(trk[0].p[P_E4] == generic_base);
    hp_elastic_p1_clear();
    hp_apply(0, 1); hp.track[0].active = 0; hp_last_valid = 0;

    /* 2. HOLD freezes the published offset; RESUME lets the spring return.
     *    HOLD on a resting spring and EXCITE while held are explicit no-ops. */
    p1_arm();
    action_key(24u);
    CHECK(!hp_el1.spring.paused && !hp_el1.spring.active);
    p1_turn(3);
    action_key(24u);
    CHECK(hp_el1.spring.paused);
    int16_t held = hp_elastic_value(&hp_el1.spring);
    run_ms(1200);
    CHECK(hp_elastic_value(&hp_el1.spring) == held);
    action_key(1u);                                  /* EXCITE while HOLD: message only */
    CHECK(hp_el1.spring.paused && hp_elastic_value(&hp_el1.spring) == held);
    action_key(24u);
    CHECK(!hp_el1.spring.paused);
    p1_settle(8000);
    CHECK(hp_elastic_value(&hp_el1.spring) == base);
    CHECK(hp_el1_audio[0] == 0u);

    /* 3. RESET ends the gesture at the anchor, without touching the preset;
     *    it also releases HOLD rather than leaving a paused, silent spring. */
    p1_turn(-5);
    CHECK(hp_el1.spring.active);
    action_key(24u);
    CHECK(hp_el1.spring.paused);
    action_key(26u);
    CHECK(!hp_el1.spring.active && !hp_el1.spring.paused && hp_el1_audio[0] == 0u);
    CHECK(hp_elastic_value(&hp_el1.spring) == base);
    CHECK(trk[0].p[P_E7] == base);

    /* 4. a manual/MIDI write to the target owns it, exactly as RANDOM does. */
    p1_turn(2);
    hp_set(hp_track_ref(0, P_E7, 0xFFFFu), 60, 1);
    CHECK(!hp_el1.on);
    CHECK(trk[0].p[P_E7] == 60);
    p1_arm(); p1_turn(2);
    hp_midi_touch(HP_TRACK, 0u, P_E7);
    CHECK(!hp_el1.on);

    /* 5. a saved performance project restores the base and cancels the overlay. */
    CHECK(web_home_save(0u));
    p1_arm(); p1_turn(2);
    CHECK(web_home_load(0u));
    CHECK(!hp_el1.on);
    CHECK(trk[0].p[P_E7] == 60);                      /* the stored base, not an overlay */

    /* 6. transport start, track change and target loss cancel the gesture. */
    p1_arm(); song.playing = 1; run_ms(40);
    CHECK(!hp_el1.on);
    song.playing = 0; p1_arm(); track_select(1); run_ms(40);
    CHECK(!hp_el1.on);
    track_select(0); p1_arm();
    trk[0].p[P_E7] = 40;                              /* target changed under the UI */
    hp.track[0].cell[hp_el1.row][hp_el1.slot].id = P_FILT;
    run_ms(40);
    CHECK(!hp_el1.on);
    CHECK(trk[0].p[P_E7] == 40);                      /* never restored to the old anchor */

    /* 7. arming can select a target outside the active HOME row. */
    p1_setup();
    hp_apply(0, 1);
    hp_ref_t target = hp.track[0].cell[0][3];
    memset(&hp.track[0].cell[0][0], 0, sizeof hp.track[0].cell[0]);
    hp.track[0].cell[1][2] = target;
    hp.track[0].count = 2; hp.track[0].active = 0;
    p1_arm();
    CHECK(hp_el1.row == 1u && hp_el1.slot == 2u && hp.track[0].active == 1u);

    /* 8. the SETUP knobs are volatile; RANGE 0 bounds the live offset at once. */
    hp_elastic_p1_setup_turn(3, -100);               /* RANGE 0 ends the offset at once */
    CHECK(hp_el1.cfg.range == 0u && !hp_el1.spring.active && hp_el1_audio[0] == 0u);
    hp_elastic_p1_setup_turn(3, 18);
    CHECK(hp_el1.cfg.range == 18u);
    char name[16], value[24];
    hp_elastic_p1_setup_caption(3, name, value);
    CHECK(strcmp(name, "RANGE") == 0);
    printf("ELASTIC P1.1 integration PASS: %u checks; ACTION bank, generic continuous target, audio bracket, HOLD/RESUME/RESET, manual/MIDI ownership, transport/track/target cancel, cross-row arm, volatile setup.\n", checks);
    return 0;
}
