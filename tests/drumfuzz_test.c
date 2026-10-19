/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* #211 stress: a synth sequence playing on track 1 while the DRUM grid of track 4 is programmed at random (grid keys,
 * held steps + knobs = locks, lanes, HIT / ACC, EDIT, pages, SEQ TOOLS, undo, engine switches, kits, buttons, menus),
 * the MENU at random (HELP, LARGE, STEP PREVIEW ..), the audio rendered between the inputs (24 blocks a frame, as the
 * ISR does) and, with -DPREEMPT, also from a CPU-time signal anywhere in the main loop (fm1_irq_off and motion_guard
 * hold it off, as on the device). "song": a SONG of two saved projects plays instead of PLAY. Each input step has a 5 s
 * alarm (a hang: the device's watchdog is 8 s). With ASan / UBSan (shifts of negative values and wrapping phases are
 * meant: -fno-sanitize=shift -fwrapv):
 *   cc -DPREEMPT -g -O1 -fwrapv -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all -w \
 *      -Ibuild/gen -Ifirmware/src -o build/host/drumfuzz tests/drumfuzz_test.c -lm
 *   build/host/drumfuzz SEED0 NSEEDS STEPS [song]
 * run_tests.sh runs a short one; #211 (1.5.5 and v1.5-src): 200 seeds x 4000 steps each way, nothing found. */
#define UI_TEST_NO_MAIN 1
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef PREEMPT
/* -DPREEMPT: the audio ISR emulated by a CPU-time signal that renders a block wherever the main loop is (as the
 * ALNK0 interrupt does), held off by fm1_irq_off / motion_guard as on the device */
#define HOST_IRQ_STUBS 1
#define FM1_IRQ_TARGET 1
static volatile int host_irq_mask, host_irq_pend;
static uint32_t fm1_icfg(void) { return (uint32_t)host_irq_mask; }
static void host_isr(void);
static void fm1_irq_off(void) { host_irq_mask = 1; }
static void fm1_irq_on(void) { host_irq_mask = 0; if (host_irq_pend) { host_irq_pend = 0; host_isr(); } }
static void fm1_icfg_set(uint32_t f) { if (f) host_irq_mask = 1; else fm1_irq_on(); }
#endif
#include "ui_test.c"
#ifdef PREEMPT
static volatile int host_in_isr;
static uint32_t host_isr_n;
static void host_isr(void)
{
    int32_t o[2u * CTL];
    host_in_isr = 1;
    mix_block(o, CTL);
    host_isr_n++;
    host_in_isr = 0;
}
static uint32_t host_sig_n;
static void on_vt(int s)
{
    (void)s;
    host_sig_n++;
    if (host_irq_mask || host_in_isr) { host_irq_pend = 1; return; }
    host_isr();
}
#endif

static uint32_t rs;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0; }
static const char *fz_what = "";
static uint32_t fz_seed, fz_i;
static void on_alarm(int s) { (void)s; fprintf(stderr, "HANG seed %u step %u in %s\n", fz_seed, fz_i, fz_what); _exit(3); }

static void fz_audio(uint32_t blocks)
{
#ifdef PREEMPT
    while (blocks--)                                      /* (the time between frames; the signal adds blocks anywhere) */
        if (!host_irq_mask)
            host_isr();
#else
    int32_t o[2u * CTL];
    while (blocks--)
        mix_block(o, CTL);
#endif
}
/* a UI frame with the audio ISR's 16 ms in the middle of it */
#include <time.h>
static double fz_ui_max;                                  /* the slowest UI frame (input + draw), seconds */
static const char *fz_ui_max_what = "";
static double now_s(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static void fz_frame(void)
{
    double t0, t1;
    host_ticks += 16000u;
    fm1_ms += 16u;
    t0 = now_s();
    ui_input();
    t1 = now_s() - t0;
    fz_audio(24);
    t0 = now_s();
    ui_draw();
    t1 += now_s() - t0;
    if (t1 > fz_ui_max) { fz_ui_max = t1; fz_ui_max_what = fz_what; }
}
static void fz_press(uint32_t b)
{
    fm1_in.buttons |= 1u << panel.btn[b];
    host_pressed |= 1u << panel.btn[b];
    fz_frame();
    fm1_in.buttons &= ~(1u << panel.btn[b]);
    fz_frame();
}
static void fz_turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    fz_frame();
    host_ticks += rnd(2) ? 200000u : 0u;
}
static void fz_key(uint32_t k, uint32_t down)
{
    if (down) {
        fm1_in.notes |= 1u << k;
        host_notes |= 1u << k;
    } else
        fm1_in.notes &= ~(1u << k);
}
static void to_grid(void)
{
    uint32_t n;
    if (ui.menu)
        menu_close();
    ui.confirm = 0;
    song.sel = 3;
    if (trk[3].eng_req != ENGI_DRUM && rnd(3) == 0)
        set_engine_of(&trk[3], ENGI_DRUM);
    for (n = 0; n < 6u && !grid_on(); n++)
        fz_press(B_SEQ);
}

static void fz_play(void);
static void one_step(void)
{
    uint32_t a = rnd(100), k;
    if (a < 25) {                                         /* a grid key tap (white: hit, black: lane / ACC / pages) */
        fz_what = "key tap";
        k = rnd(27);
        fz_key(k, 1); fz_frame(); fz_key(k, 0); fz_frame();
    } else if (a < 35) {                                  /* a step held, knobs turned: locks */
        uint32_t n = 1 + rnd(4), j;
        fz_what = "held step + knobs";
        k = key_at(0, rnd(16));
        fz_key(k, 1); fz_frame();
        if (rnd(3) == 0) { fz_key(key_at(0, rnd(16)), 1); fz_frame(); }
        for (j = 0; j < n; j++)
            fz_turn(EN_K1 + rnd(4), (int32_t)rnd(15) - 7);
        if (rnd(4) == 0) fz_turn(EN_PRESET, (int32_t)rnd(5) - 2);
        if (rnd(4) == 0) fz_turn(EN_ALGO, (int32_t)rnd(5) - 2);
        fm1_in.notes = 0; fz_frame();
    } else if (a < 40) {                                  /* ACC held + taps */
        fz_what = "ACC held";
        fz_key(key_at(1, GK_ACC), 1); fz_frame();
        k = key_at(0, rnd(16)); fz_key(k, 1); fz_frame(); fz_key(k, 0); fz_frame();
        fm1_in.notes = 0; fz_frame();
    } else if (a < 55) {                                  /* knobs (STEP LANE HIT ACC on the grid, else that page's) */
        fz_what = "knob";
        fz_turn(EN_K1 + rnd(4), (int32_t)rnd(21) - 10);
    } else if (a < 58) {
        fz_what = "select";
        fz_turn(EN_SELECT, (int32_t)rnd(5) - 2);
    } else if (a < 61) {
        fz_what = "presets knob";
        fz_turn(EN_PRESET, (int32_t)rnd(7) - 3);
    } else if (a < 63) {
        fz_what = "algo knob";
        fz_turn(EN_ALGO, (int32_t)rnd(7) - 3);
    } else if (a < 73) {                                  /* a button */
        static const uint8_t B[] = {B_EDIT, B_EDIT, B_SEQ, B_SEQ, B_OCTUP, B_OCTDN, B_FX, B_SCL, B_ENV, B_LFO, B_GLO,
                                    B_HOME, B_SAVE, B_ARP, B_REC, B_PLAY};
        fz_what = "button";
        fz_press(B[rnd(sizeof B)]);
    } else if (a < 76) {                                  /* undo */
        fz_what = "undo (SAVE held)";
        hold(B_SAVE);
    } else if (a < 79) {                                  /* the drum track's engine / the synth's */
        fz_what = "engine switch";
        k = rnd(NENGINES);
        if (k == ENGI_DIGITAL) k = ENGI_DRUM;
        set_engine_of(&trk[rnd(3) ? 3 : 0], k);
    } else if (a < 84) {                                  /* a layer: SEQ held (SEQ TOOLS) or another, keys, knobs */
        static const uint8_t L[] = {B_SEQ, B_SEQ, B_SEQ, B_SCL, B_ARP, B_ENV, B_LFO, B_EDIT, B_SAVE, B_REC};
        uint32_t b = L[rnd(sizeof L)], j, n = 1 + rnd(3);
        fz_what = "layer (SEQ TOOLS ..)";
        fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; fz_frame();
        for (j = 0; j < n; j++) {
            k = rnd(27); fz_key(k, 1); fz_frame(); fz_key(k, 0); fz_frame();
        }
        if (rnd(2)) fz_turn(EN_K1 + rnd(4), (int32_t)rnd(9) - 4);
        fm1_in.buttons &= ~(1u << panel.btn[b]); fz_frame();
        if (rnd(2)) fz_turn(EN_K1 + rnd(3), (int32_t)rnd(9) - 4);
        if (rnd(2)) fz_press(rnd(2) ? B_OCTUP : B_OCTDN);
    } else if (a < 86) {
        fz_what = "track select";
        song.sel = (uint8_t)rnd(NTRK);
        fz_frame();
    } else if (a < 88) {                                  /* GLO + keys (FILL, the FX layer ...) */
        fz_what = "GLO / FX + key";
        k = rnd(2) ? B_GLO : B_FX;
        fm1_in.buttons |= 1u << panel.btn[k]; host_pressed |= 1u << panel.btn[k]; fz_frame();
        fz_key(rnd(27), 1); fz_frame();
        fm1_in.notes = 0; fm1_in.buttons = 0; fz_frame();
    } else if (a < 90) {                                  /* OCT- held + a knob: the default */
        fz_what = "OCT- + knob";
        fm1_in.buttons |= 1u << panel.btn[B_OCTDN]; host_pressed |= 1u << panel.btn[B_OCTDN]; fz_frame();
        fz_turn(EN_K1 + rnd(4), (int32_t)rnd(5) - 2);
        fm1_in.buttons = 0; fz_frame();
    } else if (a < 92) {                                  /* the transport */
        fz_what = "transport";
        if (!song.playing) fz_play();
        else if (rnd(4) == 0) fz_press(B_PLAY);
    } else if (a < 93) {
        fz_what = "LEN";
        trk[3].p[P_SLEN] = (int16_t)(1 + rnd(64));
        trk[0].p[P_SLEN] = (int16_t)(1 + rnd(64));
        fz_frame();
    } else if (a < 94) {
        fz_what = "DIV";
        trk[3].p[P_SDIV] = (int16_t)rnd(8);
        fz_frame();
    } else {
        fz_what = "back to the grid";
        to_grid();
    }
    if (rnd(8) == 0) { fz_what = "to the grid"; to_grid(); }
    if (!song.playing && rnd(3) == 0) { fz_what = "play"; fz_play(); }
}

static int fz_song;                                        /* SONG playing (the chain) instead of PLAY */
static void fz_play(void)
{
    if (fz_song) {
        if (!song.playing && !transport_req)
            chain_prepare();
        fz_frame();
    } else
        fz_press(B_PLAY);
}
/* A real boot initializes the song before audio_init() (src/main.c). The virtual
 * ISR remains active between seeds; block only the two power-on/reset calls,
 * whose temporary zero BPM is not a state in which hardware audio can run.
 * Restore the previous signal mask immediately; every live edit remains preemptible. */
static void fz_power_on(void)
{
#ifdef PREEMPT
    sigset_t audio, previous;
    sigemptyset(&audio); sigaddset(&audio, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &audio, &previous);
#endif
    ui_power_on();
#ifdef PREEMPT
    sigprocmask(SIG_SETMASK, &previous, 0);
#endif
}

static void setup(uint32_t synth)
{
    uint32_t i;
    fz_power_on();
    for (i = 0; i < MI_VALUES; i++)                       /* the MENU at random (HELP, LARGE, PREVIEW, ACCEL ..) */
        if (menu_valued(i) && i != MI_SERIAL && rnd(2))
            menu_put(i, rnd(menu_n(i)));
    fz_power_on();
    for (i = 0; i < NTRK; i++)
        eng_mem_claim(&trk[i]);
    set_engine_of(&trk[0], synth);
    trk[0].engine = trk[0].eng_req;
    load_pat16(&trk[0], PATTERNS[1].note, PATTERNS[1].flags);
    trk[0].p[P_SLEN] = 16;
    song.g[G_BPM] = (int16_t)(60 + rnd(180));
    song.master_q12 = 4096;
    fz_audio(8);
    if (fz_song) {
        uint32_t k, j;
        for (k = 0; k < 2u; k++) {
            for (j = 0; j < 16u; j++)
                if (rnd(2)) grid_hit(&trk[3], j, rnd(NLANE), 1);
            trk[3].p[P_SLEN] = (int16_t)(8 + 8 * k);
            project_save(k);
        }
        chain_config.count = 2;
        chain_config.row[0] = chain_row_of(0, 4);
        chain_config.row[1] = chain_row_of(1, 4);
        project_save(2);
    }
    fz_play();
    fz_audio(64);
    to_grid();
}

int main(int argc, char **argv)
{
    uint32_t s0 = argc > 1 ? (uint32_t)atoi(argv[1]) : 1, ns = argc > 2 ? (uint32_t)atoi(argv[2]) : 20,
             steps = argc > 3 ? (uint32_t)atoi(argv[3]) : 2000, s;
    fz_song = argc > 4 && !strcmp(argv[4], "song");
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
#ifdef PREEMPT
    {
        struct itimerval it = {{0, 40}, {0, 40}};
        signal(SIGVTALRM, on_vt);
        setitimer(ITIMER_VIRTUAL, &it, 0);
    }
#endif
    for (s = s0; s < s0 + ns; s++) {
        uint32_t synth = s % NENGINES;
        if (synth == ENGI_DIGITAL) synth = ENGI_FM6;
        if (synth == ENGI_DRUM) synth = ENGI_PHYS;
        fz_seed = s;
        rs = s * 2654435761u + 1u;
        setup(synth);
        for (fz_i = 0; fz_i < steps; fz_i++) {
            alarm(5);
            one_step();
        }
        alarm(0);
        {
            uint32_t h = 0, j;
            for (j = 0; j < NSTEP; j++)
                h += trk[3].step[j].hit != 0;
            printf("ui max %.2f ms (%s) ", fz_ui_max * 1000, fz_ui_max_what);
#ifdef PREEMPT
            printf("isr blocks %u signals %u ", host_isr_n, host_sig_n);
#endif
            fz_ui_max = 0;
            printf("song %u seed %u (%s) ok, playing %u, voice starts %u, t4 engine %u, t4 hit steps %u, frames %u\n", chain.running, s,
                   ENGINES[synth]->name, song.playing, vage, trk[3].engine, h, fm1_ms / 16u);
        }
    }
    return 0;
}
