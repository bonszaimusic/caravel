/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __attribute__(x)
#define NENGINES 18u
#define UP_SLOTS 32u
#define ENGI_SAMPLE 4u                           /* (core.h: SAMPLE, DRUM, the retired PERC set) */
#define ENGI_DRUM 10u
#define ENGI_SEM 14u
#define SMP_SET_PERC 4u
static uint8_t fx_lowcut;
static void fm1_led_key(unsigned k, int on) { (void)k; (void)on; }
static int fm1_enc_take(unsigned k) { (void)k; return 0; }
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
static void ui_prefs_fresh(void) {}                   /* (ui.c: SELECT PAGE for never-saved settings) */
static void settings_save(void) {}
#if __has_include("../firmware/src/favorites.c")
#include "../firmware/src/favorites.c"
#endif
#include "../firmware/src/settings_persist.c"
