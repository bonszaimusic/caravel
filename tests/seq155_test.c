/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.5.5 sequencer (#209 #186 #85), on the real sequencer and UI sources (with the stubs of tests/ui_test.c):
 *   CONDITIONS  a step's 1:2 .. 4:4 by the track's loops (each track its own LEN), counted from PLAY, RESTART, each SONG
 *               section; FILL / !FILL with GLO + E4 held (and the GLO lock); a NUDGE early step 1 the next loop's; a
 *               failed condition applies no lock; a step without one rolls its chance as before (the same draws)
 *   the knob    DETAIL's KNOB 1 past 100 % into the conditions (one a detent, a fast turn stops at 100), AUTOMATION's
 *               COND rows, the marks, EDIT back to 100 %
 *   storage     FUN10 round trip (the chance byte 102..112), proj_bound, a byte past them refused; what 1.5 / 1.5.1 do
 *               with it (their proj_unpack's rule, copied: a project holding a condition is refused, one without loads)
 *   editor      STEP_SET / TRACK_STEP's condition byte (INFO 4B 01 0B), a chance of 100 without it keeps the condition
 *   EUCLID      (SEQ TOOLS F4) hits / steps / rotation, DRUM lane or notes (the step's own or the root), the live
 *               preview, OCT+ applies, OCT- cancels, one undo
 *   DOUBLE HALF (SEQ TOOLS D5 E5) LEN x2 (up to 64) / half, ties, locks and the steps' own fields with them, one undo
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"

/* track 1 alone, LEN 16 of 1/16, GATE 64, no swing; notes 60 + i on step i where on bit i; the rest REST */
static track_t *melody(uint32_t on)
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    motion_clear(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = V_POLY;
    song.g[G_SWING] = 0;
    song.g[G_BPM] = 120;
    for (i = 0; i < 16u; i++)
        if ((on >> i) & 1u)
            t->step[i] = (step_t){{(uint8_t)(60u + i)}, 1, ST_NOTE, 0, 0};
    seq_fill = 0;
    return t;
}
static void set_cond(step_t *s, uint32_t c) { step_set_chance_v(s, 100u + c); }

/* the note-ons of track t while `blocks` blocks of CTL play (seq_tick alone, the transport already running) */
static uint32_t ons(track_t *t, uint32_t blocks, uint8_t *note, uint32_t nmax)
{
    uint32_t b, n = 0, i;
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++)
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].age == v0 + 1u && n < nmax)
                    note[n++] = t->v[i].note;
    }
    return n;
}
static uint32_t loop_blocks(const track_t *t) { return (uint32_t)t->p[P_SLEN] * div_samples(2) / CTL; }
/* loop by loop from PLAY: bit L of the result = note x played in loop L (the loop's own note 60 on step 1 counts them) */
static uint32_t heard(track_t *t, uint32_t loops, uint32_t x)
{
    uint8_t nt[512];
    uint32_t n, i, L = 0, m = 0;
    seq_start();
    n = ons(t, loops * loop_blocks(t) + 2u, nt, sizeof nt);
    seq_stop();
    for (i = 0; i < n; i++) {
        if (nt[i] == 60u && i)
            L++;
        if (nt[i] == x && L < 32u)
            m |= 1u << L;
    }
    return m;
}

static int conditions(void)
{
    int bad = 0, ok;
    track_t *t = melody(1u | 1u << 2 | 1u << 4 | 1u << 6 | 1u << 8 | 1u << 10 | 1u << 12 | 1u << 14);
    set_cond(&t->step[2], 1);                          /* 1:2 */
    set_cond(&t->step[4], 2);                          /* 2:2 */
    set_cond(&t->step[6], 3);                          /* 1:3 */
    set_cond(&t->step[8], 7);                          /* 2:4 */
    set_cond(&t->step[10], 9);                         /* 4:4 */
    set_cond(&t->step[12], 10);                        /* FILL */
    set_cond(&t->step[14], 11);                        /* !FILL */
    ok = step_cond(&t->step[2]) == 1u && t->step[2].probability == 102u && t->step[14].probability == 112u &&
         step_chance(&t->step[2]) == 100u && step_chance_v(&t->step[10]) == 109u;
    bad += check("conditions: in the chance byte's free values 102..112, at 100 % (step_chance)", ok);
    bad += check("  1:2 on loops 1 3 5 .. (from PLAY, 12 loops)", heard(t, 12, 62) == 0x555u);
    bad += check("  2:2 on loops 2 4 6 ..", heard(t, 12, 64) == 0xAAAu);
    bad += check("  1:3 on loops 1 4 7 10", heard(t, 12, 66) == 0x249u);
    bad += check("  2:4 on loops 2 6 10", heard(t, 12, 68) == 0x222u);
    bad += check("  4:4 on loops 4 8 12", heard(t, 12, 70) == 0x888u);
    bad += check("  past 12 loops the count goes on (1:3 on 13 16 19 22, 4:4 on 16 20 24)",
                 heard(t, 24, 66) == 0x249249u && heard(t, 24, 70) == 0x888888u);
    ok = heard(t, 4, 72) == 0u && heard(t, 4, 74) == 0xFu;
    seq_fill = 1;
    ok &= heard(t, 4, 72) == 0xFu && heard(t, 4, 74) == 0u;
    seq_fill = 0;
    bad += check("  FILL plays only while FILL is held, !FILL only while not", ok);
    {   /* FILL taken mid-pass: from the next step on */
        uint8_t nt[64];
        uint32_t n, i, f = 0;
        seq_start();
        n = ons(t, 11u * div_samples(2) / CTL, nt, 64);   /* (into step 11) */
        seq_fill = 1;
        n += ons(t, 5u * div_samples(2) / CTL, nt + n, 64 - n);
        seq_stop();
        seq_fill = 0;
        for (i = 0; i < n; i++)
            f |= (nt[i] == 72u) | (nt[i] == 74u) << 1;
        bad += check("  FILL pressed on step 11: step 13's FILL plays, step 15's !FILL not", f == 1u);
    }
    {   /* STOP / PLAY: from loop 1 again; RESTART too */
        uint32_t a = heard(t, 3, 64), b = heard(t, 3, 64);
        bad += check("  PLAY counts from loop 1 again (2:2: loop 2 each time)", a == 2u && b == 2u);
    }
    {   /* each track counts its own LEN: T2 LEN 3 with a 1:2 on its step 1 */
        track_t *u = &trk[1];
        uint8_t nt[64];
        uint32_t n, i, c = 0;
        track_defaults_steps(u);
        u->p[P_SLEN] = 3; u->p[P_SDIV] = 2; u->p[P_SSWING] = 0; u->p[P_SGATE] = 64;
        u->step[0] = (step_t){{50}, 1, ST_NOTE, 0, 0};
        set_cond(&u->step[0], 1);
        seq_start();
        for (i = 0; i < 12u * div_samples(2) / CTL; i++)
            seq_tick(t, CTL);                          /* (T1 runs alongside) */
        seq_stop();
        seq_start();
        n = ons(u, (12u * div_samples(2) - div_samples(2) / 2u) / CTL, nt, 64);   /* T2 alone: 4 loops of 3 */
        seq_stop();
        for (i = 0; i < n; i++)
            c += nt[i] == 50u;
        bad += check("  each track counts its own loops (T2 LEN 3: 1:2 twice in 12 steps)", c == 2u);
        track_defaults_steps(u);
    }
    return bad;
}

/* a failed condition: the step plays nothing and applies none of its locks; a step without a condition rolls its chance
 * with the same draws as before (one rng() a step under 100 %, none at 100 % or with a condition) */
static int cond_locks_rolls(void)
{
    int bad = 0, ok;
    track_t *t = melody(1u | 1u << 4);
    uint32_t r0, r1, i;
    set_cond(&t->step[4], 1);                          /* 1:2 */
    motion_set_lock(t, 4, P_LEVEL, 7);
    motion_set_enabled(t, 1);
    seq_start();
    for (i = 0; i < 5u * div_samples(2) / CTL; i++)
        seq_tick(t, CTL);                              /* loop 1: plays, its lock with it */
    ok = t->p[P_LEVEL] == 7;
    for (i = 0; i < 16u * div_samples(2) / CTL; i++)
        seq_tick(t, CTL);                              /* loop 2, step 5: fails */
    ok &= t->p[P_LEVEL] != 7;
    seq_stop();
    bad += check("conditions: a step that plays applies its lock, one that fails none", ok);
    motion_clear(t);
    t = melody(0xFFFFu);
    for (i = 0; i < 16u; i++)
        if (i % 3u == 0u)
            step_set_chance(&t->step[i], 50);
        else if (i % 3u == 1u)
            set_cond(&t->step[i], 1 + i % NCOND);
    rng_state = 12345u;
    seq_start();
    for (i = 0; i < 16u * div_samples(2) / CTL; i++)
        seq_tick(t, CTL);
    seq_stop();
    r0 = rng();
    rng_state = 12345u;
    for (i = 0; i < 6u; i++)                           /* (the six 50 % steps: one draw each) */
        (void)rng();
    r1 = rng();
    bad += check("  the chance draws: one per step under 100 %, none for a condition (the same sequence as before)", r0 == r1);
    return bad;
}

/* SONG: each section's pattern counts from its first loop; the section's repeats are its loops */
/* NUDGE: step 1 played early (QUANTIZE OFF, -4/16) belongs to the loop it starts */
static int cond_nudge(void)
{
    track_t *t = melody(1u | 1u << 8);
    t->step[8].note[0] = 60;                           /* (the loop counter's note on step 9 too: two a loop) */
    t->step[0].note[0] = 80;
    set_cond(&t->step[0], 1);                          /* 1:2 */
    step_set_nudge(&t->step[0], -4);
    t->p[P_SQNT] = 0;
    {
        uint8_t nt[128];
        uint32_t n, i, c = 0;
        seq_start();
        n = ons(t, 6u * loop_blocks(t) - div_samples(2) / 2u / CTL, nt, 128);   /* (not the 7th loop's early step 1) */
        seq_stop();
        for (i = 0; i < n; i++)
            c += nt[i] == 80u;
        return check("conditions: a NUDGE early step 1 counts as the next loop's (1:2 x3 in 6 loops)", c == 3u);
    }
}

/* DETAIL's KNOB 1 and AUTOMATION */
static void go_page_title(const char *ti)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, ti); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    frame();
}
static void kt(uint32_t role, int32_t s) { memset(ui.enc_t, 0, sizeof ui.enc_t); turn(role, s); }
static int cond_ui(void)
{
    int bad = 0, ok;
    track_t *t = melody(1u | 1u << 2);
    uint16_t rw[EV_ROWS];
    uint32_t n, r, found = 0;
    go_page(GR_ROLL);
    ui.cursor = 2;
    go_page_title("DETAIL");
    kt(EN_K1, 1);
    ok = step_chance_v(&t->step[2]) == 101u && step_cond(&t->step[2]) == 1u;
    kt(EN_K1, 7);
    ok &= step_cond(&t->step[2]) == 8u;                /* 3:4 */
    kt(EN_K1, 10);
    ok &= step_cond(&t->step[2]) == NCOND;             /* the end: !FILL */
    bad += check("DETAIL: KNOB 1 past 100 % into the conditions, one a detent (1:2 .. !FILL, held at the end)", ok);
    ui.force = 1; frame();
    bad += check("  its card COND, \"!FILL\"; the step marked", step_detailed(&t->step[2]));
    kt(EN_K1, -12);
    ok = step_chance_v(&t->step[2]) == 100u && !step_cond(&t->step[2]) && !t->step[2].probability;
    kt(EN_K1, -3);
    ok &= step_chance(&t->step[2]) == 97u;
    bad += check("  turned back: 100 % (held there on the way out), then the percentages", ok);
    ui.enc_t[EN_K1 - EN_K1] = 0;
    turn(EN_K1, 40);                                   /* a fast turn from 97 %: stops at 100 */
    ok = step_chance_v(&t->step[2]) == 100u;
    kt(EN_K1, 4);
    ok &= step_cond(&t->step[2]) == 4u;                /* 2:3 */
    bad += check("  a fast turn stops at 100 % on the way in", ok);
    go_page_title("AUTOMATION");
    n = ev_list(rw);
    for (r = 0; r < n; r++)
        found |= rw[r] == EVC(EVK_CHANCE, 2);
    ui.force = 1; frame();
    bad += check("AUTOMATION: the condition is the step's CHANCE row (\"STEP 03 COND 2:3\")", found != 0u);
    go_page_title("DETAIL");
    ui.cursor = 2;
    press(B_EDIT);
    bad += check("DETAIL's EDIT: back to 100 % (no condition)", !t->step[2].probability);
    return bad;
}

/* the FILL key: GLO + E4 held; the lock (two taps) and E4 */
static int fill_key(void)
{
    int bad = 0, ok;
    melody(1);
    frames(400);
    btn_down(B_GLO); frames(100);
    key_down(key_at(0, 6)); frames(50);                /* E4: white place 6 */
    ok = seq_fill == 1u && ui.layer == LAYER_GLO;
    key_up(key_at(0, 6)); frames(50);
    ok &= !seq_fill;
    btn_up(B_GLO); frames(400);
    bad += check("FILL: GLO + E4 held is FILL, let go it is not", ok);
    ui_power_on();
    return bad;
}

/* FUN10 and older firmware */
static int old_accepts(const project_store_t *p)   /* 1.5 / 1.5.1's proj_unpack step rule: a chance byte above 101 refuses */
{
    uint32_t t, i, base;
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < NSTEP; i++) {
            base = 68u + t * (P_COUNT + 2u + NSTEP * 7u) + P_COUNT + 2u + i * 7u;
            if ((p->raw[base + 6u] & 127u) > 101u)
                return 0;
        }
    return 1;
}
static int cond_store(void)
{
    int bad = 0, ok;
    project_t a, b;
    project_store_t pk;
    track_t *t = melody(0xFFu);
    uint32_t i;
    step_set_chance(&t->step[1], 30);
    int old0;
    project_capture(&a);
    old0 = proj_pack(&pk, &a) && old_accepts(&pk);
    ok = old0;
    for (i = 0; i < NCOND; i++)
        set_cond(&a.t[0].step[i], 1u + i);
    a.sum = proj_sum(&a);
    ok &= proj_pack(&pk, &a) && proj_import(&b, &pk, sizeof pk);
    for (i = 0; i < NCOND; i++)
        ok &= step_cond(&b.t[0].step[i]) == 1u + i;
    bad += check("FUNW: the conditions round trip (the chance byte 102..112), 2784-byte native size", ok && sizeof pk == 2784u);
    bad += check("  1.5 / 1.5.1's rule refuses a project holding one (the slot reads empty there), one without loads",
                 old0 && !old_accepts(&pk));
    pk.raw[68u + P_COUNT + 2u + 6u] = 113;
    {
        uint32_t s = proj_hash(pk.raw, PROJ_STORE_SIZE - 4u);
        memcpy(pk.raw + PROJ_STORE_SIZE - 4u, &s, 4);
    }
    bad += check("  a chance byte past the conditions (113) is refused even with a correct hash", !proj_import(&b, &pk, sizeof pk));
    a.t[0].step[0].probability = 120;
    proj_bound(&a);
    bad += check("  proj_bound: a byte past them back to 100 %, the conditions kept",
                 !a.t[0].step[0].probability && step_cond(&a.t[0].step[1]) == 2u);
    return bad;
}

/* ------------------------------------------------------------ EUCLID --- */
static int tl_lock_at5(void) { int16_t v; return motion_lock_get(TSEL, 5, P_REV, &v) && v == 50; }
static void tkey(uint32_t w) { key_down(white(w)); frame(); key_up(white(w)); frame(); }
static void oct_fwd(void) { btn_down(B_OCTUP); frame(); btn_up(B_OCTUP); frame(); }
static int ref_hit(uint32_t i, uint32_t k, uint32_t n, uint32_t r) { return ((((i % n) + n - r % n) % n) * k) % n < k; }
static int eu_rest_but(const track_t *t, uint32_t len, uint32_t k, uint32_t n, uint32_t r)   /* NOTE where it hits, */
{                                                                                              /* else a REST or a TIE */
    uint32_t i;
    for (i = 0; i < len; i++)
        if ((t->step[i].time == ST_NOTE && t->step[i].n) != ref_hit(i, k, n, r) ||
            (!ref_hit(i, k, n, r) && t->step[i].time == ST_NOTE))
            return 0;
    return 1;
}
static int test_euclid(void)
{
    int bad = 0, ok;
    track_t *t = melody(1u | 1u << 5);                 /* C4 (60) tied over step 2, F4 (65) on step 6; a lock on 6 */
    step_t ref[NSTEP];
    uint32_t i;
    t->step[1] = (step_t){{0}, 0, ST_TIE, 0, 0};
    t->step[0].flags = SF_ACCENT;
    t->step[30] = (step_t){{90}, 1, ST_NOTE, 0, 0};     /* (past LEN: untouched) */
    motion_set_lock(t, 5, P_REV, 50);
    memcpy(ref, t->step, sizeof ref);
    go_page(GR_ROLL); frame();
    eu.steps = 0;                                      /* (the first EUCLID since the power came: 4 / 16) */
    btn_down(B_SEQ); frame();
    tkey(7);                                           /* F4: EUCLID */
    ok = eu.on && eu.hits == 4u && eu.steps == 16u && !eu.rot && msg_is("EUCLID 4/16") && eu_rest_but(t, 16, 4, 16, 0);
    ok &= t->step[0].note[0] == 60 && (t->step[0].flags & SF_ACCENT) && t->step[1].time == ST_TIE &&   /* C4 kept, its tie */
          t->step[4].note[0] == 60 && t->step[8].note[0] == 60 && t->step[4].n == 1u && !t->step[4].flags &&   /* the root */
          t->step[5].time == ST_REST && t->step[30].note[0] == 90;
    bad += check("EUCLID (F4): 4 / 16 at once: a NOTE step kept (C4, its accent and tie), the root on the new hits, the "
                 "rest emptied; past LEN untouched", ok);
    kt(EN_K1, 1);
    ok = eu.hits == 5u && eu_rest_but(t, 16, 5, 16, 0) && msg_is("EUCLID 5/16");
    kt(EN_K3, 2);
    ok &= eu.rot == 2u && eu_rest_but(t, 16, 5, 16, 2) && msg_is("EUCLID 5/16 +2");
    kt(EN_K2, -8);
    ok &= eu.steps == 8u && eu_rest_but(t, 16, 5, 8, 2);
    kt(EN_K2, 100);
    ok &= eu.steps == 16u;                             /* (STEPS up to LEN) */
    kt(EN_K1, 100);
    ok &= eu.hits == 16u && eu_rest_but(t, 16, 16, 16, 2);
    kt(EN_K1, -100);
    ok &= eu.hits == 0u;
    for (i = 0; i < 16u; i++)
        ok &= t->step[i].time != ST_NOTE;
    bad += check("  KNOB 1 HITS (0..STEPS), 2 STEPS (1..LEN, the cycle repeated), 3 ROT: rewritten from the copy each turn",
                 ok);
    ui.force = 1; frame();
    oct_back();
    ok = !eu.on && !memcmp(t->step, ref, sizeof ref) && tl_lock_at5() && msg_is("EUCLID CANCELLED") && ui.layer == LAYER_SEQ;
    bad += check("  OCT-: cancelled, the steps as they were (the layer stays open)", ok);
    tkey(7);
    kt(EN_K1, 4);                                      /* 4 + .. back to 4 hits: (0 -> 4) */
    ok = eu.hits == 4u && eu.on;
    oct_fwd();
    ok &= !eu.on && msg_is("EUCLID APPLIED") && eu_rest_but(t, 16, 4, 16, 2) && !tl_lock_at5();
    bad += check("  OCT+: applied; an emptied step's lock goes", ok);
    btn_up(B_SEQ); frame();
    hold(B_SAVE);
    bad += check("  SAVE held: one undo for all of it (the steps and the lock back)",
                 !memcmp(t->step, ref, sizeof ref) && tl_lock_at5());
    btn_down(B_SEQ); frame();
    tkey(7);
    ok = eu.on;
    btn_up(B_SEQ); frame();
    ok &= !eu.on && eu_rest_but(t, 16, 4, 16, 2) && !ui.layer;
    bad += check("  the layer let go with it open: applied", ok);
    {   /* a MIDI Program Change for T2 while EUCLID is open on T1 (its load takes the undo copy): T1 keeps its own */
        track_t *t2 = &trk[1];
        step_t ref2[NSTEP];
        t = melody(1u | 1u << 5);
        t->step[1] = (step_t){{0}, 0, ST_TIE, 0, 0};
        memcpy(ref, t->step, sizeof ref);
        track_defaults_steps(t2);
        for (i = 0; i < 16u; i++)
            t2->step[i] = (step_t){{72}, 1, ST_NOTE, 0, 0};
        memcpy(ref2, t2->step, sizeof ref2);
        go_page(GR_ROLL); frame();
        eu.steps = 0;
        btn_down(B_SEQ); frame();
        tkey(7);
        ui.mpc[1] = 0x80u | 2u;                       /* (the ISR's note of it: midi_control.c mpc_io) */
        frame();
        ok = eu.on && undo.trk == 2u;                  /* (the load's copy now) */
        kt(EN_K1, 1);                                  /* 5 / 16 from T1's own steps */
        ok &= eu_rest_but(t, 16, 5, 16, 0) && t->step[0].note[0] == 60 && t->step[1].time == ST_TIE &&
              t->step[4].note[0] == 60 && !memcmp(t2->step, ref2, sizeof ref2);
        for (i = 0; i < 16u; i++)
            ok &= t->step[i].time != ST_NOTE || t->step[i].note[0] != 72;
        oct_back();
        ok &= !eu.on && !memcmp(t->step, ref, sizeof ref) && !memcmp(t2->step, ref2, sizeof ref2);
        tkey(7);
        ui.mpc[1] = 0x80u | 3u;
        frame();
        kt(EN_K1, 1);                                  /* (5 kept: 6 / 16) */
        oct_fwd();
        ok &= !eu.on && eu_rest_but(t, 16, 6, 16, 0) && t->step[0].note[0] == 60 && t->step[3].note[0] == 60 &&
              !memcmp(t2->step, ref2, sizeof ref2);
        btn_up(B_SEQ); frame();
        bad += check("  a Program Change for T2 while open (its load's undo copy): T1 turned from its own steps, OCT- "
                     "puts them back, OCT+ applies; T2's steps untouched", ok);
    }
    return bad;
}

/* ------------------------------------------------------- DOUBLE, HALF --- */
static int test_double_half(void)
{
    int bad = 0, ok;
    track_t *t = melody(1u << 2 | 1u << 7);
    step_t ref[NSTEP];
    uint32_t i, mc;
    int16_t v;
    t->p[P_SLEN] = 8;
    t->step[0] = (step_t){{0}, 0, ST_TIE, 0, 0};       /* G4 on step 8 tied over the loop's end into step 1 */
    t->step[3] = (step_t){{0}, 0, ST_TIE, 0, 0};       /* E4 (62) on 3 held over 4 */
    set_cond(&t->step[2], 1);
    step_set_ratchet(&t->step[2], 2);
    step_set_nudge(&t->step[2], 3);
    t->step[2].vel = 80;
    step_set_chance(&t->step[7], 40);
    t->step[9] = (step_t){{90}, 1, ST_NOTE, 0, 0};     /* stored past LEN, where the copy goes */
    motion_set_lock(t, 2, P_REV, 50);
    motion_set_event(t, 7, P_CHOR, 20);
    motion_set_lock(t, 10, P_DIST, 30);                /* (past LEN: replaced by the copy's) */
    motion_set_enabled(t, 1);
    memcpy(ref, t->step, sizeof ref);
    mc = motion_count(t);
    go_page(GR_ROLL); frame();
    btn_down(B_SEQ); frame();
    tkey(12);                                          /* D5: DOUBLE */
    ok = t->p[P_SLEN] == 16 && msg_is("DOUBLED: 16 STEPS") && !memcmp(t->step, ref, 8u * sizeof ref[0]) &&
         !memcmp(t->step + 8, ref, 8u * sizeof ref[0]) && !memcmp(t->step + 16, ref + 16, 48u * sizeof ref[0]);
    ok &= motion_lock_get(t, 10, P_REV, &v) && v == 50 && !motion_lock_get(t, 10, P_DIST, &v) &&
          motion_find(t, 15, P_CHOR) >= 0 && motion_find(t, 7, P_CHOR) >= 0 && motion_count(t) == 4u;
    ok &= step_cond(&t->step[10]) == 1u && step_ratchet(&t->step[10]) == 2u && step_nudge(&t->step[10]) == 3 &&
          t->step[10].vel == 80u && step_chance(&t->step[15]) == 40u && t->step[8].time == ST_TIE;
    bad += check("DOUBLE (D5): LEN 8 -> 16, steps 9..16 a copy (ties, condition, RATCH, NUDGE, VEL, CHANCE), the locks and "
                 "automation copied, what was stored there replaced", ok);
    btn_up(B_SEQ); frame();
    hold(B_SAVE);
    ok = t->p[P_SLEN] == 8 && !memcmp(t->step, ref, sizeof ref) && motion_count(t) == mc &&
         motion_lock_get(t, 10, P_DIST, &v) && v == 30;
    bad += check("  SAVE held: one undo (LEN, the steps, the automation back)", ok);
    {   /* played: the doubled pattern sounds as two passes of the original (no chance, no condition) */
        uint8_t a[64], b[64];
        uint32_t na, nb;
        track_t *u = melody(1u | 1u << 3 | 1u << 6);
        u->p[P_SLEN] = 8;
        u->step[1] = (step_t){{0}, 0, ST_TIE, 0, 0};
        seq_start(); na = ons(u, 16u * div_samples(2) / CTL, a, 64); seq_stop();
        btn_down(B_SEQ); frame();
        tkey(12);
        btn_up(B_SEQ); frame();
        seq_start(); nb = ons(u, 16u * div_samples(2) / CTL, b, 64); seq_stop();
        bad += check("  played: the doubled 16 steps sound as the 8 played twice", na == nb && na == 6u && !memcmp(a, b, na));
    }
    t = melody(1u << 2 | 1u << 7);
    t->p[P_SLEN] = 8;
    t->step[3] = (step_t){{0}, 0, ST_TIE, 0, 0};
    t->step[4] = (step_t){{0}, 0, ST_TIE, 0, 0};       /* E4 held over 4 and 5 */
    t->step[9] = (step_t){{90}, 1, ST_NOTE, 0, 0};
    motion_set_lock(t, 2, P_REV, 50);
    motion_set_lock(t, 7, P_REV, 60);
    memcpy(ref, t->step, sizeof ref);
    btn_down(B_SEQ); frame();
    tkey(13);                                          /* E5: HALF */
    ok = t->p[P_SLEN] == 4 && msg_is("HALVED: 4 STEPS") && !memcmp(t->step, ref, 4u * sizeof ref[0]) &&
         t->step[4].time == ST_REST && t->step[7].time == ST_REST && t->step[9].note[0] == 90 &&
         motion_lock_get(t, 2, P_REV, &v) && !motion_lock_get(t, 7, P_REV, &v);
    bad += check("HALF (E5): LEN 8 -> 4, the first half kept, the second cleared with its locks (E4's tie ends at LEN), "
                 "past the old LEN untouched", ok);
    tkey(13); tkey(13);
    ok = t->p[P_SLEN] == 1;
    tkey(13);
    ok &= t->p[P_SLEN] == 1 && msg_is("1 STEP: NO HALF");
    bad += check("  HALF down to 1 step; there: 1 STEP: NO HALF", ok);
    btn_up(B_SEQ); frame();
    hold(B_SAVE);
    bad += check("  SAVE held: the last HALF undone (LEN 2)", t->p[P_SLEN] == 2);
    {   /* LEN 40: 64 (the first 24 copied); 64: no room; the 128 records full: refused */
        t = melody(0xFFFFu);
        t->p[P_SLEN] = 40;
        for (i = 16; i < NSTEP; i++)
            t->step[i] = (step_t){{(uint8_t)(20u + i)}, 1, ST_NOTE, 0, 0};
        memcpy(ref, t->step, sizeof ref);
        btn_down(B_SEQ); frame();
        tkey(12);
        ok = t->p[P_SLEN] == 64 && !memcmp(t->step, ref, 40u * sizeof ref[0]) && !memcmp(t->step + 40, ref, 24u * sizeof ref[0]);
        memcpy(ref, t->step, sizeof ref);
        undo.trk = 0;
        tkey(12);
        ok &= t->p[P_SLEN] == 64 && msg_is("64 STEPS: NO ROOM") && !undo.trk && !memcmp(t->step, ref, sizeof ref);
        bad += check("DOUBLE from LEN 40: 64 (steps 1..24 copied); at 64: NO ROOM, nothing done", ok);
        t->p[P_SLEN] = 8;
        for (i = 0; motion.count < MOTION_MAX - 4u; i++)
            motion_set_event(&trk[1 + i % 3u], i / 3u % NSTEP, P_REV + i / 192u, (int16_t)(i % 100u));
        for (i = 0; i < 4u; i++)
            motion_set_lock(t, i, P_REV, 40);           /* (the 128 used: no room for 4 copies) */
        memcpy(ref, t->step, sizeof ref);
        tkey(12);
        ok = t->p[P_SLEN] == 8 && msg_is("AUTOMATION FULL") && !memcmp(t->step, ref, sizeof ref) && motion_count(t) == 4u;
        btn_up(B_SEQ); frame();
        bad += check("  the 128 records with no room for the copies: AUTOMATION FULL, nothing done", ok);
        for (i = 0; i < NTRK; i++)
            motion_clear(&trk[i]);
    }
    return bad;
}

int main(void)
{
    int bad = conditions() + cond_locks_rolls() + cond_nudge() + cond_ui() + fill_key() + cond_store() + test_euclid() + test_double_half();
    printf("%s\n", bad ? "SEQ 1.5.5 TEST FAILED" : "seq 1.5.5 tests passed");
    return bad != 0;
}
