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
enum { DK_LFO, DK_LFO2, DK_PLAIN, DK_ENV, DK_EDIT, DK_GRAPH, DK_INS };   /* DK_GRAPH: one page above, its graph in the lower block;
                                                                          * DK_INS: the upper group's free cards hold the type's curve */
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
    {"REVERB", "CHORUS", "REVERB", "CHORUS", "REVERB", "CHORUS", DK_PLAIN},
    {"INSERT", "INSERT 2", "INSERT TYPE / MIX", "INSERT A / B / C", "INSERT", "INSERT A-C", DK_INS},
    {"SLICER", "", "SLICER", "", "SLICER", "", DK_GRAPH},
    {"SCL", "", "SCALE", "", "SCL", "", DK_GRAPH},
    {"CHORD", "", "CHORD KEYS", "", "CHORD", "", DK_GRAPH},
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
/* SCL: the 12 keys (black keys high, white keys low): in the scale THEME, the root the accent, out of the scale RAISE */
static void scale_block(const track_t *t)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t i, mask = scale_mask(t);
    char ti[16];
    str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
    str_cpy(ti + str_len(ti), " ", 4);
    str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    block_begin(ti, 0, T_MID);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 13 + (int32_t)i * 18;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : T_THEME) : T_RAISE;
        cv_rrect(x + 3, BLACK[i] ? 26 : 44, 10, BLACK[i] ? 26 : 31, 3, col, T_SURF);
    }
    cv_blit(0, Y_LABEL + 90u);
}
/* CHORD: its name ("Cm7": the last one the track played, else the one on its ROOT from C4) as the title, the keys from the C below
 * its lowest note (two octaves, three for a wide chord): its notes THEME, its root the accent, the others RAISE */
static void chord_block(const track_t *t)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, j, k = trk_index(t), lo, cnt, w;
    int32_t r;
    uint16_t mask;
    char b[12];
    const char *cap = "LAST";
    if (!t->p[P_CHRD] || chord_kit(t)) {
        block_begin(t->p[P_CHRD] ? "NO CHORDS ON KITS" : "CHORD KEYS OFF", 0, T_MID);
        if (!t->p[P_CHRD])
            cv_text_in(5, 40, 230, &AF_S, "K1: DIA3 PLAYS IN-KEY CHORDS", T_MID, T_SURF);
        cv_blit(0, Y_LABEL + 90u);
        return;
    }
    if (chord_last[k].n) {
        r = chord_last[k].root;
        mask = chord_last[k].mask;
        n = chord_last[k].n;
        for (i = 0; i < n; i++)
            nn[i] = chord_last[k].note[i];
    } else {
        n = chord_make(t, (uint32_t)(60 + t->p[P_ROOT]), nn, &r, &mask);
        cap = "ON ROOT";
    }
    if (trk_vmode(t) != V_POLY)
        cap = "ROOT ONLY";                              /* MONO / LEGATO / UNISON */
    chord_name(b, (uint32_t)r, mask);
    block_begin(b, cap, T_MID);
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = 0;
        int32_t x = 13 + (int32_t)(i * w);
        for (j = 0; j < n; j++)
            on |= nn[j] == note;
        cv_rrect(x, BLACK[note % 12u] ? 26 : 44, (int32_t)w - 2, BLACK[note % 12u] ? 26 : 31, w > 6u ? 3 : 2,
                 !on ? T_RAISE : note % 12u == (uint32_t)r % 12u ? T_ACCENT : T_THEME, T_SURF);
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
    if (dd->kind == DK_GRAPH && str_eq(dd->top, "CHORD")) {      /* the chord's notes: the last one played */
        uint32_t k = trk_index(TSEL);
        sig = sig * 33u + chord_last[k].n * 131u + chord_last[k].mask * 7u + (uint32_t)chord_last[k].root + trk_vmode(TSEL) * 977u +
              (uint32_t)TSEL->p[P_ROOT] * 31u;
    }
    if (dd->kind == DK_GRAPH && str_eq(dd->top, "SCL"))
        sig = sig * 33u + scale_mask(TSEL) + (uint32_t)TSEL->p[P_ROOT] * 31u;
    if (dd->kind == DK_GRAPH && str_eq(dd->top, "SLICER"))   /* the graph: the pattern, the mode, the rate, the depth, the playing step */
        sig = sig * 33u + sl_pattern(TSEL) + (uint32_t)TSEL->p[P_SLCR] * 65599u + (uint32_t)TSEL->p[P_SLDEPTH] * 131u +
              (uint32_t)TSEL->p[P_SLRATE] * 7919u + (uint32_t)TSEL->p[P_SLPAT] * 31337u + sl[TSEL - trk].idx * 977u;
    if (rps[1] && rps[1]->graph == GR_MOD)                 /* the slot pips: every slot's SRC, DST and AMT */
        for (uint32_t k = 0; k < NMSLOT * 3u; k++)
            sig = sig * 33u + (uint32_t)TSEL->p[P_M1SRC + k];
    if (dd->kind == DK_INS)                                /* the curve: TYPE and MIX are cards, A B C are not on this page */
        sig = sig * 33u + (uint32_t)TSEL->p[P_IA] * 97u + (uint32_t)TSEL->p[P_IB] * 8191u + (uint32_t)TSEL->p[P_IC] * 524287u;
    if (!ui.force && ui.graph_sig == sig)
        return;
    ui.graph_sig = sig;
    for (uint32_t row = 0; row < 2u; row++) {
        int active = row == bottom;
        int32_t y = 0;
        if (dd->kind == DK_GRAPH && row) {
            if (str_eq(dd->top, "SLICER"))
                slicer_block(TSEL);
            else if (str_eq(dd->top, "SCL"))
                scale_block(TSEL);
            else
                chord_block(TSEL);
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
        if (rps[row] || dd->kind == DK_LFO2) {            /* who the group is for: this track, or every track (a global page) */
            char tag[4];
            int global = rps[row] && rps[row]->scope == SC_GLOBAL;
            tag[0] = global ? 'A' : 'T'; tag[1] = global ? 'L' : (char)('1' + song.sel % NTRK); tag[2] = global ? 'L' : 0; tag[3] = 0;
            cv_rrect(144, y + 7, 28, 12, 3, global ? T_ACCENT : active ? T_INK : T_RAISE, head);
            cv_text_in(144, y + 6, 28, &AF_S, tag, global ? T_INK : active ? T_THEME : T_MID, global ? T_ACCENT : active ? T_INK : T_RAISE);
        }
        if (dd->kind == DK_ENV && !row)                     /* the envelope's shape beside its title */
            env_icon(76, y + 7, 70, 12, TSEL->p[rps[0]->id[0]], TSEL->p[rps[0]->id[1]], TSEL->p[rps[0]->id[2]],
                     TSEL->p[rps[0]->id[3]], active ? T_INK : T_MID);
        for (uint32_t col = 0; col < 4u; col++) {
            int32_t v;
            const param_desc_t *d = dual_card(dd, rps[row], row, col, &v);
            int32_t x = 6 + (int32_t)col * 58;
            if (col)
                cv_rect(x - 1, y + 26, 1, 47, T_RAISE);
            if (dd->kind == DK_INS && !row && col >= 2u) {     /* the two free cards: the type's curve */
                if (col == 2u)
                    insert_curve(TSEL, active ? T_THEME : T_MID, x + 6, 100, y + 49, 20);
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
