/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Capture actual rendered captions while the physical SELECT path navigates
 * global setup, the four chord roots and their durations. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
static unsigned chord_labels, duration_labels, on_labels, root_labels, scale_labels;
static void text_capture(const char *s)
{
    if (!strcmp(s,"CHORD 1")) chord_labels |= 1u;
    if (!strcmp(s,"CHORD 2")) chord_labels |= 2u;
    if (!strcmp(s,"CHORD 3")) chord_labels |= 4u;
    if (!strcmp(s,"CHORD 4")) chord_labels |= 8u;
    duration_labels += !strcmp(s,"DUR");
    on_labels += !strcmp(s,"ON");
    root_labels += !strcmp(s,"ROOT");
    scale_labels += !strcmp(s,"SCALE");
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,f) text_capture(s)
#define GFX_HOOK_BLIT(x,y,r) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define FORMAT2_PERFORMANCE_MAIN harmony_display_fixture_main
#include "format2_performance_test.c"
static void render_ms(unsigned n) { while(n--) web_render(44); }
static void select_next(void) { web_enc(0,1); render_ms(250); }
static void draw_capture(void)
{
    chord_labels=duration_labels=on_labels=root_labels=scale_labels=0;
    ui.force=1; ui_draw();
}
int main(void)
{
    web_nor_erase(); web_boot(); web_test_build(); render_ms(2800);
    web_buttons(1u << B_SCL); render_ms(70); web_buttons(0); render_ms(400);
    for(unsigned i=0;i<10 && !(cur_page()->graph==GR_HARMONY && harmony_edit_dur==0u);i++) select_next();
    assert(cur_page()->graph==GR_HARMONY && harmony_edit_dur==0u);
    harmony_on=1; harmony_key_mode=1; harmony_key_root=5; harmony_key_scale=1;
    draw_capture();
    assert(on_labels && root_labels && scale_labels && !duration_labels);
    select_next(); assert(harmony_edit_dur==1u);
    draw_capture();
    printf("HARMONY CHORDS rendered mask=%u duration_labels=%u\n",chord_labels,duration_labels);
    assert(chord_labels==15u && !duration_labels);
    uint32_t old_root=hroot_get(0), old_dur=hdur_get(0);
    web_enc(3,1); render_ms(250);
    assert(hroot_get(0)!=old_root && hdur_get(0)==old_dur);
    uint32_t changed_root=hroot_get(0);
    select_next(); assert(harmony_edit_dur==2u);
    draw_capture(); assert(duration_labels && !chord_labels);
    web_enc(3,1); render_ms(250);
    assert(hdur_get(0)!=old_dur && hroot_get(0)==changed_root);
    web_enc(0,-1); render_ms(250); assert(harmony_edit_dur==1u);
    draw_capture(); assert(chord_labels==15u && !duration_labels);
    puts("HARMONY BUILD display: actual SELECT/knobs, ON/ROOT/SCALE, four CHORD captions, duration and back navigation PASS");
    return 0;
}
