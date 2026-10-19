/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.5.6, on the real sources (with the stubs of tests/ui_test.c):
 *   HUMANIZE  (#213) the matrix's VEL and TIME destinations, appended after DEPTH: VEL moves a note-on's velocity
 *             (every note-on, RAND its own per note, the matrix's VEL source sees the new one, clamped 1..127), TIME
 *             plays a sequencer step late (0..30 ms, at most 7/16 of the step; QUANTIZE ON and OFF with NUDGE; a
 *             ratchet on time); with no such slot nothing changes (the RAND draws, mod_begin, the timing); saved
 *   KITS      (#216) DRUM KIT DEEP TIGHT PERC (9..11) from the five kits' pieces, three DRUM presets after DRUM KIT; the
 *             lanes' names, the 8 lanes play, saved; an older KIT range reads them as 77
 *   LEDS      (#212) MENU > LEDS INV LO / INV HI appended: INV with its lit LEDs at the mid level (LO the darker), saved as
 *             INV plus a level byte, older settings as before
 *   SELECT    (#84) MENU > SELECT MODW / VEL after BPM / PAGE: the selected track's mod wheel (the matrix's MODW, CC1 out) or
 *             the keys' velocity (sounding, sent); GLO + SELECT the tempo; PAGE's bit as 1.5.5, saved; CLOCK's hint
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* ---------------------------------------------------------- HUMANIZE --- */
static void mslot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[P_M1SRC + 3u * k] = (int16_t)s;
    t->p[P_M1DST + 3u * k] = (int16_t)d;
    t->p[P_M1AMT + 3u * k] = (int16_t)a;
}
static void mslots_off(track_t *t)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        mslot(t, k, 0, 0, 0);
}
/* the velocity of the voice note n started on track t (0: none) */
static uint32_t vel_of(const track_t *t, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == n)
            return t->v[i].vel;
    return 0;
}

static uint32_t P16;
static track_t *seq1(uint32_t qntz)                 /* track 1 alone: 16 notes of 1/16, 120 BPM, no swing */
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_SQNT] = (int16_t)qntz;
    song.g[G_SWING] = 0;
    song.g[G_BPM] = 120;
    P16 = div_samples(2);
    for (i = 0; i < 16u; i++)
        t->step[i] = (step_t){{(uint8_t)(60u + i)}, 1, ST_NOTE, 0, 100};
    mslots_off(t);
    return t;
}
/* the sample of the block each of the first n note-ons started in (from PLAY) */
static uint32_t play(track_t *t, uint32_t blocks, uint32_t *at, uint32_t nat)
{
    uint32_t b, n = 0;
    seq_start();
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            if (n < nat)
                at[n] = b * CTL;
    }
    seq_stop();
    return n;
}
static uint32_t blk(uint32_t i, uint32_t off) { uint32_t s = i * P16 + off; return (s + CTL - 1u) / CTL * CTL; }

static int humanize(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t i, at[32], at2[32], n, v, lo = 127, hi = 0, h;
    int16_t rnd[8];

    ok = MD_VEL == MD_DEPTH + 1 && MD_TIME == MD_VEL + 1 && MD_MORPH == MD_TIME + 1 && MD_PD_FB == MD_MORPH + 1 && MD_N == MD_PD_FB + 1 && str_eq(N_MDST[MD_VEL], "VEL") &&
         str_eq(N_MDST[MD_TIME], "TIME") && TP[P_M1DST].max == MD_N - 1 && MD_DEPTH == 20;
    ok &= str_eq(mod_dst_name(&trk[0], MD_VEL), "VEL") && str_eq(mod_dst_name(&trk[0], MD_TIME), "TIME");
    bad += check("HUMANIZE (#213): VEL and TIME appended after DEPTH (20): 21, 22; the names", ok);

    /* VEL: no slot: the velocity as asked, the RAND draws as before */
    t = seq1(1);
    mod_seed = 0x2545F491u;
    for (i = 0; i < 8u; i++) {
        trk_note_on(t, 40u + i, 100);
        rnd[i] = t->m_rnd;
    }
    ok = vel_of(t, 40) == 100u && t->m_vel == 100u;
    trk_all_off(t);
    mslot(t, 0, MS_RAND, MD_VEL, 32);
    mod_seed = 0x2545F491u;
    for (i = 0; i < 8u; i++) {
        trk_note_on(t, 40u + i, 100);
        ok &= t->m_rnd == rnd[i];                        /* (the same draws: one per note) */
        v = vel_of(t, 40u + i);
        ok &= v == t->m_vel && v >= 1u && v <= 127u;     /* (the matrix's VEL source sees the new one) */
        ok &= (int32_t)v == clamp(100 + ((((int32_t)rnd[i] * 32) >> 6) * 127 >> 15), 1, 127);
        lo = v < lo ? v : lo, hi = v > hi ? v : hi;
    }
    ok &= hi > lo + 20u;
    bad += check("  VEL: RAND -> VEL AMT 32: each note its own velocity (100 +- 63, 1..127), the RAND draws as without it", ok);

    trk_all_off(t);
    mslots_off(t);
    mslot(t, 1, MS_MODW, MD_VEL, 63);
    t->mw = 127;
    trk_note_on(t, 50, 60);
    ok = vel_of(t, 50) == 127u;                         /* (clamped at 127) */
    mslot(t, 1, MS_MODW, MD_VEL, -64);
    trk_note_on(t, 51, 60);
    ok &= vel_of(t, 51) == 1u;                          /* (and at 1: a note still plays) */
    t->mw = 0;
    trk_note_on(t, 52, 60);
    ok &= vel_of(t, 52) == 60u;
    mslot(t, 1, MS_MODW, MD_VEL, 0);                    /* (AMT 0: nothing) */
    t->mw = 127;
    trk_note_on(t, 53, 60);
    ok &= vel_of(t, 53) == 60u;
    t->mw = 0;
    trk_all_off(t);
    mslot(t, 1, MS_RAND, MD_VEL, 40);
    mslot(t, 2, MS_RAND, MD_TIME, 40);
    ok &= mod_begin(t) == 0 && !MOD(t).on;              /* (no per-block work for VEL / TIME) */
    mslot(t, 3, MS_LFO, MD_CUT, 10);
    ok &= mod_begin(t) == 1;
    mod_end(t);
    bad += check("  VEL: MODW -> VEL clamps to 127 / 1, 0 nothing; VEL / TIME slots do no per-block work", ok);

    /* TIME: no slot: every step on its start (QUANTIZE ON) */
    t = seq1(1);
    n = play(t, (15u * P16 + 2000u) / CTL, at, 32);
    ok = n == 16u;
    for (i = 0; i < 16u; i++)
        ok &= at[i] == blk(i, 0);
    /* MODW -> TIME AMT 63, MODW full: every step late by the same, below 30 ms */
    mslot(t, 0, MS_MODW, MD_TIME, 63);
    t->mw = 127;
    h = (uint32_t)(((uint32_t)((127 * 258 * 63) >> 6) * MOD_TIME_MAX) >> 15);
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok &= n == 16u && h > MOD_TIME_MAX * 9u / 10u && h <= MOD_TIME_MAX && MOD_TIME_MAX == FS * 30u / 1000u;
    for (i = 0; i < 16u; i++)
        ok &= at2[i] == blk(i, h);
    bad += check("  TIME: none: on the step; MODW full AMT 63: every step late by ~30 ms (QUANTIZE ON)", ok);

    /* RAND -> TIME: each step its own delay, 0 .. 30 ms, never early, all played once */
    mslot(t, 0, MS_RAND, MD_TIME, 63);
    t->mw = 0;
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok = n == 16u;
    lo = ~0u, hi = 0;
    for (i = 0; i < 16u; i++) {
        uint32_t d = at2[i] - blk(i, 0);
        ok &= at2[i] >= blk(i, 0) && d <= blk(0, MOD_TIME_MAX);
        lo = d < lo ? d : lo, hi = d > hi ? d : hi;
    }
    ok &= hi > lo + 8u * CTL;
    /* a ratcheted step on time; AMT < 0 with the source at 0: the most */
    step_set_ratchet(&t->step[3], 2);
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok &= n == 17u && at2[3] == blk(3, 0);
    step_set_ratchet(&t->step[3], 1);
    mslot(t, 0, MS_MODW, MD_TIME, -64);
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok &= n == 16u && at2[5] == blk(5, (32767u * MOD_TIME_MAX) >> 15);
    bad += check("  TIME: RAND: each step its own delay (0..30 ms, never early); a ratchet on time; AMT -64 at 0 the most", ok);

    /* QUANTIZE OFF: a step's NUDGE and TIME add; a fast step: no more than 7/16 of it */
    t = seq1(0);
    step_set_nudge(&t->step[2], 3);
    mslot(t, 0, MS_MODW, MD_TIME, 63);
    t->mw = 127;
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok = n == 16u && at2[2] == blk(2, 3u * P16 / NUDGE_DIV + h) && at2[4] == blk(4, h);
    t->p[P_SDIV] = 3;                                   /* 1/32 at 120 BPM: 7/16 of it is 1205 samples < 30 ms */
    t->p[P_SQNT] = 1;
    P16 = div_samples(3);
    n = play(t, (15u * P16 + 2000u) / CTL, at2, 32);
    ok &= n == 16u && at2[6] == blk(6, P16 * 7u / NUDGE_DIV) && P16 * 7u / NUDGE_DIV < h;
    t->mw = 0;
    bad += check("  TIME: QUANTIZE OFF adds to the step's NUDGE; a short step: at most 7/16 of it (as NUDGE)", ok);

    /* saved: a project keeps the slots */
    t = seq1(1);
    mslot(t, 0, MS_RAND, MD_VEL, 20);
    mslot(t, 1, MS_SH, MD_TIME, -12);
    song.playing = 0;
    ok = project_save_as(1, "HUMAN") == 0;
    mslots_off(t);
    project_load(1);
    ok &= t->p[P_M1DST] == MD_VEL && t->p[P_M1AMT] == 20 && t->p[P_M2DST] == MD_TIME && t->p[P_M2AMT] == -12 &&
          t->p[P_M2SRC] == MS_SH;
    bad += check("  a project keeps VEL / TIME slots (FUN10, no format change)", ok);
    return bad;
}

/* -------------------------------------------------------------- KITS --- */
/* (#216) DEEP TIGHT PERC: appended kit values 9..11 and DRUM presets 1..3 (the first, DRUM KIT, as it was); each lane one
 * of the five kits' (DEEP's kick tuned down); the lanes' names; they play; a project keeps them; what older firmware
 * makes of them (its KIT range ends at 77: a stored 9..11 lands there) */
static int kits(void)
{
    static const uint8_t SRC[3][DV_NLANE] = {{DK_77, DK_80, DK_77, DK_80, DK_80, DK_77, DK_80, DK_10},
                                            {DK_55, DK_66, DK_80, DK_55, DK_55, DK_55, DK_66, DK_80},
                                            {DK_10, DK_77, DK_77, DK_66, DK_66, DK_66, DK_77, DK_66}};
    int bad = 0, ok;
    uint32_t k, l;
    const param_desc_t *d = &ENG_DRUM.edit[0];
    param_desc_t old = *d;
    track_t *t;
    int32_t s;
    static int32_t kbuf[2 * CTL];
    ok = DK_DEEP == 9 && DK_TIGHT == 10 && DK_PERC == 11 && DK_DX == 12 && DK_COUNT == 13 && DV_NKIT == 8 && d->max == DK_DX &&
         str_eq(d->names[DK_DEEP], "DEEP") && str_eq(d->names[DK_TIGHT], "TIGHT") && str_eq(d->names[DK_PERC], "PERC");
    for (k = 0; k < 3u; k++)
        for (l = 0; l < DV_NLANE; l++) {
            const dv_kit_t *a = &DV_KIT[5u + k][l], *b = &DV_KIT[SRC[k][l] - DK_80][l];
            if (k == 0u && l == DV_KICK)
                ok &= a->p16 == b->p16 - 32 && a->run == b->run && !memcmp(a->v, b->v, sizeof a->v);   /* (-2 st) */
            else
                ok &= !memcmp(a, b, sizeof *a);
        }
    bad += check("KITS (#216): DEEP TIGHT PERC appended (9..11), each lane one of the five kits' (DEEP's kick 2 st down)", ok);

    ok = ENG_DRUM.npresets == 5u && str_eq(ENG_DRUM.presets[0].name, "DRUM KIT") && ENG_DRUM.presets[0].e[0] == DK_STD &&
         str_eq(ENG_DRUM.presets[1].name, "DEEP KIT") && ENG_DRUM.presets[1].e[0] == DK_DEEP &&
         str_eq(ENG_DRUM.presets[2].name, "TIGHT KIT") && ENG_DRUM.presets[2].e[0] == DK_TIGHT &&
         str_eq(ENG_DRUM.presets[3].name, "PERC KIT") && ENG_DRUM.presets[3].e[0] == DK_PERC && str_eq(ENG_DRUM.presets[4].name, "DRUM-X") && ENG_DRUM.presets[4].e[0] == DK_DX && preset_cat(ENGI_DRUM, 4u) == CAT_DRUM;
    for (k = 1; k < 4u; k++)
        ok &= preset_cat(ENGI_DRUM, k) == CAT_DRUM;
    bad += check("  DRUM's presets: DRUM KIT first as before, then DEEP KIT, TIGHT KIT, PERC KIT (category DRUM)", ok);

    ui_power_on();
    t = &trk[0];
    host_preset(t, ENGI_DRUM, 3);
    t->engine = t->eng_req;
    ok = t->p[P_E0] == DK_PERC && str_eq(drum_lane_name(t, DV_TOM), "CONGA") && str_eq(drum_lane_name(t, DV_RIM), "CLAVE");
    ok &= drum_gm(t->p, 45, &s) == DV_KTYPE(8u, DV_TOM);
    t->p[P_E0] = DK_DEEP;
    ok &= str_eq(drum_lane_name(t, DV_BELL), "CYM") && drum_gm(t->p, 36, &s) == DV_KTYPE(6u, DV_KICK);
    t->p[P_E0] = DK_TIGHT;
    ok &= str_eq(drum_lane_name(t, DV_BELL), "BELL");
    for (k = DK_DEEP; k <= DK_PERC; k++) {              /* each plays its 8 lanes */
        uint32_t i, live = 0;
        t->p[P_E0] = (int16_t)k;
        for (i = 0; i < 8u; i++)
            trk_note_on(t, DRUM_LANE_NOTE[i], 110);
        events_block(CTL);
        mix_block(kbuf, CTL);
        for (i = 0; i < DV_NLANE; i++)
            live += drum_kit_of(t)[i].v.live;
        ok &= live == 8u;
        trk_all_off(t);
    }
    bad += check("  the lanes' names (DEEP: CYM; PERC: CONGA CLAVE), the GM notes on the kit's pieces, the 8 lanes play", ok);

    t->p[P_E0] = DK_TIGHT;
    song.playing = 0;
    ok = project_save_as(2, "KITS") == 0;
    t->p[P_E0] = DK_STD;
    project_load(2);
    ok &= t->p[P_E0] == DK_TIGHT;
    old.max = DK_77;                                    /* (1.5.1's KIT: STD .. 77) */
    for (k = DK_DEEP; k <= DK_PERC; k++)
        ok &= param_fit(&old, (int32_t)k) == DK_77 && param_fit(d, (int32_t)k) == (int32_t)k;
    bad += check("  a project keeps the kit; a 1.5.1-range KIT reads 9..11 as 77 (the README's downgrade note)", ok);
    return bad;
}

/* -------------------------------------------------------------- LEDS --- */
/* (#212) MENU > LEDS INV LO / INV HI: appended values (0..3 as before); INV's lit ones at the mid level (HI ~12.5 %, LO
 * ~8 %: the darker mid), the active ones dark as INV; saved as INV in the zoom field (older firmware: plain INV) with
 * the level in a spare settings byte; older settings read as before */
static int leds_lit(const uint8_t *a, uint32_t id) { uint8_t q = led_pos[id]; return q != 0xFF && ((a[q >> 3] >> (q & 7u)) & 1u); }
static int leds(void)
{
    int bad = 0, ok;
    uint32_t id, v, key = 14u + 5u, btn;
    ui_power_on();
    ui_leds();                                          /* (led_pos: a made-up matrix, every LED its own place) */
    for (id = 0; id < 41u; id++)
        led_pos[id] = (uint8_t)((id / 4u) << 3 | (1u + id % 4u));
    btn = panel.btn[B_SEQ];
    ok = menu_n(MI_LEDS) == 6u && str_eq(menu_vname(MI_LEDS, 3), "INV") && str_eq(menu_vname(MI_LEDS, 4), "INV LO") &&
         str_eq(menu_vname(MI_LEDS, 5), "INV HI") && str_eq(menu_vname(MI_LEDS, 0), "OFF") && menu_get(MI_LEDS) == 2u;
    for (v = 0; v < 6u; v++) {
        menu_put(MI_LEDS, v);
        ok &= menu_get(MI_LEDS) == v && (settings_leds == LEDS_INV) == (v >= 3u) && leds_inv() == (v > 3u ? v - 3u : 0u);
    }
    bad += check("LEDS (#212): OFF DIM LO DIM HI INV, then INV LO INV HI appended; each its own value", ok);

    menu_put(MI_LEDS, 3);                               /* INV: the idle key and button lit */
    frame(); ui_leds();
    ok = leds_lit(fm1_led, key) && leds_lit(fm1_led, btn) && !leds_lit(fm1_led_mid, key) && !host_dim_lo;
    menu_put(MI_LEDS, 5);                               /* INV HI: at the mid level, not lit */
    frame(); ui_leds();
    ok &= !leds_lit(fm1_led, key) && leds_lit(fm1_led_mid, key) && !leds_lit(fm1_led, btn) && leds_lit(fm1_led_mid, btn) &&
          !host_dim_lo;
    menu_put(MI_LEDS, 4);                               /* INV LO: the same, the darker mid level */
    frame(); ui_leds();
    ok &= !leds_lit(fm1_led, key) && leds_lit(fm1_led_mid, key) && host_dim_lo;
    key_down(5); frame(); ui_leds();                    /* a key held: dark in every INV */
    ok &= !leds_lit(fm1_led, key) && !leds_lit(fm1_led_mid, key);
    key_up(5); frame();
    menu_put(MI_LEDS, 2);
    frame(); ui_leds();
    ok &= !leds_lit(fm1_led_mid, key) && !host_dim_lo;
    bad += check("  INV: idle lit; INV HI / LO: idle at the mid level (LO the darker one), a held key dark; DIM HI as before", ok);

    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        menu_put(MI_LEDS, 5);
        settings_export(&p);
        ok = p.zoom == (LEDS_TAG | LEDS_INV) && leds_from_stored(p.zoom) == LEDS_INV && leds_stored_ok(p.zoom) &&
             p.favorites.factory[15][25] == 2u;          /* (older firmware: INV, the byte kept unread) */
        menu_put(MI_LEDS, 2);
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_LEDS) == 5u;
        p.favorites.factory[15][25] = 0;                 /* (a setting of before: INV) */
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_LEDS) == 3u;
        p.favorites.factory[15][25] = 7;                 /* (an unknown level: INV) */
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_LEDS) == 3u;
        p.favorites.factory[15][25] = 1;                 /* (a level with DIM HI: DIM HI) */
        p.zoom = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_LEDS) == 2u && !leds_inv();
        menu_put(MI_LEDS, 2);
    }
    bad += check("  saved: INV in the LEDS field (older firmware: INV), the level in a spare byte; older settings as before", ok);
    return bad;
}

/* ------------------------------------------------------------ SELECT --- */
/* (#84) MENU > CONTROL > SELECT MODW / VEL after BPM / PAGE: MODW the selected track's mod wheel (the matrix's MODW, CC1
 * out on its channel), VEL the keys' velocity (sounding, MIDI OUT, recorded); GLO + SELECT the tempo; the bits (PAGE's
 * as 1.5.5's), saved; older settings BPM */
static int select_play(void)
{
    int bad = 0, ok;
    track_t *t;
    int16_t b0;
    uint32_t mo, pk;
    ui_power_on();
    ok = menu_n(MI_SELECT) == 4u && str_eq(menu_vname(MI_SELECT, 0), "BPM") && str_eq(menu_vname(MI_SELECT, 1), "PAGE") &&
         str_eq(menu_vname(MI_SELECT, 2), "MODW") && str_eq(menu_vname(MI_SELECT, 3), "VEL") && menu_get(MI_SELECT) == 0u;
    menu_put(MI_SELECT, 1);
    ok &= ui_prefs2 == 0x10u && sel_mode() == SEL_PAGE;     /* (PAGE: 1.5.5's bit) */
    menu_put(MI_SELECT, 2);
    ok &= ui_prefs2 == 0x40u && sel_mode() == SEL_MODW;
    menu_put(MI_SELECT, 3);
    ok &= ui_prefs2 == 0x50u && sel_mode() == SEL_VEL;
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p);
        menu_put(MI_SELECT, 0);
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_SELECT) == 3u;
        p.favorites.factory[15][24] = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_SELECT) == 0u;
    }
    bad += check("SELECT (#84): BPM PAGE MODW VEL; PAGE bit 4 as 1.5.5, MODW / VEL bit 6 too; saved, older settings BPM", ok);

    menu_put(MI_SELECT, SEL_MODW);
    go_home(); frame();
    t = TSEL;
    t->mw = 0;
    b0 = song.g[G_BPM];
    usb.config = 1;
    mo = mo_w;
    turn(EN_SELECT, 3);
    ok = t->mw == 3u && str_eq(ui.msg, "MODW 3") && song.g[G_BPM] == b0 && ui.home;
    ok &= mo_w == mo + 1u && midi_out_q[mo % MQ] == (0x0Bu | (0xB0u | trk_midi_ch(song.sel)) << 8 | 1u << 16 | 3u << 24);
    turn(EN_SELECT, -5);
    ok &= t->mw == 0u;                                  /* (stops at 0) */
    t->p[P_M1SRC] = MS_MODW; t->p[P_M1DST] = MD_VEL; t->p[P_M1AMT] = 63;
    turn(EN_SELECT, 127);
    ok &= t->mw == 127u;
    trk_note_on(t, 60, 40);
    ok &= t->m_vel == 127u;                             /* (the matrix's MODW: the wheel SELECT turns) */
    trk_all_off(t);
    t->p[P_M1SRC] = t->p[P_M1DST] = t->p[P_M1AMT] = 0;
    btn_down(B_GLO); frame();
    turn(EN_SELECT, 2);
    ok &= song.g[G_BPM] == b0 + 2 && t->mw == 127u;     /* GLO + SELECT: the tempo */
    btn_up(B_GLO); frames(400);
    song.g[G_BPM] = b0;
    t->mw = 0;
    bad += check("  MODW: the track's mod wheel 0..127 (shown), CC1 out on its channel, the matrix's MODW; GLO + SELECT tempo", ok);

    menu_put(MI_SELECT, SEL_VEL);
    go_home(); frame();
    ok = kb_vel == 100u;
    turn(EN_SELECT, -40);
    ok &= kb_vel == 60u && str_eq(ui.msg, "VEL 60") && song.g[G_BPM] == b0;
    mo = mo_w;
    key_down(white(3)); frame();
    pk = midi_out_q[mo % MQ];
    ok &= mo_w == mo + 1u && (pk & 0xF0FFu) == 0x9009u && pk >> 24 == 60u && TSEL->m_vel == 60u;
    key_up(white(3)); frame();
    turn(EN_SELECT, -100);
    ok &= kb_vel == 1u;
    turn(EN_SELECT, 200);
    ok &= kb_vel == 127u;
    kb_vel = 100;
    bad += check("  VEL: the keys' velocity 1..127 (100 at power-on), shown, sounding and sent; GLO + SELECT as ever", ok);

    menu_put(MI_SELECT, SEL_MODW);
    go_title("CLOCK");
    ok = str_eq(help_page(), "[K1]TEMPO[GLO]HOLD: TAP");
    menu_put(MI_SELECT, 0);
    ok &= str_eq(help_page(), "[SELECT]TEMPO[GLO]HOLD: TAP");
    bad += check("  HELP: CLOCK's hint names KNOB 1 the tempo with MODW / VEL too", ok);
    return bad;
}

int main(void)
{
    int bad = humanize() + kits() + leds() + select_play();
    printf("%s\n", bad ? "1.5.6 TEST FAILED" : "1.5.6 tests passed");
    return bad != 0;
}
