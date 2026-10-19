/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-X pages use fixed lane/side target IDs. Navigation never changes a learned assignment. */
static uint8_t dx_ui_lane, dx_ui_side;
static int dx_page(uint32_t g) { return g >= GR_DXKIT && g <= GR_DXLAYER; }
static uint32_t dx_page_target(uint32_t g, uint32_t k)
{
    uint32_t first = DX_SOUND_FIRST + 16u * (dx_ui_lane & 7u);
    if (g == GR_DXKIT) return k < 3u ? DX_FIRST + k : 0;
    if (g == GR_DXMOT) return DX_FIRST + DX_RATE + k;
    if (g == GR_DXSOUND) return k >= 2u ? first + dx_ui_side * 5u + k - 2u : 0;
    if (g == GR_DXSOUND2) return k ? first + dx_ui_side * 5u + k + 1u : 0;
    if (g == GR_DXMODE) return k ? first + k + 9u : 0;
    if (g == GR_DXLAYER) return k ? first + k + 12u : 0;
    return 0;
}
static void dx_cards(const page_t *pg)
{
    static const char *const lanes[] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        uint32_t id = dx_page_target(pg->graph, k);
        char val[12], label[8];
        const char *unit = "", *title;
        const param_desc_t *d = dx_desc(id);
        if (d) {
            title = d->label;
            int32_t v = dx_get(song.sel, id);
            if (id >= DX_SOUND_FIRST && (id - DX_SOUND_FIRST) % 16u == 15u && v) {
                static const char *const bus[] = {"CHO", "DLY", "REV"};
                title = bus[(v - 1) / 16];
                fmt_int(val, ((v - 1) % 16 + 1) * 100 / 16); unit = "%";
            } else param_format(d, v, val, &unit);
            draw_column(k, title, val, unit, VAL(k), (v - d->min) * 1000 / (d->max - d->min), ICON_NONE);
        } else if (pg->graph >= GR_DXSOUND && k == 0u) {
            str_cpy(label, lanes[dx_ui_lane & 7u], sizeof label);
            label[2] = ' '; label[3] = dx_ui_side ? 'B' : 'A'; label[4] = 0;
            draw_column(k, "SOUND", label, "", VAL(k), dx_ui_lane * 1000 / 7, ICON_NONE);
        } else if (pg->graph == GR_DXSOUND && k == 1u) {
            draw_column(k, "SIDE", dx_ui_side ? "B" : "A", "", VAL(k), dx_ui_side * 1000, ICON_NONE);
        } else draw_column(k, "", "", "", T_THEME, -1, ICON_NONE);
    }
}
static void dx_knob(const page_t *pg, uint32_t k, int32_t steps)
{
    uint32_t id = dx_page_target(pg->graph, k);
    const param_desc_t *d = dx_desc(id);
    if (!dx_track(TSEL)) return;
    if (d) {
        int32_t v = dx_get(song.sel, id);
        dx_set(song.sel, id, param_turn(d, v, accel(EN_K1 + k, steps, d->fmt == F_ENUM ? 0 : d->max - d->min)));
        ml_knob(TSEL, id);
    } else if (pg->graph >= GR_DXSOUND && !k) {
        dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (steps > 0 ? 1 : -1), 0, 7);
    } else if (pg->graph == GR_DXSOUND && k == 1u) dx_ui_side = steps > 0;
    ui.force = 1;
}
