/* SPDX-License-Identifier: GPL-3.0-only */
extern uint8_t link_setup_row;
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
enum { DK_LFO, DK_LFO2, DK_PLAIN, DK_ENV, DK_EDIT, DK_GRAPH, DK_INS, DK_HARMONY, DK_LINK };   /* DK_GRAPH: one page above, its graph in the lower block;
                                                                          * DK_INS: the upper group's fourth card is the type's curve (its third FX HOLD) */
typedef struct { const char *top, *bot, *ht, *hb, *ft, *fb; uint8_t kind; } dual_t;
static const dual_t DUALS[] = {
    {"LFO", "LFO SYNC", "LFO SHAPE / RATE", "LFO SYNC / TRIGGER", "LFO TOP", "LFO BOTTOM", DK_LFO},
    {"LFO2", "LFO2 SYNC", "LFO2 SHAPE / RATE", "LFO2 SYNC / TRIGGER", "LFO2 TOP", "LFO2 BOTTOM", DK_LFO2},
    {"LFO DEST", "MOD", "LFO AMOUNTS", "MOD MATRIX", "LFO DEST", "MOD", DK_PLAIN},
    {"ENV", "ENV DEST", "ENV", "ENV DEST / SYNC", "ENV TOP", "ENV BOTTOM", DK_ENV},
    {"EDIT 1", "EDIT 2", "EDIT 1", "EDIT 2", "EDIT TOP", "EDIT BOTTOM", DK_EDIT},
    {"FILTER", "MORE", "FILTER", "MORE", "FILTER", "MORE", DK_PLAIN},        /* ANALOG only: FILTER beside MORE */
    {"LANES", "LANES 2", "LANES 1-4", "LANES 5-8", "LANES TOP", "LANES BOTTOM", DK_PLAIN},
    {"FX", "DLY", "FX SENDS", "DELAY", "FX SENDS", "DELAY", DK_PLAIN},
#if CARAVEL_STASIS_EXPAND
    {"CHORUS", "REVERB", "CHORUS", "REVERB", "CHORUS", "REVERB", DK_PLAIN},
    {"GRAIN", "LAYER", "GRAIN", "LAYER", "STASIS GRAIN", "STASIS LAYER", DK_PLAIN},
#else
    {"REVERB", "CHORUS", "REVERB", "CHORUS", "REVERB", "CHORUS", DK_PLAIN},
#endif
    {"INSERT", "INSERT 2", "INSERT TYPE MIX HOLD", "INSERT A B C FILTER", "INSERT", "INSERT A-C FILT", DK_INS},
    {"SLICER", "", "SLICER", "", "SLICER", "", DK_GRAPH},
    {"SCL", "CHORD", "SCALE", "CHORD KEYS", "SCL", "CHORD", DK_PLAIN},
    {"HARMONY", "", "HARMONY", "DURATION", "HARMONY", "DURATION", DK_HARMONY},
    {"LINK", "LINK", "LINK", "HARMONY SETUP", "LINK / HARMONY SETUP", "LINK / HARMONY SETUP", DK_LINK},
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
    if (pg < PAGES || pg >= PAGES + NPAGES)             /* (a layer's own page, LY_SCL: not one of the family's) */
        return 0;
    for (uint32_t i = 0; i < sizeof DUALS / sizeof DUALS[0]; i++) {
        if (DUALS[i].kind == DK_EDIT && edit_has_picture())
            continue;
        if (str_eq(DUALS[i].top, "FILTER") && ENGINES[TSEL->eng_req % NENGINES] != &ENG_ANALOG)
            continue;                                   /* (FILTER is ANALOG's: MORE stands alone on the others) */
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
static const page_t *dual_top_page(const page_t *pg)
{
    const dual_t *dd=dual_of(pg,0);
    return dd?dual_row_page(dd,0):0;
}
/* the descriptor and value of one card of a row (0: an empty slot) */
static const param_desc_t *dual_card(const dual_t *dd, const page_t *rp, uint32_t row, uint32_t col, int32_t *v)
{
    const param_desc_t *d;
    int16_t *vp = 0;
    *v = 0;
    if (dd->kind == DK_HARMONY) {
        static const char *const DUR[] = {"1 BAR", "2 BARS", "4 BARS", "8 BARS"};
        static const param_desc_t RD[4] = {
            {"CHORD 1", F_NOTE, 0, 11, 0, 0, 0}, {"CHORD 2", F_NOTE, 0, 11, 0, 0, 0},
            {"CHORD 3", F_NOTE, 0, 11, 0, 0, 0}, {"CHORD 4", F_NOTE, 0, 11, 0, 0, 0}};
        static const param_desc_t DD = {"DUR", F_ENUM, 0, 3, 0, DUR, 0};
        if (col >= 4u) return 0;
        if (row) {
            *v = (int32_t)hdur_get(col);
            return &DD;
        }
        *v = (int32_t)harmony_root_eff(hroot_get(col));       /* the effective root: what sounds and what the name shows */
        return &RD[col];
    }
    if (dd->kind == DK_LINK) {
        static const char *const SRC[] = {"OFF", "T1", "T2", "T3", "T4"};
        static const char *const RT[] = {"OFF", "CHORD", "LINK", "BOTH"};
        static const char *const ONOFF[] = {"OFF", "ON"};
        static const param_desc_t SD = {"SOURCE", F_ENUM, 0, 4, 0, SRC, 0};
        static const param_desc_t RD = {"RETRIG", F_ENUM, 0, 3, 0, RT, 0};
        static const param_desc_t KD = {"KEY", F_NOTE, 0, 11, 0, 0, 0};
        static const param_desc_t HD = {"FOLLOW", F_ENUM, 0, 1, 0, ONOFF, 0};     /* this track follows the bus */
        static const param_desc_t HON = {"HARMONY", F_ENUM, 0, 1, 0, ONOFF, 0};   /* the bus itself, for every track */
        if (col >= 4u) return 0;
        if (row) {
            *v = col == 0u ? harmony_on : 0;
            return col == 0u ? &HON : 0;
        }
        uint32_t src = link_src[song.sel];
        if (col == 0u) { *v = src; return &SD; }
        if (col == 1u) { *v = link_retrig[song.sel]; return &RD; }
        if (col == 2u) {
            if (!src || src > NTRK) { *v = 0; return 0; }
            *v = trk[src - 1u].p[P_ROOT];
            return &KD;
        }
        *v = harmony_follow[song.sel];
        return &HD;
    }
    if (dd->kind == DK_LFO2) {
        uint32_t id = lfo2_page_target(row ? GR_LFO2SYNC : GR_LFO2, col);
        d = lfo2_desc(id);
        if (d)
            *v = lfo2_get(song.sel, id);
        return d;
    }
    if (rp && rp->graph == GR_INS && col == 2u && rp->id[0] == P_ITYPE) {   /* INSERT: FX HOLD, the device setting, as its third card (0.3.0; the fourth of INSERT 2 up to 0.2.0) */
        static const char *const ONOFF[] = {"OFF", "ON"};
        static const param_desc_t HOLD = {"FX HOLD", F_ENUM, 0, 1, 0, ONOFF, 0};
        *v = menu_get(MI_FXHOLD) ? 1 : 0;
        return &HOLD;
    }
    if (rp && rp->graph == GR_MOD && !col) {                  /* MOD: KNOB 1 picks the slot (ui_input.c), as on its own page */
        static const param_desc_t SLOT = {"SLOT", F_INT, 1, 4, 1, 0, "/4"};
        *v = (int32_t)mod_ui_slot + 1;
        return &SLOT;
    }
    d = rp ? page_desc(rp, col, &vp) : 0;
    if (d && vp)
        *v = *vp;
    return d;
}
/* DK_GRAPH's lower block: a picture of the page's settings in a frame like the groups' (inner area y 22 .. 77), headed by a
 * title and a caption. The SLICER's, SCL's and CHORD's. */
static void block_begin(const char *title, const char *right, uint16_t rcol)
{
    cv_begin(240, 80, T_BG);
    cv_rrect(3, 0, 234, 80, 5, T_RAISE, T_BG);
    cv_rrect(5, 2, 230, 76, 3, T_SURF, T_RAISE);
    cv_rect(5, 5, 230, 16, T_SURF);
    cv_text_on(10, 6, &AF_S, title, T_TEXT, T_SURF);
    if (right)
        cv_text_r(230, 6, &AF_S, right, rcol, T_SURF);
}
/* the SLICER: two lanes. Above, the 16 steps of the pattern in four groups of four (an open step a lit bar, a closed GATE
 * step what DEPTH leaves open, a closed STUT step a dashed bar), the playing step in the accent colour on a light column.
 * Below, what that does to a held tone: GATE chops it (a closed step as loud as DEPTH leaves it), STUT repeats the last open
 * step's piece at DEPTH. A picture of the settings, not the audio. */
static void slicer_block(const track_t *t)
{
    uint32_t i, pat = sl_pattern(t), mode = (uint32_t)t->p[P_SLCR], cur = sl[t - trk].idx;
    static const char *const MODE[] = {"OFF", "GATE", "STUT"};
    int32_t top = 26, hb = 18, open = hb - t->p[P_SLDEPTH] * hb / 127, cy = 63, amp = 11, px, py = cy;
    uint16_t lit = mode == SL_OFF ? T_RAISE : T_THEME, dim = mode == SL_OFF ? T_DIM : T_MID;
    char ti[16], rate[8];
    const char *unit;
    str_cpy(ti, "PATTERN ", 12);
    fmt_int(ti + str_len(ti), (int32_t)t->p[P_SLPAT]);
    param_format(&TP[P_SLRATE], t->p[P_SLRATE], rate, &unit);
    block_begin(ti, MODE[mode % 3u], mode == SL_OFF ? T_DIM : T_THEME);
    cv_text_r(188, 6, &AF_S, rate, T_MID, T_SURF);
    if (mode != SL_OFF)
        cv_rect(bar_x(cur) - 2, top - 2, 13, 51, T_RAISE);                        /* the playing step's column */
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i);
        uint16_t col = mode != SL_OFF && i == cur ? T_ACCENT : lit;
        if ((pat >> i) & 1u) {
            cv_rrect(x, top, 9, hb, 2, col, T_SURF);
        } else if (mode == SL_STUT) {
            for (int32_t y = top; y < top + hb; y += 3)
                cv_rect(x, y, 9, 1, mode != SL_OFF && i == cur ? T_ACCENT : dim);
        } else {
            cv_rrect(x, top + hb - 2, 9, 2, 1, mode != SL_OFF && i == cur ? T_ACCENT : T_RAISE, T_SURF);
            if (open > 2)
                cv_rrect(x, top + hb - open, 9, open, 2, mode != SL_OFF && i == cur ? T_ACCENT : T_RAISE, T_SURF);
        }
    }
    cv_rect(bar_x(0), cy, bar_x(15) + 9 - bar_x(0), 1, T_RAISE);                  /* the tone, chopped */
    for (px = bar_x(0); px < bar_x(15) + 9; px++) {
        uint32_t k = 0, ph = (uint32_t)(px - bar_x(0)) * (0xFFFFFFFFu / 7u);
        int32_t g = 127, y;
        for (i = 0; i < 16u; i++)
            if (bar_x(i) <= px)
                k = i;
        if (mode != SL_OFF && !((pat >> k) & 1u))
            g = mode == SL_GATE ? 127 - t->p[P_SLDEPTH] : t->p[P_SLDEPTH];
        y = cy - osc_sine(ph) * g / 127 * amp / 32768;
        if (px > bar_x(0))
            cv_line(px - 1, py, px, y, mode == SL_OFF ? T_DIM : mode != SL_OFF && k == cur ? T_ACCENT : (g == 127 ? T_THEME : T_MID));
        py = y;
    }
    cv_blit(0, Y_LABEL + 90u);
}
/* HARMONY ROOTS card col: val gets the name of the chord the slot PREPARES (its root and that root's own quality, from the
 * shared description in seq.c, never mixed with the shifted live chord). The returned unit line: nothing, or with SHIFT
 * "PREP" (and on the slot that sounds now "LIVE" plus the shifted chord) so the names cannot be read as the sounding ones. */
static const char *harmony_card_text(uint32_t col, char *val)
{
    static char u[14];
    harmony_card_name(col, val, 0);
    u[0] = 0;
#if CARAVEL_ACTION_BANKS
    if (harmony_shift) {
        if (harmony_on && harmony_chord_at(harmony_bar) == (col & 3u)) {
            char nm[12];
            harmony_card_name(col, nm, 1);
            str_cpy(u, "LIVE ", sizeof u);
            str_cpy(u + str_len(u), nm, sizeof u - str_len(u));
        } else
            str_cpy(u, "PREP", sizeof u);
    }
#endif
    return u;
}
/* The key the global HARMONY chords are built in: T1's ROOT and SCALE (harmony_scale_abs), as "C MAJ T1"; where the width w is too narrow
 * for a long scale name "T1 C" (the scale dropped before the track). */
static const char *harmony_basis(char *b, uint32_t bs, int32_t w)
{
    str_cpy(b, N_NOTE[(uint32_t)trk[0].p[P_ROOT] % 12u], bs);
    str_cpy(b + str_len(b), " ", bs - str_len(b));
    str_cpy(b + str_len(b), N_SCALE[(uint32_t)trk[0].p[P_SCALE] % (sizeof N_SCALE / sizeof N_SCALE[0])], bs - str_len(b));
    str_cpy(b + str_len(b), " T1", bs - str_len(b));
    if (text_w(&AF_S, b) > w) {
        str_cpy(b, "T1 ", bs);
        str_cpy(b + str_len(b), N_NOTE[(uint32_t)trk[0].p[P_ROOT] % 12u], bs - str_len(b));
    }
    return b;
}
static void link_card(uint32_t row, uint32_t col, int32_t x, int32_t y, int active)
{
    const char *lab = "", *val = "";
    char buf[20];
    uint32_t src = link_src[song.sel];
    if (!row) {
        static const char *const SRC[] = {"OFF", "T1", "T2", "T3", "T4"};
        static const char *const RT[] = {"OFF", "CHORD", "LINK", "BOTH"};
        if (col == 0u) { lab = "SOURCE"; val = SRC[src <= 4u ? src : 0u]; }
        else if (col == 1u) { lab = "RETRIG"; val = RT[link_retrig[song.sel] & 3u]; }
        else if (col == 2u) {
            lab = "KEY";
            if (!src || src > NTRK) val = "--";
            else {
                str_cpy(buf, N_NOTE[(uint32_t)trk[src - 1u].p[P_ROOT] % 12u], sizeof buf);
                str_cpy(buf + str_len(buf), " ", sizeof buf - str_len(buf));
                str_cpy(buf + str_len(buf), N_SCALE[(uint32_t)trk[src - 1u].p[P_SCALE] %
                        (sizeof N_SCALE / sizeof N_SCALE[0])], sizeof buf - str_len(buf));
                val = buf;
            }
        } else { lab = "FOLLOW"; val = harmony_follow[song.sel] ? "ON" : "OFF"; }
    } else {
        if (col == 0u) { lab = "HARMONY"; val = harmony_on ? "ON" : "OFF"; }
        else {                                       /* K2..K4 do nothing here: a hint, not three cards */
            if (col == 1u) {
                cv_text_in(x, y + 31 - AF_S_CAP_Y, 168, &AF_S, "ONLY K1 WORKS HERE", T_DIM, T_SURF);
                cv_text_in(x, y + 46 - AF_S_CAP_Y, 168, &AF_S, harmony_on ? "ROOTS, DURATIONS:" : "TURN K1 RIGHT FIRST,", T_MID, T_SURF);
                cv_text_in(x, y + 60 - AF_S_CAP_Y, 168, &AF_S, harmony_on ? "SELECT OR SCL KEY" : "THEN SELECT", T_MID, T_SURF);
            }
            return;
        }
    }
    uint16_t c = active ? T_TEXT : T_MID;
    cv_text_in(x, y + 27 - AF_S_CAP_Y, 56, &AF_S, lab, active ? T_TEXT : T_DIM, T_SURF);
    cv_text_in(x, y + 44 - AF_M_CAP_Y, 56, &AF_M, val, c, T_SURF);
    if (row && col == 0u) {                             /* the bus is global: the key it is built in */
        char kb[20];
        cv_text_in(x, y + 65 - AF_S_CAP_Y, 56, &AF_S, harmony_basis(kb, sizeof kb, 56), T_MID, T_SURF);
    }
}
static void lfo_cards(const page_t *pg)
{
    uint32_t bottom;
    const dual_t *dd = dual_of(pg, &bottom);
    if (dd->kind == DK_LINK) bottom = link_setup_row ? 1u : 0u;
    if (dd->kind == DK_HARMONY) bottom = harmony_edit_dur ? 1u : 0u;   /* the active row follows the SCL row key */
    const page_t *rps[2] = {dual_row_page(dd, 0), dual_row_page(dd, 1)};
    /* This layout has no rolling card digits; discard animations from the previous page. */
    for (uint32_t col = 0; col < 4u; col++)
        ui.roll[col].from[0] = 0;
    uint32_t sig = 0xD0A100u + bottom + dd->kind * 2u + (uint32_t)(dd - DUALS) * 7u + song.sel * 31u +
                   (uint32_t)ui.page * 257u + large_kind() * 113u;
    for (uint32_t row = 0; row < 2u; row++)
        for (uint32_t col = 0; col < 4u; col++) {
            int32_t v;
            sig = sig * 33u + (dual_card(dd, rps[row], row, col, &v) ? (uint32_t)v : 0x7FFFu);
        }
    if (dd->kind == DK_HARMONY) {                          /* the names follow T1's scale, the SHIFT and the sounding slot */
        sig = sig * 33u + harmony_scale_abs() * 7u + harmony_on * 3u;
        sig = sig * 33u + (uint32_t)trk[0].p[P_ROOT] * 131u + (uint32_t)trk[0].p[P_SCALE] * 17u;   /* the key label shows T1's own ROOT and SCALE */
#if CARAVEL_ACTION_BANKS
        sig = sig * 33u + (uint32_t)(int32_t)harmony_shift * 17u + (harmony_on ? harmony_chord_at(harmony_bar) * 977u : 0u);
#endif
    }
    if (dd->kind == DK_LINK) {
        uint32_t src = link_src[song.sel];
        sig = sig * 33u + harmony_follow[song.sel] * 7u + harmony_on * 3u;
        sig = sig * 33u + harmony_scale_abs() * 5u;          /* the key shown under the HARMONY switch */
        sig = sig * 33u + (uint32_t)trk[0].p[P_ROOT] * 131u + (uint32_t)trk[0].p[P_SCALE] * 17u;   /* (CHR and periodic scales: one mask for several ROOTs) */
        sig = sig * 33u + link_setup_row * 131u + (src && src <= NTRK ? (uint32_t)trk[src - 1u].p[P_SCALE] * 17u : 0u);
    }
#if CARAVEL_STASIS_EXPAND
    if(rps[1] && rps[1]->graph==GR_STLAYER)
        sig=sig*33u+st_layer.phase+st_layer.request*8u+st.auto_hold*32u+st.releasing*64u+st.stopping*128u;
#endif
    if (str_eq(dd->top, "SCL"))                            /* the CHRD card's LOCAL DIA3 note follows the bus switches */
        sig = sig * 33u + harmony_on * 3u + harmony_follow[song.sel] * 7u;
    if (dd->kind == DK_GRAPH && str_eq(dd->top, "SLICER"))   /* the graph: the pattern, the mode, the rate, the depth, the playing step */
        sig = sig * 33u + sl_pattern(TSEL) + (uint32_t)TSEL->p[P_SLCR] * 65599u + (uint32_t)TSEL->p[P_SLDEPTH] * 131u +
              (uint32_t)TSEL->p[P_SLRATE] * 7919u + (uint32_t)TSEL->p[P_SLPAT] * 31337u + sl[TSEL - trk].idx * 977u;
    if (rps[1] && rps[1]->graph == GR_MOD)                 /* the slot pips: every slot's SRC, DST and AMT */
        for (uint32_t k = 0; k < NMSLOT * 3u; k++)
            sig = sig * 33u + (uint32_t)TSEL->p[P_M1SRC + k];
    if (dd->kind == DK_INS)                                /* the curve: TYPE and MIX are cards, A B C are not on this group */
        sig = sig * 33u + (uint32_t)TSEL->p[P_IA] * 97u + (uint32_t)TSEL->p[P_IB] * 8191u + (uint32_t)TSEL->p[P_IC] * 524287u;
    if (!ui.force && ui.graph_sig == sig)
        return;
    ui.graph_sig = sig;
    for (uint32_t row = 0; row < 2u; row++) {
        int active = row == bottom;
        int32_t y = 0;
        if (dd->kind == DK_GRAPH && row) {
            slicer_block(TSEL);                            /* (DK_GRAPH is SLICER's alone) */
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
        {
            char htxt[12];
            if (dd->kind == DK_HARMONY) {
                str_cpy(htxt, harmony_edit_dur ? "DUR " : "ROOT ", sizeof htxt);
                str_cpy(htxt + str_len(htxt), harmony_on ? "ON" : "OFF", sizeof htxt - str_len(htxt));
                cv_text_r(230, y + 6, &AF_S, htxt, active ? T_INK : T_MID, head);
            } else
                cv_text_r(230, y + 6, &AF_S, !active ? "VIEW" : dd->kind == DK_LINK && row ? "EDIT K1" : "EDIT K1-4", active ? T_INK : T_MID, head);   /* (SETUP: only K1 works) */
        }
        if (rps[row] && rps[row]->graph == GR_MOD) {          /* the four slots at a glance: filled = in use, the one edited underlined */
            for (uint32_t k = 0; k < NMSLOT; k++) {
                const int16_t *mp = &TSEL->p[P_M1SRC + 3u * k];
                int on = mp[0] && mp[1] && mp[2];
                int32_t px = 90 + (int32_t)k * 11;
                cv_rrect(px, y + 8, 8, 8, 2, on ? (active ? T_INK : T_MID) : T_RAISE, head);
                if (!on)
                    cv_rrect(px + 1, y + 9, 6, 6, 1, head, T_RAISE);
                if (k == mod_ui_slot)
                    cv_rect(px, y + 18, 8, 1, active ? T_INK : T_MID);
            }
        }
        int bus_row = dd->kind == DK_HARMONY || (dd->kind == DK_LINK && row);   /* the bus (progression, durations, its switch) is global, keyed from T1 */
        if (rps[row] || dd->kind == DK_LFO2 || bus_row) {   /* who the group is for: this track, or every track (a global page) */
            char tag[4];
            int global = bus_row || (rps[row] && rps[row]->scope == SC_GLOBAL);
            tag[0] = global ? 'A' : 'T'; tag[1] = global ? 'L' : (char)('1' + song.sel % NTRK); tag[2] = global ? 'L' : 0; tag[3] = 0;
            cv_rrect(144, y + 7, 28, 12, 3, global ? T_ACCENT : active ? T_INK : T_RAISE, head);
            cv_text_in(144, y + 6, 28, &AF_S, tag, global ? T_INK : active ? T_THEME : T_MID, global ? T_ACCENT : active ? T_INK : T_RAISE);
        }
        if (dd->kind == DK_HARMONY) {                       /* the key all four chords are in, between the title and the ALL badge */
            char kb[20];
            const char *kt = harmony_basis(kb, sizeof kb, 68);
            cv_text_r(141, y + 6, &AF_S, kt, active ? T_INK : T_MID, head);
        }
        if (dd->kind == DK_ENV && !row)                     /* the envelope's shape beside its title */
            env_icon(76, y + 7, 70, 12, TSEL->p[rps[0]->id[0]], TSEL->p[rps[0]->id[1]], TSEL->p[rps[0]->id[2]],
                     TSEL->p[rps[0]->id[3]], active ? T_INK : T_MID);
        for (uint32_t col = 0; col < 4u; col++) {
            int32_t v;
            const param_desc_t *d = dual_card(dd, rps[row], row, col, &v);
            int32_t x = 6 + (int32_t)col * 58;
            if (col && !(dd->kind == DK_LINK && row && col >= 2u))   /* (the SETUP hint spans K2..K4) */
                cv_rect(x - 1, y + 26, 1, 47, T_RAISE);
#if CARAVEL_STASIS_EXPAND
            if(rps[row] && rps[row]->graph==GR_STLAYER && col==3u){
                const char *action=stasis_layer_can_cancel()?"CANCEL":song.g[G_STMODE]==1?"BLEND":"ADD";
                cv_text_in(x,y+27-AF_S_CAP_Y,56,&AF_S,action,T_TEXT,T_SURF);
                cv_text_in(x,y+44-AF_M_CAP_Y,56,&AF_M,stasis_layer_status(),T_ACCENT,T_SURF);
                cv_text_in(x,y+65-AF_S_CAP_Y,56,&AF_S,active?"OCT+":"SELECT",T_MID,T_SURF);
                continue;
            }
#endif
            if (dd->kind == DK_INS && !row && col == 3u) {     /* the fourth card: the type's curve, one card wide */
                insert_curve(TSEL, active ? T_THEME : T_MID, x + 3, 50, y + 49, 20);
                continue;
            }
            if (dd->kind == DK_LINK) {
                link_card(row, col, x, y, active);
                continue;
            }
            if (!d || !d->label || d->label[0] == '-') {
                cv_text_in(x, y + 43, 56, &AF_M, "--", T_DIM, T_SURF);
                continue;
            }
            char val[12]; const char *unit;
            int icon = (dd->kind == DK_LFO || dd->kind == DK_LFO2) && !row && col == 1u;      /* LFO: the WAVE card carries a pictogram */
            int disabled = !row && !col && (dd->kind == DK_LFO2 ? lfo2_config[song.sel].p[L2_SYNC] :
                                            dd->kind == DK_LFO ? TSEL->p[P_LSYNC] : 0);
#if CARAVEL_STASIS_EXPAND
            if(rps[row] && rps[row]->graph==GR_STLAYER &&
                ((col==1u && song.g[G_STMODE]==1)||(col==2u && song.g[G_STMODE]!=1)))disabled=1;
#endif
            const aafont_t *vf = &AF_M;
            param_format(d, v, val, &unit);
            if (dd->kind == DK_HARMONY && !row)    /* the top row: the PREPARED chord's name; SHIFT says what sounds */
                unit = harmony_card_text(col, val);
#if CARAVEL_STASIS_EXPAND
            /* The generic formatter intentionally limits values to five
             * letters. This view has room for the complete effect name. */
            if(rps[row] && rps[row]->id[0]==G_RTYPE && !col && v==3)
                str_cpy(val,"STASIS",sizeof val);
#endif
            if (rps[row] && rps[row]->graph == GR_MOD && col == 2u) {   /* MOD's DST: E1..E8 by the engine's own labels (mod.c mod_dst_name) */
                str_cpy(val, mod_dst_name(TSEL, v), sizeof val);
                unit = "";
            }
            if ((ui_prefs & PREF_LARGE) && !icon && large_face_has(val) && text_w(&AF_L, val) <= 48)
                vf = &AF_L;
#if CARAVEL_STASIS_EXPAND
            if(text_w(vf,val)>54)vf=&AF_S; /* full STASIS name in the narrow TYPE card */
#endif
            cv_text_in(x, y + 27 - AF_S_CAP_Y, 56, &AF_S, d->label, disabled ? T_DIM : T_TEXT, T_SURF);
            cv_text_in(x, y + (vf == &AF_L ? 40 - AF_L_CAP_Y : vf == &AF_S ? 46 - AF_S_CAP_Y : 44 - AF_M_CAP_Y), 56, vf, val,
                       disabled ? T_DIM : T_TEXT, T_SURF);
            if (rps[row] && rps[row]->id[col] == P_CHRD && v == CH_BUS && !(harmony_on && harmony_follow[song.sel]))
                unit = text_w(&AF_S, "OWN DIA3") <= 56 ? "OWN DIA3" : "LOCAL";   /* U10: no bus for this track: its own in-key triad plays */
            if (dd->kind == DK_HARMONY && row)                  /* U09/U12/U13: the row's own line: what HARMONY does, in what unit, how to switch it on */
                unit = "";
            if (unit[0])
                cv_text_in(x, y + 65 - AF_S_CAP_Y, 56, &AF_S, unit, T_MID, T_SURF);
            if (icon)
                lfo2_wave_icon(x + 10, y + 59, 36, 11, v, active ? T_THEME : T_MID);
            int32_t fill = d->max > d->min ? (v - d->min) * 46 / (d->max - d->min) : 0;
            cv_rect(x + 5, y + 76, 46, 1, T_RAISE);
            if (fill)
                cv_rect(x + 5, y + 76, fill, 1, disabled ? T_DIM : active ? T_THEME : T_MID);
        }
        if (dd->kind == DK_HARMONY && row) {                  /* one line under the DUR cards */
            char hl[32];
            str_cpy(hl, "ARP EVERY NOTE, SEQ 1 5 9 13", sizeof hl);
#if CARAVEL_ACTION_BANKS
            if (harmony_shift) {
                str_cpy(hl, "SHIFT ", sizeof hl);
                fmt_int(hl + str_len(hl) + (harmony_shift > 0 ? 1 : 0), harmony_shift);
                if (harmony_shift > 0) hl[6] = '+';
                str_cpy(hl + str_len(hl), " DEG (SCALE STEPS)", sizeof hl - str_len(hl));
            }
#endif
            cv_text_in(6, y + 65 - AF_S_CAP_Y, 228, &AF_S, hl, T_MID, T_SURF);
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
