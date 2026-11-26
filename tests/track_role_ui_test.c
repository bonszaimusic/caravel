/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Exercise the production renderer, LCD framebuffer and NOR project codec. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static char captions[128][64];
static unsigned caption_count, clipped;
static void capture_text(const char *s, unsigned flags)
{
    if (caption_count < 128) snprintf(captions[caption_count++], 64, "%s", s);
    clipped += !!(flags & 3u);
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,f) capture_text(s,f)
#define GFX_HOOK_BLIT(x,y,r) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define CARAVEL_STASIS2 1
#define CARAVEL_UX_CAPTURE 1
#define CARAVEL_STASIS_SCOPE_V6 1
#define CARAVEL_UX_BUILD_FX_PLAY_STYLE 1
#define FORMAT2_PERFORMANCE_MAIN track_role_project_fixture_main
#include "format2_performance_test.c"

/* Independent acceptance masks, copied from the UI specification. */
static const uint16_t expected[5][12] = {
 {0x030,0x028,0x024,0x020,0x020,0x020,0x020,0x020,0x3E0,0x7E0,0x7E0,0x3C0},
 {0xC00,0xC00,0x400,0x400,0x410,0x408,0x7FE,0x008,0x010,0,0,0},
 {0x0F0,0x1F8,0x1F8,0x0F0,0,0,0x79E,0xFDF,0xFDF,0x79E,0,0},
 {0,0x3C0,0x7E4,0x3C4,0x01F,0x3C4,0x7E4,0x3C0,0,0,0,0},
 {0x060,0x462,0x264,0x1F8,0x0F0,0xFFF,0x0F0,0x1F8,0x264,0x462,0x060,0}
};
static const char *const names[] = {"NOTES","FOLLOW","CHORD","EXT","CUSTOM"};
static int caption(const char *s)
{
    for (unsigned i=0;i<caption_count;i++) if (!strcmp(s,captions[i])) return 1;
    return 0;
}
static void role(unsigned t, unsigned r)
{
    if (r == HROLE_CUSTOM) { /* A mixed legacy project, not a new user choice. */
        trk[t].p[P_CSRC]=0; trk[t].p[P_CHRD]=CH_DIA3; harmony_follow[t]=1;
    } else harm_choose_role(&trk[t],r);
    assert(harm_role(&trk[t])==r);
}
static void page(const char *title)
{
    for (unsigned i=0;i<NPAGES;i++) if (!strcmp(PAGES[i].title,title)) {
        ui.home=0; ui.page=i; return;
    }
    assert(!"page missing");
}
static void clean(void)
{
    ui.home=ui.layer=ui.menu=ui.confirm=ui.uboot=0;
    ui.msg_t=ui.hot_t=ui.bpm_t=0; ui.msg[0]=ui.msg2[0]=0;
    uxl.inter=uxl.lvl=uxl.hot_t=uxl.cap_view=uxl.cap_auto=uxl.pv=0;
    uxl.hint_until=0; link_setup_row=0;
    fm1_in.buttons=fm1_in.notes=0; song.playing=song.rec=0;
    caption_count=clipped=0;
}
static void box(unsigned x, unsigned y, unsigned r, uint16_t bg)
{
    for (unsigned row=0;row<12;row++) for (unsigned col=0;col<12;col++) {
        uint16_t want=expected[r][row] & (0x800u>>col) ? T_TEXT : bg;
        if (web_fb[(y+row)*240+x+col]!=swap16(want)) {
            fprintf(stderr,"role %s box %u,%u pixel %u,%u got %04x want %04x\n",
                    names[r],x,y,col,row,web_fb[(y+row)*240+x+col],swap16(want));
            assert(0);
        }
    }
}
static void play_draw(void) { ui.force=1; ui_draw(); }
static void head_draw(void) { caption_count=clipped=0; uxl_build_mark(); }
static void test_roles(void)
{
    for (unsigned palette=0;palette<4;palette++) {
        ui_style=ux.style=palette==3 ? ST_LINE : ST_FLAT;
        palette_set(palette==1 ? UI_GREY_INDEX : palette==2 ? UI_BW_INDEX : UI_DEFAULT_INDEX);
        for (unsigned r=0;r<5;r++) {
            clean(); role(song.sel,r); uxl.play=1; uxl.area=0;
            uint32_t before=web_draws; play_draw();
            assert(web_draws-before==2); /* The existing two strips, no glyph transfer. */
            box(8,30,r,T_BG);
            uxl.hot_t=30; uxl.hot_k=0; play_draw(); box(8,30,r,T_BG);
            uxl.hot_t=0; uxl.cap_view=1; play_draw(); box(8,30,r,T_BG);
            uxl.cap_view=0; uxl.pv=1; uxl.pv_idx=0; play_draw(); box(8,30,r,T_BG);
            uxl.pv=0; uxl.play=0; page("ENV"); head_draw();
            box(77,6,r,ux_mix(T_SURF,T_ACCENT,20));
            /* HARM OFF and missing EXT input do not alter the configured glyph. */
            harmony_on=0; head_draw(); box(77,6,r,ux_mix(T_SURF,T_ACCENT,20));
            page("HARM"); caption_count=0; ui.force=1; uxl_ring_build();
            box(8,89,r,T_BG); assert(caption(names[r]));
            caption_count=clipped=0; cv_begin(240,100,T_BG);
            link_card(0,0,4,0,1); assert(caption(names[r]) && !clipped);
        }
    }
    puts("all five masks: PLAY/hot/CAPTURE/PREVIEW, BUILD, HARM ring and full MODE/card names in four styles PASS");
}
static void test_quiet(void)
{
    clean(); page("ENV"); uxl.play=1; uxl.area=0; role(song.sel,HROLE_FOLLOW);
    play_draw(); ui.force=0;
    uint32_t before=web_draws; uxl_draw_play(); assert(web_draws==before);
    /* No clock tick, message, force, track, parameter under a knob or transport change. */
    role(song.sel,HROLE_CHORD); assert(!ui.force);
    uxl_draw_play(); assert(web_draws-before==2); box(8,30,HROLE_CHORD,T_BG);
    uxl.play=0; head_draw(); ui.force=0; before=web_draws;
    head_draw(); assert(web_draws==before);
    role(song.sel,HROLE_NOTES); head_draw(); assert(web_draws==before+1);
    box(77,6,HROLE_NOTES,ux_mix(T_SURF,T_ACCENT,20));
    page("HARM"); ui.force=1; uxl_ring_build(); ui.force=0; before=web_draws;
    uxl_ring_build(); assert(web_draws==before);
    role(song.sel,HROLE_FOLLOW); uxl_ring_build(); assert(web_draws==before+2);
    box(8,89,HROLE_FOLLOW,T_BG);
    ui.force=1; lfo_cards(cur_page()); ui.force=0; before=web_draws;
    lfo_cards(cur_page()); assert(web_draws==before);
    role(song.sel,HROLE_CUSTOM); caption_count=0; lfo_cards(cur_page());
    assert(web_draws>before && caption("CUSTOM"));
    puts("quiet PLAY role change, BUILD pixel hash and HARM ring/card signatures PASS");
}
static void test_headers(void)
{
    static const char *const blocks[]={"TRACKS","SHAPE","MOVE","SPACE","HARM","SOUND","MIX","SAVE","ARP","SEQ","TRK"};
    clean(); uxl.play=0; role(song.sel,HROLE_CUSTOM);
    unsigned cases=0, omitted=0;
    for (unsigned eng=0;eng<NENGINES;eng++) {
        TSEL->eng_req=eng;
        for (unsigned i=0;i<NPAGES;i++) {
            ui.page=i; const page_t *pg=&PAGES[i]; char title[28];
            strcpy(title,pg->title);
            if (pg->graph==GR_EVOLVE) {
                char pos[12]; uxl_page_pos(pos,sizeof pos);
                strcat(title," "); strcat(title,pos);
            }
            const char *blk=blocks[pg->fam];
            int old_fit=77+text_w(&AF_M,blk)+10+text_w(&AF_M,title)<=238;
            int fit=77+18+text_w(&AF_M,blk)+10+text_w(&AF_M,title)<=238;
            int eligible=(pg->scope==SC_TRACK || pg->scope==SC_ENGINE) && pg->fam!=FAM_SAVE && pg->fam!=FAM_GLO;
            head_draw(); assert(caption(blk)==old_fit);
            if (old_fit) assert(caption(title) && !clipped);
            if (eligible && fit) box(77,6,HROLE_CUSTOM,ux_mix(T_SURF,T_ACCENT,20));
            else { /* Changing role must not alter even one header pixel. */
                uint16_t saved[240*28]; memcpy(saved,web_fb,sizeof saved);
                role(song.sel,HROLE_NOTES); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
                role(song.sel,HROLE_CUSTOM); omitted++;
            }
            if (!eng) printf("HEADER\t%s\t%u\t%u\t%s\t%s\n",pg->title,pg->scope,pg->fam,eligible?(fit?"glyph":"omit-fit"):"omit-scope",title);
            cases++;
        }
    }
    page("JOURNEY"); head_draw(); assert(caption("MOVE") && caption("JOURNEY 5/8"));
    page("ENV"); ui.msg_t=20; strcpy(ui.msg,"MESSAGE"); head_draw(); assert(caption("MESSAGE") && !caption("SHAPE"));
    uint16_t saved[240*28]; memcpy(saved,web_fb,sizeof saved);
    role(song.sel,HROLE_EXT); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
    ui.msg_t=0; uxl.inter=1; head_draw(); memcpy(saved,web_fb,sizeof saved);
    role(song.sel,HROLE_CUSTOM); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
    uxl.inter=0; page("INSERT"); ui.msg_t=20; strcpy(ui.msg,"HOLD [FX] QUICK");
    head_draw(); memcpy(saved,web_fb,sizeof saved);
    role(song.sel,HROLE_EXT); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
    ui.msg_t=0;
    uxl.inter=0; page("HARM"); link_setup_row=1; head_draw(); memcpy(saved,web_fb,sizeof saved);
    role(song.sel,HROLE_EXT); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
    link_setup_row=0; ui.home=1; head_draw(); memcpy(saved,web_fb,sizeof saved);
    role(song.sel,HROLE_CUSTOM); head_draw(); assert(!memcmp(saved,web_fb,sizeof saved));
    printf("all BUILD headers: %u engine/page combinations (%u scope/fit omissions), names/messages/INTERACT/global HARM/overview PASS\n",cases,omitted);
}
static void test_overlays(void)
{
    const unsigned layers[]={LAYER_FX,LAYER_SCL,LAYER_GLO};
    for (unsigned state=0;state<6;state++) {
        clean(); uxl.play=1; page("ENV");
        if (state<3) ui.layer=layers[state];
        if (state==3) uxl.lvl=1;
        if (state==4) ui.menu=1;
        if (state==5) ui.confirm=1;
        for (unsigned r=0;r<5;r++) {
            role(song.sel,r); play_draw();
            unsigned ink=0, mask_pixels=0;
            for (unsigned y=0;y<12;y++) for (unsigned x=0;x<12;x++) {
                uint16_t px=web_fb[(30+y)*240+8+x];
                /* A layer may legitimately show role-dependent cards. It
                 * must never receive the normal PLAY mask over those cards. */
                if (expected[r][y] & (0x800u>>x)) {
                    mask_pixels++;
                    ink+=px==swap16(T_TEXT);
                }
            }
            assert(ink<mask_pixels);
        }
    }
    puts("PLAY layer cards FX/SEL/GLO, LEVELS, menu and dialog own their pixels PASS");
}
static void test_project(void)
{
    const unsigned roles[]={HROLE_NOTES,HROLE_FOLLOW,HROLE_CUSTOM,HROLE_EXT};
    clean(); uxl.play=1; uxl.area=0;
    for (unsigned t=0;t<NTRK;t++) role(t,roles[t]);
    assert(!hp_project_save(0,0));
    for (unsigned t=0;t<NTRK;t++) role(t,HROLE_CHORD);
    assert(hp_project_load(0));
    clean(); uxl.play=1;
    for (unsigned t=0;t<NTRK;t++) {
        assert(harm_role(&trk[t])==roles[t]);
        song.sel=t; ui.force=0; uxl_draw_play(); box(8,30,roles[t],T_BG);
        uxl.play=0; page("ENV"); head_draw(); box(77,6,roles[t],ux_mix(T_SURF,T_ACCENT,20)); uxl.play=1;
    }
    puts("T1-T4 distinct configured roles, selection redraw and real NOR project save/load incl legacy CUSTOM PASS");
}
int main(void)
{
    setvbuf(stdout,0,_IONBF,0);
    web_nor_erase(); web_boot();
    for (unsigned i=0;i<2800;i++) web_render(44);
    test_roles(); test_quiet(); test_headers(); test_overlays(); test_project();
    puts("track role pictograms PASS"); return 0;
}
