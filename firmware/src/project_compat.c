/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental reduced builds: explicit audition, no writes to its source.
 * Re-read the durable source on confirmation; no additional project/audio buffer. */
static struct {
    uint8_t warning, source, approved, trial, protected_slots, hold_autosave, previous_hold;
} project_compat_state;

static uint32_t project_compat_flags(const project_t *p)
{
    uint32_t flags = 0;
#if CARAVEL_REVERB_MINIMAL
    if (p->g[G_RTYPE] == 0) flags |= 2u;
    if (p->g[G_RTYPE] == 1) flags |= 4u;
#endif
#if !FELUCCA_SLICE
    for (uint32_t t = 0; t < NTRK; t++)
        if (p->t[t].engine == 13u) flags |= 1u;
#endif
    return flags;
}
static uint32_t project_compat_warning(void) { return project_compat_state.warning; }
static int project_compat_request(const project_t *p, uint32_t source)
{
    uint32_t flags = project_compat_flags(p);
    if (!flags || (project_compat_state.approved == flags && project_compat_state.source == source)) return 0;
    if (!project_compat_state.approved && ui.confirm != CF_COMPAT_PROJECT)
        project_compat_state.previous_hold = project_compat_state.hold_autosave;
    project_compat_state.warning = (uint8_t)flags;
    project_compat_state.source = (uint8_t)source; /* A-D, or 4 = RESTORE LAST */
    project_compat_state.hold_autosave = 1;
    ui.act = 0;
    confirm_open(CF_COMPAT_PROJECT, 0);
    return 1;
}
static int project_compat_save_blocked(uint32_t slot)
{
    if (slot < 4u && (project_compat_state.protected_slots & (1u << slot))) {
        ui_message("ORIGINAL PROTECTED");
        str_cpy(ui.msg2, "SAVE TO ANOTHER SLOT", sizeof ui.msg2);
        return 1;
    }
    return 0;
}
static void project_compat_saved(void)
{
    project_compat_state.trial = project_compat_state.hold_autosave = 0;
}
static void project_compat_cancel(void)
{
    /* Cancelling a slot audition restores the active session's autosave policy.
     * A skipped incompatible boot autosave must remain protected. */
    if (project_compat_state.source < 4u) project_compat_state.hold_autosave = project_compat_state.previous_hold;
    project_compat_state.approved = 0;
}
static void project_compat_confirm(void)
{
    if (transport_busy() || chain_busy()) {
        project_compat_cancel();
        ui_message("STOP TO TEST LOAD");
        return;
    }
    project_compat_state.approved = project_compat_state.warning;
    if (project_compat_state.source < 4u) project_load(project_compat_state.source);
    else (void)autosave_boot(1);
    project_compat_state.approved = 0; /* a failed load must never leave permission behind */
}
