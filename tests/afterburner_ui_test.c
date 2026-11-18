/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Actual UI/parameter/project integration; no FM-1 timing claim. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CPU2 0
static void action_refresh(void);
static void action_dispatch(unsigned int);
#include "ui_harness.h"
#include "../experiments/action-banks/action_control.c"
#include <assert.h>

static project_t saved;
int main(void)
{
    ui_power_on();
    uxl.play = 0; uxl.lvl = 0;
    uint32_t page = NPAGES;
    for (uint32_t i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, "AFTER")) page = i;
    assert(page < NPAGES && PAGES[page].fam == FAM_FX);
    uint8_t order[NPAGES];
    uint32_t count = stasis_fx_pages(order), found = 0;
    for (uint32_t i = 1; i < count; i++)
        if (order[i] == page) {
#if CARAVEL_STASIS_EXPAND
            assert(str_eq(PAGES[order[i-1]].title, "LAYER"));
#else
            assert(str_eq(PAGES[order[i-1]].title, "REVERB"));
#endif
            found++;
        }
    assert(found == 1);
    ui.home = ui.menu = ui.confirm = ui.layer = 0;
    ui.page = (uint8_t)page; page_entered();
    int16_t *fields[4] = {&afterburner.on, &afterburner.time, &afterburner.amt, &afterburner.damp};
    for (uint32_t i = 0; i < 4; i++) {
        int16_t *value = 0;
        assert(page_desc(cur_page(), i, &value) && value == fields[i]);
        hp_last_valid = 1; hp_touch(i); assert(!hp_last_valid);
        hp_ref_t ref;
        assert(!uxl_inter_ref(i, &ref));
        assert(!mom_turn(i, 1));
    }
    edit_param(0, 1); assert(afterburner.on == 1);
    edit_param(1, 1000); assert(afterburner.time == 60);
    edit_param(1, -1000); assert(afterburner.time == 10);
    edit_param(2, 1000); assert(afterburner.amt == 127);
    edit_param(3, -1000); assert(afterburner.damp == 0);
    ui.force = 1; ui_draw();
#if CARAVEL_STASIS_EXPAND
    stasis_tone = 97;
#endif
    project_capture(&saved);
    assert(saved.af_on == 1 && saved.af_time == 10 &&
           saved.af_amt == 127 && saved.af_damp == 0);
#if CARAVEL_STASIS_EXPAND
    assert(saved.st_tone == 97);
#endif
    project_store_t wire;
    assert(proj_pack(&wire, &saved));
    assert(proj_import(&saved, &wire, sizeof wire));
    afterburner.on = 0; afterburner.time = 30;
    afterburner.amt = 32; afterburner.damp = 70;
#if CARAVEL_STASIS_EXPAND
    stasis_tone = 64;
#endif
    assert(project_restore_runtime(&saved) == 0);
    assert(afterburner.on && afterburner.time == 10 &&
           afterburner.amt == 127 && afterburner.damp == 0);
#if CARAVEL_STASIS_EXPAND
    assert(stasis_tone == 97 && stasis_tone_sm == 97 * 256);
#endif
    project_init_runtime();
    assert(!afterburner.on && afterburner.time == 30 &&
           afterburner.amt == 32 && afterburner.damp == 70);
#if CARAVEL_STASIS_EXPAND
    assert(stasis_tone == 64);
#endif
    puts("Afterburner UI: FX order, virtual refs, AFTER/TONE project save/load, INIT defaults PASS");
    return 0;
}
