/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 gospodindilly-ops */
/* The second core (fx.c mix_block, hal/fm1_cpu1.h) on the host, after Jangada's tests/dualcore_test.c
 * (github.com/zednaked/jangada, GPL-3.0). Four parts of the same engine at once (every preset of every engine), and
 * mixes of different engines; every part with something that keeps state of its own while it renders: an INSERT, the
 * SLICER (part 1), SPREAD (part 2), the matrix with LFO S&H, which draws from the shared generator (parts 0 and 3),
 * ENV SYNC (part 0); and PHYS's model changed under a sounding note. Each scene is rendered on one core, and again with
 * a thread as core 1 (tests/c1_host.h) taking every split of the parts in turn, the firmware's own choice
 * (mix_split) and fixed splits. The outputs must be bit for bit the same; built with -fsanitize=thread
 * (run_tests.sh TSAN=1), nothing two parts share may be touched by both at once.
 *   build/host/dualcore_test          SCENE=text: only the scenes whose name holds it */
#include "c1_host.h"
#include "../experiments/home-perf/home_elastic_overlay.h"
#define main hostsim_main
#include "hostsim.c"
#undef main

#if defined(ELASTIC_DUALCORE_TEST) && ELASTIC_DUALCORE_TEST
/* The ownership gate compiles the DSP TU without home_ui.c. These hooks are
 * no-ops because this test installs its own overlay bracket around mix_block;
 * the actual HOME macro/MOTION path is covered by the integration test. */
static void hp_begin(track_t *t) { (void)t; }
static void hp_end(track_t *t) { (void)t; }
static void hp_motion_step(track_t *t, uint32_t step, uint32_t play) { (void)t; (void)step; (void)play; }
static void hp_motion_stop(void) {}
static void hp_midi_touch(uint32_t kind, uint32_t track, uint32_t id) { (void)kind; (void)track; (void)id; }
#endif

#define SECS 3u

typedef struct { const char *name; uint8_t eng[NPART], pre[NPART]; uint8_t morph; } scen_t;

static int32_t peak[NPART];                         /* each part's loudest (track_t.peak), for the check */

static void part_setup(track_t *t, uint32_t k, uint32_t e)
{
    if (getenv("LFO2_TEST")) {
        lfo2_load(k,0);
        lfo2_set(k,L2_FIRST + L2_RATE,80 + k*10);
        lfo2_set(k,L2_FIRST + L2_WAVE,k==1?4:1);
        lfo2_set(k,L2_FIRST + L2_POL,k&1u);
        t->p[P_M3SRC] = k==2 ? MS_SLEW2 : MS_LFO2;
        t->p[P_M3DST] = MD_CUT; t->p[P_M3AMT] = 14;
        t->p[P_M4SRC] = MS_SH2; t->p[P_M4DST] = MD_PAN; t->p[P_M4AMT] = 10;
    }
    if (e == 0u || e == 2u || e == 3u) t->p[P_MORPH] = (int16_t)(40u + k * 20u);
    if (e == 2u) t->p[P_PD_FB] = 48;
    if (dx_track(t)) {
        dx_config_t *c = dx_of(k);
        c->image = 100; c->send[0] = 127; c->send[1] = 191; c->send[2] = 255;
        c->mot[0] = 7; c->mot[1] = 60;
    }
    t->p[P_ITYPE] = (int16_t)((k * 4u + e) % IT_N);  /* an INSERT on most parts (its own state, ins[]) */
    t->p[P_IMIX] = 90;
    t->p[P_REV] = (int16_t)(20 + 10 * k);
    t->p[P_DLY] = (int16_t)(10 * k);
    t->p[P_CHOR] = (int16_t)(30 - 5 * k);
    if (k == 0u) {
        t->p[P_ESYNC] = 1;                          /* ENV SYNC: t->esync_k */
        t->p[P_LWAVE] = 4;                          /* S&H: lfo_rand, the shared generator (parts 0..2) */
        t->p[P_LRATE] = 110;
        t->p[P_LD_FLT] = 20;
    }
    if (k == 1u) {
        t->p[P_SLCR] = 1;                           /* the SLICER: sl[1] */
        t->p[P_SLDEPTH] = 100;
    }
    if (k == 2u)
        t->p[P_SPRD] = 100;                         /* SPREAD: t->spread_on, spread_buf[2] */
    if (k == 3u) {
        t->p[P_LWAVE] = 4;                          /* S&H (part 3: its own generator) into the matrix */
        t->p[P_LRATE] = 100;
        t->p[P_M1SRC] = MS_SH;
        t->p[P_M1DST] = MD_CUT;
        t->p[P_M1AMT] = 40;
        t->p[P_M2SRC] = MS_LFO;
        t->p[P_M2DST] = MD_RATE;                    /* (a per-block destination: t->p while it renders) */
        t->p[P_M2AMT] = 20;
    }
}

static uint64_t render(const scen_t *sc, int dual, const char *mask)
{
    static const uint8_t CH[3] = {0, 7, 12};
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t f, k, i;
    int32_t o[2 * CTL];
    host_tracks_init();
    voice_policy = getenv("FIXED_VOICES") ? VOICE_POLICY_FIXED | 0x2222u : 0u;
    c1h_off = !dual;
    if (mask)
        setenv("C1_MASK", mask, 1);
    for (k = 0; k < NPART; k++) {
        host_preset(&trk[k], sc->eng[k], sc->pre[k]);
        part_setup(&trk[k], k, sc->eng[k]);
    }
    hp_elo_frame_t elastic_frame[NPART][2];
    int16_t elastic_base[NPART][2];
#if defined(ELASTIC_DUALCORE_TEST) && ELASTIC_DUALCORE_TEST
    /* Direct two-overlay render bracket on EACH part, under every one-core/
     * two-core scheduling split. This is the overlay/restore invariant, not
     * the UI publication or project_capture; the P1.2 integration test owns
     * those. It cannot reproduce an ISR interrupted between two publications.
     */
    for(k=0;k<NPART;k++){
        const param_desc_t *d0=track_desc(&trk[k],P_FILT);
        const param_desc_t *d1=track_desc(&trk[k],P_REV);
        elastic_base[k][0]=trk[k].p[P_FILT];
        elastic_base[k][1]=trk[k].p[P_REV];
        hp_elo_begin(&trk[k].p[P_FILT],(int16_t)(5+k),d0->min,d0->max,&elastic_frame[k][0]);
        hp_elo_begin(&trk[k].p[P_REV],(int16_t)(8+k),d1->min,d1->max,&elastic_frame[k][1]);
    }
#endif
    for (f = 0; f < SECS * FS; f += CTL) {
        uint32_t ms = f / (FS / 1000u);
        for (k = 0; k < NPART; k++) {
            uint32_t on = 100u + 330u * k, off = on + 1400u, base = 48u + 5u * k;
            if (ms == on || ms == on + 700u)
                for (i = 0; i < 2u; i++)
                    trk_note_on(&trk[k], base + CH[i + (ms != on)], 90 + 10 * k);
            if (ms == off)
                for (i = 0; i < 3u; i++)
                    trk_note_off(&trk[k], base + CH[i]);
        }
        if (ms == 900u || ms == 2000u)              /* PHASE: FOLD on, then off under sounding notes (phase_xfade: pd_scratch, one copy per core) */
            for (k = 0; k < NPART; k++)
                if (sc->eng[k] == 2u)
                    trk[k].p[P_E7] = ms == 900u ? 90 : 0;
        if (sc->morph && ms == 600u)                /* PHYS: MODEL changed under a sounding note (phys_render) */
            trk[0].p[P_E0] = (int16_t)((trk[0].p[P_E0] + 1) % PM_COUNT);
        mix_block(o, CTL);
#if defined(ELASTIC_DUALCORE_TEST) && ELASTIC_DUALCORE_TEST
        /* Both overlays must restore before the next block/SAVE observation.
         * This checks the post-barrier render state, not the project format.
         */
        for(k=0;k<NPART;k++){
            hp_elo_end(&trk[k].p[P_FILT],&elastic_frame[k][0]);
            hp_elo_end(&trk[k].p[P_REV],&elastic_frame[k][1]);
            if(trk[k].p[P_FILT]!=elastic_base[k][0] ||
               trk[k].p[P_REV]!=elastic_base[k][1])
                _exit(7);
        }
#endif
        for (k = 0; k < NPART; k++)
            if (trk[k].peak > peak[k])
                peak[k] = trk[k].peak;
        for (i = 0; i < 2u * CTL; i++) {
            uint32_t u = (uint32_t)o[i];
            for (k = 0; k < 4u; k++) {
                h ^= (u >> (8u * k)) & 0xFFu;
                h *= 0x100000001B3ull;
            }
        }
    }
    return h;
}

/* in a child of its own: every render starts from the state at boot */
static uint64_t render_fork(const scen_t *sc, int dual, const char *mask, uint32_t *taken)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    pid = fork();
    if (!pid) {
        h = render(sc, dual, mask);
        if (write(fd[1], &h, sizeof h) != sizeof h || write(fd[1], peak, sizeof peak) != sizeof peak ||
            write(fd[1], &c1h_taken, sizeof c1h_taken) != sizeof c1h_taken)
            _exit(2);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h || read(fd[0], peak, sizeof peak) != sizeof peak ||
        read(fd[0], taken, sizeof *taken) != sizeof *taken)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

int main(void)
{
    static scen_t sc[256];
    static char names[256][40];
    static const char *const MASKS[] = {"rot", "auto", "5", "10", "3", "12", "1"};
    uint32_t n = 0, e, i, k, g, fail = 0;
    for (e = 0; e < NENGINES; e++) {                    /* every preset: four parts at a time */
        if (!eng_ok(e))                                 /* (a retired number: DIGITAL without FELUCCA_FM4) */
            continue;
        for (g = 0; g * NPART < ENGINES[e]->npresets; g++) {
            snprintf(names[n], sizeof names[n], "4 x %s %u..%u", ENGINES[e]->name, g * NPART, g * NPART + NPART - 1u);
            sc[n].name = names[n];
            for (k = 0; k < NPART; k++) {
                sc[n].eng[k] = (uint8_t)e;
                sc[n].pre[k] = (uint8_t)((g * NPART + k) % ENGINES[e]->npresets);
            }
            sc[n++].morph = 0;
        }
    }
    sc[n++] = (scen_t){"4 x Custom DRUM-X", {10, 10, 10, 10}, {4, 4, 4, 4}, 0};
    sc[n++] = (scen_t){"Custom MORPH BYTE FLOAT DRUM-X", {0, 3, 3, 10}, {1, 12, 13, 4}, 0};
    sc[n++] = (scen_t){"Custom HADES SEM FM6 DRUM-X", {ENGI_HADES, ENGI_SEM, ENGI_FM6, ENGI_DRUM}, {4, 4, 4, 4}, 0};
    sc[n++] = (scen_t){"SEM DRUM-X FM6 ANALOG", {ENGI_SEM, ENGI_DRUM, ENGI_FM6, 0}, {4, 4, 4, 1}, 0};
    sc[n++] = (scen_t){"PHYS FM6 ANALOG DRUM", {ENGI_PHYS, ENGI_FM6, 0, ENGI_DRUM}, {1, 4, 7, 0}, 0};
    sc[n++] = (scen_t){"GRAIN NOISE VOICE LOFI", {8, 11, 5, 3}, {1, 4, 7, 10}, 0};
    sc[n++] = (scen_t){"TRIO WHEEL PHASE FM6", {6, 7, 2, ENGI_FM6}, {1, 4, 7, 10}, 0};
    sc[n++] = (scen_t){"PHYS morph + DRUM GRAIN ANALOG", {ENGI_PHYS, ENGI_DRUM, 8, 0}, {2, 1, 3, 5}, 1};
#if FELUCCA_SLICE
    sc[n++] = (scen_t){"SAMPLE SLICE GRAIN PHASE", {ENGI_SAMPLE, 13, 8, 2}, {2, 1, 3, 5}, 0};
    sc[n++] = (scen_t){"SLICE FM6 SAMPLE DRUM", {13, ENGI_FM6, ENGI_SAMPLE, ENGI_DRUM}, {0, 2, 3, 4}, 0};
#else
    sc[n++] = (scen_t){"SAMPLE FM6 GRAIN PHASE", {ENGI_SAMPLE, ENGI_FM6, 8, 2}, {2, 1, 3, 5}, 0};
#endif
    for (i = 0; i < n; i++) {
        uint64_t ref;
        uint32_t m, bad = 0, quiet = 0, taken = 0, took = 0;
        if (getenv("SCENE") && !strstr(sc[i].name, getenv("SCENE")))
            continue;
        ref = render_fork(&sc[i], 0, 0, &taken);
        for (k = 0; k < NPART; k++)
            quiet += peak[k] < 200;
        if (quiet) {                                    /* a silent part proves nothing */
            printf("dualcore: %-34s %u part(s) silent: peaks %d %d %d %d   FAIL\n", sc[i].name, quiet, peak[0],
                   peak[1], peak[2], peak[3]);
            fail++;
            continue;
        }
        for (m = 0; m < sizeof MASKS / sizeof MASKS[0]; m++) {
            uint64_t h = render_fork(&sc[i], 1, MASKS[m], &taken);
            took += taken != 0;
            if (h != ref || !h) {
                printf("dualcore: %-34s split %-4s FAIL (%016llx, one core %016llx)\n", sc[i].name, MASKS[m],
                       (unsigned long long)h, (unsigned long long)ref);
                bad++;
            }
        }
        if (took < 2u) {                                /* the thread must have rendered parts (auto may hand none) */
            printf("dualcore: %-34s the second core never took parts   FAIL\n", sc[i].name);
            bad++;
        }
        if (!bad)
            printf("dualcore: %-34s the same on two cores, every split   ok\n", sc[i].name);
        fail += bad != 0;
    }
    printf("dualcore: %u of %u scenes differ\n", fail, n);
    return fail != 0;
}

