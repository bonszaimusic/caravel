/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.6, on the real UI sources (with the stubs of tests/ui_test.c):
 * CUSTOM (#244, Discussion #239): the palette of the user's as COLOR's 11th value (the menu steps it, a built-in palette leaves
 *   it stored, none stored = 10 values), every screen drawn in a light, a dark and an edge-of-the-rule palette, nothing else
 *   of the settings touched (the built-in palette stays settings.palette). The editor's side: editor_test.c custom_palette
 * Discussion #238 (#242): SEQ's piano roll follows the play position:
 *   follows    a melodic pattern longer than 16 steps, the transport playing: the roll's bank is the playhead's;
 *              STEP and DETAIL alike; the cursor, the steps and the pattern length are not touched
 *   leaves     a 16-step pattern, the transport stopped, DRUM's grid, SEQ > AUTOMATION, HOME: the bank stays
 *   holds      KNOB 1 (STEP) or PRESETS (DETAIL) walking the cursor, an edit of the cursor's step (KNOB 2..4, DETAIL's
 *              four), step entry: the roll stays on the cursor's bank
 *   resumes    after 3 s without that, or once the playhead has gone one pass round the pattern, whichever is first
 *   resets     the transport stopped, another track
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* track 1 alone, `len` steps of 1/16, a note on every fourth step; the transport "playing" with the playhead at idx
 * (the sequencer itself is not run: only the UI's view of it), SEQ > STEP, the cursor on step `cur` */
static track_t *pattern(uint32_t len, uint32_t cur, uint32_t idx)
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_VOICE] = V_POLY;
    for (i = 0; i < len; i += 4)
        t->step[i] = (step_t){{(uint8_t)(48 + i)}, 1, ST_NOTE, 0, 0};
    go_page(GR_ROLL);
    ui.cursor = (uint8_t)cur;
    ui.bank = (uint8_t)(cur / 16u);
    song.playing = 1;
    t->seq_idx = (uint16_t)idx;
    frame();
    return t;
}
static void go_detail(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "DETAIL"); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    frame();
}
static void kt(uint32_t role, int32_t s)             /* a knob turned by s detents, unaccelerated */
{
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    turn(role, s);
}
/* the playhead at idx, one UI frame later */
static void at(track_t *t, uint32_t idx)
{
    t->seq_idx = (uint16_t)idx;
    frame();
}

static int follows(void)
{
    int bad = 0, ok;
    track_t *t = pattern(40, 0, 0);
    step_t ref[NSTEP];
    memcpy(ref, t->step, sizeof ref);
    ok = ui.bank == 0u;
    at(t, 20);
    ok &= ui.bank == 1u;
    at(t, 36);
    ok &= ui.bank == 2u;
    at(t, 5);
    ok &= ui.bank == 0u;
    bad += check("SEQ > STEP, 40 steps, playing: the roll's bank is the playhead's (0, 1, 2, 0)", ok);
    ok = ui.cursor == 0u && t->p[P_SLEN] == 40 && !memcmp(ref, t->step, sizeof ref);
    at(t, 36);
    ok &= ui.cursor == 0u && !memcmp(ref, t->step, sizeof ref);
    bad += check("  the cursor, the steps and the length are not touched", ok);
    go_detail();
    at(t, 20);
    ok = ui.bank == 1u && detail_on();
    at(t, 3);
    ok &= ui.bank == 0u;
    bad += check("  SEQ > DETAIL follows the same way", ok);
    at(t, 39);
    ok = ui.bank == 2u;
    bad += check("  a last bank of 8 steps (steps 33..40) is followed too", ok);
    return bad;
}

static int leaves(void)
{
    int bad = 0, ok;
    track_t *t = pattern(16, 0, 0);
    at(t, 9);
    ok = ui.bank == 0u;
    bad += check("a 16-step pattern: nothing to follow (bank 0)", ok);
    t = pattern(40, 0, 0);
    song.playing = 0;
    at(t, 36);
    ok = ui.bank == 0u;
    bad += check("the transport stopped: the playhead does not move the roll", ok);
    t = pattern(40, 0, 0);
    go_page(GR_EVENTS);
    at(t, 36);
    ok = ui.bank == 0u;
    bad += check("SEQ > AUTOMATION (not the roll): the bank stays", ok);
    t = pattern(40, 0, 0);
    ui.home = 1;
    at(t, 36);
    ok = ui.bank == 0u;
    bad += check("HOME: the bank stays", ok);
    ui_power_on();
    song.sel = 3;
    t = &trk[3];
    track_defaults_steps(t);
    t->p[P_SLEN] = 40;
    go_page(GR_ROLL);
    ui.cursor = 0;
    ui.bank = 0;
    song.playing = 1;
    ok = drum_track(t);
    at(t, 36);
    ok &= ui.bank == 0u;
    bad += check("a DRUM track (its grid): the bank stays, the keys keep their steps", ok);
    return bad;
}

static int holds(void)
{
    int bad = 0, ok;
    track_t *t = pattern(40, 0, 0);
    at(t, 20);
    ok = ui.bank == 1u;
    kt(EN_K1, 1);                                     /* the cursor to step 2: the bank is its own (0) */
    ok &= ui.cursor == 1u && ui.bank == 0u;
    at(t, 36);
    ok &= ui.bank == 0u;
    at(t, 37);
    ok &= ui.bank == 0u;
    bad += check("STEP: KNOB 1 walks the cursor: the roll stays on the cursor's bank while the playhead moves on", ok);
    fm1_ms += 3100u;
    frame();
    ok = ui.bank == 2u;
    bad += check("  3 s without a touch: following again (bank 2)", ok);
    t = pattern(40, 0, 0);
    ui.cursor = 33;
    ui.bank = 2;
    kt(EN_K1, 1);                                     /* step 34 (bank 2), playhead on 0: held */
    ok = ui.cursor == 34u && ui.bank == 2u;
    {
        uint32_t i;
        for (i = 1; i < 40u; i++) {                   /* a pass: 39 steps gone, still held (bank 2 though the playhead is in 0..2) */
            at(t, i);
            if (i < 39u)
                ok &= ui.bank == 2u;
        }
        ok &= ui.bank == 2u;
        at(t, 0);                                     /* the 40th step: one pass round */
        ok &= ui.bank == 0u;
    }
    bad += check("  one pass round the pattern (well under 3 s): following again", ok);
    t = pattern(40, 0, 0);
    at(t, 20);
    ok = ui.bank == 1u && ui.cursor == 0u;
    kt(EN_K2, 1);                                     /* NOTE of the cursor's step */
    ok &= ui.bank == 0u && ui.cursor == 0u;
    at(t, 21);
    ok &= ui.bank == 0u;
    bad += check("KNOB 2 (the cursor's step edited): the roll goes back to the cursor and stays", ok);
    t = pattern(40, 0, 0);
    at(t, 20);
    ui.entry_open = 1;
    frame();
    ok = ui.bank == 0u;
    at(t, 36);
    ok &= ui.bank == 0u;
    bad += check("step entry has the keys: the roll stays on the cursor", ok);
    ui.entry_open = 0;
    t = pattern(40, 0, 0);
    go_detail();
    at(t, 20);
    ok = ui.bank == 1u;
    kt(EN_K2, 1);                                     /* DETAIL: RATCH of the cursor's step */
    ok &= ui.bank == 0u;
    at(t, 36);
    ok &= ui.bank == 0u;
    bad += check("DETAIL: KNOB 2 (a field of the cursor's step) holds the roll too", ok);
    return bad;
}

static int resets(void)
{
    int bad = 0, ok;
    track_t *t = pattern(40, 0, 0);
    kt(EN_K1, 1);
    song.playing = 0;                                 /* held, then stopped ... */
    at(t, 3);
    song.playing = 1;                                 /* ... and played again: following at once */
    at(t, 36);
    ok = ui.bank == 2u;
    bad += check("stop, play: the hold is gone (the roll follows at once)", ok);
    t = pattern(40, 0, 0);
    kt(EN_K1, 1);
    song.playing = 1;
    track_select(1);
    trk[1] = *t;
    trk[1].seq_idx = 36;
    frame();
    ok = ui.bank == 2u;
    bad += check("another track: the hold is gone (the roll follows that track's playhead)", ok);
    return bad;
}

/* ---- CUSTOM, the user's palette (#244) ---- */
static uint16_t rgb(uint32_t r, uint32_t g, uint32_t b) { return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3); }
static void custom_store(const uint16_t *c, int use)
{
    cpal_store(c);
    if (use) palette_choose(NPALETTES);
}
static int screens_lit(const char *what)          /* every page, HOME, the menu: drawn, ink on the screen, the screen not flat */
{
    uint32_t i, x, y, bad = 0, ink;
    for (i = 0; i < NPAGES; i++) {
        uint16_t bg;
        ui.home = 0; ui.menu = 0; ui.page = (uint8_t)i;
        page_entered();
        ui.force = 1;
        frame();
        bg = host_screen[0];
        ink = 0;
        for (y = 0; y < 240u; y += 2u)
            for (x = 0; x < 240u; x += 2u)
                ink += host_screen[y * 240u + x] != bg;
        if (ink < 200u) { printf("  %s: page %u (%s) is flat (%u of 14400 samples differ from the corner)\n", what, i, PAGES[i].title, ink); bad++; }
    }
    ui.home = 1; ui.force = 1; frame();
    ink = 0;
    for (y = 0; y < 240u; y += 2u) for (x = 0; x < 240u; x += 2u) ink += host_screen[y * 240u + x] != host_screen[0];
    if (ink < 200u) bad++;
    return !bad;
}
static int custom_palette_ui(void)
{
    int bad = 0, ok;
    /* Exercise the menu callback after each palette mutation. GCC 14 can otherwise
     * reuse the earlier count across cpal_store in this single-translation-unit host fixture. */
    uint32_t (*volatile color_values)(uint32_t) = menu_n;
    const uint16_t dark[5] = {rgb(8, 16, 40), rgb(24, 32, 64), rgb(240, 240, 255), rgb(120, 200, 255), rgb(255, 160, 40)};
    const uint16_t light[5] = {rgb(250, 246, 235), rgb(230, 224, 208), rgb(20, 24, 40), rgb(30, 90, 160), rgb(200, 60, 40)};
    const uint16_t edge[5] = {rgb(0, 0, 0), rgb(36, 36, 36), rgb(136, 136, 136), rgb(88, 88, 88), rgb(70, 70, 70)};
    ui_power_on();
    ok = color_values(MI_COLOR) == NPALETTES && !(ui_cpal_flags & CPAL_STORED);
    palette_choose(NPALETTES);                       /* (none stored: nothing changes) */
    ok &= palette_cur() == settings.palette && !cpal_on();
    bad += check("CUSTOM: none stored, COLOR has the 10 built-in values (CUSTOM does nothing)", ok);
    palette_choose(4);
    custom_store(dark, 0);
    ok = color_values(MI_COLOR) == NPALETTES + 1u && menu_get(MI_COLOR) == 4u && !strcmp(menu_vname(MI_COLOR, NPALETTES), "CUSTOM") &&
         cpal_on() == 0u;
    bad += check("CUSTOM: stored (not in use): COLOR has 11 values, the built-in 4 still in use, CUSTOM named last", ok);
    palette_choose(NPALETTES);
    ok = menu_get(MI_COLOR) == NPALETTES && T_BG == dark[0] && T_TEXT == dark[2] && settings.palette == 4u;
    bad += check("  CUSTOM chosen: its colours are the palette (BG TEXT), settings.palette stays 4 (what older firmware shows)", ok);
    ok = menu_step(MI_COLOR, 1) == 0u && menu_step(MI_COLOR, -1) == NPALETTES - 1u;
    palette_choose(0);
    ok &= menu_step(MI_COLOR, -1) == NPALETTES && menu_step(MI_COLOR, 1) == 1u;
    bad += check("  the menu steps COLOR through CUSTOM: past the last built-in one, round to the first", ok);
    {
        persist_t p;
        palette_choose(NPALETTES);
        settings_export(&p);
        ok = p.palette == palette_to_stored(0u) && p.favorites.factory[15][12] == 3u && palette_from_stored(p.palette) == 0u;
        bad += check("  saved: persist_t.palette is the built-in palette (the base), the colours and flags in favorites", ok);
    }
    ok = screens_lit("dark");
    bad += check("every page and HOME drawn in a dark CUSTOM palette (ink on the screen)", ok);
    cpal_store(light);
    palette_apply();
    ok = T_BG == light[0] && ux.light && screens_lit("light");
    bad += check("  in a light one (the REC red for light backgrounds)", ok);
    cpal_store(edge);
    palette_apply();
    ok = screens_lit("edge");
    bad += check("  in one on the edge of the readable rule (the editor's: TEXT / BG 128 ..)", ok);
    {   /* colours nobody can read are the user's to choose: drawn without a fault, and the way back stays (COLOR steps to a built-in palette) */
        const uint16_t same[5] = {rgb(90, 90, 90), rgb(90, 90, 90), rgb(90, 90, 90), rgb(90, 90, 90), rgb(90, 90, 90)};
        const uint16_t white[5] = {rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255)};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            cpal_store(k ? white : same);
            palette_choose(NPALETTES);
            ui.home = 0; ui.menu = 0; ui.page = 0; page_entered(); ui.force = 1;
            frame(); frame();
            ui.menu = 1; ui.force = 1; frame();            /* the menu drawn too */
            ui.menu = 0;
        }
        ok = cpal_on() && menu_step(MI_COLOR, 1) == 0u;
        palette_choose(menu_step(MI_COLOR, 1));
        ok &= !cpal_on() && T_BG == UI_PALETTES[0].bg;
        bad += check("  colours nobody can read (one grey, all white): drawn without a fault, COLOR steps back to a built-in palette", ok);
    }
    palette_choose(0);
    return bad;
}

int main(void)
{
    int bad = follows() + leaves() + holds() + resets() + custom_palette_ui();
    printf("%s\n", bad ? "1.6 TEST FAILED" : "1.6 tests passed");
    return bad != 0;
}
