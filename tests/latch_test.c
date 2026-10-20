/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* LATCH (firmware/src/seq.c latch_*, params P_LATCH on the CHORD page) against the real keyboard, MIDI IN, arp, recording and
 * voice code: OFF plays as before; NEW and STACK keep the notes of keys, chord keys and MIDI notes after their release;
 * NEW replaces the set with a press when nothing is held; STACK adds and a press of notes that all sound latched ends them
 * (its release does nothing); the arp keeps its held list; LATCH OFF, ARP on, a panic, STOP and the OCT- + OCT+ gesture
 * (latch_req) end it; MONO keeps its stack; the recording sees only the key; MIDI OUT stays the key's; a kit takes no latch;
 * stored values other than 1 / 2 count as OFF; a sound load keeps LATCH; the parameter layout is unchanged.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void reset(void)
{
    uint32_t k;
    ui_power_on();
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord); memset(chord_last, 0, sizeof chord_last);
    for (k = 0; k < NTRK; k++) latch_forget(k);
    latch_req = 0;
    fm1_in.notes = kb_prev = 0; mo_w = mo_r = 0; usb.config = 1;
    song.sel = 0; song.playing = 0; song.rec = 0;
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SCALE] = 1; trk[0].p[P_ROOT] = 0;   /* C major, POLY */
    events_block(CTL);
}
static void blk(void) { events_block(CTL); }
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_enqueue(st >> 4 | st << 8 | d1 << 16 | d2 << 24, 1);
    events_block(CTL);
}
#define K_C4 7u                                   /* keys from F3 (0): C4 7 (note 60), D4 9, E4 11, G4 14 */
#define K_D4 9u
#define K_E4 11u
#define K_G4 14u

static int keys_new(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    key_down(K_C4); key_up(K_C4);
    bad += check("OFF: the note ends with its key (no latch)", !ngated(t));
    t->p[P_LATCH] = LT_NEW;
    key_down(K_C4);
    bad += check("NEW: the key sounds", gate_note(t, 60) && ngated(t) == 1u);
    key_up(K_C4);
    bad += check("  key up: the note sounds on", gate_note(t, 60) && ngated(t) == 1u);
    key_down(K_E4);
    bad += check("  a new press (nothing held) replaces the set", !gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 1u);
    key_down(K_G4);
    bad += check("  a second key while one is held adds", gate_note(t, 64) && gate_note(t, 67) && ngated(t) == 2u);
    key_up(K_E4); key_up(K_G4);
    bad += check("  both up: both sound on", gate_note(t, 64) && gate_note(t, 67) && ngated(t) == 2u);
    key_down(K_G4);                                                 /* the same note again: not a toggle in NEW */
    bad += check("  the same key again (NEW) replaces the set with it (retriggered)", gate_note(t, 67) && !gate_note(t, 64) &&
                 ngated(t) == 1u);
    key_up(K_G4);
    t->p[P_CHRD] = CH_DIA3;
    key_down(K_C4); key_up(K_C4);
    bad += check("  a chord key latches its three notes (C E G), the old set gone",
                 gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && ngated(t) == 3u);
    key_down(K_D4); key_up(K_D4);
    bad += check("  the next chord replaces it (D F A)", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) && ngated(t) == 3u);
    bad += check("  MIDI OUT stays the keys' own (a note-on and note-off per tone)", mo_w >= 10u);
    t->p[P_LATCH] = LT_OFF;
    blk();
    bad += check("LATCH OFF ends the latched notes", !ngated(t) && !lat_any(lat_stay[0]));
    return bad;
}

static int keys_stack(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    t->p[P_LATCH] = LT_STACK;
    key_down(K_C4); key_up(K_C4); key_down(K_E4); key_up(K_E4);
    bad += check("STACK: notes join (C, E)", gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 2u);
    key_down(K_C4);
    bad += check("  the same key again ends only its note (C), starts nothing", !gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 1u);
    key_up(K_C4);
    bad += check("  its release does nothing (E sounds, C stays off)", !gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 1u);
    key_down(K_C4); key_up(K_C4);
    bad += check("  and C joins again", gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 2u);
    t->p[P_CHRD] = CH_DIA3;
    key_down(K_C4); key_up(K_C4);                                   /* C E G: C and E latched, G not: adds G, no toggle */
    bad += check("  a chord with notes not latched adds them (C E G)", gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) &&
                 ngated(t) == 3u);
    key_down(K_C4);
    bad += check("  the chord again (all latched) ends all three", !ngated(t));
    key_up(K_C4);
    bad += check("  its release: still silent", !ngated(t));
    key_down(K_C4); key_up(K_C4);
    bad += check("  and it latches again", ngated(t) == 3u);
    latch_req = 1; blk();
    bad += check("the release gesture ends everything", !ngated(t) && !lat_any(lat_stay[0]));
    key_down(K_E4);
    bad += check("  LATCH stays on: the next key latches again", gate_note(t, 64) && ngated(t) == 3u);       /* (chord E G B) */
    key_up(K_E4);
    bad += check("  (the chord E G B stays)", ngated(t) == 3u);
    t->p[P_LATCH] = LT_OFF;
    key_down(K_C4);
    t->p[P_LATCH] = LT_STACK;                                       /* switched on while a key is held: it latches at release */
    bad += check("LATCH off -> held key", gate_note(t, 60));
    key_up(K_C4);
    bad += check("  switched on while held: latched at its release", gate_note(t, 60));
    t->p[P_LATCH] = LT_OFF;
    key_down(K_G4);
    blk();
    bad += check("  LATCH OFF with a key held: the latched notes end, the held key stays", !gate_note(t, 60) && gate_note(t, 67));
    key_up(K_G4);
    bad += check("  and it ends with its key", !ngated(t));
    return bad;
}

static int voices_mono_panic_stop(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    t->p[P_LATCH] = LT_STACK; t->p[P_VOICE] = V_MONO;
    key_down(K_C4); key_up(K_C4); key_down(K_E4); key_up(K_E4);
    bad += check("MONO + STACK: the last note plays (E), the stack keeps both", t->mono_note == 64 && t->nmono == 2u);
    key_down(K_E4);
    bad += check("  E again ends it: back to C", t->mono_note == 60 && t->nmono == 1u);
    key_up(K_E4);
    bad += check("  its release does nothing", t->mono_note == 60 && t->nmono == 1u);
    t->p[P_VOICE] = V_POLY;
    panic_req = 1; blk();
    bad += check("a panic (preset / engine / voice change) ends the notes, LATCH stays on", !ngated(t) && !lat_any(lat_stay[0]) &&
                 t->p[P_LATCH] == LT_STACK);
    key_down(K_C4); key_up(K_C4);
    bad += check("  and the next key latches again", gate_note(t, 60));
    seq_stop();
    bad += check("STOP ends the latch", !ngated(t) && !lat_any(lat_stay[0]));
    t->p[P_LATCH] = LT_OFF;
    key_down(K_C4); key_up(K_C4);
    bad += check("OFF again: plain", !ngated(t));
    /* voice stealing: a latched note that no longer sounds is no toggle */
    t->p[P_LATCH] = LT_STACK;
    key_down(K_C4); key_up(K_C4);
    t->v[0].gate = 0; t->v[0].stage = 3;                           /* (the voice went away: a steal) */
    key_down(K_C4);
    bad += check("a latched note that stopped sounding is not toggled: the press starts it", gate_note(t, 60));
    key_up(K_C4);
    return bad;
}

static int arp_latch(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    t->p[P_AMODE] = 1; t->p[P_LATCH] = LT_NEW;
    blk();
    key_down(K_C4); key_up(K_C4);
    bad += check("ARP + NEW: the held list stays after the key (as ARP HOLD)", t->nheld == 1u && t->held[0] == 60);
    key_down(K_E4); key_up(K_E4);
    bad += check("  a new press replaces the chord", t->nheld == 1u && t->held[0] == 64);
    t->p[P_LATCH] = LT_STACK;
    blk();
    key_down(K_G4); key_up(K_G4);
    bad += check("ARP + STACK: a new press adds", t->nheld == 2u && t->held[0] == 64 && t->held[1] == 67);
    key_down(K_G4);
    bad += check("  the same key again takes its note out", t->nheld == 1u && t->held[0] == 64);
    key_up(K_G4);
    bad += check("  its release does nothing", t->nheld == 1u && t->held[0] == 64 && !t->arp_phys);
    latch_req = 1; blk();
    bad += check("  the release gesture empties the list", !t->nheld);
    key_down(K_C4); key_up(K_C4);
    t->p[P_LATCH] = LT_OFF;
    blk();
    bad += check("LATCH OFF with no key down drops the chord", !t->nheld);
    t->p[P_LATCH] = LT_NEW; t->p[P_AMODE] = 0;
    blk();
    key_down(K_C4); key_up(K_C4);
    bad += check("(no ARP: the note sounds on)", gate_note(t, 60));
    t->p[P_AMODE] = 1;
    blk();
    bad += check("ARP turned on ends the latched notes", !ngated(t) && !lat_any(lat_stay[0]));
    t->p[P_AHOLD] = 1; t->p[P_LATCH] = LT_OFF;
    key_down(K_C4); key_up(K_C4);
    bad += check("ARP HOLD alone: unchanged (the chord stays)", t->nheld == 1u);
    key_down(K_E4); key_up(K_E4);
    bad += check("  and replaces on a new press", t->nheld == 1u && t->held[0] == 64);
    t->p[P_LATCH] = LT_NEW;
    key_down(K_G4); key_up(K_G4);
    seq_stop();
    bad += check("STOP with LATCH on empties the arp's chord", !t->nheld);
    t->p[P_LATCH] = LT_OFF;
    key_down(K_G4); key_up(K_G4);
    seq_stop();
    bad += check("  ARP HOLD alone keeps it over STOP (as before)", t->nheld == 1u);
    return bad;
}

static int midi_latch(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    t->p[P_LATCH] = LT_NEW;
    midi(0x90, 60, 100); midi(0x80, 60, 0);
    bad += check("MIDI IN + NEW: the note sounds after its note-off", gate_note(t, 60));
    midi(0x90, 64, 100);
    bad += check("  a new note replaces the set", !gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 1u);
    midi(0x90, 67, 100);
    bad += check("  with a note held, the next adds", gate_note(t, 64) && gate_note(t, 67));
    midi(0x80, 64, 0); midi(0x80, 67, 0);
    bad += check("  both notes sound on", ngated(t) == 2u);
    t->p[P_LATCH] = LT_STACK;
    midi(0x90, 64, 100);
    bad += check("MIDI IN + STACK: the note again ends it", !gate_note(t, 64) && gate_note(t, 67) && ngated(t) == 1u);
    midi(0x80, 64, 0);
    bad += check("  its note-off does nothing", gate_note(t, 67) && ngated(t) == 1u);
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 60, 100); midi(0x80, 60, 0);
    bad += check("  a chord (C E G) joins", gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && ngated(t) == 3u);
    midi(0x90, 60, 100);
    bad += check("  the chord again ends its notes (G too: it was the C chord's)", ngated(t) == 0u);
    midi(0x80, 60, 0);
    bad += check("  chord table empty, nothing sounds", !ngated(t) && !mchord[0].id);
    t->p[P_CHRD] = CH_OFF;
    midi(0x90, 62, 100); midi(0x80, 62, 0);
    bad += check("  (a latched note sounds)", ngated(t) == 1u);
    midi(0xB0, 123, 0);
    bad += check("All Notes Off ends the latched note too", !ngated(t) && !lat_any(lat_stay[0]));
    midi(0x90, 62, 100); midi(0x80, 62, 0);
    midi(0xB0, 120, 0);
    bad += check("All Sound Off ends everything", !ngated(t) && !lat_any(lat_stay[0]));
    return bad;
}

static int gesture(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    t->p[P_LATCH] = LT_NEW;
    key_down(K_C4); key_up(K_C4);
    song.octave = 2;
    btn_down(B_OCTDN); frame();
    btn_down(B_OCTUP); frame();                                     /* both down: the octave resets, the latch is released */
    btn_up(B_OCTDN); btn_up(B_OCTUP); frame();
    bad += check("OCT- + OCT+ together: the octave is 0 as before", song.octave == 0);
    blk();
    bad += check("  and the latched notes end", !ngated(t) && !lat_any(lat_stay[0]));
    key_down(K_E4); key_up(K_E4);
    song.octave = 1;
    btn_down(B_OCTUP); frame(); btn_up(B_OCTUP); frame();
    blk();
    bad += check("  OCT+ alone shifts the octave and releases nothing", song.octave == 2 && gate_note(t, 64));
    song.octave = 0;
    t->p[P_LATCH] = LT_OFF;
    blk();
    return bad;
}

static int rec_and_kits(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i;
    reset();
    for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    t->p[P_LATCH] = LT_NEW;
    song.rec = 1; song.playing = 1;
    t->seq_idx = 0; t->seq_pos = 0;
    key_down(K_C4);
    bad += check("recording: the pressed key is written", t->step[0].n == 1u && t->step[0].note[0] == 60);
    key_up(K_C4);
    bad += check("  the recorded hold ends with the key, the sound goes on", t->rh_n == 0u && gate_note(t, 60));
    song.rec = 0; song.playing = 0;
    reset();
    song.sel = 3;                                                  /* the DRUM track: no latch */
    trk[3].p[P_LATCH] = LT_NEW;
    key_down(K_C4);
    bad += check("a kit has no latch mode", latch_mode(&trk[3]) == LT_OFF);
    key_up(K_C4);
    song.sel = 0;
    for (i = 0; i < 2u; i++) {                                     /* stored values other than 1, 2 are OFF */
        trk[0].p[P_LATCH] = i ? -40 : 40;
        bad += check("a legacy value (40, -40) counts as OFF", latch_mode(&trk[0]) == LT_OFF);
    }
    return bad;
}

static int params(void)
{
    int bad = 0;
    page_t pg;
    uint32_t i, on = 0;
    reset();
    bad += check("layout: P_LATCH is P_ED_FX (0.2.0), P_COUNT 114 and P_E0 106 with 0.3.0's FILTER", P_LATCH == P_ED_FX && P_COUNT == 114 && P_E0 == 106);
    bad += check("descriptor: LATCH, OFF NEW STACK, default OFF",
                 str_eq(TP[P_LATCH].label, "LATCH") && TP[P_LATCH].min == 0 && TP[P_LATCH].max == 2 && TP[P_LATCH].def == 0 &&
                 str_eq(TP[P_LATCH].names[0], "OFF") && str_eq(TP[P_LATCH].names[1], "NEW") && str_eq(TP[P_LATCH].names[2], "STACK"));
    for (i = 0; i < NELEM(PAGES); i++)
        if (str_eq(PAGES[i].title, "CHORD")) { pg = PAGES[i]; on = pg.id[0] == P_CHRD && pg.id[1] == P_VOIC && pg.id[2] == P_LATCH; }
    bad += check("the CHORD page: CHRD VOIC LATCH", on);
    trk[0].p[P_LATCH] = LT_STACK;
    apply_preset_to(&trk[0], 1);
    bad += check("a sound load keeps LATCH (it is the track's setting)", trk[0].p[P_LATCH] == LT_STACK);
    trk[0].p[P_LATCH] = LT_OFF;
    return bad;
}

int main(void)
{
    int bad = keys_new() + keys_stack() + voices_mono_panic_stop() + arp_latch() + midi_latch() + gesture() + rec_and_kits() + params();
    printf("%s\n", bad ? "LATCH TEST FAILED" : "latch test passed");
    return bad != 0;
}
