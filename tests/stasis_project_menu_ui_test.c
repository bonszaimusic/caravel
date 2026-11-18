/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Narrow UI regression: modal SYSTEM exits, PLAY SAVE opens real SAVE
 * workflow, AFTER/TONE data survive native project encoding.
 *
 * Build with the existing tests/ui_test.c host flags and libraries.
 * Host-only; no FM-1 timings/firmware-package assertions. */
#define UI_TEST_NO_MAIN 1
#define CARAVEL_UX_LAYERS 1
#define CARAVEL_STASIS_EXPAND 1
#define CARAVEL_REVERB_MINIMAL 1
#define CARAVEL_AFTERBURNER 1
#include "ui_test.c"
#include <assert.h>

int main(void)
{
    ui_power_on();
    uxl.play = 0; uxl.lvl = 0; ui.home = 1;
    ui.menu = 1; ui.force = 1;
    press(B_FX);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_FX);
    ui.menu = 3; ui.force = 1; /* INFO: SAVE navigates without HOME */
    press(B_SAVE);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_SAVE);
    /* SAVE release must not inadvertently cycle the SAVE page. */
    uint32_t saved_page = ui.page;
    frame();
    assert(ui.page == saved_page);
    ui.menu = 1; ui.force = 1;
    press(B_ENV);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_ENV);
    /* PLAY contains no destructive direct-save gesture: a tap opens
     * the fully featured BUILD SAVE screen. */
    uxl.play = 1; ui.menu = 0; ui.confirm = 0;
    uint8_t old_page = ui.page, old_home = ui.home;
    press(B_SAVE);
    assert(play_project_save_only && !uxl.play && !ui.home &&
           cur_page()->graph == GR_SLOTS);
    /* SAVE and SELECT cannot wander to USER / PRESETS / PHRASES. */
    press(B_SAVE);
    assert(play_project_save_only && cur_page()->graph == GR_SLOTS);
    page_turn(1);
    assert(play_project_save_only && cur_page()->graph == GR_SLOTS);
    ui.act = 3u; /* K2 INIT/ERASE is blocked in this shortcut */
    act_do();
    assert(!ui.confirm && cur_page()->graph == GR_SLOTS);
    press(B_HOME);
    assert(!play_project_save_only && uxl.play &&
           ui.page == old_page && ui.home == old_home);

    project_t p, q;
    project_store_t bytes;
    afterburner.on = 1; afterburner.time = 49;
    afterburner.amt = 88; afterburner.damp = 21;
    stasis_tone = 111;
    project_capture(&p);
    assert(proj_pack(&bytes, &p));
    assert(bytes.raw[65] == PROJ_FX_EXT_MARK);
    assert(proj_import(&q, bytes.raw, sizeof bytes));
    afterburner.on = 0; afterburner.time = 30;
    afterburner.amt = 32; afterburner.damp = 70;
    stasis_tone = 64;
    assert(!project_restore_runtime(&q));
    assert(afterburner.on && afterburner.time == 49 &&
           afterburner.amt == 88 && afterburner.damp == 21);
    assert(stasis_tone == 111);
    project_init_runtime();
    assert(afterburner.on == 0 && stasis_tone == 64);
    puts("STASIS/AFTER PROJECT + PLAY SAVE + MENU jump host checks PASS");
    return 0;
}
