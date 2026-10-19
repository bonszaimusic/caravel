/* SPDX-License-Identifier: GPL-3.0-only */
/* Visual prototype capture plus SELECT focus and independent knob-target checks. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static char path0[320];
static void save_screen(const char *path)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n240 240\n255\n");
    for (uint32_t i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[] = {(c >> 11) * 255u / 31u, ((c >> 5) & 63u) * 255u / 63u, (c & 31u) * 255u / 31u};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    for (uint32_t palette = 0; palette < NPALETTES; palette++) {
        for (uint32_t large = 0; large < 2; large++) {
            ui_power_on();
            settings.palette = palette;
            palette_set(palette);
            ui_prefs = large ? PREF_LARGE : 0;
            menu_put(MI_SELECT, SEL_PAGE);
            lfo2_set(0, L2_FIRST + L2_RATE, 45);
            lfo2_set(0, L2_FIRST + L2_WAVE, 1);
            lfo2_set(0, L2_FIRST + L2_PHASE, 32);
            lfo2_set(0, L2_FIRST + L2_TRIG, 1);
            for (uint32_t bottom = 0; bottom < 2; bottom++) {
                char path[320];
                go_title(bottom ? "LFO2 SYNC" : "LFO2"); frame();
                snprintf(path, sizeof path, "%s/palette-%02u-%s-%s.ppm", argv[1], palette,
                         large ? "large" : "normal", bottom ? "bottom" : "top");
                save_screen(path);
            }
            go_title("LFO2"); frame();
            int32_t rate = lfo2_get(0, L2_FIRST + L2_RATE);
            int32_t sync = lfo2_get(0, L2_FIRST + L2_SYNC);
            turn(EN_K1, 1);
            assert(lfo2_get(0, L2_FIRST + L2_RATE) == rate + 1);
            assert(lfo2_get(0, L2_FIRST + L2_SYNC) == sync);
            turn(EN_SELECT, 1);
            assert(cur_page()->graph == GR_LFO2SYNC);
            turn(EN_K1, 1);
            assert(lfo2_get(0, L2_FIRST + L2_SYNC) == sync + 1);
            assert(lfo2_get(0, L2_FIRST + L2_RATE) == rate + 1);
            turn(EN_SELECT, 1);
            assert(str_eq(cur_page()->title, "LFO DEST"));
            turn(EN_SELECT, 1);
            assert(cur_page()->graph == GR_MOD);
            turn(EN_SELECT, -1);
            assert(str_eq(cur_page()->title, "LFO DEST"));
            turn(EN_SELECT, -1);
            assert(cur_page()->graph == GR_LFO2SYNC);
            turn(EN_SELECT, -1);
            assert(cur_page()->graph == GR_LFO2);
            turn(EN_SELECT, -1);
            assert(str_eq(cur_page()->title, "LFO SYNC"));
            /* LFO (1) uses the same two groups; its K1-4 edit the track's own parameters */
            for (uint32_t b1 = 0; b1 < 2; b1++) {
                char path[320];
                go_title(b1 ? "LFO SYNC" : "LFO"); frame();
                snprintf(path, sizeof path, "%s/lfo1-palette-%02u-%s-%s.ppm", argv[1], palette,
                         large ? "large" : "normal", b1 ? "bottom" : "top");
                save_screen(path);
            }
            go_title("LFO"); frame();
            int32_t r1 = TSEL->p[P_LRATE], s1 = TSEL->p[P_LSYNC];
            turn(EN_K1, 1);
            assert(TSEL->p[P_LRATE] == r1 + 1 && TSEL->p[P_LSYNC] == s1);
            turn(EN_SELECT, 1);
            assert(cur_page()->graph == GR_LFOSYNC);
            turn(EN_K1, 1);
            assert(TSEL->p[P_LSYNC] == s1 + 1 && TSEL->p[P_LRATE] == r1 + 1);
            turn(EN_SELECT, 1);
            assert(cur_page()->graph == GR_LFO2);
            turn(EN_SELECT, -1);
            assert(cur_page()->graph == GR_LFOSYNC);
            turn(EN_SELECT, -1);
            assert(cur_page()->graph == GR_LFO);
            /* ARP and VOICE: the same two-group layout over their two normal pages */
            static const struct { const char *top, *bot, *name; uint32_t a, b; int ga, gb; } PAIRS[] = {
                {"ARP", "ARP 2", "arp", P_AMODE, P_ASWING, 0, 0}, {"VOICE", "VOICE 2", "voice", P_VOICE, P_ALLOC, 0, 0},
                {"ENV", "ENV DEST", "env", P_ATK, P_ED_FLT, 0, 0}, {"EDIT 1", "EDIT 2", "edit", P_E0, P_E4, 0, 0},
                {"LANES", "LANES 2", "lanes", P_LN0, P_LN4, 0, 0}, {"FX", "DLY", "fx-delay", P_DIST, G_DTIME, 0, 1},
                {"REVERB", "CHORUS", "reverb-chorus", G_RTYPE, G_CRATE, 1, 1}, {"INSERT", "INSERT 2", "insert", P_ITYPE, P_IA, 0, 0}};
            #define PV(k, ab) (PAIRS[k].g##ab ? &song.g[PAIRS[k].ab] : &TSEL->p[PAIRS[k].ab])
            for (uint32_t k = 0; k < sizeof PAIRS / sizeof PAIRS[0]; k++) {
                char path[320];
                uint16_t keep_eng = TSEL->eng_req;
                if (k == 4) { TSEL->eng_req = ENGI_DRUM; frame(); }   /* (LANES: DRUM only) */
                for (uint32_t b2 = 0; b2 < 2; b2++) {
                    go_title(b2 ? PAIRS[k].bot : PAIRS[k].top); frame();
                    snprintf(path, sizeof path, "%s/%s-palette-%02u-%s-%s.ppm", argv[1], PAIRS[k].name, palette,
                             large ? "large" : "normal", b2 ? "bottom" : "top");
                    save_screen(path);
                }
                go_title(PAIRS[k].top); frame();
                int32_t a0 = *PV(k, a), b0 = *PV(k, b);
                turn(EN_K1, 1);
                if (*PV(k, a) == a0) turn(EN_K1, -1);       /* (at its maximum: the other way) */
                assert(*PV(k, a) != a0 && *PV(k, b) == b0);
                turn(EN_SELECT, 1);
                assert(str_eq(cur_page()->title, PAIRS[k].bot));
                a0 = *PV(k, a);
                turn(EN_K1, 1);
                if (*PV(k, b) == b0) turn(EN_K1, -1);
                assert(*PV(k, b) != b0 && *PV(k, a) == a0);
                turn(EN_SELECT, -1);
                assert(str_eq(cur_page()->title, PAIRS[k].top));
                TSEL->eng_req = keep_eng; frame();
            }
            {   /* LFO DEST above MOD: KNOB 1 of the lower group picks the slot, 2..4 edit its SRC DST AMT */
                char p1[320];
                TSEL->p[P_M1SRC] = 1; TSEL->p[P_M1DST] = 2; TSEL->p[P_M1AMT] = 90;          /* slots 1 and 3 in use */
                TSEL->p[P_M1SRC + 6] = 2; TSEL->p[P_M1DST + 6] = 3; TSEL->p[P_M1AMT + 6] = 40;
                mod_ui_slot = 2;
                for (uint32_t b3 = 0; b3 < 2; b3++) {
                    go_title(b3 ? "MOD" : "LFO DEST"); frame();
                    snprintf(p1, sizeof p1, "%s/lfodest-mod-palette-%02u-%s-%s.ppm", argv[1], palette, large ? "large" : "normal", b3 ? "bottom" : "top");
                    save_screen(p1);
                }
                go_title("MOD"); frame();
                assert(dual_bottom(cur_page()));
                uint32_t slot0 = mod_ui_slot;
                int16_t d0 = TSEL->p[P_LD_PIT];
                turn(EN_K1, 1);
                assert(mod_ui_slot == slot0 + 1u && TSEL->p[P_LD_PIT] == d0);
                int16_t m0 = TSEL->p[P_M1SRC + 3u * mod_ui_slot];
                turn(EN_K2, 1);
                assert(TSEL->p[P_M1SRC + 3u * mod_ui_slot] != m0 && TSEL->p[P_LD_PIT] == d0);
                mod_ui_slot = 0;
            }
            {   /* MOD's DST card names the engine's own parameter (DRONE: SPREAD .. TONE), not E1 .. E8 */
                char dst[16];
                uint16_t keep = TSEL->eng_req;
                set_engine_of(TSEL, ENGI_DRONE); TSEL->engine = TSEL->eng_req;
                TSEL->p[P_M1SRC] = MS_LFO; TSEL->p[P_M1DST] = MD_E1; TSEL->p[P_M1AMT] = 64; mod_ui_slot = 0;
                go_title("MOD"); frame();
                const dual_t *dd = dual_of(cur_page(), 0);
                const page_t *rp = dual_row_page(dd, 1);
                int32_t v; const param_desc_t *d = dual_card(dd, rp, 1, 2, &v);
                assert(d && v == MD_E1);
                str_cpy(dst, mod_dst_name(TSEL, v), sizeof dst);
                assert(str_eq(dst, "SPREAD"));
                TSEL->p[P_M1SRC] = TSEL->p[P_M1DST] = TSEL->p[P_M1AMT] = 0;
                set_engine_of(TSEL, keep); TSEL->engine = TSEL->eng_req;
                snprintf(path0, sizeof path0, "%s/mod-drone-palette-%02u-%s.ppm", argv[1], palette, large ? "large" : "normal");
                save_screen(path0);
            }
            {   /* INSERT 2: KNOB 4 is FX HOLD, the device setting that MENU > VOICES has as well */
                go_title("INSERT 2"); frame();
                uint32_t h0 = menu_get(MI_FXHOLD) ? 1u : 0u;
                int16_t ia = TSEL->p[P_IA];
                turn(EN_K4, h0 ? -1 : 1);
                assert((menu_get(MI_FXHOLD) ? 1u : 0u) != h0 && TSEL->p[P_IA] == ia);
                turn(EN_K4, h0 ? 1 : -1);
                assert((menu_get(MI_FXHOLD) ? 1u : 0u) == h0);
            }
            {   /* SCL and CHORD: one page above, the keys in the lower block (captured; the chord for a C minor 7 on the root) */
                static const char *const T2[] = {"SCL", "CHORD"};
                TSEL->p[P_ROOT] = 3; TSEL->p[P_SCALE] = 2;
                for (uint32_t k = 0; k < 2; k++) {
                    char p2[320];
                    if (k) {
                        uint8_t tmp[CHORD_MAX];
                        TSEL->p[P_CHRD] = CH_DIA3; TSEL->p[P_VOICE] = V_POLY;
                        chord_build(TSEL, 62u, tmp);                       /* a chord played: D# MIN's second degree */
                    }
                    go_title(T2[k]); frame();
                    assert(dual_page(cur_page()) && !dual_bottom(cur_page()));
                    snprintf(p2, sizeof p2, "%s/%s-palette-%02u-%s.ppm", argv[1], k ? "chord" : "scl", palette, large ? "large" : "normal");
                    save_screen(p2);
                }
                TSEL->p[P_CHRD] = 0;
            }
            {   /* INSERT: the type's curve beside TYPE and MIX (a drive, a folder, a phaser); not asserted, captured */
                static const struct { const char *name; uint32_t type; } IT[] = {{"soft", IT_SOFT}, {"fold", IT_FOLD}, {"crush", IT_CRUSH}, {"phaser", IT_PHASER}};
                for (uint32_t k = 0; k < sizeof IT / sizeof IT[0]; k++) {
                    TSEL->p[P_ITYPE] = (int16_t)IT[k].type; TSEL->p[P_IMIX] = 100;
                    go_title("INSERT"); frame();
                    snprintf(path0, sizeof path0, "%s/insert-%s-palette-%02u-%s.ppm", argv[1], IT[k].name, palette, large ? "large" : "normal");
                    save_screen(path0);
                }
                TSEL->p[P_ITYPE] = 0;
            }
            {   /* SLICER: one page, its graph in the lower block; SELECT leaves it for the next view */
                go_title("SLICER"); frame();
                assert(dual_page(cur_page()) && !dual_bottom(cur_page()));
                snprintf(path0, sizeof path0, "%s/slicer-palette-%02u-%s.ppm", argv[1], palette, large ? "large" : "normal");
                TSEL->p[P_SLCR] = 1; TSEL->p[P_SLPAT] = 8; frame();
                save_screen(path0);
            }
            {   /* EDIT keeps its normal layout (and its picture) for FM6, SAMPLE and WHEEL */
                uint16_t keep = TSEL->eng_req;
                go_title("EDIT 1"); frame();
                assert(dual_page(cur_page()));
                TSEL->eng_req = ENGI_FM6; frame();
                assert(!dual_page(cur_page()));
                TSEL->eng_req = ENGI_SAMPLE; frame();
                assert(!dual_page(cur_page()));
                TSEL->eng_req = keep; frame();
            }
        }
    }
    {   /* the settings menu: SELECT PAGE walks the rows and on into the next tab; other SELECT modes leave it alone */
        ui_power_on();
        menu_put(MI_SELECT, SEL_PAGE);
        hold(B_HOME);
        assert(ui.menu == 1 && ui.menu_sel == MI_COLOR && menu_tab() == MTAB_DISPLAY);
        uint32_t n0 = mtab_rows(MTAB_DISPLAY);
        for (uint32_t i = 1; i < n0; i++) turn(EN_SELECT, 1);
        assert(ui.menu_sel == mtab_first(MTAB_DISPLAY) + n0 - 1u);       /* the last row of DISPLAY */
        turn(EN_SELECT, 1);
        assert(menu_tab() == MTAB_CONTROL && ui.menu_sel == MI_HOLD);   /* on into CONTROL's first row */
        turn(EN_SELECT, -1);
        assert(menu_tab() == MTAB_DISPLAY && ui.menu_sel == mtab_first(MTAB_DISPLAY) + n0 - 1u);   /* and back */
        turn(EN_SELECT, -1);
        assert(ui.menu_sel == mtab_first(MTAB_DISPLAY) + n0 - 2u);
        menu_put(MI_SELECT, SEL_BPM);
        uint32_t row = ui.menu_sel, b0 = (uint32_t)song.g[G_BPM];
        turn(EN_SELECT, 3);
        assert(ui.menu_sel == row && (uint32_t)song.g[G_BPM] == b0);    /* BPM mode: SELECT does nothing here, as before */
    }
    {   /* the footer's numbering: a two-group view counts once and shows its number on both halves */
        static const struct { const char *page, *want; } T[] = {
            {"LFO", "LFO TOP 1/3"}, {"LFO SYNC", "LFO BOTTOM 1/3"}, {"LFO2", "LFO2 TOP 2/3"}, {"LFO2 SYNC", "LFO2 BOTTOM 2/3"},
            {"LFO DEST", "LFO DEST 3/3"}, {"MOD", "MOD 3/3"},
            {"FX", "FX SENDS 1/4"}, {"DLY", "DELAY 1/4"}, {"REVERB", "REVERB 2/4"}, {"CHORUS", "CHORUS 2/4"},
            {"INSERT", "INSERT 3/4"}, {"INSERT 2", "INSERT A-C 3/4"}, {"SLICER", "SLICER 4/4"},
            {"ARP", "ARP TOP"}, {"ARP 2", "ARP BOTTOM"}};
        ui_power_on();
        for (uint32_t i = 0; i < sizeof T / sizeof T[0]; i++) {
            char ti[20];
            go_title(T[i].page); frame();
            page_title(ti);
            if (!str_eq(ti, T[i].want)) printf("footer %s: '%s', wanted '%s'\n", T[i].page, ti, T[i].want);
            assert(str_eq(ti, T[i].want));
        }
        {   /* EDIT family (its page count depends on the build): VOICE and VOICE 2 share one number, MORE the next */
            char a[20], b[20], c[20];
            go_title("VOICE"); frame(); page_title(a);
            go_title("VOICE 2"); frame(); page_title(b);
            go_title("MORE"); frame(); page_title(c);
            const char *sa = a + str_len("VOICE TOP "), *sb = b + str_len("VOICE BOTTOM "), *sc = c + str_len("MORE ");
            assert(!strncmp(a, "VOICE TOP ", 10) && !strncmp(b, "VOICE BOTTOM ", 13) && !strncmp(c, "MORE ", 5));
            assert(!strcmp(sa, sb) && sa[0] + 1 == sc[0] && sa[1] == '/' && sc[1] == '/' && !strcmp(sa + 2, sc + 2));
        }
    }
    {   /* ANALOG: FILTER above MORE (one view); SEM: MORE is not offered (SEM MODE has its SPREAD); other engines: MORE alone */
        char path[320], ti[20];
        ui_power_on();
        TSEL->eng_req = 0; frame();
        go_title("FILTER"); frame();
        assert(dual_page(cur_page()) && !dual_bottom(cur_page()));
        page_title(ti); assert(!strncmp(ti, "FILTER ", 7));
        snprintf(path, sizeof path, "%s/analog-filter-more-palette-%02u.ppm", argv[1], settings.palette);
        save_screen(path);
        go_title("MORE"); frame();
        assert(dual_bottom(cur_page()));
        TSEL->eng_req = ENGI_SEM; frame();
        go_title("EDIT 1");
        for (uint32_t i = 0; i < 12; i++) { assert(!str_eq(cur_page()->title, "MORE")); turn(EN_SELECT, 1); frame(); }
        TSEL->eng_req = ENGI_FM6; frame();
        go_title("MORE"); frame();
        assert(!dual_page(cur_page()));
    }
    {   /* MORE: SPREAD, MORPH, PHS FB on one page; a slot the engine lacks is empty and its knob does nothing */
        static const struct { uint16_t eng; int morph, fb; } E[] = {{0, 1, 0}, {2, 1, 1}, {3, 1, 0}, {4, 0, 0}, {12, 0, 0}};
        for (uint32_t i = 0; i < sizeof E / sizeof E[0]; i++) {
            int16_t *vp;
            ui_power_on();
            TSEL->eng_req = E[i].eng; frame();
            go_title("MORE"); frame();
            assert(str_eq(cur_page()->title, "MORE"));
            assert(page_desc(cur_page(), 0, &vp) && vp == &TSEL->p[P_SPRD]);
            assert((page_desc(cur_page(), 1, &vp) != 0) == E[i].morph);
            assert((page_desc(cur_page(), 2, &vp) != 0) == E[i].fb);
            assert(!page_desc(cur_page(), 3, &vp));
            int16_t m0 = TSEL->p[P_MORPH], f0 = TSEL->p[P_PD_FB];
            turn(EN_K2, 3); turn(EN_K3, 3); turn(EN_K4, 3);
            assert((TSEL->p[P_MORPH] != m0) == E[i].morph && (TSEL->p[P_PD_FB] != f0) == E[i].fb);
            char path[320];
            snprintf(path, sizeof path, "%s/more-eng%u.ppm", argv[1], E[i].eng);
            save_screen(path);
        }
    }
    {   /* STEP: EDIT opens EDIT; the step is cleared by SEQ held + F5 (one undo); an empty step is passed over */
        ui_power_on();
        track_t *t = TSEL;
        my_steps(t);
        go_page(GR_ROLL); frame();
        ui.cursor = 0;
        assert(step_on(&t->step[0]) && !step_on(&t->step[1]));
        press(B_EDIT);
        assert(cur_page()->fam == FAM_EDIT && step_on(&t->step[0]));      /* EDIT: the pages, the step stays */
        go_page(GR_ROLL); frame();
        ui.cursor = 0;
        tool_clear_step();
        assert(!step_on(&t->step[0]) && ui.cursor == 1u && cur_page()->graph == GR_ROLL);
        frames(320);
        tool_clear_step();                                                /* step 1 is empty: on to step 2, nothing else */
        assert(ui.cursor == 2u && !step_on(&t->step[1]));
        frames(320);
        hold(B_SAVE);                                                     /* SAVE held: the undo of the last clear */
        assert(step_on(&t->step[0]));
    }
    puts("Dual-block preview (LFO, LFO2, ARP, VOICE, ENV, EDIT, LANES, FX, settings menu SELECT, EDIT / CLEAR STEP): all palettes, normal/LARGE, focus, navigation and independent knob targets passed.");
    return 0;
}
