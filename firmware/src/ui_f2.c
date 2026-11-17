/* SPDX-License-Identifier: GPL-3.0-only */
/* FORMAT 2, the first start with an older version's data in flash (docs/FORMAT_2_ENTWURF, section 3, level 2).
 *
 * 1.0 reads one format only (storage.c ST_FMT2, project.c FUNW, upreset.c UPB2, the HOME store's IFP2). What an older
 * firmware stored is never converted and never loaded; it is also never deleted or written over without the owner's say.
 * This file finds it at power-on (f2_old: storage.c) and, until it was answered once (ui_prefs3 PREF_F2_ACK, kept in the
 * settings, which 1.0 still reads), puts two screens in front of everything:
 *   1 OLD DATA FOUND   - locked for 5 s, then OCT+ held 3 s goes on (a tap does nothing, so no one passes it by habit)
 *   2 BACKUP MADE?     - OCT+ held 3 s: keep the old data, go on; OCT- held 3 s: erase it (asks once more);
 *                        any other button: back to screen 1
 *   3 ERASE OLD DATA?  - OCT- held 3 s: erased for good (both sectors of each pair); anything else: back to screen 2
 * The old slots stay protected after "keep": a save to one says OLD PROJECT HERE (project_compat_save_blocked), a user
 * preset into an old bank OLD SOUNDS HERE (up_put), and each of them puts screen 2 up again (f2_offer: keep it, or erase it
 * and have the slot); the old autosave may be overwritten once the notice was answered. Going back to the old firmware
 * brings the old data back: nothing here touched it. (A SYSTEM menu row for it is part of the SYSTEM re-sort, still open:
 * the tab holds 8 rows, the most the page fits.) */
#define PREF_F2_ACK (0x20u << 24)                    /* ui_prefs3 bit 5: the notice was answered */
#define F2_LOCK_MS 5000u
#define F2_HOLD_MS 3000u
enum { F2_NONE, F2_WARN, F2_SURE, F2_ERASE };
static struct { uint8_t stage, holding, done; uint32_t t0, h0, sig; } f2;

static uint32_t f2_scan(void)
{
    uint32_t m = 0, i;
    if (!flash_ok) return 0;
    for (i = 0; i < 4u; i++)
#if FELUCCA_PERF_HW
        if (hp_hw_old(i)) m |= 1u << i;
    if (hp_hw_old(4u)) m |= F2_OLD_AUTO;
#else
        if (st_old(OBJ_PROJECT0 + i)) m |= 1u << i;
    if (st_old(OBJ_AUTOSAVE)) m |= F2_OLD_AUTO;
#endif
    for (i = 0; i < UP_SLOTS / UP_PER_BANK; i++)
        if (st_old(OBJ_UPRESET0 + i)) m |= F2_OLD_BANK0 << i;
    return m;
}
/* power-on (project.c autosave_boot, once): what is old, and the notice unless it was answered before */
static void f2_boot(void)
{
    if (f2.done) return;
    f2.done = 1;
    f2_old = (uint8_t)f2_scan();
    if (!f2_old) return;
    if (ui_prefs3 & (PREF_F2_ACK >> 24)) {            /* answered: the old autosave may go, the slots stay protected */
        f2_old &= (uint8_t)~F2_OLD_AUTO;
        return;
    }
    f2.stage = F2_WARN;
    f2.t0 = fm1_ms;
    f2.sig = ~0u;
    ui.force = 1;
}
static int f2_active(void) { return f2.stage != F2_NONE; }
/* a save wanted a slot with old data (project_compat_save_blocked, up_put): the question again, unlocked, so a slot can be
 * freed by the one who wants it (the only way back to the notice once it was answered) */
static void f2_offer(void)
{
    if (f2.stage != F2_NONE || !(f2_old & (0x0Fu | F2_OLD_BANK0 | (F2_OLD_BANK0 << 1)))) return;
    f2.stage = F2_SURE;
    f2.holding = 0;
    f2.t0 = fm1_ms - F2_LOCK_MS;
    f2.sig = ~0u;
    ui.force = 1;
}
static void f2_answered(void)
{
    f2.stage = F2_NONE;
    f2.holding = 0;
    ui_prefs3 |= (uint8_t)(PREF_F2_ACK >> 24);
    settings_save();
    f2_old &= (uint8_t)~F2_OLD_AUTO;
    ui.head_sig = ~0u;
    ui.force = 1;
}
/* every old object erased for good; 0 ok */
static int f2_erase(void)
{
    uint32_t i, bad = 0;
    uint8_t z = 0;
    if (transport_busy()) { ui_message("STOP TO ERASE"); return 1; }
    for (i = 0; i < 5u; i++) {
        uint32_t bit = i < 4u ? 1u << i : F2_OLD_AUTO;
        if (!(f2_old & bit)) continue;
#if FELUCCA_PERF_HW
        bad |= (uint32_t)hp_hw_erase_old(i);
#else
        {   uint32_t obj = i < 4u ? OBJ_PROJECT0 + i : OBJ_AUTOSAVE;
            bad |= (uint32_t)(st_save(obj, &z, 0) || st_save(obj, &z, 0)); }
#endif
    }
    for (i = 0; i < UP_SLOTS / UP_PER_BANK; i++)
        if (f2_old & (F2_OLD_BANK0 << i))
            bad |= (uint32_t)(st_save(OBJ_UPRESET0 + i, &z, 0) || st_save(OBJ_UPRESET0 + i, &z, 0));
    (void)z;
    if (bad) { ui_message("ERASE ERROR"); return 2; }
    f2_old = 0;
    return 0;
}
/* the buttons the screens answer to */
static uint32_t f2_held(uint32_t up)
{
    return (fm1_in.buttons >> panel.btn[up ? B_OCTUP : B_OCTDN]) & 1u;
}
/* ui_input, before anything else: 1 = swallowed */
static int f2_input(uint32_t pressed)
{
    uint32_t k, up, dn, other, want;
    if (f2.stage == F2_NONE) return 0;
    for (k = 0; k < NE; k++) (void)panel_enc(k);       /* (the knobs' turns go) */
    scrn.idle = fm1_ms;
    up = f2_held(1u); dn = f2_held(0u);
    other = pressed & ~((1u << panel.btn[B_OCTUP]) | (1u << panel.btn[B_OCTDN]));
    if (f2.stage == F2_WARN) {
        want = up && !dn && (int32_t)(fm1_ms - f2.t0) >= (int32_t)F2_LOCK_MS;
    } else if (f2.stage == F2_SURE) {
        if (other) { f2.stage = F2_WARN; f2.holding = 0; f2.sig = ~0u; ui.force = 1; return 1; }
        want = (up != dn);
    } else {
        if (other || (pressed & (1u << panel.btn[B_OCTUP]))) { f2.stage = F2_SURE; f2.holding = 0; f2.sig = ~0u; ui.force = 1; return 1; }
        want = dn && !up;
    }
    if (!want) {
        f2.holding = 0;
        return 1;
    }
    if (!f2.holding) {
        f2.holding = 1;
        f2.h0 = fm1_ms;
        f2.sig = ~0u;
    }
    if ((int32_t)(fm1_ms - f2.h0) < (int32_t)F2_HOLD_MS)
        return 1;
    f2.holding = 0;
    f2.sig = ~0u;
    ui.force = 1;
    if (f2.stage == F2_WARN) {
        f2.stage = F2_SURE;
    } else if (f2.stage == F2_SURE) {
        if (up) {
            f2_answered();
            ui_message("OLD DATA KEPT");
            str_cpy(ui.msg2, "NOT OVERWRITTEN", sizeof ui.msg2);
        } else {
            f2.stage = F2_ERASE;
        }
    } else {
        if (f2_erase() == 0) {
            f2_answered();
            ui_message("OLD DATA ERASED");
        } else {
            f2.stage = F2_SURE;
        }
    }
    return 1;
}
/* the frame (ui_draw, instead of the page) */
static void f2_draw(void)
{
    uint32_t prog = 0, lock = 0, sig;
    const char *a, *b, *c, *d, *e, *hint;
    if (f2.holding) prog = (fm1_ms - f2.h0) * 16u / F2_HOLD_MS;
    if (prog > 16u) prog = 16u;
    if (f2.stage == F2_WARN && (int32_t)(fm1_ms - f2.t0) < (int32_t)F2_LOCK_MS)
        lock = (F2_LOCK_MS - (fm1_ms - f2.t0) + 999u) / 1000u;
    sig = f2.stage | prog << 4 | lock << 12;
    if (!ui.force && sig == f2.sig) return;
    f2.sig = sig;
    ui.force = 0;
    ui.head_sig = ~0u;
    lcd_fill(0, 0, 240, 240, T_BG);
    if (f2.stage == F2_WARN) {
        a = "OLD DATA FOUND";
        b = "PROJECTS AND SOUNDS OF AN";
        c = "OLDER VERSION DO NOT LOAD";
        d = "HERE. NOTHING IS DELETED.";
        e = "THE OLD FIRMWARE BRINGS IT BACK";
        hint = lock ? "WAIT" : "HOLD OCT+ 3 S: NEXT";
    } else if (f2.stage == F2_SURE) {
        a = "BACKUP MADE?";
        b = "USE THE OLD VERSION'S EDITOR";
        c = "OR ITS BACKUP FILE FIRST.";
        d = "NOTHING IS WRITTEN OVER IT";
        e = "WITHOUT YOUR OK.";
        hint = "OCT+ 3 S: KEEP    OCT- 3 S: ERASE";
    } else {
        a = "ERASE OLD DATA?";
        b = "ALL PROJECTS AND SOUNDS OF";
        c = "THE OLD VERSION ARE GONE.";
        d = "THIS CANNOT BE UNDONE.";
        e = "";
        hint = "OCT- 3 S: ERASE    OCT+: BACK";
    }
    draw_text_box(0, 22, 240, &AF_M, a, f2.stage == F2_ERASE ? T_ACCENT : T_THEME, 1);
    draw_text_box(0, 66, 240, &AF_S, b, T_TEXT, 1);
    draw_text_box(0, 84, 240, &AF_S, c, T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_S, d, T_TEXT, 1);
    draw_text_box(0, 130, 240, &AF_S, e, T_MID, 1);
    if (lock) {
        char n[8];
        n[0] = '0' + (char)lock; n[1] = 0;
        draw_text_box(0, 168, 240, &AF_S, hint, T_MID, 1);
        draw_text_box(0, 186, 240, &AF_L, n, T_THEME, 1);
    } else {
        draw_text_box(0, 168, 240, &AF_S, hint, T_TEXT, 1);
        lcd_fill(32, 196, 176, 8, T_DIM);                 /* the hold: 16 steps over 3 s */
        if (prog) lcd_fill(32, 196, (int32_t)(prog * 11u), 8, T_THEME);
    }
    lcd_sync();
}
