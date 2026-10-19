/* SPDX-License-Identifier: GPL-3.0-only */
static int lfo2_page(uint32_t graph) { return graph == GR_LFO2 || graph == GR_LFO2SYNC; }
static uint32_t lfo2_page_target(uint32_t graph, uint32_t col)
{
    uint32_t i = (graph == GR_LFO2SYNC ? L2_SYNC : 0u) + col;
    return i < L2_N ? L2_FIRST + i : 0u;
}
/* the selected shape as a two-cycle pictogram in a w x h box (same shapes as graph_lfo; 2 px trace) */
static void lfo2_wave_icon(int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t wave, uint16_t c)
{
    int32_t a = h - 2, py = 0;
    if (wave == 2) {                                   /* a clean ramp and drop per cycle, not a staircase */
        for (int32_t cyc = 0; cyc < 2; cyc++) {
            int32_t xa = x0 + cyc * (w / 2), xb = xa + w / 2 - 1;
            cv_line_t(xa, y0 + a, xb, y0, c, 2);
            if (cyc == 0)
                cv_line_t(xb, y0, xb + 1, y0 + a, c, 2);
        }
        return;
    }
    for (int32_t x = 0; x < w; x++) {
        uint32_t phase = (uint32_t)x * (0xFFFFFFFFu / ((uint32_t)w / 2u));
        int32_t s = wave == 1 ? osc_tri(phase) : wave == 2 ? (int32_t)(phase >> 16) - 32768 :
            wave == 3 ? (phase < 0x80000000u ? 32767 : -32767) : osc_sine(phase);
        if (wave == 4)
            s = (int32_t)(((uint32_t)x / 6u * 2654435761u) >> 16) - 32768;
        int32_t y = y0 + a / 2 - s * (a / 2) / 32768;
        if (x)
            cv_line_t(x0 + x - 1, py, x0 + x, y, c, 2);
        py = y;
    }
}
/* the ADSR as a small polyline in a w x h box (segment widths grow with the times, the sustain height is SUS) */
static void env_icon(int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t atk, int32_t dec, int32_t sus, int32_t rel, uint16_t c)
{
    int32_t a = 5 + atk * 20 / 127, d = 5 + dec * 20 / 127, hold = 12, r = 5 + rel * 20 / 127;
    int32_t sy = y0 + (h - 1) - sus * (h - 1) / 127, bot = y0 + h - 1;
    int32_t x1 = x0 + a, x2 = x1 + d, x3 = x2 + hold, x4 = x3 + r;
    (void)w;
    cv_line_t(x0, bot, x1, y0, c, 2);
    cv_line_t(x1, y0, x2, sy, c, 2);
    cv_line_t(x2, sy, x3, sy, c, 2);
    cv_line_t(x3, sy, x4, bot, c, 2);
}
/* Pages shown as two groups at once: SHAPE / RATE above SYNC / TRIGGER and the like. The rows are normal pages (K1-4
 * edit them through the page path) except LFO2's, which come from its separate config (lfo2_knob). */
enum { DK_LFO, DK_LFO2, DK_PLAIN, DK_ENV, DK_EDIT, DK_GRAPH };   /* DK_GRAPH: one page above, its graph in the lower block */
typedef struct { const char *top, *bot, *ht, *hb, *ft, *fb; uint8_t kind; } dual_t;
static const dual_t DUALS[] = {
    {"LFO", "LFO SYNC", "LFO SHAPE / RATE", "LFO SYNC / TRIGGER", "LFO TOP", "LFO BOTTOM", DK_LFO},
    {"LFO2", "LFO2 SYNC", "LFO2 SHAPE / RATE", "LFO2 SYNC / TRIGGER", "LFO2 TOP", "LFO2 BOTTOM", DK_LFO2},
    {"ENV", "ENV DEST", "ENV", "ENV DEST / SYNC", "ENV TOP", "ENV BOTTOM", DK_ENV},
    {"EDIT 1", "EDIT 2", "EDIT 1", "EDIT 2", "EDIT TOP", "EDIT BOTTOM", DK_EDIT},
    {"LANES", "LANES 2", "LANES 1-4", "LANES 5-8", "LANES TOP", "LANES BOTTOM", DK_PLAIN},
    {"FX", "DLY", "FX SENDS (THIS TRACK)", "DELAY (ALL TRACKS)", "FX SENDS", "DELAY", DK_PLAIN},
    {"REVERB", "CHORUS", "REVERB (ALL TRACKS)", "CHORUS (ALL TRACKS)", "REVERB", "CHORUS", DK_PLAIN},
    {"INSERT", "INSERT 2", "INSERT TYPE / A B C", "INSERT MIX", "INSERT", "INSERT MIX", DK_PLAIN},
    {"SLICER", "", "SLICER", "STEP PATTERN", "SLICER", "", DK_GRAPH},
    {"ARP", "ARP 2", "ARP MODE / RATE", "ARP SWING / ORDER", "ARP TOP", "ARP BOTTOM", DK_PLAIN},
    {"VOICE", "VOICE 2", "VOICE MODE / GLIDE", "VOICE ALLOC / PAN", "VOICE TOP", "VOICE BOTTOM", DK_PLAIN},
};
/* EDIT 1 / 2 keep their normal layout for the engines whose panel is a picture of the sound (drawbars, the sample's
 * waveform, FM6's algorithm chart, DIGITAL's chart): the two-group layout has no room for it */
static int edit_has_picture(void)
{
    uint32_t e = TSEL->eng_req % NENGINES;
    return ENGINES[e] == &ENG_WHEEL || ENGINES[e] == &ENG_SAMPLE || e == ENGI_FM6 || (FELUCCA_FM4 && e == ENGI_DIGITAL);
}
static const dual_t *dual_of(const page_t *pg, uint32_t *bottom)
{
    for (uint32_t i = 0; i < sizeof DUALS / sizeof DUALS[0]; i++) {
        if (DUALS[i].kind == DK_EDIT && edit_has_picture())
            continue;
        if (str_eq(pg->title, DUALS[i].top)) { if (bottom) *bottom = 0; return &DUALS[i]; }
        if (DUALS[i].bot[0] && str_eq(pg->title, DUALS[i].bot)) { if (bottom) *bottom = 1; return &DUALS[i]; }
    }
    return 0;
}
static int dual_page(const page_t *pg) { return dual_of(pg, 0) != 0; }
static int dual_bottom(const page_t *pg) { uint32_t b = 0; return dual_of(pg, &b) && b; }
static const char *dual_foot(const page_t *pg)         /* the footer badge text, 0: not a dual page */
{
    uint32_t b;
    const dual_t *dd = dual_of(pg, &b);
    return dd ? (b ? dd->fb : dd->ft) : 0;
}
static const page_t *dual_row_page(const dual_t *dd, uint32_t row)
{
    for (uint32_t i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, row ? dd->bot : dd->top))
            return &PAGES[i];
    return 0;
}
/* the descriptor and value of one card of a row (0: an empty slot) */
static const param_desc_t *dual_card(const dual_t *dd, const page_t *rp, uint32_t row, uint32_t col, int32_t *v)
{
    const param_desc_t *d;
    int16_t *vp = 0;
    *v = 0;
    if (dd->kind == DK_LFO2) {
        uint32_t id = lfo2_page_target(row ? GR_LFO2SYNC : GR_LFO2, col);
        d = lfo2_desc(id);
        if (d)
            *v = lfo2_get(song.sel, id);
        return d;
    }
    d = rp ? page_desc(rp, col, &vp) : 0;
    if (d && vp)
        *v = *vp;
    return d;
}
/* DK_GRAPH's lower block: the SLICER's 16-step pattern in a frame like the groups' (graph_slicer's bars, half the height):
 * an open step a full bar, a closed GATE step what DEPTH leaves open, a closed STUT step a dashed bar, the playing step marked */
static void slicer_block(const track_t *t, const char *title)
{
    uint32_t i, pat = sl_pattern(t), mode = (uint32_t)t->p[P_SLCR], cur = sl[t - trk].idx;
    static const char *const MODE[] = {"OFF", "GATE", "STUT"};
    int32_t top = 28, hb = 40, open = hb - t->p[P_SLDEPTH] * hb / 127;       /* px a closed GATE step keeps */
    uint16_t col = mode == SL_OFF ? T_DIM : T_MID;
    cv_begin(240, 80, T_BG);
    cv_rrect(3, 0, 234, 80, 5, T_RAISE, T_BG);
    cv_rrect(5, 2, 230, 76, 3, T_SURF, T_RAISE);
    cv_rect(5, 5, 230, 16, T_SURF);
    cv_text_on(10, 6, &AF_S, title, T_TEXT, T_SURF);
    cv_text_r(230, 6, &AF_S, MODE[mode % 3u], mode == SL_OFF ? T_DIM : T_MID, T_SURF);
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i);
        if ((pat >> i) & 1u) {
            cv_rrect(x, top, 9, hb, 2, mode == SL_OFF ? T_RAISE : col, T_SURF);
        } else if (mode == SL_STUT) {
            for (int32_t y = top; y < top + hb; y += 4)
                cv_rect(x, y, 9, 1, col);
        } else {
            cv_rrect(x, top + hb - 3, 9, 3, 1, T_RAISE, T_SURF);
            if (open > 3)
                cv_rrect(x, top + hb - open, 9, open, 2, T_RAISE, T_SURF);
        }
        if (mode != SL_OFF && i == cur)
            cv_rect(x, top + hb + 3, 9, 2, T_ACCENT);
    }
    cv_blit(0, Y_LABEL + 90u);
}
static void lfo_cards(const page_t *pg)
{
    uint32_t bottom;
    const dual_t *dd = dual_of(pg, &bottom);
    const page_t *rps[2] = {dual_row_page(dd, 0), dual_row_page(dd, 1)};
    /* This layout has no rolling card digits; discard animations from the previous page. */
    for (uint32_t col = 0; col < 4u; col++)
        ui.roll[col].from[0] = 0;
    uint32_t sig = 0xD0A100u + bottom + dd->kind * 2u + (uint32_t)(dd - DUALS) * 7u + song.sel * 31u + large_kind() * 113u;
    for (uint32_t row = 0; row < 2u; row++)
        for (uint32_t col = 0; col < 4u; col++) {
            int32_t v;
            sig = sig * 33u + (dual_card(dd, rps[row], row, col, &v) ? (uint32_t)v : 0x7FFFu);
        }
    if (dd->kind == DK_GRAPH)                              /* the graph: the pattern, the mode, the depth, the playing step */
        sig = sig * 33u + sl_pattern(TSEL) + (uint32_t)TSEL->p[P_SLCR] * 65599u + (uint32_t)TSEL->p[P_SLDEPTH] * 131u +
              sl[TSEL - trk].idx * 977u;
    if (!ui.force && ui.graph_sig == sig)
        return;
    ui.graph_sig = sig;
    for (uint32_t row = 0; row < 2u; row++) {
        int active = row == bottom;
        int32_t y = 0;
        if (dd->kind == DK_GRAPH && row) {
            slicer_block(TSEL, dd->hb);
            continue;
        }
        /* Two small blits reuse the existing canvas instead of increasing pool RAM. */
        cv_begin(240, row ? 80 : 90, T_BG);
        uint16_t edge = active ? T_THEME : T_RAISE;
        uint16_t head = active ? T_THEME : T_SURF;
        cv_rrect(3, y, 234, 80, 5, edge, T_BG);
        cv_rrect(5, y + 2, 230, 76, 3, T_SURF, edge);
        cv_rect(5, y + 5, 230, 16, head);
        cv_text_on(10, y + 6, &AF_S, row ? dd->hb : dd->ht, active ? T_INK : T_TEXT, head);
        cv_text_r(230, y + 6, &AF_S, active ? "EDIT K1-4" : "VIEW", active ? T_INK : T_MID, head);
        if (dd->kind == DK_ENV && !row)                     /* the envelope's shape beside its title */
            env_icon(76, y + 7, 70, 12, TSEL->p[rps[0]->id[0]], TSEL->p[rps[0]->id[1]], TSEL->p[rps[0]->id[2]],
                     TSEL->p[rps[0]->id[3]], active ? T_INK : T_MID);
        for (uint32_t col = 0; col < 4u; col++) {
            int32_t v;
            const param_desc_t *d = dual_card(dd, rps[row], row, col, &v);
            int32_t x = 6 + (int32_t)col * 58;
            if (col)
                cv_rect(x - 1, y + 26, 1, 47, T_RAISE);
            if (!d || !d->label || d->label[0] == '-') {
                cv_text_in(x, y + 43, 56, &AF_M, "--", T_DIM, T_SURF);
                continue;
            }
            char val[12]; const char *unit;
            int icon = (dd->kind == DK_LFO || dd->kind == DK_LFO2) && !row && col == 1u;      /* LFO: the WAVE card carries a pictogram */
            int disabled = !row && !col && (dd->kind == DK_LFO2 ? lfo2_config[song.sel].p[L2_SYNC] :
                                            dd->kind == DK_LFO ? TSEL->p[P_LSYNC] : 0);
            const aafont_t *vf = &AF_M;
            param_format(d, v, val, &unit);
            if ((ui_prefs & PREF_LARGE) && !icon && large_face_has(val) && text_w(&AF_L, val) <= 48)
                vf = &AF_L;
            cv_text_in(x, y + 27 - AF_S_CAP_Y, 56, &AF_S, d->label, disabled ? T_DIM : T_TEXT, T_SURF);
            cv_text_in(x, y + (vf == &AF_L ? 40 - AF_L_CAP_Y : 44 - AF_M_CAP_Y), 56, vf, val,
                       disabled ? T_DIM : T_TEXT, T_SURF);
            if (unit[0])
                cv_text_in(x, y + 65 - AF_S_CAP_Y, 56, &AF_S, unit, T_MID, T_SURF);
            if (icon)
                lfo2_wave_icon(x + 10, y + 59, 36, 11, v, active ? T_THEME : T_MID);
            int32_t fill = d->max > d->min ? (v - d->min) * 46 / (d->max - d->min) : 0;
            cv_rect(x + 5, y + 76, 46, 1, T_RAISE);
            if (fill)
                cv_rect(x + 5, y + 76, fill, 1, disabled ? T_DIM : active ? T_THEME : T_MID);
        }
        cv_blit(0, Y_LABEL + row * 90u);
    }
}
static void lfo2_knob(const page_t *pg, uint32_t k, int32_t steps)
{
    uint32_t id = lfo2_page_target(pg->graph, k);
    const param_desc_t *d = lfo2_desc(id);
    if (!d) return;
    lfo2_set(song.sel, id, param_turn(d, lfo2_get(song.sel, id),
        accel(EN_K1 + k, steps, d->fmt == F_ENUM ? 0 : d->max - d->min)));
    ml_knob(TSEL, id);
    load_extend(TSEL);
    ui.force = 1;
}
