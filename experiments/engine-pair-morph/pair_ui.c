/* SPDX-License-Identifier: GPL-3.0-only */
/* Device-sized prototype UI. Included after card helpers, before ui_input.c. */
static uint8_t pair_ui_partner[NTRK] = {2, 0, 0, 0};
static void pair_ui_home_knob(uint32_t slot, int32_t steps)
{
    if (!ep.on || !steps) return;
    if (slot == 0) ep.base = clamp(ep.base + steps * 256,0,32767);
    if (slot == 1) ep.motion = steps > 0;
    if (slot == 2) trk[ep.a].p[P_LEVEL] = clamp(trk[ep.a].p[P_LEVEL]+steps,0,127);
    if (slot == 3) trk[ep.a].p[P_PAN] = clamp(trk[ep.a].p[P_PAN]+steps,-64,63);
    ui.force = 1;
}
static void pair_ui_home_cards(void)
{
    char val[12];const char *unit;
    fmt_int(val,(ep.base*100+16383)/32767);fmt_named=1;
    draw_column(0,"MORPH",val,"%",VAL(0),ep.base*1000/32767,ICON_NONE);
    draw_column(1,"MOTION",ep.motion?"ON":"OFF","",VAL(1),-1,ICON_NONE);
    param_format(&TP[P_LEVEL],ep.level,val,&unit);
    draw_column(2,"PAIR LVL",val,unit,VAL(2),-1,ICON_NONE);
    param_format(&TP[P_PAN],ep.pan,val,&unit);
    draw_column(3,"PAIR PAN",val,unit,VAL(3),-1,ICON_NONE);
}
static uint32_t pair_ui_other(void)
{ return pair_member(TSEL) ? (song.sel == ep.a ? ep.b : ep.a) : pair_ui_partner[song.sel]; }
static void pair_ui_knob(uint32_t slot, int32_t steps)
{
    if (!steps) return;
    if (slot == 0) {
        if (ep.on) { ui_message("UNPAIR FIRST"); return; }
        uint32_t other = pair_ui_other();
        do { other = (other + (steps > 0 ? 1u : NTRK-1u)) % NTRK; } while (other == song.sel);
        pair_ui_partner[song.sel] = other;
    } else if (slot == 1) {
        if (steps < 0 && pair_member(TSEL)) { pair_disable(); pair_last_reason = "PAIR OFF"; }
        else if (steps > 0 && !ep.on) {
            if (!pair_enable(song.sel, pair_ui_other())) ui_message(pair_last_reason);
            else {
#if FELUCCA_HOME_PERF
 ui_message("PAIR ON / K3 MORPH");
#else
 go_home();
#endif
 }
        }
    } else if (pair_member(TSEL)) {
        if (slot == 2) ep.base = clamp(ep.base + steps * 256, 0, 32767);
        if (slot == 3) ep.motion = steps > 0;
    }
    ui.force = 1;
}
static void pair_ui_cards(void)
{
    static int32_t drawn_base = -1;
    char val[12] = "T1";
    val[1] += pair_ui_other();
    draw_column(0, "PARTNER", val, "", VAL(0), -1, ICON_NONE);
    draw_column(1, "PAIR", pair_member(TSEL) ? "ON" : "OFF", "", VAL(1), -1, ICON_NONE);
    fmt_int(val, (ep.base * 100 + 16383) / 32767);
    /* Keep the rapidly changing control readable without a rolling-number trail. */
    if (drawn_base != ep.base) { ui.col[2][0] = 0; drawn_base = ep.base; }
    ui.roll[2].from[0] = 0; fmt_named = 1;
    draw_column(2, "MORPH", pair_member(TSEL) ? val : "-", "%", VAL(2), -1, ICON_NONE);
    draw_column(3, "MOTION", pair_member(TSEL) ? (ep.motion ? "ON" : "OFF") : "-", "", VAL(3), -1, ICON_NONE);
}
static void pair_ui_graph(void)
{
    /* Only control/UI rate; no new audio-rate work or allocations. Same layout in LARGE. */
    char line[48];
    cv_begin(240, H_GRAPH, T_BG);
    cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
    cv_bg = T_SURF;
    cv_oy = 0;
    for (uint32_t t = 0; t < NTRK; t++) {
        int member = pair_member(&trk[t]);
        int32_t gain = member ? ep.weight[t == ep.b][CTL-1] : 0;
        int y = 2 + t * 24;
        if (member) perf_format(line, sizeof line, "%s =T%u %u%%", ENGINES[trk[t].engine]->name,
            t == ep.a ? ep.b+1 : ep.a+1, (unsigned)((gain * 100 + 16383) / 32767));
        else perf_format(line, sizeof line, "%s  SINGLE", ENGINES[trk[t].engine]->name);
        cv_icon_on(8,y+2,16,trk_icon(t,1),t == song.sel ? T_ACCENT : T_MID,T_SURF);
        cv_text(30, y, &AF_M, line, t == song.sel ? T_ACCENT : member ? T_TEXT : T_MID);
        if (t == song.sel) cv_rect(5, y+4, 2, 13, T_ACCENT);
        cv_rect(10, y+19, 220, member ? 3 : 1, T_RAISE);
        if (member) cv_rect(10, y+19, gain * 220 / 32767, 3, t == song.sel ? T_ACCENT : T_THEME);
    }
    if (ep.on) {
        perf_format(line, sizeof line, "A:T%u B:T%u BASE %u%% LIVE %u%%", ep.a+1, ep.b+1,
                 (unsigned)(ep.base*100/32767), (unsigned)(ep.effective*100/32767));
        cv_text(10, 96, &AF_S, line, T_MID);
#if FELUCCA_HOME_PERF
        if(hp_pair_auto_count())perf_format(line,sizeof line,"SEQ T%u: PAIR AUTO %s",ep.a+1,motion_enabled(&trk[ep.a])?"ON":"OFF");
        else if(ep.cells) perf_format(line,sizeof line,"%u NOTES / %s",ep.budget,
            ep.cells==3 ? "TWO CELL NETWORKS" : "CELLS + POLY");
        else perf_format(line,sizeof line,"%u %s / GLO+HOME: ADD",ep.budget,ep.budget==1?"NOTE":"NOTES");
#else
        perf_format(line, sizeof line, ui.home ? "%u NOTES / K1 MORPH" : "%u NOTES / HOME: MORPH", ep.budget);
#endif
        cv_text(10, 107, &AF_S, line, T_MID);
    } else {
        const char *why = pair_enable_reason(song.sel,pair_ui_other());
        cv_text(10, 96, &AF_S, why ? why : "READY / K2 RIGHT: PAIR", T_ACCENT);
        cv_text(10, 107, &AF_S, "ALGO TRACK / K1 PARTNER / K2 ON", T_MID);
    }
    cv_blit(0, Y_GRAPH);
}
