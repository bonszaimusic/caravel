/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens of the START HERE tutorial and the cards of the tutorials overview, from the actual firmware renderer, driven through
 * the panel path a player uses (web_buttons / web_keys / web_enc). Frames land in build/release-prep/*.ppm; tools/start_tutorial_frames.py
 * turns them into docs/tutorials/img/*.png. No drawn UI substitutes: every frame is the 240 x 240 framebuffer. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN start_tutorial_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
static void shot(const char *name)
{
    char path[256]; snprintf(path, sizeof path, "build/release-prep/%s.ppm", name);
    FILE *f = fopen(path, "wb"); CHECK(f); fprintf(f, "P6\n240 240\n255\n");
    for (unsigned i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((web_fb[i] >> 8) | (web_fb[i] << 8));
        unsigned char p[] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        fwrite(p, 1, 3, f);
    }
    fclose(f);
}
static void ready(const char *n) { ui.msg_t = ui.hot_t = 0; ui.force = 1; run_ms(80); shot(n); }   /* the screen at rest, no message line */
static void press(uint32_t b) { web_buttons(1u << b); run_ms(80); web_buttons(0); run_ms(80); }
static void hold(uint32_t b, uint32_t ms) { web_buttons(1u << b); run_ms(ms); }
static void release(void) { web_buttons(0); run_ms(80); }
static void tapkey(uint32_t k, uint32_t ms) { web_keys(1u << k); run_ms(ms); web_keys(0); run_ms(120); }
static uint32_t preset_by_name(uint32_t id, const char *name)
{
    const engine_t *e = ENGINES[id];
    for (uint32_t i = 0; i < e->npresets; i++) if (!strcmp(e->presets[i].name, name)) return i;
    fprintf(stderr, "no preset %s\n", name); exit(2);
}
/* one engine on track 1 with a named factory preset, shown on its first EDIT page */
static void engine_card(const char *shotname, uint32_t id, const char *preset)
{
    press(B_HOME); host_preset(&trk[0], id, preset_by_name(id, preset)); run_ms(300);
    press(B_EDIT); ready(shotname);
    press(B_HOME);
}
int main(void)
{
    web_nor_erase(); web_boot(); run_ms(1400); normal_off(); pair_disable();
    song.sel = 0; song.g[G_CLOCK] = 0; song.g[G_BPM] = 120; seq_stop(); run_ms(200);
    ui_prefs2 |= (uint8_t)(PREF_SEL_PAGE >> 16);
    go_home(); ready("start-home");                                              /* power-on state: track 1, ANALOG ACID */
    const char *pname = ENGINES[trk[0].eng_req]->presets[trk[0].preset].name; CHECK(!strcmp(pname, "ACID"));
    /* 1 a sound: PRESETS picks the next sounds of the engine; the HOME cards show the sound's values */
    web_enc(EN_PRESET, 2); run_ms(200); ready("start-preset");
    web_enc(EN_PRESET, -2); run_ms(200);
    /* 2 the engine picker: hold EDIT, a white key chooses the engine */
    hold(B_EDIT, 700); ready("start-engine-picker");
    web_keys(1u << 9); run_ms(300); ready("start-engine-picked"); web_keys(0); run_ms(80); release();
    press(B_HOME); host_preset(&trk[0], 0u, preset_by_name(0u, "ACID")); run_ms(300); go_home(); run_ms(200);
    /* 3 a first sequence: SEQ opens the STEP page, REC arms step entry, every key writes a note and moves one step on */
    press(B_SEQ); ready("start-step");
    press(B_REC); ready("start-step-armed");
    tapkey(7, 200); tapkey(7, 200); tapkey(9, 200); tapkey(11, 200); ready("start-step-notes");
    press(B_REC); press(B_PLAY); run_ms(1500); ready("start-step-play"); press(B_PLAY); run_ms(300);
    /* 4 saving the work: SAVE, four presses on, PROJECT (A to D) */
    press(B_SAVE); ready("start-save-user");
    press(B_SAVE); press(B_SAVE); press(B_SAVE); ready("start-save-project");
    web_enc(EN_K4, 1); run_ms(150); ready("start-save-arm");
    press(B_OCTUP); ready("start-save-name");
    press(B_OCTUP); run_ms(800); ready("start-saved");
    press(B_HOME);
    /* cards of the overview: one real screen per topic */
    ready("card-start");
    hold(B_EDIT, 700); ready("card-basics"); release(); press(B_HOME);
    engine_card("card-sem", ENGI_SEM, "WARM PAD");
    engine_card("card-hades", ENGI_HADES, "DEEP SUB");
    engine_card("card-drone", ENGI_DRONE, "STILL AIR");
    engine_card("card-ensemble", ENGI_ENSEMBLE, "VINTAGE STRINGS");
    host_preset(&trk[0], 0u, preset_by_name(0u, "ACID")); run_ms(300);
    press(B_ARP); ready("card-seq-arp"); press(B_HOME);
    press(B_FX); ready("card-filter-fx"); press(B_HOME);
    web_buttons(1u << B_GLO); run_ms(80); web_keys(1u << 9); run_ms(300); web_keys(0); run_ms(80); ready("card-midi"); web_buttons(0); run_ms(80); press(B_HOME);
    hold(B_HOME, 800); ready("card-help"); release();
    /* HARMONY: the bus on with C Am F G, ACTION on (HOME twice), two taps of D#5 from RANDOM to HARM, the transport running */
    go_home(); run_ms(200);
    { static const uint8_t r[4] = {0, 9, 5, 7}, d[4] = {1, 0, 0, 2}; for (uint32_t i = 0; i < 4u; i++) { hroot_set(i, r[i]); hdur_set(i, d[i]); } }
    trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 1; trk[0].p[P_QUANT] = QN_OFF;
    harmony_on = 1; harmony_bar = 0; harmony_follow[0] = 1; harmony_perf_clear();
    press(B_HOME); run_ms(90); press(B_HOME); run_ms(300); CHECK(action_enabled);
    tapkey(22, 140); tapkey(22, 140); CHECK(action_bank == ACT_BANK_HARMONY);
    seq_start(); run_ms(900); ready("card-harmony");
    printf("START TUTORIAL FRAMES: %u checks.\n", checks);
    return 0;
}
