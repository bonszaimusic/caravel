/* SPDX-License-Identifier: GPL-3.0-only
 * Common host UI fixture, separated from legacy feature tests. */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (as src/felucca.c: the console built in, MENU > USB SERIAL) */
#endif
#include <stddef.h>
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3 (zero: empty), as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static uint32_t host_dim_lo;                      /* hal/fm1_input.h: the glow MENU > LEDS asked for (1: DIM LO) */
static void fm1_led_dim_level(uint32_t lo) { host_dim_lo = lo; }
static int host_anim;                             /* hal/fm1_input.h: the power-on sweep running (it has the LEDs) */
static int host_idle, host_idle_glow;             /* .. the idle animation (1.2: MENU > ANIM IDLE) running, its glow */
static int fm1_led_anim_on(void) { return host_anim || host_idle; }
static void fm1_led_idle_start(uint32_t glow) { host_idle = 1; host_idle_glow = (int)glow; }
static int fm1_led_idle_on(void) { return host_idle; }
static void fm1_led_anim_stop(void) { host_anim = host_idle = 0; }
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_notes;
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(int x) { uint32_t p = host_pressed; (void)x; host_pressed = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
/* host_enc_late: detents the TIMER5 scan counts just after a read (#102: while ui_input runs); the next read has them */
static int32_t host_enc_late[7];
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e % 7u];
    host_enc[e % 7u] = host_enc_late[e % 7u];
    host_enc_late[e % 7u] = 0;
    return s;
}
static void (*host_wdt_hook)(void);                /* (a test's: time passing in a blocking loop, as panel_setup's) */
static void fm1_wdt_feed(void) { if (host_wdt_hook) host_wdt_hook(); }
#ifndef HOST_IRQ_STUBS                                   /* (tests/drumfuzz_test.c: its own, an emulated ISR) */
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#endif
static uint16_t host_screen[240 * 240];
/* #181 a blank frame: while host_fl_on, every write of a frame is watched (host_fl_begin .. host_fl_end); a pixel that
 * shows, between two writes, a value that is neither the one before the frame nor the one after (a fill of the
 * background before a canvas lands over it) flashed */
static uint8_t host_fl_on, host_fl_n[240 * 240];
static uint16_t host_fl_was[240 * 240], host_fl_mid[240 * 240];
static void host_put(uint32_t k, uint16_t v)
{
    if (host_fl_on) {
        if (!host_fl_n[k])
            host_fl_was[k] = host_screen[k];
        else if (host_screen[k] != host_fl_was[k] && host_fl_n[k] < 2u)
            host_fl_mid[k] = host_screen[k], host_fl_n[k] = 2;   /* (2: an in-between value kept) */
        if (!host_fl_n[k])
            host_fl_n[k] = 1;
    }
    host_screen[k] = v;
}
static void host_fl_begin(void) { memset(host_fl_n, 0, sizeof host_fl_n); host_fl_on = 1; }
static uint32_t host_fl_end(uint32_t y0, uint32_t y1)   /* the rows y0..y1-1: pixels that flashed */
{
    uint32_t k, n = 0;
    host_fl_on = 0;
    for (k = y0 * 240u; k < y1 * 240u; k++)
        n += host_fl_n[k] == 2u && host_fl_mid[k] != host_screen[k];
    return n;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
#ifdef UI_FILL_HOOK
    UI_FILL_HOOK(x, y, w, h);                     /* (tests/ui_render.c: what a fill covers is gone) */
#endif
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_put((y + j) * 240u + x + i, (uint16_t)((c >> 8) | (c << 8)));
}
static void lcd_sync(void) {}
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
static uint8_t host_scope_ready=1u;
static uint8_t lcd_scope_ready(void) { return host_scope_ready; }
#endif
/* MENU > SCREEN OFF (lcd.c lcd_power / lcd_wake_now): what the panel was told, in order (0 off: DISPOFF, SLPIN;
 * 1 SLPOUT; 2 DISPON; 3 lcd_wake_now), with the time; host_bl the panel showing (the backlight itself stays on: PA2 is
 * the key matrix's /OE too, lcd.c lcd_power) */
static uint32_t host_pw_n, host_pw[256], host_pw_ms[256];
static int host_bl = 1;
static void host_pw_log(uint32_t s)
{
    if (host_pw_n < 256u) { host_pw[host_pw_n] = s; host_pw_ms[host_pw_n] = fm1_ms; }
    host_pw_n++;
    host_bl = s != 0u;
}
static void lcd_power(uint32_t s) { host_pw_log(s); }
static void lcd_wake_now(void) { host_pw_log(3u); }
/* (the HOME oscilloscope's buffer, scope_buf: fx.c, with hostsim.c) */
static uint32_t host_blit_rows;                    /* rows blitted (what a frame draws) */
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    host_blit_rows += h;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) host_put((y + j) * 240u + x + i, p[j * w + i]);
}
static struct { uint32_t stage; } felucca_dbg;
#define FELUCCA_FLASH 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "TEST"
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_graph.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/ui_layer.c"
static int st_read(uint32_t off,void*p,uint32_t n){(void)off;(void)p;(void)n;return -1;}
static int st_prog(uint32_t off,const void*p,uint32_t n){(void)off;(void)p;(void)n;return -1;}
static int st_erase(uint32_t off){(void)off;return -1;}
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
static uint32_t flash_ok; /* this fixture intentionally has no flash device */
#include "../firmware/src/ui_f2.c"

static int check(const char *what, int ok)
{
    printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* main.c felucca_init */
static void ui_power_on(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(kb_chord, 0, sizeof kb_chord);
    memset(kb_chn, 0, sizeof kb_chn);
    memset(kb_midi, 0, sizeof kb_midi);
    memset(kb_midin, 0, sizeof kb_midin);
    memset(kb_live, 0, sizeof kb_live);
    memset(chord_live, 0, sizeof chord_live);
    memset(&song, 0, sizeof song);
    memset(&motion, 0, sizeof motion);
    memset(midi_lit_ms, 0, sizeof midi_lit_ms); midi_seen = 0;   /* (1.5.5: no MIDI IN light) */
    memset(&project_launch,0,sizeof project_launch);
    step_record_mask = 0;

    memset(link_src, 0, sizeof link_src);
    memset(link_retrig, 0, sizeof link_retrig);
    memset(harmony_root, 0, sizeof harmony_root);
    memset(harmony_dur, 0, sizeof harmony_dur);
    memset(harmony_follow, 1, sizeof harmony_follow);
    harmony_on = 0;
    harmony_edit_dur = 0;
    harmony_bar = 0;
    memset(&ui, 0, sizeof ui);
    page_over = 0;                                /* (no layer's knobs; roll_settle's test page gone) */
    cin_left = cin_total = 0;                     /* (no count-in) */
    memset(&nm, 0, sizeof nm);                    /* NAME closed, no project name */
    memset(&mom, 0, sizeof mom);                  /* MOMENTARY: no value held (the pool at power-on) */
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    memset(&favorites, 0, sizeof favorites);
    memset(engine_favorites, 0, sizeof engine_favorites);
    voice_policy = voice_policy_active = 0;
    memset(ml_extra, 0, sizeof ml_extra);
    memset(ml_virtual, 0, sizeof ml_virtual);
    memset(ml_base,0,sizeof ml_base);

    ml_off();                                     /* (MIDI LEARN off, nothing learned) */
    memset(&undo, 0, sizeof undo);
    /* Removed operator UI. */ //                         /* (FM6's operator pages: OP 1, stage 1, as at power-on) */
    memset(pat_last, 0, sizeof pat_last);
    memset(pat_from, 0, sizeof pat_from);                /* (the header's slot letter: none) */
    memset(proj_slot, 0, sizeof proj_slot);
    project_ram_set(0);
    memset(up_bank, 0, sizeof up_bank);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    undo_depth--;
    song.sel = 0;
    ui.home = 1;
    ui.force = 1;
    panel = PANEL_DEFAULT;
    settings.palette = UI_GREY_INDEX;
    palette_set(UI_GREY_INDEX);
    transport_req = 0;
    panic_req = 0;
    midi_clock.resync = midi_clock.bar_valid = midi_resync_notice = 0;
    midi_clock.resync_start = 0;
    midi_grid_reset();
    mi_r = mi_w = midi_in_overflow = 0;
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);                   /* chord.c: no key, MIDI chord or last chord */
    memset(mchord, 0, sizeof mchord);
    memset(chord_last, 0, sizeof chord_last);
    perf_held = perf_act = kb_layer = perf_mask = 0;      /* the FX layer: nothing held */
    perf_latched = 0;
    memset(perf_motion, 0, sizeof perf_motion);
    memset(motion_fx_owner, 0, sizeof motion_fx_owner);
    memset(motion_fx_next, 0, sizeof motion_fx_next);
    motion_fx_recording = 0;
    memcpy((void *)perf_map, PF_DEF, sizeof PF_DEF);    /* .. its default key map (the settings' 0), no key's effect */
    perf_remap = 0;
    memset(kb_fx, 0, sizeof kb_fx);
    perf_kill = 0;
    perf_mask = 1u << panel.btn[B_FX];                  /* (ui_input sets them every frame) */
    kb_mask = layer_bits();
    perf_solo = 0;
    memset(&lys, 0, sizeof lys);
    perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    settings_hold = HOLD_DEF;
    fm1_in.buttons = fm1_in.notes = 0;
    memset(&motion, 0, sizeof motion);                  /* (main.c: no motion at power-on) */
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_locked, 0, sizeof motion_locked);
    motion_base_valid = motion_full = 0;
    memset(snd_said, 0, sizeof snd_said);               /* (.bss at power-on: nothing said yet) */
    memset(&scrn, 0, sizeof scrn);                        /* MENU > SCREEN OFF: on, idle from now (main.c) */
    scrn.idle = fm1_ms;
    kb_asleep = 0;
    panel_moved = 0;
}
/* card slot drawn with MOTION's mark (ui_draw.c card_mot: its icon, or a long label in the theme colour) */
static int card_motion(uint32_t slot) { return (card_mot >> slot) & 1u; }

/* one UI frame (input, then the drawing on the stub display) */
static void frame(void)
{
    host_ticks += 16000u;
    fm1_ms += 16u;
    ui_input();
    ui_draw();
}
static void press(uint32_t label)                 /* a tap: down, a frame, up, a frame */
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static void tool_clear_step(void);                 /* SEQ held + F5 (below): the cursor's step cleared */
static void btn_down(uint32_t label) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
static void btn_up(uint32_t label) { fm1_in.buttons &= ~(1u << panel.btn[label]); }
static void hold(uint32_t label)                  /* held 0.8 s, then let go */
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < 52u; k++)
        frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    frame();
    host_ticks += 200000u;                        /* (no knob acceleration between the turns) */
}
static void stop_transport(void) { transport_req = 0; song.playing = 0; }
static int16_t stored_param(uint32_t slot, uint32_t track, uint32_t id)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].p[id] : -32768;
}
static uint8_t stored_note(uint32_t slot, uint32_t track, uint32_t step)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].step[step].note[0] : 0;
}
static int msg_is(const char *s) { return ui.msg_t && str_eq(ui.msg, s); }

static void go_title(const char*title){uint32_t i;for(i=0;i<NPAGES&&!str_eq(PAGES[i].title,title);i++){}ui.home=0;ui.page=(uint8_t)i;page_entered();}
static void go_page(uint32_t graph){uint32_t i;for(i=0;i<NPAGES&&PAGES[i].graph!=graph;i++){}ui.home=0;ui.page=(uint8_t)i;page_entered();}

static uint32_t key_at(int black,uint32_t p){uint32_t k;for(k=0;k<27;k++)if(key_black(k)==black&&key_place(k)==p)return k;return 0;}
static void key_down(uint32_t k){fm1_in.notes|=1u<<k;host_notes|=1u<<k;keyboard_block();}
static void key_up(uint32_t k){fm1_in.notes&=~(1u<<k);keyboard_block();}
static uint32_t white(uint32_t w){return key_at(0,w);}
static void lay_combo(uint32_t btn,uint32_t k){btn_down(btn);key_down(k);frame();}

static uint32_t gates(void){uint32_t n=0;for(uint32_t t=0;t<NTRK;t++)for(uint32_t v=0;v<NVOICE;v++)n+=trk[t].v[v].gate!=0;return n;}
static uint32_t leds_at(uint32_t ms){uint32_t br;fm1_ms=ms;uint32_t m=layer_leds(&br);return m|(((ms/250)&1)?0:br);}
static int same_sound(const track_t*a,const track_t*b){for(uint32_t i=0;i<P_COUNT;i++)if(!param_kept(i)&&a->p[i]!=b->p[i])return 0;return a->eng_req==b->eng_req&&a->preset==b->preset;}
static void my_steps(track_t*t){track_defaults_steps(t);for(uint32_t i=0;i<32;i+=3){t->step[i].note[0]=40+i;t->step[i].n=1;t->step[i].time=ST_NOTE;t->step[i].vel=99;}t->p[P_SLEN]=32;}

static void screen_save(const char *dir, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    ui.msg_t = 0; ui.hot_t = 0; ui.force = 1;
    ui_draw();
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);           /* canvas stores panel-order RGB565 */
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}

static void go_auto_top(void)
{
    go_page(GR_EVENTS);
    ui.ev_row = 0;
    ui.ev_keep = 0;
}
static void go_auto_add(uint32_t id, uint32_t step)
{
    go_page(GR_EVENTS);
    ui.ev_row = 0xFFFFu;                          /* (ev_row: the last, + ADD) */
    ui.ev_keep = 0;
    ui.ev_id = (uint8_t)id;
    ui.ev_step = (uint8_t)step;
}
/* the row showing c (EVC), selected; 0: not listed */
static int auto_pick(uint32_t c)
{
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), r;
    for (r = 0; r < n && rw[r] != c; r++)
        ;
    if (r == n)
        return 0;
    ui.ev_row = (uint16_t)r;
    return 1;
}

static uint32_t auto_cur(void){uint16_t rw[EV_ROWS];return ev_cur(rw,0);}

static int pr_is_bar(uint32_t x,uint32_t y) { uint16_t c=swap16(host_screen[y*240u+x]);return c==T_THEME||c==T_ACCENT||c==T_TEXT; }

static void tap_key(uint32_t k) { key_down(k);frame();key_up(k);frame(); }
static void oct_back(void) { btn_down(B_OCTDN);frame();btn_up(B_OCTDN);frame(); }

static uint32_t black(uint32_t w){return key_at(1,w);}

static void tool_clear_step(void) { btn_down(B_SEQ);frames(4);key_down(white(TL_STEPCLR));frame();key_up(white(TL_STEPCLR));frame();btn_up(B_SEQ);frame(); }
