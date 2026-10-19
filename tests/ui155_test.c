/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.5.5, fixes and UI, on the real UI sources (with the stubs of tests/ui_test.c):
 *   PAN        (#199) a bipolar value (-64..63, F_BIPCT) reads -100..+100 %: +100 at the top (was +98), the stored
 *              range and the sound unchanged; the MIXER strip's number the same
 *   (and AUTOMATION KNOB 3, SELECT PAGE, PROJECT ERASE, CLEAN INIT; OCT- on the press as in 1.5.1 (1.5.6: #205 taken
 *   out), next to EUCLID, FILL)
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* ---------------------------------------------------------------- PAN --- */
static int fmt_is(const param_desc_t *d, int32_t v, const char *want)
{
    char val[12];
    const char *unit;
    param_format(d, v, val, &unit);
    if (!str_eq(val, want))
        printf("  %s %d: \"%s\", want \"%s\"\n", d->label, (int)v, val, want);
    return str_eq(val, want) && str_eq(unit, "%");
}
static int pan_range(void)
{
    int bad = 0, ok = 1;
    int32_t v, last = -1000;
    char val[12];
    const char *unit;
    ok &= fmt_is(&TP[P_PAN], 63, "+100") && fmt_is(&TP[P_PAN], -64, "-100") && fmt_is(&TP[P_PAN], 0, "0");
    ok &= fmt_is(&TP[P_PAN], 32, "+50") && fmt_is(&TP[P_PAN], -32, "-50") && fmt_is(&TP[P_PAN], 1, "+1") &&
          fmt_is(&TP[P_PAN], -1, "-1");
    for (v = -64; v <= 63; v++) {                       /* every value: rising, never twice the same number */
        int32_t n = 0, i;
        param_format(&TP[P_PAN], v, val, &unit);
        for (i = val[0] == '+' || val[0] == '-'; val[i]; i++)
            n = n * 10 + (val[i] - '0');
        if (val[0] == '-')
            n = -n;
        ok &= n > last && n >= -100 && n <= 100;
        last = n;
    }
    bad += check("PAN (#199): -64..63 read -100..+100 %, +100 at the top (was +98), rising, each value its own", ok);
    ok = fmt_is(&TP[P_M1AMT], 63, "+100") && fmt_is(&TP[P_ED_FLT], -64, "-100") &&
         TP[P_PAN].min == -64 && TP[P_PAN].max == 63 && TP[P_PAN].def == 0;
    bad += check("  every bipolar value the same (MOD AMT, ENV FLT); the stored range -64..63 as before", ok);
    return bad;
}

/* --------------------------------------------- AUTOMATION's KNOB 3 --- */
static uint32_t ev_at(uint32_t r) { uint16_t rw[EV_ROWS]; return r < ev_list(rw) ? rw[r] : 0xFFFFu; }
static uint32_t ev_sel(void) { uint16_t rw[EV_ROWS]; return ev_cur(rw, 0); }
static void kslow(uint32_t role, int32_t s)        /* a knob turned by s detents, unaccelerated */
{
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    turn(role, s);
}
static int auto_k3(void)
{
    int bad = 0, ok = 1, i;
    track_t *t;
    uint32_t r0, c0, c1, id1;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_home();
    ui.cursor = 0;
    go_page(GR_EVENTS);
    frame();
    kslow(EN_K1, 2);                                    /* + ADD on step 1: a lock of the page's first parameter */
    press(B_OCTUP);
    kslow(EN_K1, 1);
    kslow(EN_K3, 3);                                    /* .. and one three parameters on */
    press(B_OCTUP);
    ok = motion_count(t) == 2u && (ev_at(2) >> 8) == EVK_REC && (ev_at(3) >> 8) == EVK_REC;
    c0 = ev_at(2);
    c1 = ev_at(3);
    id1 = MOTION_ID(&motion.event[c1 & 0xFFu]);
    ok &= MOTION_ID(&motion.event[c0 & 0xFFu]) < id1 && ui.ev_row == 3u && ev_sel() == c1;
    bad += check("AUTOMATION: two locks on step 1, the second (a later parameter) selected", ok);
    r0 = ui.ev_row;
    for (i = 0; i < 8; i++) {                           /* KNOB 3 left, past the first one's parameter */
        kslow(EN_K3, -1);
        ok &= ui.ev_row == r0 && ev_sel() == c1 && ev_at(2) == c0 && ev_at(3) == c1;
    }
    ok &= MOTION_ID(&motion.event[c1 & 0xFFu]) < MOTION_ID(&motion.event[c0 & 0xFFu]);
    bad += check("  KNOB 3 (#199): the row stays where it is and selected while its parameter goes past the other's",
        ok);
    ok = 1;
    for (i = 0; i < 8; i++) {                           /* .. and back */
        kslow(EN_K3, 1);
        ok &= ui.ev_row == r0 && ev_sel() == c1 && ev_at(2) == c0;
    }
    ok &= MOTION_ID(&motion.event[c1 & 0xFFu]) > MOTION_ID(&motion.event[c0 & 0xFFu]);
    bad += check("  and back to the right: no jump either way", ok);
    kslow(EN_K1, -1);                                    /* the first record: its rows as they were */
    ok = ev_sel() == c0;
    for (i = 0; i < 4; i++) {
        kslow(EN_K3, 1);
        ok &= ui.ev_row == 2u && ev_sel() == c0 && ev_at(3) == c1;
    }
    bad += check("  the other row the same (a step's records listed in the order they were added)", ok);
    return bad;
}

/* a HELP hint fits its footer row with every word (as tests/ui_test.c test_help) */
static int help_fits(const char *s)
{
    khint_t kh[3];
    char b[48];
    uint32_t n = help_parse(s, kh, b, sizeof b), i;
    int32_t w = 6 * ((int32_t)n - 1);
    int ok = n >= 2u && n <= 3u;
    for (i = 0; i < n; i++) {
        ok &= kh[i].key < KC_COUNT && kh[i].act[0] != 0;
        w += kh_w(kh[i].key, kh[i].act);
    }
    if (!ok || w > 224) printf("  help: \"%s\" %u hints, %d px of 224\n", s, n, w);
    return ok && w <= 224;
}

/* --------------------------------------------------- SELECT = PAGE --- */
static int title_is(const char *t) { return !ui.home && str_eq(cur_page()->title, t); }
static int select_page(void)
{
    int bad = 0, ok;
    int16_t b0;
    uint8_t before[32];
    uint32_t i;
    ui_power_on();
    memcpy(before, favorites.factory[15], 32);
    ok = menu_get(MI_SELECT) == 0u && str_eq(MI_NAME[MI_SELECT], "SELECT") && MI_TAB[MI_SELECT] == MTAB_CONTROL &&
         MI_SELECT < MI_VALUES && str_eq(menu_vname(MI_SELECT, 0), "BPM") && str_eq(menu_vname(MI_SELECT, 1), "PAGE") &&
         mtab_rows(MTAB_CONTROL) <= MTAB_ROWS;
    go_title("ENV"); frame();
    b0 = song.g[G_BPM];
    turn(EN_SELECT, 2);
    ok &= song.g[G_BPM] == b0 + 2 && title_is("ENV");
    bad += check("SELECT (#188): MENU > CONTROL > SELECT, BPM by default: SELECT sets the tempo as before", ok);

    menu_put(MI_SELECT, 1);
    ok = menu_get(MI_SELECT) == 1u && favorites.factory[15][24] == (uint8_t)(before[24] | 0x10u);
    for (i = 0; i < 32u; i++)
        ok &= i == 24u || favorites.factory[15][i] == before[i];
    {   /* saved with the settings; settings from before it read BPM */
        persist_t p;
        settings_export(&p);
        ui_prefs2 = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_SELECT) == 1u;
        p.favorites.factory[15][24] &= (uint8_t)~0x10u;
        ok &= settings_import(&p, (int)sizeof p) == 1 && menu_get(MI_SELECT) == 0u;
        menu_put(MI_SELECT, 1);
    }
    bad += check("  PAGE: a bit of its own (MENU's 1.2 flags, bit 4), kept with the settings; 0 (older settings) = BPM", ok);

    {   /* Custom: settings that were never saved start on PAGE; saved settings (even a 0) keep their choice */
        __typeof__(favorites) fav = favorites;
        __typeof__(settings) set = settings;
        memset(&favorites, 0, sizeof favorites); memset(&settings, 0, sizeof settings);
        settings_init();
        ok = menu_get(MI_SELECT) == 1u;
        menu_put(MI_SELECT, 0);
        settings_init();                                  /* (valid magic now: nothing is re-defaulted) */
        ok &= menu_get(MI_SELECT) == 0u;
        favorites = fav; settings = set;
        palette_apply();
    }
    bad += check("  PAGE is the default of settings that were never saved; a saved BPM stays BPM (no format change)", ok);

    b0 = song.g[G_BPM];
    go_title("ENV"); frame();
    turn(EN_SELECT, 1);
    ok = title_is("ENV DEST");
    turn(EN_SELECT, 1);
    ok &= title_is("ENV");                              /* (round) */
    turn(EN_SELECT, -1);
    ok &= title_is("ENV DEST");
    go_title("LFO"); frame();
    turn(EN_SELECT, 2);
    ok &= title_is("LFO DEST");
    turn(EN_SELECT, -3);
    ok &= title_is("MOD");                              /* LFO > LFO SYNC > LFO DEST > LFO2 > LFO2 SYNC > MOD, round */
    ok &= song.g[G_BPM] == b0;
    bad += check("  PAGE: SELECT steps the family's pages, a detent each, round both ways; the tempo stays", ok);

    go_home(); frame();
    turn(EN_SELECT, 1);
    ok = title_is("MIXER");
    turn(EN_SELECT, 1);
    ok &= title_is("CLOCK");
    turn(EN_SELECT, 1);
    ok &= ui.home;
    turn(EN_SELECT, -1);
    ok &= title_is("CLOCK");
    b0 = song.g[G_BPM];
    turn(EN_K1, 1);
    ok &= song.g[G_BPM] == b0 + 1;                      /* CLOCK's KNOB 1: the tempo */
    bad += check("  PAGE: HOME -> MIXER -> CLOCK -> HOME (as HOME's taps), back too; CLOCK's KNOB 1 still the tempo", ok);

    go_title("EDIT 1"); frame();
    for (i = 0; i < 12u && ok; i++) {                   /* EDIT's pages: only the ones the engine shows */
        turn(EN_SELECT, 1);
        ok &= !ui.home && cur_page()->fam == FAM_EDIT && page_visible(ui.page);
    }
    go_title("SONG"); frame();
    turn(EN_SELECT, 1);
    ok &= title_is("SONG");                             /* (GLO: one page) */
    bad += check("  PAGE: EDIT's round is the engine's pages; a family of one page stays", ok);

    go_title("ENV"); frame();
    b0 = song.g[G_BPM];
    btn_down(B_GLO); frame();
    turn(EN_SELECT, 3);
    ok = song.g[G_BPM] == b0 + 3 && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    ok &= title_is("ENV");
    btn_down(B_LFO); frame();                           /* LFO held (MOMENTARY): no page */
    turn(EN_SELECT, 1);
    ok &= title_is("ENV");
    btn_up(B_LFO); frame();
    bad += check("  PAGE: GLO + SELECT is the tempo (no page); nothing with LFO held", ok);

    go_title("CLOCK"); frame();
    ok = help_page() && str_eq(help_page(), "[K1]TEMPO[GLO]HOLD: TAP") && help_fits(help_page());
    menu_put(MI_SELECT, 0);
    ok &= str_eq(help_page(), "[SELECT]TEMPO[GLO]HOLD: TAP");
    bad += check("  HELP: CLOCK's hint says KNOB 1 is the tempo with PAGE", ok);
    return bad;
}

/* ---------------------------------------------- OCT- on the press (1.5.6) --- */
/* 1.5.6: 1.5.5's "OCT- held + a knob = default" (#205) is taken out to keep OCT- as it was: the octave moves when OCT-
 * is pressed (as in 1.5.1), and a knob turned with OCT- held is an ordinary turn. EDIT 2's hint is EDIT 1's again */
static int oct_press(void)
{
    int bad = 0, ok;
    track_t *t;
    int16_t v;
    uint32_t mv;
    ui_power_on();
    t = TSEL;
    go_title("ENV"); frame();
    song.octave = 0;
    btn_down(B_OCTDN); frame();
    ok = song.octave == -1;                             /* (on the press) */
    btn_up(B_OCTDN); frame();
    ok &= song.octave == -1;                            /* (nothing more when let go) */
    press(B_OCTDN);
    ok &= song.octave == -2;
    press(B_OCTUP);
    ok &= song.octave == -1;
    btn_down(B_OCTDN); frame();
    ok &= song.octave == -2;
    btn_down(B_OCTUP); frame(); btn_up(B_OCTUP); btn_up(B_OCTDN); frame(); frame();
    ok &= song.octave == 0;                             /* (both: 0) */
    for (v = 0; v < 5; v++) press(B_OCTDN);
    ok &= song.octave == -3;                            /* (stops at -3) */
    song.octave = 0;
    bad += check("OCT- (1.5.6, #205 taken out): the octave down on the press as in 1.5.1, OCT+ up, both 0, -3 the floor", ok);

    t->p[P_ATK] = 50;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 2);
    ok = t->p[P_ATK] == 52 && !str_eq(ui.msg, "ATK DEFAULT");
    btn_up(B_OCTDN); frame();
    ok &= song.octave == -1;
    song.octave = 0;
    go_title("EDIT 2");
    ok &= str_eq(help_page(), "[K1-4]SOUND[EDIT]HOLD: ENGINES");
    bad += check("  OCT- held + KNOB: an ordinary turn (no default), the octave as pressed; EDIT 2's hint as EDIT 1's", ok);

    hold(B_HOME);
    ok = ui.menu != 0;
    mv = menu_get(ui.menu_sel);
    press(B_OCTDN);
    ok &= ui.menu != 0 && song.octave == 0;             /* MENU: OCT- steps the setting back (as before), no octave */
    menu_put(ui.menu_sel, mv);                          /* (the setting as it was) */
    press(B_HOME);
    ok &= ui.menu == 0;
    bad += check("  the menu: OCT- is the menu's as before (the setting back), the octave untouched", ok);
    return bad;
}

/* ------------------------------------------------- SAVE > PROJECT ERASE --- */
static int project_erase_ui(void)
{
    int bad = 0, ok;
    char a[24], b[24];
    ui_power_on();
    song.playing = 0;
    project_save_as(1, "DUB SKETCH");
    project_save_as(2, "KEEP ME");
    song.g[G_SLOT] = 2;                                 /* B */
    go_page(GR_SLOTS); frame();
    ok = act_cols() == 14u && str_eq(act_name(1), "INIT") && str_eq(act_name(2), "LOAD") && str_eq(act_name(3), "SAVE");
    turn(EN_K2, 1); turn(EN_K2, 1);
    ok &= ui.act == 2u && act_ready();
    press(B_OCTUP);
    ok &= ui.confirm == CF_ERASE_PROJ && ui.confirm_trk == 1u;
    confirm_text(a, b);
    ok &= str_eq(a, "ERASE PROJECT B?") && str_eq(b, "DUB SKETCH");
    press(B_OCTDN);
    ok &= !ui.confirm && project_used(1);
    bad += check("SAVE > PROJECT (#199): KNOB 2 ERASE, OCT+ asks \"ERASE PROJECT B?\" with its name, OCT- keeps it", ok);

    turn(EN_K2, 1); turn(EN_K2, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    ok = !ui.confirm && !project_used(1) && project_used(2) && str_eq(ui.msg, "ERASED (RAM)") && cur_page()->graph == GR_SLOTS;
    turn(EN_K2, 1); turn(EN_K2, 1);
    ok &= !act_ready();
    press(B_OCTUP);
    ok &= !ui.confirm && str_eq(ui.msg, "EMPTY SLOT");
    bad += check("  OCT+ again: the slot empty, the others kept; an empty slot: nothing to erase", ok);

    song.g[G_SLOT] = 3;
    song.playing = 1;
    turn(EN_K2, 1); turn(EN_K2, 1);
    ok = !act_ready();
    press(B_OCTUP);
    ok &= !ui.confirm && str_eq(ui.msg, "STOP TO ERASE") && project_used(2);
    song.playing = 0;
    turn(EN_K3, 1);
    ok &= ui.act == 3u && str_eq(act_name(2), "LOAD");
    go_page(GR_SLOTS);
    ok &= help_page() && str_eq(help_page(), "[K2]INIT, ERASE[K4]SAVE") && help_fits(help_page());
    bad += check("  not while playing (STOP TO ERASE); LOAD and SAVE where they were; its HELP hint", ok);
    return bad;
}

/* --------------------------------------------------------- CLEAN INIT --- */
/* a sound with everything added: sends, INSERT, LFO / ENV amounts, MOD, GLIDE, MONO, its E1..E8 off their defaults */
static void dress(track_t *t)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = (int16_t)(30 + i);
    t->p[P_ITYPE] = 2; t->p[P_IMIX] = 90;
    t->p[P_LD_FLT] = 20; t->p[P_LD_PIT] = -9; t->p[P_LD_AMP] = 40; t->p[P_ED_FLT] = 33; t->p[P_ED_SHP] = -12;
    t->p[P_M1SRC] = 1; t->p[P_M1DST] = 2; t->p[P_M1AMT] = 50; t->p[P_M3AMT] = -20;
    t->p[P_GLIDE] = 60; t->p[P_VOICE] = V_MONO; t->p[P_SPRD] = 70;
    t->p[P_ATK] = 80; t->p[P_SUS] = 10; t->p[P_REL] = 100;
    t->p[P_LEVEL] = 77; t->p[P_PAN] = -30;
}
static int clean_is(const track_t *t, uint32_t e, int16_t lvl, int16_t pan)
{
    const engine_t *en = ENGINES[e];
    uint32_t i;
    int ok = t->eng_req == e && t->p[P_LEVEL] == lvl && t->p[P_PAN] == pan;
    for (i = 0; i < 4u; i++)
        ok &= t->p[P_DIST + i] == 0;
    ok &= t->p[P_ITYPE] == 0 && t->p[P_LD_FLT] == 0 && t->p[P_LD_PIT] == 0 && t->p[P_LD_AMP] == 0 && t->p[P_ED_FLT] == 0 &&
          t->p[P_ED_SHP] == 0 && t->p[P_M1SRC] == 0 && t->p[P_M1DST] == 0 && t->p[P_M1AMT] == 0 && t->p[P_M3AMT] == 0 &&
          t->p[P_GLIDE] == 0 && t->p[P_VOICE] == V_POLY && t->p[P_SPRD] == 0;
    for (i = 0; i < 8u; i++)
        ok &= t->p[P_E0 + i] == (e == ENGI_FM6 && i == 7u ? (int16_t)FM6_OWN : en->edit[i].def);
    if (e == ENGI_DRUM)
        ok &= t->p[P_ATK] == en->presets[0].env[0] && t->p[P_SUS] == en->presets[0].env[2] &&
              t->p[P_REL] == en->presets[0].env[3];
    else
        ok &= t->p[P_ATK] == 0 && t->p[P_SUS] == 127 && t->p[P_REL] == CLEAN_REL;
    if (e == ENGI_FM6) {
        uint8_t v[FP_SIZE + 1u];
        fm6_unpack(FM6_INIT, v);
        fm6_sanitize(v);
        ok &= !memcmp(v, fm6_patch[trk_index(t)], FP_SIZE) && fm6_slot[trk_index(t)] == FM6_OWN;
    }
    return ok;
}
/* a note on track t after the load: it sounds (some energy, below full scale), and ends after its release */
static int clean_sounds(track_t *t, uint32_t note)
{
    int32_t o[2u * CTL];
    uint32_t b, i, mq = song.master_q12, act = 0;
    uint64_t e = 0;
    int32_t pk = 0;
    song.master_q12 = 4096;
    for (b = 0; b < 256u && (b < 1u || t->xf_on || t->eng_req != t->engine); b++)
        mix_block(o, CTL);
    trk_note_on(t, note, 100);
    for (b = 0; b < 96u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            e += (uint64_t)a;
            if (a > pk) pk = a;
        }
    }
    trk_note_off(t, note);
    for (b = 0; b < 1500u; b++)
        mix_block(o, CTL);
    for (i = 0; i < NVOICE; i++)
        act += t->v[i].active;
    song.master_q12 = mq;
    if (!(e > 0u && pk < 32767 && !act))
        printf("  %s: energy %llu peak %d voices left %u\n", ENGINES[t->engine % NENGINES]->name, (unsigned long long)e,
               (int)pk, act);
    return e > 0u && pk < 32767 && !act;
}
static int clean_init(void)
{
    int bad = 0, ok = 1, ok2 = 1, ok3 = 1;
    uint32_t r, e;
    track_t *t;
    char a[24], b[24];
    for (r = 0; r < NENG_SHOWN; r++) {                 /* every engine one can pick: TOOLS' KNOB 2 on, CLEAN */
        step_t st[NSTEP];
        ui_power_on();
        song.playing = 0;
        t = TSEL;
        e = eng_vis(r);
        set_engine(e);
        dress(t);
        my_steps(t);
        memcpy(st, t->step, sizeof st);
        go_page(GR_TOOLS); frame();
        turn(EN_K2, 1);
        ok &= ui.act == 2u && str_eq(act_name(1), "INIT");
        turn(EN_K2, 1);
        ok &= ui.act == 2u && str_eq(act_name(1), "CLEAN");
        press(B_OCTUP);
        ok &= ui.confirm == CF_CLEAN_SOUND;
        press(B_OCTUP);
        ok &= !ui.confirm && msg_is("CLEAN INIT") && !memcmp(st, t->step, sizeof st);
        if (!clean_is(t, e, 77, -30)) {
            printf("  CLEAN INIT on %s: not clean\n", ENGINES[e]->name);
            ok = 0;
        }
        ok3 &= clean_sounds(t, e == ENGI_DRUM ? 36u : 60u);
        hold(B_SAVE);                                   /* one undo: the dressed sound back */
        ok2 &= t->p[P_DIST] == 30 && t->p[P_ITYPE] == 2 && t->p[P_M1AMT] == 50 && t->p[P_ATK] == 80 && t->eng_req == e;
    }
    bad += check("CLEAN INIT (#190), every engine: its E1..E8 defaults, no sends, INSERT OFF, no LFO / ENV / MOD amounts, POLY,", ok);
    bad += check("  a neutral ENV (DRUM: its kit's), FM6 the init voice; LEVEL PAN and the steps kept; one undo", ok2);
    bad += check("  each engine's clean sound plays a note (heard, below full scale) and ends after its release", ok3);

    ui_power_on();
    t = TSEL;
    set_engine(ENGI_FM6);
    dress(t);
    go_page(GR_TOOLS); frame();
    turn(EN_K2, 1); turn(EN_K2, 1); turn(EN_K2, -1);
    ok = ui.act == 2u && str_eq(act_name(1), "INIT");   /* (left: back to INIT, then none) */
    turn(EN_K2, -1);
    ok &= ui.act == 0u;
    turn(EN_K2, 1);
    press(B_OCTUP);
    ok &= ui.confirm == CF_INIT_SOUND;
    press(B_OCTUP);
    ok &= msg_is("SOUND INIT") && t->p[P_E7] == 0 && t->p[P_DLY] == ENGINES[ENGI_FM6]->presets[0].fx[2] - 1;
    bad += check("  TOOLS' KNOB 2: right INIT, right again CLEAN, left back; INIT as before (the first preset, its sends)", ok);

    ui_power_on();
    go_home(); frame();
    lay_combo(B_EDIT, white(LY_CLEAN));
    ok = ui.confirm == CF_CLEAN_SOUND && LY_CLEAN == LY_INIT + 1u && LY_CLEAN < 16u;
    key_up(white(LY_CLEAN)); btn_up(B_EDIT); frame();
    confirm_text(a, b);
    ok &= str_eq(a, "CLEAN INIT SOUND?") && str_eq(b, "NO FX, NO MOD");
    dress(TSEL);
    press(B_OCTUP);
    ok &= msg_is("CLEAN INIT") && clean_is(TSEL, TSEL->eng_req, 77, -30);
    go_page(GR_TOOLS);
    ok &= str_eq(help_page(), "[K1]CLEAR[K2]INIT, CLEAN[OCT+]DO") && help_fits(help_page());
    bad += check("  EDIT + the key after INIT (F5): the dialog \"CLEAN INIT SOUND?\"; TOOLS' HELP hint", ok);
    return bad;
}

/* OCT- (on the press again in 1.5.6) next to 1.5.5's other OCT- / layer users: SEQ TOOLS' EUCLID (OCT- cancels, OCT+
 * applies, its knobs HITS STEPS ROT even with OCT- held) and FILL (GLO + E4) leave the octave alone; a plain OCT- after
 * them still lowers it. (MIDI LEARN's OCT- clear: tests/midi_control_test.c checks the octave stays) */
static int oct_layers(void)
{
    int bad = 0, ok;
    uint32_t h;
    ui_power_on();
    go_page(GR_ROLL); frame();
    song.octave = 0;
    eu.steps = 0;
    btn_down(B_SEQ); frame();
    key_down(white(7)); frame(); key_up(white(7)); frame();   /* F4: EUCLID */
    ok = eu.on;
    h = eu.hits;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 1);                                    /* (OCT- held: still EUCLID's HITS) */
    ok &= eu.hits == h + 1u;
    btn_up(B_OCTDN); frame();
    ok &= !eu.on && str_eq(ui.msg, "EUCLID CANCELLED") && song.octave == 0;
    key_down(white(7)); frame(); key_up(white(7)); frame();
    ok &= eu.on;
    btn_down(B_OCTUP); frame(); btn_up(B_OCTUP); frame();
    ok &= !eu.on && str_eq(ui.msg, "EUCLID APPLIED") && song.octave == 0;
    btn_up(B_SEQ); frames(400);
    bad += check("OCT- and EUCLID (#186): OCT- cancels, OCT+ applies, OCT- + KNOB 1 is HITS; no octave", ok);

    btn_down(B_GLO); frames(100);
    key_down(key_at(0, 6)); frames(50);                /* E4: FILL while held */
    ok = seq_fill == 1u && ui.layer == LAYER_GLO;
    key_up(key_at(0, 6)); frames(50);
    ok &= !seq_fill;
    btn_up(B_GLO); frames(400);
    ok &= song.octave == 0;
    press(B_OCTDN);
    ok &= song.octave == -1;
    song.octave = 0;
    bad += check("  FILL (GLO + E4) as before, the octave untouched; a plain OCT- after it the octave down", ok);
    return bad;
}

int main(void)
{
    int bad = pan_range() + auto_k3() + select_page() + oct_press() + project_erase_ui() + clean_init() + oct_layers();
    printf("%s\n", bad ? "1.5.5 UI TEST FAILED" : "1.5.5 ui tests passed");
    return bad != 0;
}

