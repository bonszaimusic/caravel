/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 bonszaimusic */
/* The caravel: the header's watermark (a pixel of the picture in SURF, RAISE in LINE style) and the power-on splash's sailing ship. Writes
 * PPM captures of the splash frames and of a few pages for a look, and checks that the ship is where it should be. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void save_screen(const char *path)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n240 240\n255\n");
    for (uint32_t i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[] = {(c >> 11) * 255u / 31u, ((c >> 5) & 63u) * 255u / 63u, (c & 31u) * 255u / 31u};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static uint16_t px(int32_t x, int32_t y) { return (uint16_t)((host_screen[y * 240 + x] >> 8) | (host_screen[y * 240 + x] << 8)); }

int main(int argc, char **argv)
{
    char path[320];
    assert(argc == 2);
    for (uint32_t palette = 0; palette < NPALETTES; palette++) {
        ui_power_on();
        settings.palette = palette;
        palette_set(palette);
        for (uint32_t style = 0; style < 2; style++) {
            ui_style = style ? ST_LINE : ST_FLAT; style_apply();
            go_title("ENV"); ui.force = 1; frame();
            /* a lit pixel of the picture in the header (row 8 of the picture is its widest: x 6 .. 22) */
            int32_t x = HEAD_SHIP_X + 12, y = (H_HEAD - SHIP_H) / 2 + 8;
            assert(px(x, y) == (ux.surf != ux.bg ? T_SURF : T_RAISE));
            assert(px(HEAD_SHIP_X + 1, y) == T_BG);                /* beside it (row 8, left of x 6): the BG */
            snprintf(path, sizeof path, "%s/header-%s-palette-%02u.ppm", argv[1], style ? "line" : "flat", palette);
            save_screen(path);
        }
        ui_style = ST_FLAT; style_apply();
        for (uint32_t f = 0; f < 13; f += 4) {
            draw_splash();
            draw_splash_ship(f);
            snprintf(path, sizeof path, "%s/splash-%02u-palette-%02u.ppm", argv[1], f, palette);
            save_screen(path);
        }
    }
    printf("caravel logo: header watermark and splash frames captured, positions checked\n");
    return 0;
}
