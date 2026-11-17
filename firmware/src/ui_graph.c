/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
#if FELUCCA_PAIR_MORPH
static void pair_ui_graph(void);
#endif
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;



/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || (ui_prefs & PREF_ANIM_OFF);
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor)
            cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
        if (step_detailed(&seq_steps(t)[si]))         /* 1.5.1: CHANCE / RATCH / NUDGE set: a mark over it (DETAIL) */
            cv_rect(x + 4, 2, 5, 3, c);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? T_ACCENT : c;
        if (s == si)
            pr_bars(st, x + 2, 9, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing. 1.4.1: a muted lane (the EDIT layer's
 * black keys, eng_drum.c drum_mute): its name DIM, the MUTE icon after it, its hits DIM */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, mute = drum_mute[trk_index(t) % NPART];
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t y0 = 6;
    for (i = 0; i < 16u && base + i < len; i++) {   /* a parameter lock: a mark over the step; CHANCE / RATCH / */
        if ((locks >> (base + i)) & 1u)               /* NUDGE set (1.5.1, DETAIL): a dot under it */
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
        if (step_detailed(&seq_steps(t)[base + i]))
            cv_rect(43 + (int32_t)i * 12, 3, 4, 2, c);
    }
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane, mu = (mute >> l) & 1u;
        uint16_t row = sel ? T_RAISE : T_SURF, hc = mu ? T_DIM : c, ac = mu ? T_DIM : T_ACCENT;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, 14, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + 2, 0, y + 12, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y - 1, &AF_S, drum_lane_abbr(t, l), mu ? T_DIM : sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        if (mu) {                                    /* (27 .. 39: between the name and the first hit) */
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "drum lane mute icon centred up/down");
            cv_icon_in(27, y, 0, 14, 12, ICON_MUTE, T_DIM, row);
        }
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + 2, 10, 10, (step_accents(st) & b) ? ac : hc, step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rrect(x, y + 2, 10, 10, 2, (step_accents(st) & b) ? ac : hc, row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && t->seq_idx < len && t->seq_idx / 16u == ui.bank)
        cv_rect(40 + (int32_t)(t->seq_idx % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* SEQ > DETAIL (1.5.1, Discussion #199): the cursor's 16 steps as four lanes, the knobs' order top to bottom, each
 * named on the left (the lane of the knob just turned in the accent): CHANCE a bar as high as the chance, RATCH the
 * step's bar in its 2..4 parts (as the roll draws a ratchet), NUDGE a tick left / right of the step's middle, VEL a bar
 * as high as the velocity (an ACC step's full, TEXT). Off its default a value is bright (THEME, the cursor's ACCENT),
 * at it faint (RAISE: 100 %, x1, 0, a VEL never set); DIM where it does not play (a NUDGE with QUANTIZE ON or on a
 * ratchet, a VEL under ACC). A step that plays nothing: a dot (a value left on it DIM); past LEN nothing. The cursor
 * framed through the lanes, the step playing a line */
#define SD_X0 36                                    /* the first step column (12 px each) */
static void graph_detail(const track_t *t, uint16_t c)
{
    static const char *const NAME[4] = {"CHN", "RAT", "NDG", "VEL"};
    static const uint8_t LY[4] = {4, 38, 62, 86}, LH[4] = {28, 18, 18, 28};
    uint32_t i, l, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol;
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    for (l = 0; l < 4u; l++) {
        int32_t yb = LY[l] + LH[l];
        cv_rect(SD_X0, yb, (int32_t)ncol * 12 + 1, 1, T_GRID);   /* the lane's floor */
        cv_text_on(8, LY[l] + LH[l] / 2 - 8, &AF_S, NAME[l], ui.hot_t && ui.hot_col == l ? T_ACCENT : T_MID, T_SURF);
    }
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        const step_t *st = &seq_steps(t)[si];
        int32_t x = SD_X0 + (int32_t)i * 12;
        uint16_t hi = si == ui.cursor ? T_ACCENT : c;
        if (si == ui.cursor)
            cv_frame(x, 1, 13, 116, T_TEXT);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, 2, 1, 114, T_ACCENT);
        for (l = 0; l < 4u; l++) {
            uint32_t k = SD_KIND[l];
            int32_t v = ev_sval(st, k), y = LY[l], h = LH[l], set = v != ev_sdefv(k), bh;
            uint16_t col;
            if (!sd_takes(st, k) && !set) {           /* nothing plays there */
                cv_rect(x + 5, y + h - 3, 2, 2, T_DIM);
                continue;
            }
            col = !sd_takes(st, k) ? T_DIM : set ? hi : T_RAISE;
            if (k == EVK_CHANCE && v > 100) {         /* 1.5.5: a condition: a:b as b cells up the lane, the a-th lit; */
                int32_t c = v - 100, j, nb = c < 10 ? COND_B[c - 1] : 0;   /* FILL a frame, !FILL a frame crossed */
                for (j = 0; j < nb; j++)
                    cv_rect(x + 2, y + h - 6 - 7 * j, 9, 5, j == COND_A[c - 1] - 1 ? col : T_GRID);
                if (!nb)
                    cv_frame(x + 2, y + 2, 9, h - 2, col);
                if (c == (int32_t)COND_NFILL)
                    cv_rect(x + 2, y + h / 2, 9, 1, col);
            } else if (k == EVK_CHANCE) {
                bh = v * h / 100;
                cv_rect(x + 2, y + h - (bh < 2 ? 2 : bh), 9, bh < 2 ? 2 : bh, col);
            } else if (k == EVK_RATCH) {
                pr_split(x + 2, y + 5, 9, 8, col, (uint32_t)v);
            } else if (k == EVK_NUDGE) {
                if (set && (t->p[P_SQNT] || !ev_nudge_plays(t, st)))
                    col = T_DIM;
                cv_rect(x + 6, y + 2, 1, h - 4, T_GRID);
                if (!set)
                    cv_rect(x + 5, y + h / 2 - 1, 3, 3, col);
                else
                    cv_rect(x + 5 + v * 5 / 8, y + 3, 3, h - 6, col);
            } else if (st->flags & SF_ACCENT) {       /* VEL: an ACC step plays 127 */
                cv_rect(x + 2, y, 9, h, sd_takes(st, k) ? T_TEXT : T_DIM);
            } else {
                bh = v * h / 127;
                cv_rect(x + 2, y + h - (bh < 2 ? 2 : bh), 9, bh < 2 ? 2 : bh, col);
            }
        }
    }
}
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
/* INSERT pages (1.5, fx.c track_insert): the drives and CRUSH as their curve, input (-1.5 .. +1.5 of a voice's full
 * scale) across, output up, dry and wet blended by MIX (fx.c ins_shape: the same shape; the dry diagonal RAISE); the
 * swept ones as two cycles of their sweep, as deep as DEPTH (FLANG the squared triangle, CHOR a sine, PHASR a
 * triangle). OFF: the dry diagonal, dim */
static void insert_curve(const track_t *t, uint16_t c, int32_t x0, int32_t pw, int32_t cy, int32_t a)
{
    int32_t x, py = cy, ty = clamp(t->p[P_ITYPE], 0, IT_N - 1);
    int32_t m = ins_mixq(t->p[P_IMIX]), g = 4096 + t->p[P_IA] * t->p[P_IA] * 3, lvl = LEVEL_Q12[t->p[P_IC] & 127];
    uint32_t sh = ins_bits_sh(t->p[P_IA]);
    cv_rect(x0, cy, pw, 1, T_RAISE);
    cv_line(x0 + pw / 2, cy - a, x0 + pw / 2, cy + a, T_RAISE);
    if (ty == IT_OFF || ty >= IT_PHASER || !m)
        cv_line(x0 + pw / 6, cy + a, x0 + pw * 5 / 6, cy - a, ty ? T_RAISE : T_DIM);
    if (ty == IT_OFF)
        return;
    for (x = 0; x < pw; x++) {
        int32_t y, w;
        if (ty >= IT_PHASER) {                       /* the sweep, two cycles */
            uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / (pw / 2u)), tr = pf_tri(ph);
            w = ty == IT_CHORUS ? osc_sine(ph) * 2 : ty == IT_FLANGER ? (int32_t)((tr * tr) >> 16) * 2 - 65536 : (int32_t)tr * 2 - 65536;
            y = cy - (w >> 1) * a / 32768 * t->p[P_IB] / 127;
        } else {                                     /* the curve: v in, dry v, wet shaped */
            int32_t v = (x - pw / 2) * 49152 / (pw / 2), o;
            o = ty == IT_CRUSH ? ins_shape(IT_CRUSH, v, sh) : (ins_shape((uint32_t)ty, ((v >> 3) * g) >> 9, 0) * lvl) >> 12;
            w = v + mulq16(o - v, (uint32_t)m << 1);
            y = cy - clamp(w, -49152, 49152) * a / 49152;
        }
        if (x)
            cv_line_t(x0 + x - 1, py, x0 + x, y, c, 2);
        py = y;
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ (st->vel | (uint32_t)st->probability << 8)) * 16777619u;   /* (1.5.1: DETAIL, the marks) */
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */

static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint16_t graph_plen[4][NTRK];                  /* .. each track's pattern length (project_info: 1/48 quarters) */
static uint32_t graph_pden[4][NTRK];                  /* .. and its picture for SONG (project.c proj_density: 2 bits a cell) */
static uint8_t graph_pcell[4][NTRK];                  /* .. in this many cells (min(LEN, 16)) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++)
            mask |= (uint8_t)((project_info(i, graph_pname[i], graph_plen[i], graph_pden[i], graph_pcell[i]) != 0) << i);
        graph_pname_sig = fnv(fnv(2166136261u, graph_pname, sizeof graph_pname), graph_plen, sizeof graph_plen);
        graph_pname_sig = fnv(fnv(graph_pname_sig, graph_pden, sizeof graph_pden), graph_pcell, sizeof graph_pcell);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}


/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : (g == GR_LFO || g == GR_LFOSYNC || g == GR_LFO2 || g == GR_LFO2SYNC) ? SK_LFO : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static int dual_bottom(const page_t *pg);              /* ui_lfo2.c: the lower half of a two-group view */
static const char *dual_foot(const page_t *pg);
static const page_t *dual_top_page(const page_t *pg);
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    const char *df = ui.home ? 0 : dual_foot(pg);                                    /* a two-group view: "LFO TOP" */
    const page_t *number_page = df ? dual_top_page(pg) : pg;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i) && !dual_bottom(&PAGES[i])) {   /* (top and bottom: one view) */
            n++;
            if (&PAGES[i] == number_page)
                k = n;
        }
    str_cpy(ti, df ? df : pt ? pt : grid_on() && !detail_on() ? "GRID" : pg->title, df ? 16 : 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t) && !detail_on()) {   /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
#if CARAVEL_REVERB_MINIMAL
    if (pg->graph == GR_STLAYER) h ^= ui.frame / 2u;
#endif
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP) h ^= ui.frame / 2u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig;
        h ^= project_ram_mask() * 2654435761u;
    }
    if (pg->graph == GR_EVENTS)                      /* AUTOMATION: the rows, the one selected, + ADD's place, PLAY,
                                                      * QUANTIZE */
        h = (h ^ ev_sig()) * 16777619u;
    if (pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u +
             drum_mute[trk_index(t) % NPART] * 2654435761u;
        {                                            /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (detail_on())                             /* DETAIL: QUANTIZE (NUDGE's DIM), the lane named in the accent */
            h = (h ^ (0xD1u + (uint32_t)t->p[P_SQNT] * 31u + (ui.hot_t ? ui.hot_col + 1u : 0u) * 977u)) * 2654435761u;
        else if (!drum_track(t)) {                   /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px), in M as every list's (PRESETS, USER, PHRASES, PROJECT, AUTOMATION, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + LIST_TY, &AF_M, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(LIST_NX, y + LIST_TY, &AF_M, name, sel ? T_INK : nc, bg, x1 - LIST_NX);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* preset browser: the global list (every engine), the current one selected; tag DIM, name TEXT,
 * favourites starred (the accent), the selected row's suggested pattern at its right */
static void graph_browse(void)
{
    uint32_t total, cur = preset_pos(&total), e, k;
    int32_t row;
    if (!total) {
        char b[24];
        if (list_mode() == 1u) {
            panel_note("NO FAVORITES", "LIST ALL TO ADD SOUNDS", 0);
            return;
        }
        str_cpy(b, "NO ", sizeof b);                    /* a category with no sound (OTHER: tag your own) */
        str_cpy(b + 3, list_name(list_mode()), sizeof b - 3);
        str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
        panel_note(b, "[K4] ANOTHER LIST", "TAG YOURS WHEN SAVING");
        return;
    }
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3), x1 = 212;
        char tag[6], nm[13], pt[4], pn[13];
        uint32_t index = preset_visible(cur, total, (uint32_t)(row + 3));
        int sel = index == cur;
        int32_t hint = sel ? preset_pat_hint() : -1;    /* the suggested pattern */
        if (index >= total) continue;
        e = preset_at(index, &k);
        if (e == NENGINES) {                             /* user preset: "U07" and its name */
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        if (hint >= 0) {
            pat_label((uint32_t)hint, pt, pn);
            x1 = 206 - text_w(&AF_M, pt) - 6;
        }
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, x1);
        if (hint >= 0) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(206, y + LIST_TY, &AF_M, pt, T_INK, T_THEME);
        }
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
            cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, e->npresets ? e->presets[TSEL->preset % e->npresets].name : "", sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* user preset slots around the selected one: "U07  NAME" / EMPTY */
static void graph_user(void)
{
    int32_t row, first = clamp((int32_t)ui.uslot - 3, 0, UP_SLOTS - 7);
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        int used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "--", sizeof nm);
        list_row(LIST_Y(row), k == ui.uslot, tag, T_MID, nm, used ? T_TEXT : T_DIM, 232);
    }
}
/* SAVE > PHRASES: the pattern list around the one picked ("03  MELODY", "U07  MY BASS") */
static void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, T_TEXT, 232);
    }
}
/* the AUTOMATION rows' columns (M): the kind at 14 (LOCK AUTO STEP PLAY, to x 54), the step / PLAY's ON at EV_SX, the
 * name / QUANTIZE's ON at EV_NX (QUANTIZE ends at x 86), the value right at 226 */
#define EV_SX 60
#define EV_NX 92
/* a row's value (M) and its unit (S, on the value's baseline, as a card's) ending at x 226; the value's left x */
static int32_t ev_value(int32_t y, const char *v, const char *unit, uint16_t fg, uint16_t bg)
{
    int32_t x = 226;
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    if (unit && unit[0]) {
        GFX_HOOK_ALIGN(0, 0, 0, y + LIST_TY + AF_M.asc, AL_B, "list row unit on its value's baseline");
        x = cv_text_r(x, y + LIST_TY + AF_M.asc - AF_S.asc, &AF_S, unit, fg, bg) - 2;
    }
    return cv_text_r(x, y + LIST_TY, &AF_M, v, fg, bg);
}
/* SEQ > AUTOMATION (ui_events.c): the track's rows around the one selected: PLAY first (ON / OFF and the counts),
 * QUANTIZE (ON / OFF and the steps nudged), then a row each: the kind (LOCK in the accent, AUTO; STEP: a step's CHANCE,
 * RATCH, NUDGE or VEL, a NUDGE or VEL that does not play DIM), the step, the name, the value at the right; the last row
 * + ADD (LOCK, CHANCE, RATCH, NUDGE or VEL: what OCT+ adds) */
static void graph_events(void)
{
    const track_t *t = TSEL;
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), cur = ev_row(n), k;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        int32_t y = LIST_Y(row);
        int sel;
        uint16_t bg, c;
        k = (uint32_t)(first + row);
        c = rw[k];
        sel = k == cur;
        bg = sel ? T_THEME : T_SURF;
        if (sel)
            cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        if (c >> 8 == EVK_ADD) {
            ev_fix();
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, ui.ev_id == EV_CHANCE ? "+ ADD CHANCE" : ui.ev_id == EV_RATCH ?
                       "+ ADD RATCH" : ui.ev_id == EV_NUDGE ? "+ ADD NUDGE" : ui.ev_id == EV_VEL ?
                       (ui.ev_vel ? "+ HIDE VEL" : "+ SHOW VEL") : "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            break;
        }
        if (c >> 8 == EVK_TOP) {                     /* PLAY ON / OFF, the counts: "EVENT 3  LOCK 2" */
            char b[24];
            str_cpy(b, "EVENT ", sizeof b);
            fmt_int(b + str_len(b), (int32_t)(ev_count(t,0) - ev_count(t,1)));
            str_cpy(b + str_len(b), "  LOCK ", sizeof b - str_len(b));
            fmt_int(b + str_len(b), (int32_t)ev_count(t,1));
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "PLAY", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, motion_enabled(t) ? "ON" : "OFF",
                       sel ? T_INK : motion_enabled(t) ? T_TEXT : T_DIM, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            continue;
        }
        if (c >> 8 == EVK_QNT) {                     /* QUANTIZE ON / OFF (1.2), the steps nudged: "NUDGED 3" */
            char b[16];
            str_cpy(b, "NUDGED ", sizeof b);
            fmt_int(b + str_len(b), (int32_t)ev_nudged(t));
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "QUANTIZE", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_NX, y + LIST_TY, &AF_M, t->p[P_SQNT] ? "ON" : "OFF", sel ? T_INK : T_TEXT, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            continue;
        }
#if FELUCCA_HOME_PERF
        if(c>>8==EVK_PERF){const hp_event_t *e=&hp.event[c&255u];char b[16];const char *unit;
            const param_desc_t *d=hp_vdesc(HP_VFIRST+e->control);int32_t x;
            b[0]=(char)('0'+(e->step+1u)/10u);b[1]=(char)('0'+(e->step+1u)%10u);b[2]=0;
            cv_text_on(14,y+LIST_TY,&AF_M,e->lock?"LOCK":"AUTO",sel?T_INK:e->lock?T_ACCENT:T_MID,bg);
            cv_text_on(EV_SX,y+LIST_TY,&AF_M,b,sel?T_INK:T_MID,bg);
            param_format(d,e->value,b,&unit);x=ev_value(y,b,unit,sel?T_INK:T_TEXT,bg);
            cv_free_text(EV_NX,y+LIST_TY,&AF_M,ev_perf_name(e->control,0),sel?T_INK:T_TEXT,bg,x-8-EV_NX);continue;}
#endif
        if (c >> 8 != EVK_REC) {                     /* a step's CHANCE / RATCH / NUDGE: "STEP 05 CHANCE  50%" */
            uint32_t s = c & 0xFFu, kd = c >> 8;
            int32_t v = ev_sval(&t->step[s], kd);
            uint16_t vc = (kd == EVK_NUDGE && !ev_nudge_plays(t, &t->step[s])) ||   /* (QUANTIZE ON ..) */
                          (kd == EVK_VEL && !ev_vel_plays(&t->step[s])) ? T_DIM : T_TEXT;
            char b[8];
            b[0] = (char)('0' + (s + 1u) / 10u);
            b[1] = (char)('0' + (s + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "STEP", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_NX, y + LIST_TY, &AF_M, kd == EVK_CHANCE ? (v > 100 ? "COND" : "CHANCE") : kd == EVK_RATCH ? "RATCH" :
                       kd == EVK_VEL ? "VEL" : "NUDGE", sel ? T_INK : vc, bg);
            if (kd == EVK_CHANCE) {                  /* (1.5.5: "STEP 05 COND  1:4") */
                ev_value(y, b, ev_chance_fmt(b, v), sel ? T_INK : vc, bg);
                continue;
            } else if (kd == EVK_VEL) {
                fmt_int(b, v);
            } else if (kd == EVK_RATCH) {
                b[0] = 'x';
                b[1] = (char)('0' + v);
                b[2] = 0;
            } else {                                 /* "+3" "/16" */
                ev_nudge_fmt(b, v);
            }
            ev_value(y, b, kd == EVK_CHANCE ? "%" : kd == EVK_NUDGE ? "/16" : "", sel ? T_INK : vc, bg);
            continue;
        }
        {
            const motion_event_t *e = &motion.event[c & 0xFFu];
            uint32_t id = MOTION_ID(e), lock = (e->param & MOTION_LOCK) != 0u;
            char b[16], nm[12];
            const char *unit;
            const param_desc_t *d = MOTION_IS_FX(e) ? 0 : track_desc(t, id);
            int32_t x;
            b[0] = (char)('0' + ((e->place & 63u) + 1u) / 10u);
            b[1] = (char)('0' + ((e->place & 63u) + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, MOTION_IS_FX(e) ? "FX" : lock ? "LOCK" : "AUTO", sel ? T_INK : lock ? T_ACCENT : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            if (MOTION_IS_FX(e)) { str_cpy(b, lock ? "OFF" : "ON", sizeof b); unit = ""; }
            else param_format(d, e->value, b, &unit);
            ev_name(t, id, nm);
            x = ev_value(y, b, unit, sel ? T_INK : T_TEXT, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_free_text(EV_NX, y + LIST_TY, &AF_M, nm, sel ? T_INK : T_TEXT, bg, x - 8 - EV_NX);
        }
    }
}
/* project slots: the name (none: USED) / EMPTY, the selected one filled */
static void graph_slots(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t y = 12 + (int32_t)i * 26;
        char b[4], label[20];
        int sel = (int32_t)i + 1 == song.g[G_SLOT], used = graph_project_used(i);
        const char *n = graph_project_name(i);
        b[0] = (char)('A' + i);
        b[1] = 0;
        const char *value = !used ? "--" : n[0] ? n : "USED";
        if (used && (project_ram_mask() & (1u << i))) {
            str_cpy(label, "RAM: ", sizeof label);
            str_cpy(label + 5, value, sizeof label - 5);
            value = label;
        }
        list_row(y, sel, b, T_MID, value, used ? T_TEXT : T_DIM, 232);
    }
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b, uint32_t lit)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_icon_on(4, 4, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c), LG_Y_GRAPH);
}
static void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK;
    if (ui.force) {                                  /* the BG beside the columns only, they cover the rest (#181: */
        draw_gaps(graph_y(), graph_h());             /* the whole panel cleared first flashed on every sound load and
                                                      * track change); LINE: divided as the cards above */
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0, lit = (uint32_t)midi_lit(c);
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u +
              lit * 15485863u;                         /* (1.5.5: MIDI IN lights the cushion) */
#if FELUCCA_PAIR_MORPH
        sig ^= pair_member(t) ? 0x6A09E667u : 0u;     /* pairing changes must invalidate every member's cushion */
#endif
        if (!ui.force && sig == ts.col[c])
            continue;
        ts.col[c] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b, lit);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        cv_icon_on(4, 5, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10[lvl];
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / (pan > 0 ? 63 : 64));   /* (+100 at the top: param_format) */
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        cv_blit((uint32_t)CARD_X(c), Y_GRAPH);
    }
}
#if !FELUCCA_HOME_PERF                              /* (HOME_PERF: ui_draw_page hands HOME to hp_draw_home before draw_graph can run with ui.home set;
 * the rows below are drawn only by that path, so the shipped profile leaves them out) */
/* HOME > TRACKS (1.3, Discussions #112 / #134: MENU > DISPLAY > HOME TRACKS): the four tracks as rows in the panel, one
 * under the other (29 px, 2 apart), in place of the scope; the cards above stay HOME's four (the selected track's). A row:
 * the track's cushion at the left; on its first line the engine's icon and the sound's name (MENU > LARGE: the name in M,
 * no icon), a MUTE badge at its right when muted; under it the 16 steps of the page of the track's pattern that plays
 * (stopped: the selected track's cursor page, the others' first): a note a bar (accented: taller, the accent), a TIE after
 * a note the bar carried on from the one before, DRUM hits a pad (two hits or more: two pads), an empty step or a REST a
 * stub (a TIE with nothing to carry: brighter); the step playing on a raised cell. LEN > 16: a pip per page right of the
 * steps (the page shown THEME). At the right the track's output meter. The selected track's row is the selection (THEME,
 * its ink INK, as a list's selected row); a muted track's steps DIM. Each row is its own canvas with its own signature
 * (ts.col, the MIXER's: the two never show together); a meter that moved alone redraws its slot only. LINE: the rows on
 * BG, 1 px rules between them */
#define TR_H 29                                      /* a row; rows TR_PITCH apart from Y_GRAPH, 3 .. 237 across */
#define TR_PITCH 31
#define TR_W 234
#define TR_NY(lg) ((lg) ? 2 : 4)                     /* the name line's capitals from row 4 (LARGE, M: 2) */
#define TR_SB 26                                     /* the steps: bars standing on row 25 */
#define TR_BW 9                                      /* a step's bar, 11 px apart, 4 more between groups of 4 */
#define TR_PX 216                                    /* the page pips */
#define TR_MX 226                                    /* the meter: 4 x 22 from row 4 */
#define TR_MY 4
#define TR_MH 22
static int32_t tr_bx(uint32_t i) { return 24 + (int32_t)i * 11 + (int32_t)(i / 4u) * 4; }
static void tr_meter(int32_t x, int32_t y, uint32_t c, uint16_t slot, uint16_t fill, uint16_t under)
{
    int32_t m = ts.meter[c] * TR_MH / (TS_MH - 2);
    cv_rrect(x, y, 4, TR_MH, 2, slot, under);
    if (m)
        cv_rrect(x, y + TR_MH - m, 4, m, m >= 4 ? 2 : 0, fill, slot);
}
static void home_track_row(uint32_t c)
{
    track_t *t = &trk[c];
    const step_t *s = seq_steps(t);
    uint32_t sel = c == song.sel, mute = t->p[P_MUTE] != 0, large = (ui_prefs & PREF_LARGE) != 0u, lit = (uint32_t)midi_lit(c);
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), np = (len + 15u) / 16u, pg, p0, ph, i, sig, carry = 0;
    int32_t y = Y_GRAPH + (int32_t)c * TR_PITCH, m, x, xr = 222, ch = large ? AF_M_CAP_H : AF_S_CAP_H;
    uint16_t bg = sel ? T_THEME : T_SURF, half = ux_mix(T_THEME, T_INK, 35);
    uint16_t cn = sel ? (mute ? ux_mix(T_THEME, T_INK, 55) : T_INK) : mute ? T_DIM : T_THEME;   /* a note */
    uint16_t ca = sel || mute ? cn : T_ACCENT, cs = sel ? half : T_RAISE;   /* accented; a stub */
    uint16_t cp = sel ? ux_mix(T_THEME, T_INK, 20) : T_RAISE, cpl = sel ? T_INK : mute ? T_MID : T_TEXT;   /* playing */
#if FELUCCA_PAIR_MORPH
    char b[24];
#else
    char b[16];
#endif
    pg = song.playing ? t->seq_idx / 16u : sel ? ui.cursor / 16u : 0u;
    if (pg >= np)
        pg = np - 1u;
    p0 = pg * 16u;
    ph = song.playing && t->seq_idx / 16u == pg ? t->seq_idx % 16u : 0xFFu;
    trk_short_name(c, b);
#if FELUCCA_PAIR_MORPH
    if (pair_member(t)) perf_format(b, sizeof b, "%s =%u %u%%", ENGINES[t->engine]->name,
        (unsigned)(c == ep.a ? ep.b+1 : ep.a+1), (unsigned)(ep.weight[c == ep.b][CTL-1]*100+16383)/32767);
#endif
    m = mute ? 0 : meter_px(t->peak);
    t->peak = 0;
    if (m < ts.meter[c] - 1)
        m = ts.meter[c] - 1;                         /* (falls as the MIXER's) */
    sig = str_hash(1u + sel + mute * 2u + large * 4u + t->eng_req * 8u, b) + steps_hash(t) * 31u + pg * 7919u +
          ph * 104729u + len * 1299709u + lit * 15485863u;   /* (1.5.5: MIDI IN lights the cushion) */
#if FELUCCA_PAIR_MORPH
    sig ^= pair_member(t) ? 0x6A09E667u : 0u;
#endif
    if (!ui.force && sig == ts.col[c]) {
        if (m != ts.meter[c]) {                      /* the meter alone: its slot */
            ts.meter[c] = (uint8_t)m;
            cv_begin(4, TR_MH, bg);
            tr_meter(0, 0, c, cs, sel ? T_INK : T_MID, bg);
            cv_blit(3 + TR_MX, (uint32_t)(y + TR_MY));
        }
        return;
    }
    ts.col[c] = sig;
    ts.meter[c] = (uint8_t)m;
    cv_begin(TR_W, TR_H, T_BG);
    cv_rrect(0, 0, TR_W, TR_H, 5, bg, T_BG);
    cv_icon_mid(4, TR_H / 2, 16, trk_icon(c, 1), lit ? (sel ? half : T_ACCENT) : sel ? T_INK : mute ? T_DIM : T_MID, bg);
    if (mute) {                                      /* MUTE: its badge at the name line's end */
        xr -= kc_w(KC_MUTE);
        GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row badge on the name's line");
        cv_keycap(xr, TR_NY(large) + (ch - KC_H) / 2, KC_MUTE, sel ? T_INK : T_KEY, sel ? T_THEME : T_INK, bg);
        xr -= 6;
    }
    x = 24;
    if (FELUCCA_ICONS && !large) {                   /* the engine's icon on the name's capitals */
        uint32_t ei = engine_icon(ENGINES[t->eng_req % NENGINES]->name);
        if (ei != ICON_NONE)
            GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row icon on the name's line");
        x += cv_icon_in(x, TR_NY(large), 0, ch, 12, ei, sel ? T_INK : T_MID, bg) + 5;
    }
    cv_free_text(x, TR_NY(large) - (large ? AF_M_CAP_Y : AF_S_CAP_Y), large ? &AF_M : &AF_S, b, sel ? T_INK : mute ? T_DIM : T_TEXT,
                 bg, xr - x);
    for (i = p0; i-- > 0u;)                          /* a TIE at the page's start: carried from a note before it? */
        if (s[i].time != ST_TIE) {
            carry = s[i].time == ST_NOTE && s[i].n;
            break;
        }
    for (i = 0; i < 16u && p0 + i < len; i++) {
        const step_t *st = &s[p0 + i];
        int32_t bx = tr_bx(i);
        uint16_t under = i == ph ? cp : bg;
        if (i == ph)                                 /* the step playing: a raised cell */
            cv_rrect(bx - 1, 15, TR_BW + 2, 13, 2, cp, bg);
        if (st->time == ST_TIE && carry) {           /* carried on: joined to the bar before */
            int32_t x0 = i ? tr_bx(i - 1u) + TR_BW : bx - 2;
            cv_rect(x0, TR_SB - 7, bx + 2 - x0, 7, i == ph ? cpl : cn);
            cv_rrect(bx, TR_SB - 7, TR_BW, 7, 2, i == ph ? cpl : cn, under);
        } else if (st->time == ST_NOTE && st->n) {   /* notes: a bar, accented taller */
            int acc = (st->flags & SF_ACCENT) != 0u;
            cv_rrect(bx, TR_SB - (acc ? 10 : 7), TR_BW, acc ? 10 : 7, 2, i == ph ? cpl : acc ? ca : cn, under);
            carry = 1;
            continue;
        } else if (st->time == ST_NOTE && st->hit) { /* DRUM hits: a pad, two for two hits or more */
            uint16_t pc = i == ph ? cpl : (st->acc & st->hit) ? ca : cn;
            cv_rrect(bx, TR_SB - 4, TR_BW, 4, 1, pc, under);
            if (st->hit & (st->hit - 1u))
                cv_rrect(bx, TR_SB - 9, TR_BW, 4, 1, pc, under);
        } else {                                     /* empty, a REST, a TIE carrying nothing */
            cv_rect(bx, TR_SB - 2, TR_BW, 2, i == ph ? (sel ? T_INK : T_MID) : st->time == ST_TIE ? (sel ? T_INK : T_MID) : cs);
        }
        carry = st->time == ST_TIE && carry;
    }
    if (np > 1u)                                     /* LEN > 16: the pages, top to bottom */
        for (i = 0; i < np; i++)
            cv_rect(TR_PX, TR_SB - 3 * (int32_t)(np - i) + 1, 3, 2, i == pg ? (sel ? T_INK : T_THEME) : cs);
    tr_meter(TR_MX, TR_MY, c, cs, sel ? T_INK : T_MID, bg);
    cv_blit(3, (uint32_t)y);
}
static void draw_home_tracks(void)
{
    uint32_t c;
    if (ui.force) {                                  /* the BG round the rows only, they cover the rest (#181) */
        lcd_fill(0, Y_GRAPH, 3, H_GRAPH, T_BG);
        lcd_fill(3 + TR_W, Y_GRAPH, 240 - 3 - TR_W, H_GRAPH, T_BG);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
        for (c = 1; c < NTRK; c++) {                 /* the 2 rows between two rows; LINE: a rule on the first */
            uint32_t y = (uint32_t)(Y_GRAPH + (int32_t)c * TR_PITCH - 2);
            if (ux.style) {
                lcd_fill(3, y, TR_W, 1, T_RULE);
                GFX_HOOK_RULE(3, y, TR_W, 1);
            }
            lcd_fill(3, ux.style ? y + 1u : y, TR_W, ux.style ? 1u : 2u, T_BG);
        }
    }
    for (c = 0; c < NTRK; c++)
        home_track_row(c);
}
#endif
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)                  /* (E25: no scope: a flat line, nothing sampled) */
{
    int32_t cy = (int32_t)cv_h / 2;
    (void)c;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
}

static void draw_graph(void)
{
#if FELUCCA_HOME_PERF
    if (!ui.home && (cur_page()->graph==GR_MACRO || cur_page()->graph==GR_MACROMAP || cur_page()->graph==GR_HOMEEDIT)) { hp_page_graph(); return; }
#endif
#if FELUCCA_PAIR_MORPH
    if ((ui.home && ep.on) || (!ui.home && cur_page()->graph == GR_PAIR)) { pair_ui_graph(); return; }
#endif
#if CARAVEL_UX_EVOLVE
    if (!ui.home && cur_page()->graph == GR_EVOLVE) { uxl_ev_graph(); return; }
#endif
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
#if !FELUCCA_HOME_PERF
    if (ui.home && home_tracks()) {                  /* MENU > HOME TRACKS (1.3) */
        draw_home_tracks();
        return;
    }
#endif
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        graph_scope(c);
    } else {
        switch (pg->graph) {
        case GR_ROLL:
            cv_oy = 0;
            if (detail_on())                          /* 1.5.1 (#199) */
                graph_detail(t, c);
            else if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_BROWSE:
            cv_oy = 0;
            graph_browse();
            break;
        case GR_SLOTS:
            cv_oy = 0;
            graph_slots();
            break;
        case GR_USER:
            cv_oy = 0;
            graph_user();
            break;
        case GR_PATS:
            cv_oy = 0;
            graph_pats();
            break;
        case GR_EVENTS:
            cv_oy = 0;
            graph_events();
            break;
        case GR_TOOLS:
            cv_oy = 0;
            panel_note("TURN TO PICK", "[OCT+] CONFIRM", 0);
            break;
#if CARAVEL_REVERB_MINIMAL
        case GR_STLAYER:
            cv_oy = 0;
            panel_note(stasis_layer_status(), stasis_layer_can_cancel() ? "[OCT+] CANCEL" : "[OCT+] ADD / BLEND", 0);
            break;
#if CARAVEL_STASIS_EXPAND
        case GR_STGRAIN:
            cv_oy = 0;
            panel_note("STASIS GRAIN", "K1 SIZE / K2 SCAN / K3 WINDOW / K4 REV", 0);
            break;
#endif
#endif
        case GR_FMOP:
        case GR_FMOP2:
            break;
        case GR_FMENV:
            break;
        default:
            { cv_oy = 0; graph_scope(c); }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, graph_y());
}


