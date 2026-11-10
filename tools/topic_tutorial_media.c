/* SPDX-License-Identifier: GPL-3.0-only */
/* Media for the topic tutorials (docs/tutorials/*.md), from the actual firmware code on the host: the screens (the firmware renderer, driven through
 * the panel path of a player) and the sound examples (the firmware DSP, MIDI notes in, WAV out).
 *   topic_tutorial_media frames     build/release-prep/*.ppm
 *   topic_tutorial_media audio      build/release-prep/audio/*.wav (stereo 16 bit, 44.1 kHz)
 * tools/topic_tutorial_media.py runs both and makes PNG / MP3 files. The audio is a native DSP render, not a recording of an FM-1. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN topic_baseline_main
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
static void ready(const char *n) { ui.msg_t = ui.hot_t = 0; ui.force = 1; run_ms(80); shot(n); }
static void keepmsg(const char *n) { ui.force = 1; run_ms(80); shot(n); }                      /* with its message line */
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
static void load(uint32_t id, const char *preset) { host_preset(&trk[0], id, preset_by_name(id, preset)); run_ms(300); }
static void boot(void) { web_nor_erase(); web_boot(); run_ms(1400); normal_off(); pair_disable(); song.sel = 0; song.g[G_CLOCK] = 0; song.g[G_BPM] = 120; seq_stop(); run_ms(200); go_home(); run_ms(200); }

/* ------------------------------------------------------------------------------------------------ the engine tours */
typedef struct { const char *key; uint32_t id; const char *preset; } tour_t;
static const tour_t TOURS[] = {
    {"sem-warm-pad", ENGI_SEM, "WARM PAD"}, {"sem-brass", ENGI_SEM, "SEM BRASS"}, {"sem-notch-pad", ENGI_SEM, "NOTCH PAD"}, {"sem-round-bas", ENGI_SEM, "ROUND BAS"},
    {"hades-deep-sub", ENGI_HADES, "DEEP SUB"}, {"hades-dark-saw", ENGI_HADES, "DARK SAW"}, {"hades-dark-plk", ENGI_HADES, "DARK PLK"}, {"hades-raw-lead", ENGI_HADES, "RAW LEAD"},
    {"drone-still-air", ENGI_DRONE, "STILL AIR"}, {"drone-slow-chain", ENGI_DRONE, "SLOW CHAIN"}, {"drone-dark-ring", ENGI_DRONE, "DARK RING"}, {"drone-tide", ENGI_DRONE, "TIDE"},
    {"ensemble-vintage-strings", ENGI_ENSEMBLE, "VINTAGE STRINGS"}, {"ensemble-glass-choir", ENGI_ENSEMBLE, "GLASS CHOIR"},
    {"ensemble-abyss-pad", ENGI_ENSEMBLE, "ABYSS PAD"}, {"ensemble-glass-shimmer", ENGI_ENSEMBLE, "GLASS SHIMMER"},
};
static void frames(void)
{
    boot();
    ui_prefs2 |= (uint8_t)(PREF_SEL_PAGE >> 16);
    /* engines: HOME and the PRESETS list at the first preset of each, the EDIT screen at every preset of the tour */
    static const struct { const char *name; uint32_t id; const char *first; } ENG[] = {{"sem", ENGI_SEM, "WARM PAD"}, {"hades", ENGI_HADES, "DEEP SUB"}, {"drone", ENGI_DRONE, "STILL AIR"}, {"ensemble", ENGI_ENSEMBLE, "VINTAGE STRINGS"}};
    for (unsigned i = 0; i < sizeof ENG / sizeof ENG[0]; i++) {
        char n[64];
        load(ENG[i].id, ENG[i].first);
        snprintf(n, sizeof n, "eng-%s-presets", ENG[i].name); press(B_SAVE); press(B_SAVE); ready(n); press(B_HOME);
    }
    for (unsigned i = 0; i < sizeof TOURS / sizeof TOURS[0]; i++) {
        char n[96]; load(TOURS[i].id, TOURS[i].preset);
        press(B_EDIT); snprintf(n, sizeof n, "eng-%s", TOURS[i].key); ready(n); press(B_HOME);
    }
    host_preset(&trk[0], 0u, preset_by_name(0u, "ACID")); run_ms(300); go_home(); run_ms(200);
    /* basics and shaping pages */
    press(B_EDIT); ready("basics-edit"); press(B_HOME);
    press(B_SCL); ready("basics-scl"); press(B_HOME);
    press(B_ENV); ready("basics-env"); press(B_HOME);
    press(B_LFO); ready("basics-lfo"); press(B_HOME);
    press(B_GLO); ready("basics-mixer"); web_enc(EN_SELECT, 1); run_ms(150); ready("basics-clock"); web_enc(EN_K3, 1); run_ms(150); ready("midi-clock-usb");
    web_enc(EN_K3, 1); run_ms(150); ready("midi-clock-trs"); web_enc(EN_K3, -2); run_ms(150); press(B_HOME);
    press(B_FX); ready("fx-sends");
    web_enc(EN_SELECT, 1); run_ms(150); web_enc(EN_SELECT, 1); run_ms(150); ready("fx-chorus-reverb");
    web_enc(EN_SELECT, 1); run_ms(150); web_enc(EN_SELECT, 1); run_ms(150); ready("fx-insert");     /* INSERT upper group, then the lower group (INSERT A B C FILTER) */
    web_enc(EN_SELECT, 1); run_ms(150); web_enc(EN_K4, -32); run_ms(300); ready("fx-filter");
    web_enc(EN_K4, 32); run_ms(300); press(B_HOME);
    press(B_LFO); web_enc(EN_SELECT, 1); run_ms(150); web_enc(EN_SELECT, 1); run_ms(150); ready("fx-lfo-dest");
    press(B_HOME);
    hold(B_HOME, 800); release(); ready("basics-menu"); web_enc(EN_ALGO, 1); run_ms(150); ready("basics-menu-control"); web_enc(EN_ALGO, 1); run_ms(150); web_enc(EN_ALGO, 1); run_ms(150); ready("midi-menu"); web_enc(EN_ALGO, 1); run_ms(150); ready("basics-menu-system"); press(B_HOME);
    /* MIDI Learn: GLO + D4, a knob, a CC */
    web_buttons(1u << B_GLO); run_ms(80); ready("midi-glo-map"); web_keys(1u << 9); run_ms(300); web_keys(0); run_ms(80); web_buttons(0); run_ms(80);
    keepmsg("midi-learn-on"); web_enc(EN_K1, 1); run_ms(150); web_midi(0xB0, 20, 64); run_ms(60); keepmsg("midi-learn-mapped");
    web_buttons(1u << B_GLO); run_ms(80); web_keys(1u << 9); run_ms(300); web_keys(0); run_ms(80); web_buttons(0); run_ms(80);
    web_midi(0xB0, 20, 100); run_ms(60); ready("midi-learn-moved");
    /* sequencer */
    press(B_SEQ); press(B_REC); tapkey(7, 200); tapkey(9, 200); tapkey(11, 200); tapkey(12, 200); press(B_REC);
    web_enc(EN_K1, -3); run_ms(150); ready("seq-cursor"); web_enc(EN_K4, 1); run_ms(150); ready("seq-flag");
    press(B_SEQ); ready("seq-detail"); press(B_SEQ); run_ms(100); press(B_HOME);
    press(B_ARP); web_enc(EN_K1, 1); run_ms(150); ready("seq-arp-up"); press(B_HOME);
    /* HOME performance: ACTION on, the six banks */
    go_home(); run_ms(200); press(B_HOME); run_ms(90); press(B_HOME); run_ms(300); CHECK(action_enabled);
    ready("home-bank-random"); tapkey(22, 140); ready("home-bank-journey"); tapkey(22, 140);
    tapkey(22, 140); ready("home-bank-stasis"); tapkey(22, 140); ready("home-bank-pair"); tapkey(22, 140); ready("home-bank-live");
    /* HARMONY: the bus on with C Am F G (2 / 1 / 1 / 4 bars), the SCL pages one SELECT detent per row, the HARM bank running, HOLD, SETUP with SHIFT */
    action_enabled = action_enabled;                                  /* ACTION is still on */
    { static const uint8_t r[4] = {0, 9, 5, 7}, d[4] = {1, 0, 0, 2}; for (uint32_t i = 0; i < 4u; i++) { hroot_set(i, r[i]); hdur_set(i, d[i]); } }
    trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 1; trk[0].p[P_QUANT] = QN_OFF;
    harmony_on = 1; harmony_bar = 0; harmony_follow[0] = 1; harmony_perf_clear();
    press(B_SCL); ready("harmony-scl");
    web_enc(EN_SELECT, 1); run_ms(150); ready("harmony-chord");
    web_enc(EN_SELECT, 1); run_ms(150); ready("harmony-link");
    web_enc(EN_SELECT, 1); run_ms(150); ready("harmony-setup-row");
    web_enc(EN_SELECT, 1); run_ms(150); ready("harmony-roots");
    web_enc(EN_SELECT, 1); run_ms(150); ready("harmony-durations");
    press(B_HOME); run_ms(200);
    { uint32_t g = 0; while (action_bank != ACT_BANK_HARMONY && g++ < 8u) tapkey(22, 140); } CHECK(action_bank == ACT_BANK_HARMONY);
    seq_start(); run_ms(900); ready("harmony-bank");
    tapkey(21, 140); run_ms(300); ready("harmony-hold"); tapkey(21, 140);
    web_keys(1u << 20); run_ms(700); web_keys(0); run_ms(150); web_enc(EN_K1, 1); run_ms(150); ready("harmony-setup");
    web_keys(1u << 22); run_ms(700); web_keys(0); run_ms(150);
    printf("TOPIC FRAMES: %u checks.\n", checks);
}

/* ------------------------------------------------------------------------------------------------ the sound examples */
typedef struct { const char *key; uint32_t id; const char *preset; int phrase; int mono; } clip_t;
enum { PH_CHORD, PH_BASS, PH_LEAD, PH_LOW };
static const clip_t CLIPS[] = {
    {"sem-warm-pad", ENGI_SEM, "WARM PAD", PH_CHORD, 0}, {"sem-brass", ENGI_SEM, "SEM BRASS", PH_CHORD, 0},
    {"sem-notch-pad", ENGI_SEM, "NOTCH PAD", PH_CHORD, 0}, {"sem-round-bas", ENGI_SEM, "ROUND BAS", PH_BASS, 1},
    {"hades-deep-sub", ENGI_HADES, "DEEP SUB", PH_BASS, 1}, {"hades-dark-saw", ENGI_HADES, "DARK SAW", PH_BASS, 1},
    {"hades-dark-plk", ENGI_HADES, "DARK PLK", PH_BASS, 1}, {"hades-raw-lead", ENGI_HADES, "RAW LEAD", PH_LEAD, 1},
    {"drone-still-air", ENGI_DRONE, "STILL AIR", PH_LOW, 0}, {"drone-slow-chain", ENGI_DRONE, "SLOW CHAIN", PH_LOW, 0},
    {"drone-dark-ring", ENGI_DRONE, "DARK RING", PH_LOW, 0}, {"drone-tide", ENGI_DRONE, "TIDE", PH_LOW, 0},
    {"ensemble-vintage-strings", ENGI_ENSEMBLE, "VINTAGE STRINGS", PH_CHORD, 0}, {"ensemble-glass-choir", ENGI_ENSEMBLE, "GLASS CHOIR", PH_CHORD, 0},
    {"ensemble-abyss-pad", ENGI_ENSEMBLE, "ABYSS PAD", PH_CHORD, 0}, {"ensemble-glass-shimmer", ENGI_ENSEMBLE, "GLASS SHIMMER", PH_LEAD, 0},
};
static FILE *g_wav; static uint64_t g_frames, g_ms;
static void le32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void le16(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void wav_header(FILE *f, uint32_t frames)
{
    fwrite("RIFF", 1, 4, f); le32(f, 36 + frames * 4); fwrite("WAVEfmt ", 1, 8, f);
    le32(f, 16); le16(f, 1); le16(f, 2); le32(f, 44100); le32(f, 44100 * 4); le16(f, 4); le16(f, 16); fwrite("data", 1, 4, f); le32(f, frames * 4);
}
static void tick(void)
{
    uint64_t want = ((g_ms + 1u) * 44100u) / 1000u; uint32_t n = (uint32_t)(want - g_frames);
    web_render(n);
    for (uint32_t i = 0; g_wav && i < n; i++) {
        float l = out_l[i], r = out_r[i]; CHECK(isfinite(l) && isfinite(r));
        if (l > 1) l = 1; if (l < -1) l = -1; if (r > 1) r = 1; if (r < -1) r = -1;
        le16(g_wav, (uint32_t)(int32_t)(l * 32767.0f) & 0xFFFF); le16(g_wav, (uint32_t)(int32_t)(r * 32767.0f) & 0xFFFF);
    }
    g_frames += n; g_ms++;
}
static void wait_ms(uint32_t ms) { while (ms--) tick(); }
static void note(int n, int on) { web_midi(on ? 0x90 : 0x80, (uint32_t)n, on ? 100 : 0); }
static void render_clip(const clip_t *c)
{
    char path[256]; snprintf(path, sizeof path, "build/release-prep/audio/%s.wav", c->key);
    boot();
    host_preset(&trk[0], c->id, preset_by_name(c->id, c->preset));
    if (c->mono) trk[0].p[P_VOICE] = V_MONO;
    wait_ms(300);
    g_wav = fopen(path, "wb"); CHECK(g_wav); wav_header(g_wav, 0); g_frames = 0; g_ms = 0;
    switch (c->phrase) {
    case PH_CHORD: { static const int n[] = {48, 55, 64}; for (int i = 0; i < 3; i++) note(n[i], 1); wait_ms(5500); for (int i = 0; i < 3; i++) note(n[i], 0); wait_ms(3000); break; }
    case PH_LOW: { static const int n[] = {48, 55}; for (int i = 0; i < 2; i++) note(n[i], 1); wait_ms(8000); for (int i = 0; i < 2; i++) note(n[i], 0); wait_ms(3000); break; }
    case PH_BASS: { static const int n[] = {36, 36, 43, 41, 36, 39, 43, 31}; for (int i = 0; i < 8; i++) { note(n[i], 1); wait_ms(420); note(n[i], 0); wait_ms(130); } wait_ms(1800); break; }
    case PH_LEAD: { static const int n[] = {60, 63, 67, 70, 67, 63, 65, 60}; for (int i = 0; i < 8; i++) { note(n[i], 1); wait_ms(380); note(n[i], 0); wait_ms(120); } wait_ms(2200); break; }
    }
    fseek(g_wav, 0, SEEK_SET); wav_header(g_wav, (uint32_t)g_frames); fclose(g_wav);
    fprintf(stderr, "clip %s: %llu frames\n", c->key, (unsigned long long)g_frames);
}
int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "audio")) {                      /* one clip per process: every clip starts from a fresh boot */
        for (unsigned i = 0; i < sizeof CLIPS / sizeof CLIPS[0]; i++) if (!strcmp(CLIPS[i].key, argv[2])) { render_clip(&CLIPS[i]); return 0; }
        fprintf(stderr, "no clip %s\n", argv[2]); return 2;
    }
    if (argc > 1 && !strcmp(argv[1], "list")) { for (unsigned i = 0; i < sizeof CLIPS / sizeof CLIPS[0]; i++) printf("%s\n", CLIPS[i].key); return 0; }
    frames();
    return 0;
}
