/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* CHORD SOURCE / CHORD EXT, step 0: the differential baseline "SOURCE OFF = FORMAT 2 without CHORD EXT" (D2 of the Phase 2 test plan).
 *
 * 18 deterministic scenes drive the REAL firmware code (seq.c, chord.c, voice.c, fx.c, the engines, via tests/ui_test.c
 * and tests/hostsim.c, as tests/chord_test.c / latch_test.c / midi_control_test.c do) with the block-driven event
 * scheduler of tests/chordext_sim.h: panel keys POLY / MONO / UNISON, CHRD DIA3 / DIA7 / BUS and the fixed shapes, VOIC,
 * LATCH NEW / STACK, every ARP mode (HOLD, CHORD, octaves), SEQ steps with HARMONY on (strong steps), LINK / RETRIG with
 * a HARMONY bar change and a key change, MIDI IN in channel mode and TRACK / SEL routing, sustain CC64 and panic
 * CC120 / CC123, FOLLOW. Per scene it prints and compares three 64-bit FNV-1a hashes:
 *   audio    every sample (int32) the mix_block path rendered, all 4 parts and the FX;
 *   events   every note-on / note-off the voices saw: block index, track, voice slot, note, velocity (read off the
 *            voice gates after each block, a retrigger inside one block counts through the voice age), and the count
 *            of voice starts (vage) at the end;
 *   midiout  every USB-MIDI OUT packet the firmware queued, with its block index.
 * plus the counts (blocks, note events, MIDI OUT packets) so a scene cannot silently become empty.
 *
 * Every scene is run twice, each time in a fresh forked process (so no state, PRNG or function-local static leaks from
 * one run to the next): CLEAN, and JUNK: trk[t].p[61..80] (P_FM1_ATK .. P_FM4_LEVEL: the FM operator ids, inert in this
 * build, FELUCCA_FM4 = 0) set to non-default values on every track. A later feature reuses ids in that range, so the
 * baseline has to prove that old values there change nothing: both runs must give identical hashes (asserted, on every
 * invocation).
 *
 * Usage (from the repository root, after the generated headers exist: ./build.sh once, or tools/build.py generate):
 *   chordext_diff_test            check (the default, what tests/run_tests.sh runs): CLEAN == JUNK, and both == the
 *                                 baseline tests/chordext_baseline.txt; any difference fails with the scene and the hash
 *   chordext_diff_test check      the same
 *   chordext_diff_test run        CLEAN == JUNK and print the lines (no baseline needed: for developing a scene)
 *   chordext_diff_test fingerprint  print the hash of the firmware sources that `record` checks
 *   chordext_diff_test record     write tests/chordext_baseline.txt. Allowed ONCE: it refuses when the file exists, and
 *                                 refuses when the firmware sources are not those of the base commit 8243ec0 (a hash of
 *                                 firmware/src, firmware/hal and firmware/engines/src, CE_BASE_FWFP below). There is NO
 *                                 update flag, no environment variable that rewrites it, no GOLDEN_UPDATE-style path: a
 *                                 changed baseline is a deleted-and-recreated file in the diff of a commit, on the base tree.
 *   CE_DIFF_PERTURB=1 chordext_diff_test check
 *                                 negative control: the velocity of one MIDI note of one scene (midi_channels) is one
 *                                 higher; the check must FAIL (the audio and the note events of that scene change).
 *   CE_DIFF_VERBOSE=1             print the line of every run (CLEAN and JUNK), and the junk ids a scene saw changed.
 *   CE_DIFF_TRACE=name            print the note events of that scene's clean run (block, ms, track, voice, note, velocity).
 * Built and run by tests/run_tests.sh. Not a sound regression test (the goldens are): a change of the firmware that is
 * meant to change the sound or the note stream is done by the slice that does it, with the scene listed as the reason. */
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
#include <dirent.h>
#include <sys/stat.h>
#include "chordext_sim.h"

#define CE_BASELINE_PATH (getenv("CE_F2_HARM") ? "tests/chordext_format2_harm.txt" : (getenv("CE_F2_RETRIG") && !strcmp(getenv("CE_F2_RETRIG"), "2") ? "tests/chordext_format2_scale.txt" : "tests/chordext_format2_off.txt"))
#define CE_BASE_COMMIT "8243ec0"
/* FNV-1a of firmware/src, firmware/hal and firmware/engines/src at the base commit (names and contents, sorted):
 * what `record` insists on */
#define CE_BASE_FWFP 0xc6b80037fbc2558aull

_Static_assert(P_FM1_ATK == 61 && P_FM4_LEVEL == 80, "the junk range is the FM operator ids 61..80");

/* ------------------------------------------------------------------ hashing --- */
#define FNV_OFF 0xcbf29ce484222325ull
#define FNV_PRIME 0x100000001b3ull
static uint64_t fnv_byte(uint64_t h, uint8_t b) { return (h ^ b) * FNV_PRIME; }
static uint64_t fnv_u32(uint64_t h, uint32_t v)
{
    h = fnv_byte(h, (uint8_t)v); h = fnv_byte(h, (uint8_t)(v >> 8));
    h = fnv_byte(h, (uint8_t)(v >> 16)); return fnv_byte(h, (uint8_t)(v >> 24));
}

/* ----------------------------------------------------------------- observer --- */
typedef struct { uint8_t on, note; uint32_t age; } cv_t;
static cv_t cv_prev[NTRK][NVOICE];
static uint64_t h_aud, h_evt, h_out;
static uint32_t n_evt, n_out, n_blk;

static void obs_reset(void)
{
    memset(cv_prev, 0, sizeof cv_prev);
    h_aud = h_evt = h_out = FNV_OFF;
    n_evt = n_out = n_blk = 0;
}
static int ce_trace;                                   /* CE_DIFF_TRACE=scene: the clean run prints its note events (stderr) */
static void obs_event(uint32_t blk, uint32_t t, uint32_t vi, uint32_t on, uint32_t note, uint32_t vel)
{
    if (ce_trace)
        fprintf(stderr, "  blk %5u (%7.1f ms) T%u v%u %s %3u vel %u\n", blk, (double)blk * 32000.0 / 44100.0, t + 1u, vi, on ? "ON " : "off", note, vel);
    h_evt = fnv_u32(h_evt, blk);
    h_evt = fnv_u32(h_evt, t << 24 | vi << 16 | on << 15 | note << 8 | vel);
    n_evt++;
}
static void obs_block(uint32_t blk, const int32_t *audio)
{
    uint32_t t, i;
    for (i = 0; i < 2u * CTL; i++)
        h_aud = fnv_u32(h_aud, (uint32_t)audio[i]);
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[t].v[i];
            cv_t *p = &cv_prev[t][i];
            uint8_t on = (uint8_t)(v->active && v->gate);
            if (p->on && (!on || p->note != v->note || p->age != v->age))
                obs_event(blk, t, i, 0, p->note, 0);          /* (an off before the on of a retrigger) */
            if (on && (!p->on || p->note != v->note || p->age != v->age))
                obs_event(blk, t, i, 1, v->note, v->vel);
            p->on = on; p->note = v->note; p->age = v->age;
        }
    while (mo_r != mo_w) {
        h_out = fnv_u32(h_out, blk);
        h_out = fnv_u32(h_out, midi_out_q[mo_r % MQ]);
        n_out++;
        mo_r++;
    }
    n_blk++;
}

/* ------------------------------------------------------------- scene machinery --- */
static int ce_junk, ce_perturb, ce_junk_applied;
static uint32_t ce_retrig(uint32_t r) {
    const char *m=getenv("CE_F2_RETRIG");
    return r & (m ? (uint32_t)strtoul(m,0,0) : 0u);
}
static uint32_t ce_junk_changed;
static int16_t junk_value(uint32_t t, uint32_t id) { return (int16_t)(7 + (int32_t)((id * 13u + t * 29u) % 90u) - (id & 1u ? 40 : 0)); }
static void junk_apply(void)
{
    uint32_t t, id;
    for (t = 0; t < NTRK; t++)
        for (id = P_FM1_ATK; id <= P_FM4_LEVEL; id++)
            trk[t].p[id] = id == 76u ? 0 : id == 77u ? 1 + (t & 1u) : id == 79u ? (t & 1u ? -2 : 2) : junk_value(t, id);
}
static void junk_verify(void)
{
    uint32_t t, id;
    for (t = 0; t < NTRK; t++)
        for (id = P_FM1_ATK; id <= P_FM4_LEVEL; id++)
            ce_junk_changed += trk[t].p[id] != junk_value(t, id);
}

static ce_ev_t E[CE_MAX_EV];
static uint32_t En;
static void ev_add(uint32_t us, uint32_t kind, uint32_t a, uint32_t b, uint32_t c)
{
    if (En >= CE_MAX_EV) { printf("scene: too many events\n"); exit(2); }
    E[En++] = (ce_ev_t){us, (uint8_t)kind, (uint8_t)a, (uint8_t)b, (uint8_t)c};
}
static void u_key(uint32_t ms, uint32_t k, uint32_t dn) { ev_add(ms * 1000u, CE_KEY, k, dn, 0); }
static void u_usb(uint32_t ms, uint32_t st, uint32_t d1, uint32_t d2) { ev_add(ms * 1000u, CE_USB, st, d1, d2); }
static void u_trs(uint32_t ms, uint32_t a, uint32_t b, uint32_t c) { ev_add(ms * 1000u, CE_TRS, a, b, c); }
static void u_trs_rs(uint32_t ms, uint32_t b, uint32_t c) { ev_add(ms * 1000u, CE_TRS_RS, 0, b, c); }
static void run_to(uint32_t ms)               /* queue what the scene added, run until ms */
{
    if (ce_junk && !ce_junk_applied) { junk_apply(); ce_junk_applied = 1; }
    if (getenv("CE_F2_HARM")) harmony_on = 1;
    if (getenv("CE_F2_HARM")) for (uint32_t t = 0; t < NTRK; t++) {
        trk[t].p[P_CSRC] = 1; trk[t].p[P_CEXT] = 0; trk[t].p[P_COCT] = 0;
    }
    if (En) { ce_sched(E, En); En = 0; }
    ce_run_until(ms * 1000u);
}

#define K_F3 0u
#define K_C4 7u
#define K_D4 9u
#define K_E4 11u
#define K_F4 12u
#define K_G4 14u
#define K_A4 16u
#define K_B4 18u
#define K_C5 19u

static void scene_reset(void)                 /* the state of a fresh unit with every track empty and plain */
{
    uint32_t k;
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners); memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target); memset(&midi_clock, 0, sizeof midi_clock);
    memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = mo_r = mo_w = 0; midi_in_overflow = 0; midi_beat_samples = 0;
    for (k = 0; k < NTRK; k++) latch_forget(k);
    latch_req = 0; panic_req = 0;
    fm1_in.notes = kb_prev = 0; fm1_ms = 0; usb.config = 1;
    memset(song.mch, 0, sizeof song.mch); song.g[G_ROUTE] = 0; midi_seen = 0;
    song.sel = 0; song.playing = 0; song.rec = 0; song.master_q12 = 4096;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memset(t->step, 0, sizeof t->step);   /* no pattern: empty steps */
        t->p[P_VOICE] = V_POLY; t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_QUANT] = 0; t->p[P_MUTE] = 0;
        t->p[P_CHRD] = CH_OFF; t->p[P_VOIC] = VC_CLOSE; t->p[P_LATCH] = LT_OFF; t->p[P_AMODE] = 0; t->p[P_AHOLD] = 0;
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2;
    }
    events_block(CTL); events_block(CTL);     /* (settled; not observed) */
    obs_reset();
    ce_reset();
    ce_render = 1; ce_post_block = obs_block;
    En = 0; ce_junk_applied = 0; ce_junk_changed = 0;
}

/* the steps: note step with up to three notes */
static void stp(track_t *t, uint32_t i, uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t flags, uint32_t vel)
{
    t->step[i] = (step_t){{(uint8_t)a, (uint8_t)b, (uint8_t)c}, (uint8_t)n, ST_NOTE, (uint8_t)flags, (uint8_t)vel};
}
static void stp_tie(track_t *t, uint32_t i) { t->step[i] = (step_t){{0}, 0, ST_TIE, 0, 96}; }
static void stp_rest(track_t *t, uint32_t i) { t->step[i] = (step_t){{0}, 0, ST_REST, 0, 96}; }

static void harm(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t dur)
{
    hroot_set(0, (uint8_t)r0); hroot_set(1, (uint8_t)r1); hroot_set(2, (uint8_t)r2); hroot_set(3, (uint8_t)r3);
    hdur_set(0, (uint8_t)dur); hdur_set(1, (uint8_t)dur); hdur_set(2, (uint8_t)dur); hdur_set(3, (uint8_t)dur);
    harmony_on = 1;
}
static void u_play(void) { transport_req = 1; }

/* ======================================================================== scenes === */
/* 1  POLY panel keys: overlapping presses on T1, then T2 */
static void sc_poly_keys(void)
{
    scene_reset();
    trk[0].p[P_VOICE] = V_POLY;
    u_key(0, K_C4, 1); u_key(80, K_E4, 1); u_key(160, K_G4, 1); u_key(300, K_B4, 1);
    u_key(420, K_C4, 0); u_key(470, K_E4, 0); u_key(520, K_G4, 0); u_key(560, K_B4, 0);
    u_key(600, K_D4, 1); u_key(618, K_D4, 0); u_key(620, K_D4, 1); u_key(640, K_D4, 0);       /* a quick re-press */
    run_to(750);
    song.sel = 1;
    trk[1].p[P_VOICE] = V_POLY;
    u_key(760, K_C4, 1); u_key(760, K_G4, 1); u_key(900, K_F4, 1); u_key(1000, K_C4, 0); u_key(1000, K_G4, 0); u_key(1100, K_F4, 0);
    run_to(1500);
}
/* 2  MONO / LEGATO panel keys: last / low / high priority, glide */
static void sc_mono_keys(void)
{
    uint32_t pr;
    scene_reset();
    trk[0].p[P_VOICE] = V_MONO;
    for (pr = 0; pr < 3u; pr++) {                    /* LAST, LOW, HIGH on the same key play */
        uint32_t b = pr * 600u;
        u_key(b, K_E4, 1); u_key(b + 100, K_C4, 1); u_key(b + 200, K_G4, 1); u_key(b + 300, K_G4, 0);
        u_key(b + 380, K_C4, 0); u_key(b + 460, K_E4, 0);
    }
    run_to(200);
    trk[0].p[P_PRIO] = 1;
    run_to(800);
    trk[0].p[P_PRIO] = 2;
    run_to(1400);
    trk[0].p[P_VOICE] = V_LEGATO; trk[0].p[P_PRIO] = 0; trk[0].p[P_GLIDE] = 40;
    u_key(1500, K_C4, 1); u_key(1600, K_G4, 1); u_key(1700, K_E4, 1); u_key(1800, K_E4, 0); u_key(1900, K_G4, 0); u_key(2000, K_C4, 0);
    run_to(2300);
}
/* 3  UNISON panel keys */
static void sc_unison_keys(void)
{
    scene_reset();
    trk[0].p[P_VOICE] = V_UNISON; trk[0].p[P_DETUNE] = 55;
    u_key(0, K_C4, 1); u_key(150, K_G4, 1); u_key(400, K_G4, 0); u_key(500, K_C4, 0);
    u_key(600, K_E4, 1); u_key(900, K_E4, 0);
    run_to(1100);
    song.sel = 2; trk[2].p[P_VOICE] = V_UNISON;
    u_key(1150, K_D4, 1); u_key(1350, K_A4, 1); u_key(1500, K_D4, 0); u_key(1650, K_A4, 0);
    run_to(2000);
}
/* 4  CHRD DIA3 with every VOIC, DIA7 over a held key, a scale change */
static void sc_chrd_dia3_voic(void)
{
    uint32_t v, ms = 0;
    scene_reset();
    trk[0].p[P_CHRD] = CH_DIA3;
    for (v = 0; v < 5u; v++) {
        run_to(ms);
        trk[0].p[P_VOIC] = (int16_t)v;
        u_key(ms, K_C4, 1); u_key(ms + 40, K_E4, 1); u_key(ms + 160, K_C4, 0); u_key(ms + 200, K_E4, 0);
        ms += 260;
    }
    run_to(ms);
    trk[0].p[P_VOIC] = VC_CLOSE;
    u_key(ms, K_G4, 1);
    run_to(ms + 100);
    trk[0].p[P_CHRD] = CH_DIA7;                       /* changed while the key is held: its release ends what it started */
    u_key(ms + 150, K_D4, 1); u_key(ms + 250, K_G4, 0); u_key(ms + 350, K_D4, 0);
    run_to(ms + 450);
    trk[0].p[P_SCALE] = 2; trk[0].p[P_ROOT] = 9; trk[0].p[P_CHRD] = CH_DIA3;         /* A minor */
    u_key(ms + 500, K_C4, 1); u_key(ms + 520, K_E4, 1); u_key(ms + 700, K_C4, 0); u_key(ms + 760, K_E4, 0);
    run_to(ms + 900);
}
/* 5  DIA7, the fixed shapes, MONO plays the root, a key held across a CHRD change */
static void sc_chrd_shapes(void)
{
    uint32_t c, ms = 0;
    scene_reset();
    trk[0].p[P_CHRD] = CH_DIA7;
    for (c = 2; c <= CH_POW; c++) {
        trk[0].p[P_CHRD] = (int16_t)c;
        trk[0].p[P_VOIC] = (int16_t)(c % 5u);
        u_key(ms, K_C4, 1); u_key(ms + 110, K_C4, 0);
        run_to(ms + 150);
        ms += 150;
    }
    trk[0].p[P_VOICE] = V_MONO; trk[0].p[P_CHRD] = CH_MAJ7;
    u_key(ms, K_E4, 1); u_key(ms + 120, K_E4, 0);
    run_to(ms + 160);
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_CHRD] = CH_MIN7;
    u_key(ms + 200, K_G4, 1);
    run_to(ms + 260);
    trk[0].p[P_CHRD] = CH_OFF;
    u_key(ms + 300, K_G4, 0);
    run_to(ms + 420);
    trk[1].p[P_CHRD] = CH_SUS4; song.sel = 1;
    u_key(ms + 450, K_D4, 1); u_key(ms + 450, K_A4, 1); u_key(ms + 600, K_D4, 0); u_key(ms + 650, K_A4, 0);
    run_to(ms + 800);
}
/* 6  CHRD BUS: the HARMONY bus chord follows the bar; RETRIG CHORD */
static void sc_chrd_bus(void)
{
    scene_reset();
    song.g[G_BPM] = 240;
    harm(0, 9, 5, 7, 0);                              /* C Am F G, a bar each (1 s at 240) */
    trk[0].p[P_CHRD] = CH_BUS; link_retrig[0] = ce_retrig(1);
    trk[1].p[P_CHRD] = CH_BUS; link_retrig[1] = 0; trk[1].p[P_VOIC] = VC_OPEN;
    u_play();
    u_key(100, K_C4, 1);
    u_key(1500, K_E4, 1); u_key(1900, K_C4, 0);
    u_key(3300, K_E4, 0);
    run_to(2200);
    song.sel = 1;
    u_key(2300, K_D4, 1); u_key(3500, K_D4, 0);
    run_to(3800);
    song.sel = 0;
    u_key(3900, K_G4, 1);
    run_to(4200);
    transport_req = 2;                                /* STOP: a bus chord ends */
    run_to(4600);
    u_key(4700, K_G4, 0);
    run_to(5000);
}
/* 7  MIDI IN chords: DIA7 and VOIC on a USB part, the release ends what the key started */
static void sc_chrd_midi(void)
{
    scene_reset();
    trk[0].p[P_CHRD] = CH_DIA7;
    u_usb(0, 0x90, 60, 100); u_usb(200, 0x90, 64, 90); u_usb(300, 0x80, 60, 0); u_usb(500, 0x80, 64, 0);
    run_to(600);
    trk[0].p[P_VOIC] = VC_INV1;
    u_usb(620, 0x90, 67, 100);
    run_to(750);
    trk[0].p[P_CHRD] = CH_MIN;                        /* changed while held */
    u_usb(800, 0x80, 67, 0); u_usb(820, 0x90, 67, 110); u_usb(1000, 0x80, 67, 0);
    run_to(1100);
    trk[0].p[P_CHRD] = CH_BUS; harm(0, 9, 5, 7, 0);
    u_usb(1200, 0x90, 65, 100); u_usb(1400, 0x80, 65, 0);
    trk[1].p[P_CHRD] = CH_POW;
    u_usb(1500, 0x91, 48, 100); u_usb(1500, 0x91, 55, 100); u_usb(1700, 0x81, 48, 0); u_usb(1700, 0x81, 55, 0);
    run_to(1900);
}
/* 8  LATCH NEW: keys, chord keys, MIDI IN, the release gesture, LATCH OFF */
static void sc_latch_new(void)
{
    scene_reset();
    trk[0].p[P_LATCH] = LT_NEW;
    u_key(0, K_C4, 1); u_key(100, K_C4, 0);
    u_key(200, K_E4, 1); u_key(250, K_G4, 1); u_key(300, K_E4, 0); u_key(320, K_G4, 0);
    u_key(450, K_D4, 1); u_key(520, K_D4, 0);                                  /* a new press: replaces the set */
    run_to(600);
    trk[0].p[P_CHRD] = CH_DIA3;
    u_key(650, K_C4, 1); u_key(720, K_C4, 0);
    u_key(800, K_F4, 1); u_key(870, K_F4, 0);
    run_to(950);
    latch_req = 1;                                    /* OCT- + OCT+ */
    run_to(1050);
    trk[1].p[P_LATCH] = LT_NEW;
    u_usb(1100, 0x91, 60, 100); u_usb(1180, 0x81, 60, 0); u_usb(1250, 0x91, 64, 100); u_usb(1300, 0x91, 67, 100);
    u_usb(1350, 0x81, 64, 0); u_usb(1400, 0x81, 67, 0);
    run_to(1550);
    trk[1].p[P_LATCH] = LT_OFF;
    run_to(1700);
    trk[0].p[P_LATCH] = LT_NEW; trk[0].p[P_CHRD] = CH_OFF;
    u_key(1750, K_A4, 1); u_key(1800, K_A4, 0);
    run_to(1900);
    panic_req = 1;
    run_to(2000);
}
/* 9  LATCH STACK: toggle on the same key, chords, MONO's stack, CC123 */
static void sc_latch_stack(void)
{
    scene_reset();
    trk[0].p[P_LATCH] = LT_STACK;
    u_key(0, K_C4, 1); u_key(60, K_C4, 0); u_key(120, K_E4, 1); u_key(180, K_E4, 0);
    u_key(250, K_C4, 1); u_key(300, K_C4, 0);                                  /* the same key again: ends C only */
    u_key(380, K_G4, 1); u_key(430, K_G4, 0);
    run_to(520);
    trk[0].p[P_CHRD] = CH_DIA3;
    u_key(560, K_C4, 1); u_key(620, K_C4, 0);
    u_key(700, K_C4, 1); u_key(760, K_C4, 0);                                  /* all latched: ends them */
    run_to(860);
    trk[0].p[P_CHRD] = CH_OFF; trk[0].p[P_VOICE] = V_MONO;
    u_key(900, K_D4, 1); u_key(950, K_D4, 0); u_key(1000, K_F4, 1); u_key(1050, K_F4, 0);
    run_to(1150);
    latch_req = 1;
    run_to(1250);
    trk[1].p[P_LATCH] = LT_STACK;
    u_usb(1300, 0x91, 62, 100); u_usb(1360, 0x81, 62, 0); u_usb(1420, 0x91, 65, 100); u_usb(1480, 0x81, 65, 0);
    u_usb(1560, 0x91, 62, 100); u_usb(1600, 0x81, 62, 0);
    u_usb(1700, 0xB1, 123, 0);
    run_to(1900);
}
/* 10 ARP modes UP DN UPDN RND ORD REPEAT DNUP, octaves 1/2 */
static void sc_arp_modes_a(void)
{
    uint32_t m, ms = 0;
    scene_reset();
    song.g[G_BPM] = 180;
    trk[0].p[P_ARATE] = 2; trk[0].p[P_AGATE] = 64; trk[0].p[P_AMODE] = 1;
    u_key(0, K_C4, 1); u_key(10, K_E4, 1); u_key(20, K_G4, 1); u_key(30, K_B4, 1);
    for (m = 1; m <= 7u; m++) {
        run_to(ms + 40);
        trk[0].p[P_AMODE] = (int16_t)m;
        trk[0].p[P_AOCT] = (int16_t)(1 + (m & 1u));
        trk[0].p[P_AORDER] = (int16_t)(m == 5u);
        ms += 450;
    }
    run_to(ms + 40);
    u_key(ms + 60, K_B4, 0); u_key(ms + 120, K_G4, 0); u_key(ms + 180, K_E4, 0); u_key(ms + 240, K_C4, 0);
    run_to(ms + 400);
}
/* 11 ARP modes UP+8 CONV DIVG PINKY THUMB WALK CHORD TURING, octaves up to 4, with PROB and SWG */
static void sc_arp_modes_b(void)
{
    uint32_t m, ms = 0;
    scene_reset();
    song.g[G_BPM] = 180;
    trk[0].p[P_ARATE] = 2; trk[0].p[P_AGATE] = 80; trk[0].p[P_AMODE] = 8;
    u_key(0, K_C4, 1); u_key(10, K_E4, 1); u_key(20, K_G4, 1);
    for (m = 8; m <= 15u; m++) {
        run_to(ms + 40);
        trk[0].p[P_AMODE] = (int16_t)m;
        trk[0].p[P_AOCT] = (int16_t)(1 + (m % 4u));
        trk[0].p[P_APROB] = (int16_t)(m == 13u ? 90 : 127);
        trk[0].p[P_ASWING] = (int16_t)(m == 11u ? 40 : 0);
        ms += 450;
    }
    run_to(ms + 40);
    u_key(ms + 60, K_G4, 0); u_key(ms + 120, K_E4, 0); u_key(ms + 180, K_C4, 0);
    run_to(ms + 300);
}
/* 12 ARP HOLD, ARP CHORD with CHRD DIA3 keys, harmony on (the ARP follows it on every step), MIDI into the ARP */
static void sc_arp_hold_chord(void)
{
    scene_reset();
    song.g[G_BPM] = 240;
    harm(0, 9, 5, 7, 0);
    u_play();
    trk[0].p[P_ARATE] = 2; trk[0].p[P_AMODE] = 14; trk[0].p[P_AOCT] = 2; trk[0].p[P_AHOLD] = 1;
    u_key(0, K_C4, 1); u_key(20, K_E4, 1); u_key(40, K_G4, 1);
    u_key(300, K_C4, 0); u_key(320, K_E4, 0); u_key(340, K_G4, 0);               /* released: HOLD keeps the arp */
    run_to(1100);
    u_key(1150, K_D4, 1); u_key(1200, K_D4, 0);                                /* a new press replaces the held set */
    run_to(2300);
    trk[0].p[P_CHRD] = CH_DIA3; trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = 0;
    u_key(2350, K_C4, 1);
    run_to(3100);
    u_key(3150, K_C4, 0);
    run_to(3400);
    trk[1].p[P_AMODE] = 3; trk[1].p[P_ARATE] = 3; trk[1].p[P_AOCT] = 3;
    u_usb(3450, 0x91, 60, 100); u_usb(3460, 0x91, 64, 100); u_usb(3470, 0x91, 67, 100);
    u_usb(4000, 0x81, 60, 0); u_usb(4000, 0x81, 64, 0); u_usb(4000, 0x81, 67, 0);
    run_to(4500);
}
/* 13 SEQ with HARMONY on (the strong steps take chord tones): notes, chords, TIE, REST, SLIDE, ACCENT, QNT SEQ, a drum track */
static void sc_seq_harmony(void)
{
    track_t *a = &trk[0], *b = &trk[1], *c = &trk[2], *d = &trk[3];
    uint32_t i;
    scene_reset();
    song.g[G_BPM] = 240;
    harm(0, 9, 5, 7, 0);
    static const uint8_t PAT[16] = {60, 62, 64, 65, 67, 69, 71, 72, 61, 63, 66, 68, 70, 59, 58, 57};
    for (i = 0; i < 16u; i++) stp(a, i, 1, PAT[i], 0, 0, i == 4u ? SF_ACCENT : 0, 100);
    stp_tie(a, 3); stp_rest(a, 6); a->step[10].flags = SF_SLIDE; stp_rest(a, 14);
    stp(b, 0, 3, 48, 52, 55, 0, 90); stp(b, 4, 2, 53, 57, 0, 0, 90); stp(b, 8, 3, 55, 59, 62, 0, 90); stp(b, 12, 1, 50, 0, 0, SF_ACCENT, 110);
    stp(b, 2, 1, 61, 0, 0, 0, 90); stp_tie(b, 6);
    b->p[P_VOICE] = V_POLY;
    c->p[P_QUANT] = QN_SEQ; c->p[P_SCALE] = 1;
    for (i = 0; i < 16u; i += 2u) stp(c, i, 1, 72u + i % 7u, 0, 0, 0, 96);
    d->step[0].hit = 1u << DV_KICK; d->step[4].hit = 1u << DV_SNARE; d->step[8].hit = 1u << DV_KICK;
    d->step[12].hit = 1u << DV_SNARE | 1u << DV_HATC; d->step[2].hit = 1u << DV_HATC; d->step[10].hit = 1u << DV_HATC;
    u_play();
    run_to(4600);
}
/* 14 LINK / RETRIG: a HARMONY bar change and a key change of the source retrigger held CHRD BUS chords */
static void sc_link_retrig(void)
{
    scene_reset();
    song.g[G_BPM] = 240;
    harm(0, 9, 5, 7, 0);
    u_play();
    link_src[1] = 1; link_retrig[1] = ce_retrig(3);                                   /* T2 follows T1's key; CHORD + LINK */
    link_src[2] = 1; link_retrig[2] = ce_retrig(2);                                   /* T3: LINK only */
    link_retrig[0] = ce_retrig(1);
    trk[0].p[P_CHRD] = CH_BUS; trk[1].p[P_CHRD] = CH_BUS; trk[2].p[P_CHRD] = CH_BUS;
    song.sel = 1;
    u_key(100, K_C4, 1); u_key(160, K_G4, 1);
    u_usb(150, 0x92, 62, 100);                                               /* T3 by MIDI */
    run_to(1200);
    trk[0].p[P_ROOT] = 2; key_link_sync();                                  /* T1's key D major: T2, T3 follow */
    run_to(1500);
    trk[0].p[P_SCALE] = 2; key_link_sync();                                 /* D minor */
    run_to(2300);
    harmony_follow[1] = 0;                                                  /* T2 stops following the bus */
    run_to(2800);
    link_retrig[1] = 0; trk[0].p[P_ROOT] = 5; key_link_sync();              /* no retrigger now */
    run_to(3300);
    u_key(3350, K_C4, 0); u_key(3350, K_G4, 0); u_usb(3350, 0x82, 62, 0);
    run_to(3600);
}
/* 15 MIDI IN channel mode (CH1-4): USB and TRS (running status), a drum part, velocities, an ignored channel, bend, CC1 */
static void sc_midi_channels(void)
{
    uint32_t v = ce_perturb ? 101u : 100u;
    scene_reset();
    song.g[G_ROUTE] = 0;
    trk[1].p[P_CHRD] = CH_DIA3;
    u_usb(0, 0x90, 60, v); u_usb(40, 0x91, 64, 90); u_usb(80, 0x92, 67, 80); u_usb(120, 0x93, 36, 110);
    u_usb(140, 0x93, 38, 100); u_usb(160, 0x93, 42, 70);
    u_usb(200, 0x95, 70, 100);                                              /* channel 6: not heard */
    u_usb(250, 0xE0, 0, 96); u_usb(300, 0xB0, 1, 90); u_usb(350, 0xE0, 0, 64);
    u_usb(400, 0x80, 60, 0); u_usb(420, 0x81, 64, 0); u_usb(440, 0x82, 67, 0);
    u_usb(460, 0x83, 36, 0); u_usb(480, 0x83, 38, 0); u_usb(500, 0x83, 42, 0);
    run_to(600);
    u_trs(620, 0x90, 60, 100); u_trs_rs(625, 64, 100); u_trs_rs(630, 67, 100);    /* 90 3C 64 40 64 43 64 */
    u_trs(900, 0x90, 60, 0); u_trs_rs(905, 64, 0); u_trs_rs(910, 67, 0);          /* note-offs as running status velocity 0 */
    u_trs(1000, 0x91, 55, 100); u_trs_rs(1020, 59, 100);
    u_usb(1100, 0x80, 60, 0);                                                 /* a USB off for a TRS note: the same channel owns it */
    u_trs(1300, 0x81, 55, 0); u_trs_rs(1310, 59, 0);
    u_usb(1350, 0x91, 50, 100); u_usb(1350, 0x91, 50, 100); u_usb(1500, 0x81, 50, 0);   /* a repeated note */
    run_to(1700);
    u_trs(1710, 0x90, 62, 100); u_trs(1900, 0x80, 62, 0);
    run_to(2000);
}
/* 16 MIDI IN TRACK routing (each track its channel, OMNI, shared channel), then SEL (the selected track) */
static void sc_midi_track_sel(void)
{
    scene_reset();
    song.g[G_ROUTE] = ROUTE_TRACK;
    mch_set(0, 9); mch_set(1, 9); mch_set(2, MCH_OMNI); mch_set(3, 2);
    u_usb(0, 0x98, 60, 100);                                                 /* channel 9: T1 (first with it) */
    u_usb(50, 0x9C, 64, 100);                                                /* channel 13: OMNI T3 */
    u_usb(100, 0x92, 36, 100);                                               /* channel 3: T4, the drum part */
    u_usb(150, 0x95, 67, 100);                                               /* channel 6: OMNI too */
    u_usb(300, 0x88, 60, 0); u_usb(320, 0x8C, 64, 0); u_usb(340, 0x82, 36, 0); u_usb(360, 0x85, 67, 0);
    run_to(500);
    song.g[G_ROUTE] = ROUTE_SEL;
    song.sel = 1;
    u_usb(550, 0x90, 62, 100); u_usb(600, 0x95, 65, 100);
    run_to(700);
    song.sel = 2;                                                          /* the note-off follows its note-on */
    u_usb(750, 0x80, 62, 0); u_usb(800, 0x90, 69, 100);
    run_to(900);
    song.sel = 0;
    u_usb(950, 0x85, 65, 0); u_usb(1000, 0x80, 69, 0);
    run_to(1100);
    song.g[G_ROUTE] = 0;                                                   /* back to CH1-4: the SEL notes are let go */
    u_usb(1150, 0x90, 72, 100);
    run_to(1250);
    song.g[G_ROUTE] = 2;                                                   /* CH5-8 */
    u_usb(1300, 0x94, 55, 100); u_usb(1300, 0x95, 57, 100); u_usb(1500, 0x84, 55, 0); u_usb(1500, 0x85, 57, 0);
    run_to(1700);
}
/* 17 sustain CC64, CC120, CC123, a preset panic */
static void sc_sustain_panic(void)
{
    scene_reset();
    u_usb(0, 0x90, 60, 100); u_usb(40, 0xB0, 64, 127); u_usb(80, 0x80, 60, 0);       /* held by the pedal */
    u_usb(120, 0x90, 64, 100); u_usb(200, 0x80, 64, 0);
    u_usb(300, 0xB0, 64, 0);                                                      /* pedal up: both end */
    u_usb(400, 0x90, 62, 100); u_usb(430, 0xB0, 64, 127); u_usb(460, 0xB0, 123, 0);   /* all notes off honours the pedal */
    u_usb(520, 0xB0, 64, 0);
    u_usb(560, 0x90, 67, 100); u_usb(580, 0x90, 71, 100); u_usb(600, 0xB0, 64, 127); u_usb(620, 0x80, 67, 0);
    u_usb(700, 0xB0, 120, 0);                                                     /* all sound off */
    run_to(800);
    u_usb(820, 0x91, 60, 100); u_usb(840, 0x93, 38, 100); u_usb(860, 0xB3, 64, 127); u_usb(880, 0x83, 38, 0);
    u_usb(900, 0x81, 60, 0);
    u_usb(1000, 0xB1, 123, 0);
    run_to(1100);
    u_key(1150, K_C4, 1); u_key(1200, K_E4, 1);
    u_usb(1250, 0x90, 70, 100);
    run_to(1350);
    panic_req = 0xFu;                                                           /* a preset load's panic */
    run_to(1450);
    trk[0].p[P_LATCH] = LT_NEW;
    u_key(1500, K_G4, 1); u_key(1550, K_G4, 0);
    u_usb(1600, 0xB0, 123, 0);
    run_to(1700);
    u_usb(1750, 0x90, 66, 100); u_usb(1800, 0xB0, 121, 0);
    run_to(1900);
}
/* 18 FOLLOW: tracks that follow the bus and tracks that do not, switched while playing */
static void sc_follow(void)
{
    uint32_t i;
    scene_reset();
    song.g[G_BPM] = 240;
    harm(0, 9, 5, 7, 0);
    u_play();
    for (i = 0; i < 16u; i += 2u) { stp(&trk[0], i, 1, 61u + (i * 3u) % 11u, 0, 0, 0, 100); stp(&trk[1], i, 1, 61u + (i * 3u) % 11u, 0, 0, 0, 100); }
    harmony_follow[1] = 0;
    trk[2].p[P_AMODE] = AM_CHORD; trk[2].p[P_ARATE] = 2; trk[2].p[P_AOCT] = 2;
    song.sel = 2;
    u_key(100, K_C4, 1); u_key(110, K_E4, 1); u_key(120, K_G4, 1);
    run_to(1500);
    harmony_follow[1] = 1; harmony_follow[2] = 0;
    run_to(2500);
    u_key(2550, K_G4, 0); u_key(2560, K_E4, 0); u_key(2570, K_C4, 0);
    song.sel = 0; trk[0].p[P_AMODE] = 0;
    u_key(2700, K_D4, 1); u_key(2760, K_D4, 0);
    harmony_follow[0] = 0;
    u_key(2900, K_D4, 1); u_key(2960, K_D4, 0);
    u_usb(3000, 0x91, 61, 100); u_usb(3100, 0x81, 61, 0);                         /* MIDI into T2, which follows */
    u_usb(3150, 0x92, 61, 100); u_usb(3250, 0x82, 61, 0);                         /* .. and T3, which does not */
    run_to(3600);
}

typedef struct { const char *name; void (*run)(void); } scene_t;
static const scene_t SCENES[] = {
    {"poly_keys", sc_poly_keys},          {"mono_keys", sc_mono_keys},        {"unison_keys", sc_unison_keys},
    {"chrd_dia3_voic", sc_chrd_dia3_voic}, {"chrd_shapes", sc_chrd_shapes},    {"chrd_bus", sc_chrd_bus},
    {"chrd_midi", sc_chrd_midi},          {"latch_new", sc_latch_new},        {"latch_stack", sc_latch_stack},
    {"arp_modes_a", sc_arp_modes_a},      {"arp_modes_b", sc_arp_modes_b},    {"arp_hold_chord", sc_arp_hold_chord},
    {"seq_harmony", sc_seq_harmony},      {"link_retrig", sc_link_retrig},    {"midi_channels", sc_midi_channels},
    {"midi_track_sel", sc_midi_track_sel}, {"sustain_panic", sc_sustain_panic}, {"follow", sc_follow},
};
#define NSCENE ((int)(sizeof SCENES / sizeof SCENES[0]))
#define PERTURB_SCENE 14                                  /* midi_channels */

/* --------------------------------------------------------------------- process --- */
typedef struct { uint64_t aud, evt, out; uint32_t blocks, nev, nout, junk_changed, ok; } res_t;

static void scene_child(int sc, int junk, int fd)         /* in a forked process: run one scene, hand back the hashes */
{
    res_t r;
    ce_junk = junk;
    ce_trace = !junk && getenv("CE_DIFF_TRACE") && !strcmp(getenv("CE_DIFF_TRACE"), SCENES[sc].name);
    ce_perturb = getenv("CE_DIFF_PERTURB") && atoi(getenv("CE_DIFF_PERTURB")) == 1 && sc == PERTURB_SCENE;
    SCENES[sc].run();
    if (junk) junk_verify();
    h_evt = fnv_u32(h_evt, vage);
    memset(&r, 0, sizeof r);
    r.aud = h_aud; r.evt = h_evt; r.out = h_out; r.blocks = n_blk; r.nev = n_evt; r.nout = n_out;
    r.junk_changed = ce_junk_changed; r.ok = 1;
    if (write(fd, &r, sizeof r) != (ssize_t)sizeof r) _exit(3);
    _exit(0);
}
static int scene_run(int sc, int junk, res_t *r)           /* 0 ok */
{
    int fd[2], st;
    pid_t pid;
    ssize_t n;
    memset(r, 0, sizeof *r);
    fflush(stdout); fflush(stderr);
    if (pipe(fd)) return 1;
    pid = fork();
    if (pid < 0) return 1;
    if (!pid) {
        close(fd[0]);
        scene_child(sc, junk, fd[1]);
    }
    close(fd[1]);
    n = read(fd[0], r, sizeof *r);
    close(fd[0]);
    waitpid(pid, &st, 0);
    if (n != (ssize_t)sizeof *r || !r->ok || !WIFEXITED(st) || WEXITSTATUS(st)) {
        printf("chordext: scene %s (%s) did not finish (status %d)\n", SCENES[sc].name, junk ? "junk" : "clean", st);
        return 1;
    }
    return 0;
}
static void res_line(char *b, size_t n, int sc, const res_t *r)
{
    snprintf(b, n, "%-15s audio=%016llx events=%016llx midiout=%016llx blocks=%u noteevents=%u midiout_packets=%u",
             SCENES[sc].name, (unsigned long long)r->aud, (unsigned long long)r->evt, (unsigned long long)r->out,
             r->blocks, r->nev, r->nout);
}

/* ------------------------------------------------------- the firmware fingerprint --- */
static int fp_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int fp_dir(const char *dir, uint64_t *h)
{
    DIR *d = opendir(dir);
    char *names[512];
    int n = 0, i, bad = 0;
    struct dirent *e;
    if (!d) return 1;
    while ((e = readdir(d)) && n < 512)
        if (e->d_name[0] != '.')
            names[n++] = strdup(e->d_name);
    closedir(d);
    qsort(names, (size_t)n, sizeof *names, fp_cmp);
    for (i = 0; i < n; i++) {
        char path[512];
        struct stat sb;
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (stat(path, &sb)) { bad = 1; continue; }
        if (S_ISDIR(sb.st_mode))
            bad |= fp_dir(path, h);
        else {
            FILE *f = fopen(path, "rb");
            int c;
            const char *p;
            if (!f) { bad = 1; continue; }
            for (p = path; *p; p++) *h = fnv_byte(*h, (uint8_t)*p);
            *h = fnv_byte(*h, 0);
            while ((c = fgetc(f)) != EOF) *h = fnv_byte(*h, (uint8_t)c);
            fclose(f);
        }
        free(names[i]);
    }
    return bad;
}
static int fw_fingerprint(uint64_t *out)
{
    uint64_t h = FNV_OFF;
    int bad = fp_dir("firmware/src", &h);
    bad |= fp_dir("firmware/hal", &h);
    bad |= fp_dir("firmware/engines/src", &h);
    *out = h;
    return bad;
}

/* --------------------------------------------------------------------------- main --- */
static int read_baseline(char lines[][512], int *n)
{
    FILE *f = fopen(CE_BASELINE_PATH, "r");
    char b[1024];
    *n = 0;
    if (!f) return 1;
    while (fgets(b, sizeof b, f)) {
        size_t l = strlen(b);
        while (l && (b[l - 1] == '\n' || b[l - 1] == '\r')) b[--l] = 0;
        if (!l || b[0] == '#') continue;
        if (*n < 64) { strncpy(lines[*n], b, 511); lines[*n][511] = 0; (*n)++; }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "check";
    int verbose = getenv("CE_DIFF_VERBOSE") != 0, bad = 0, sc;
    static char want[64][512];
    int nwant = 0;
    char line[512];
    res_t clean[64], junk[64];
    int do_record = !strcmp(mode, "record"), do_check = !strcmp(mode, "check"), do_run = !strcmp(mode, "run");
    if (!strcmp(mode, "fingerprint")) {            /* the hash `record` compares with CE_BASE_FWFP */
        uint64_t fp;
        int e = fw_fingerprint(&fp);
        printf("firmware-fingerprint: %016llx%s\n", (unsigned long long)fp, e ? " (unreadable files)" : "");
        return e;
    }
    if (!do_record && !do_check && !do_run) {
        printf("usage: chordext_diff_test [check|run|record|fingerprint]\n");
        return 2;
    }
    if (do_record) {
        uint64_t fp;
        FILE *f = fopen(CE_BASELINE_PATH, "r");
        if (f) {
            fclose(f);
            printf("chordext record: %s exists; it is recorded once on the base commit %s and never rewritten\n", CE_BASELINE_PATH, CE_BASE_COMMIT);
            return 2;
        }
        if (fw_fingerprint(&fp) || fp != CE_BASE_FWFP) {
            printf("chordext record: the firmware sources (%016llx) are not those of the base commit %s (%016llx): refused\n",
                   (unsigned long long)fp, CE_BASE_COMMIT, (unsigned long long)CE_BASE_FWFP);
            return 2;
        }
        if (getenv("CE_DIFF_PERTURB")) {
            printf("chordext record: not with CE_DIFF_PERTURB\n");
            return 2;
        }
    }
    {   /* the scheduler's own test (in a process of its own) */
        pid_t pid;
        int st;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int b = 0;
            ui_power_on();
            song.master_q12 = 4096;
            b = ce_selftest();
            fflush(stdout);
            _exit(b ? 1 : 0);
        }
        waitpid(pid, &st, 0);
        printf("chordext: block-driven scheduler self-test %s\n", WIFEXITED(st) && !WEXITSTATUS(st) ? "ok" : "FAIL");
        if (!WIFEXITED(st) || WEXITSTATUS(st)) return 1;
    }
    if (do_check && read_baseline(want, &nwant)) {
        printf("chordext check: %s is missing\n", CE_BASELINE_PATH);
        return 1;
    }
    for (sc = 0; sc < NSCENE; sc++) {
        char lc[512], lj[512];
        int bad_here = 0;
        if (scene_run(sc, 0, &clean[sc]) || scene_run(sc, 1, &junk[sc])) { bad++; continue; }
        res_line(lc, sizeof lc, sc, &clean[sc]);
        res_line(lj, sizeof lj, sc, &junk[sc]);
        if (verbose) {
            printf("  clean  %s\n  junk   %s  (ids 61..80 seen changed by the scene: %u)\n", lc, lj, junk[sc].junk_changed);
        }
        if (strcmp(lc, lj)) {
            printf("chordext: %s: the junk in ids 61..80 CHANGES the result\n   clean %s\n   junk  %s\n", SCENES[sc].name, lc, lj);
            bad_here = 1;
        }
        if (!clean[sc].nev || !clean[sc].blocks) {
            printf("chordext: %s: the scene sounds nothing (no note events)\n", SCENES[sc].name);
            bad_here = 1;
        }
        if (do_check) {
            int k, found = 0;
            for (k = 0; k < nwant; k++)
                if (!strncmp(want[k], SCENES[sc].name, strlen(SCENES[sc].name)) && want[k][strlen(SCENES[sc].name)] == ' ') {
                    found = 1;
                    if (strcmp(want[k], lc)) {
                        printf("chordext: %s DIFFERS from the baseline\n   baseline %s\n   now      %s\n", SCENES[sc].name, want[k], lc);
                        bad_here = 1;
                    }
                }
            if (!found) {
                printf("chordext: %s is not in the baseline\n", SCENES[sc].name);
                bad_here = 1;
            }
        }
        if (do_run) printf("%s\n", lc);
        printf("chordext: %-15s clean == junk%s  %s\n", SCENES[sc].name, do_check ? " == baseline" : "", bad_here ? "FAIL" : "ok");
        bad += bad_here;
    }
    if (do_check && nwant != NSCENE) {
        printf("chordext: the baseline has %d scenes, the test %d\n", nwant, NSCENE);
        bad++;
    }
    if (do_record && !bad) {
        FILE *f = fopen(CE_BASELINE_PATH, "w");
        uint64_t fp;
        if (!f || fw_fingerprint(&fp)) { printf("chordext record: cannot write %s\n", CE_BASELINE_PATH); return 2; }
        fprintf(f, "# chordext_baseline v1: tests/chordext_diff_test.c (CHORD SOURCE / EXT step 0). RECORDED ONCE, on the base commit %s,\n"
                   "# in the CI container image (linux/amd64, tools/docker_validate.sh's caravel-validate). Never rewritten: there is no update flag.\n"
                   "# A scene line is: name, FNV-1a 64 of the audio, of the note events, of the MIDI OUT packets, then the counts.\n"
                   "# base-commit: %s\n# firmware-fingerprint: %016llx\n", CE_BASE_COMMIT, CE_BASE_COMMIT, (unsigned long long)fp);
        for (sc = 0; sc < NSCENE; sc++) {
            res_line(line, sizeof line, sc, &clean[sc]);
            fprintf(f, "%s\n", line);
        }
        fclose(f);
        printf("chordext record: wrote %s (%d scenes)\n", CE_BASELINE_PATH, NSCENE);
    }
    if (bad) {
        printf("chordext diff test FAILED (%d)\n", bad);
        return 1;
    }
    printf("chordext diff test passed (%d scenes%s)\n", NSCENE, do_check ? ", every one equal to the baseline" : "");
    return 0;
}
