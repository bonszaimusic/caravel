/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.5.1, Discussion #199: SEQ > DETAIL, the step page between STEP and AUTOMATION, on the real sequencer and UI
 * sources (with the stubs of tests/ui_test.c):
 *   the page   SEQ goes STEP -> DETAIL -> AUTOMATION -> STEP; the cursor is STEP's
 *   knobs      KNOB 1 CHANCE, 2 RATCH, 3 NUDGE, 4 VEL of the cursor step, PRESETS walks the cursor (KNOB 1 does not),
 *              nothing else of the step changes; a REST / an empty step takes none (NO NOTE), a TIE its CHANCE only
 *   AUTOMATION the same fields: a value set here is its row there, back at its default the row is gone; EDIT there
 *              still deletes a row (the field back to its default)
 *   EDIT       the step's four back to 100 % / x1 / 0 / 96, its notes kept
 *   undo       one per knob gesture on one step (walking to the next step starts another), EDIT one
 *   DRUM       the white keys pick the step (no hit toggled), held together several; the key LEDs the steps with hits
 *   chords     one value for the whole chord (RATCH x3 = every note three times)
 *   playback   the sequencer plays them (RATCH parts, CHANCE 0 silent, VEL), a pattern untouched plays as before
 *   STEP       unchanged: KNOB 1 the cursor; modified steps marked over the roll
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"

/* track 1 alone (ANALOG, POLY), 16 steps of 1/16, GATE 64, no swing: C4 on 0, a REST on 1, E4 on 2 with a TIE on 3,
 * a chord C E G on 4, step 5 empty (a REST), A4 on 6; SEQ > STEP on step `cur` */
static track_t *pattern(uint32_t cur)
{
    track_t *t;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = V_POLY;
    song.g[G_SWING] = 0;
    song.g[G_BPM] = 120;
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 0};
    t->step[1] = (step_t){{0}, 0, ST_REST, 0, 0};
    t->step[2] = (step_t){{64}, 1, ST_NOTE, 0, 0};
    t->step[3] = (step_t){{0}, 0, ST_TIE, 0, 0};
    t->step[4] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 0};
    t->step[6] = (step_t){{69}, 1, ST_NOTE, 0, 0};
    go_page(GR_ROLL);
    ui.cursor = (uint8_t)cur;
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
/* the steps = ref but step `at`, which is `want` (at 0xFF: all equal) */
static int same_but1(const track_t *t, const step_t *ref, uint32_t at, const step_t *want)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (memcmp(&t->step[i], i == at ? want : &ref[i], sizeof ref[i]))
            return 0;
    return 1;
}
/* a knob turned by s detents, unaccelerated (as a slow hand: ui_input.c accel) */
static void kt(uint32_t role, int32_t s)
{
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    turn(role, s);
}
static int listed(uint32_t c)                      /* SEQ > AUTOMATION lists row c (EVC) */
{
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), r;
    for (r = 0; r < n; r++)
        if (rw[r] == c)
            return 1;
    return 0;
}
/* note-ons of track t over `blocks` blocks from PLAY (their velocities in vel[], at most nvel) */
static uint32_t play(track_t *t, uint32_t blocks, uint8_t *vel, uint32_t nvel)
{
    uint32_t b, n = 0, i;
    seq_start();
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].age == v0 + 1u && n < nvel)
                    vel[n] = t->v[i].vel;
    }
    seq_stop();
    return n;
}

static int page_cycle(void)
{
    int bad = 0, ok;
    pattern(4);
    ok = str_eq(cur_page()->title, "STEP");
    press(B_SEQ); frames(320);
    ok &= str_eq(cur_page()->title, "DETAIL") && detail_on() && cur_page()->graph == GR_ROLL && ui.cursor == 4u &&
          song.seq_mode;
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_EVENTS && !detail_on();
    press(B_SEQ); frames(320);
    ok &= str_eq(cur_page()->title, "STEP") && !detail_on();
    bad += check("SEQ: STEP -> DETAIL (the same cursor) -> AUTOMATION -> STEP", ok);
    go_detail();
    {
        char ti[20];
        page_title(ti);
        ok = str_eq(ti, "DETAIL 2/3");
    }
    ok &= help_page() && str_eq(help_page(), "[PRESETS]STEP[K1]CHANCE, 1:4, FILL");
    bad += check("  its footer title DETAIL 2/3 (not the scale), its HELP hint", ok);
    return bad;
}

static int knobs(void)
{
    int bad = 0, ok;
    track_t *t = pattern(0);
    step_t ref[NSTEP], want;
    go_detail();
    memcpy(ref, t->step, sizeof ref);
    kt(EN_K2, 1);                                   /* step 1: RATCH x2 */
    want = ref[0]; step_set_ratchet(&want, 2);
    ok = same_but1(t, ref, 0, &want) && ui.cursor == 0u;
    kt(EN_K1, -1);                                  /* KNOB 1 is CHANCE: the cursor stays */
    want = ref[0]; step_set_ratchet(&want, 2); step_set_chance(&want, 99);
    ok &= same_but1(t, ref, 0, &want) && ui.cursor == 0u;
    bad += check("DETAIL: KNOB 2 the cursor step's RATCH, KNOB 1 its CHANCE (the cursor stays), nothing else", ok);
    turn(EN_PRESET, 2);                               /* PRESETS: to step 3 */
    ok = ui.cursor == 2u;
    kt(EN_K1, -10);
    kt(EN_K3, 3);
    kt(EN_K4, -16);
    ok &= step_chance(&t->step[2]) == 90u && step_nudge(&t->step[2]) == 3 && t->step[2].vel == 80u &&
          t->step[2].n == 1u && t->step[2].note[0] == 64u && t->step[2].time == ST_NOTE && step_ratchet(&t->step[2]) == 1u;
    bad += check("  PRESETS walks the cursor; on step 3 CHANCE 90 %, NUDGE +3, VEL 80 (96 - 16), the note kept", ok);
    ok = listed(EVC(EVK_RATCH, 0)) && listed(EVC(EVK_CHANCE, 0)) && listed(EVC(EVK_CHANCE, 2)) &&
         listed(EVC(EVK_NUDGE, 2)) && !listed(EVC(EVK_RATCH, 2)) && !motion_count(t);
    bad += check("  AUTOMATION lists them as rows (CHANCE, RATCH, NUDGE; no motion record used)", ok);
    turn(EN_PRESET, -2);
    kt(EN_K2, -1);
    kt(EN_K1, 1);
    ok = !listed(EVC(EVK_RATCH, 0)) && !listed(EVC(EVK_CHANCE, 0)) && !memcmp(&t->step[0], &ref[0], sizeof ref[0]);
    bad += check("  turned back to x1 / 100 %: the step as it was, its rows gone from AUTOMATION", ok);
    kt(EN_K3, 100);
    kt(EN_K2, 9);
    ok = step_nudge(&t->step[0]) == 7 && step_ratchet(&t->step[0]) == 4u;
    kt(EN_K3, -100);
    kt(EN_K4, 100);
    ok &= step_nudge(&t->step[0]) == -8 && t->step[0].vel == 127u;
    kt(EN_K4, -200);
    ok &= t->step[0].vel == 1u;
    bad += check("  the ends hold: NUDGE -8..+7, RATCH x4, VEL 1..127", ok);

    memcpy(ref, t->step, sizeof ref);
    ui.cursor = 1;                                    /* the REST */
    ui.msg_t = 0;
    kt(EN_K2, 1);
    ok = msg_is("NO NOTE") && !memcmp(t->step, ref, sizeof ref);
    ui.cursor = 5;                                    /* an empty step */
    ui.msg_t = 0;
    kt(EN_K1, -5);
    ok &= msg_is("NO NOTE") && !memcmp(t->step, ref, sizeof ref);
    ui.cursor = 3;                                    /* the TIE: CHANCE only */
    ui.msg_t = 0;
    kt(EN_K4, -5);
    ok &= msg_is("NO NOTE") && !memcmp(t->step, ref, sizeof ref);
    kt(EN_K1, -40);
    want = ref[3]; step_set_chance(&want, 60);
    ok &= same_but1(t, ref, 3, &want);
    bad += check("  a REST, an empty step: NO NOTE, nothing stored; a TIE takes its CHANCE only", ok);

    pattern(2);                                       /* QUANTIZE ON (the default): NUDGE kept, said */
    go_detail();
    ui.msg_t = 0;
    kt(EN_K3, 2);
    ok = msg_is("QUANTIZE IS ON") && step_nudge(&trk[0].step[2]) == 2;
    t = &trk[0];
    t->step[2].flags |= SF_ACCENT;
    ui.msg_t = 0;
    kt(EN_K4, -2);
    ok &= msg_is("ACC PLAYS 127") && t->step[2].vel == 94u;
    bad += check("  NUDGE with QUANTIZE ON: QUANTIZE IS ON (kept); VEL on an ACC step: ACC PLAYS 127 (kept)", ok);

    /* SONG-only edit guard retired with Chain; live edit tests remain below. */
    return bad;
}

static int edit_undo(void)
{
    int bad = 0, ok;
    track_t *t = pattern(0);
    step_t ref[NSTEP], want;
    go_detail();
    memcpy(ref, t->step, sizeof ref);
    kt(EN_K2, 1); kt(EN_K2, 1);                   /* step 1: x3, one gesture */
    turn(EN_PRESET, 2);
    kt(EN_K2, 1);                                   /* step 3: x2 (another gesture: another step) */
    hold(B_SAVE);
    want = ref[0]; step_set_ratchet(&want, 3);
    ok = same_but1(t, ref, 0, &want);
    hold(B_SAVE);
    ok &= step_ratchet(&t->step[2]) == 2u && step_ratchet(&t->step[0]) == 3u;
    bad += check("undo: the last gesture (step 3's) alone; held again redone", ok);
    hold(B_SAVE);                                     /* (step 3 back at x1) */
    ui.cursor = 0;
    frames(2000);
    kt(EN_K1, -30);
    kt(EN_K3, 2);
    kt(EN_K4, 10);
    memcpy(ref, t->step, sizeof ref);
    press(B_EDIT);
    want = ref[0];
    step_set_chance(&want, 100); step_set_ratchet(&want, 1); step_set_nudge(&want, 0); want.vel = 0;
    ok = same_but1(t, ref, 0, &want) && msg_is("STEP RESET") && ui.cursor == 0u && t->step[0].n == 1u &&
         str_eq(cur_page()->title, "DETAIL");
    press(B_EDIT);
    ok &= msg_is("NOTHING TO RESET");
    hold(B_SAVE);
    ok &= !memcmp(t->step, ref, sizeof ref);
    bad += check("EDIT: the step's CHANCE RATCH NUDGE VEL back to 100 % x1 0 96 (the note and the cursor stay; again: "
                 "NOTHING TO RESET); SAVE held brings them back", ok);
    {   /* AUTOMATION's EDIT deletes a row of them one by one (1.2, unchanged) */
        go_auto_top(); frame();
        ok = auto_pick(EVC(EVK_RATCH, 0));
        press(B_EDIT);
        ok &= step_ratchet(&t->step[0]) == 1u && !listed(EVC(EVK_RATCH, 0)) && listed(EVC(EVK_CHANCE, 0)) &&
              listed(EVC(EVK_NUDGE, 0)) && msg_is("DELETED");
        bad += check("AUTOMATION: EDIT on a row deletes that entry alone (RATCH x1; the step's CHANCE and NUDGE stay)", ok);
    }
    return bad;
}

static int chords_play(void)
{
    int bad = 0, ok;
    uint8_t v[32];
    uint32_t n0, n, i;
    track_t *t = pattern(0);
    uint32_t b16 = div_samples(2) / CTL;
    n0 = play(t, b16 * 16u, v, 32);                   /* a pass as it is: 1 + 1 + 3 + 1 notes */
    ok = n0 == 6u;
    for (i = 0; i < n0; i++)
        ok &= v[i] == 96u;
    bad += check("playback: the pattern untouched, six note-ons at 96", ok);
    go_detail();
    ui.cursor = 4;                                    /* the chord: RATCH x3 */
    kt(EN_K2, 2);
    ui.cursor = 6;                                    /* A4: CHANCE 0 */
    kt(EN_K1, -100);
    ui.cursor = 2;                                    /* E4: VEL 50 */
    kt(EN_K4, -46);
    ok = step_ratchet(&t->step[4]) == 3u && step_ratchet(&t->step[0]) == 1u && t->step[4].n == 3u;
    bad += check("chords: RATCH on the chord's step (one value for its three notes)", ok);
    n = play(t, b16 * 16u, v, 32);
    ok = n == 1u + 1u + 9u;                           /* C4, E4, the chord x3, A4 never */
    ok &= v[0] == 96u && v[1] == 50u;
    bad += check("  played: the chord three times (9 note-ons), A4 at CHANCE 0 never, E4 at VEL 50", ok);
    return bad;
}

static int step_unchanged(void)
{
    int bad = 0, ok;
    track_t *t = pattern(0);
    int32_t x;
    uint16_t a, b;
    kt(EN_K1, 2);
    ok = ui.cursor == 2u && !detail_on();
    bad += check("STEP: KNOB 1 is still the cursor", ok);
    step_set_chance(&t->step[2], 50);
    ui.force = 1;
    frame();
    x = 35 + 2 * 12 + 6;                              /* (ui_graph.c PR_X0, PR_CW: over step 3's column) */
    a = host_screen[(uint32_t)(Y_GRAPH + 3) * 240u + (uint32_t)x];
    b = host_screen[(uint32_t)(Y_GRAPH + 3) * 240u + (uint32_t)(x + 12)];
    ok = a != b && step_detailed(&t->step[2]) && !step_detailed(&t->step[3]);
    bad += check("  a step with a CHANCE is marked over its column on the roll (its neighbour not)", ok);
    return bad;
}

int main(void)
{
    int bad = page_cycle() + knobs() + edit_undo() + chords_play() + step_unchanged();
    printf("%s\n", bad ? "DETAIL TEST FAILED" : "detail tests passed");
    return bad != 0;
}
