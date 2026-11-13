/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1.1 developer experiment: one continuous per-track parameter.
 * Opt-in CARAVEL_ELASTIC_P1=1 only. No project format/ID/flash writes.
 * All gesture/mode state belongs to the UI thread. Audio cores consume only
 * an aligned 32-bit publication (engine, parameter id, signed delta) and own
 * their render-frame saves.
 */
#if CARAVEL_ELASTIC_P1 && CARAVEL_ACTION_BANKS
#include "home_elastic_core.h"
#include "home_elastic_overlay.h"
#if CARAVEL_ELASTIC_PLAYABLE
#include "home_elastic_row.h"
#include "home_elastic_trajectory.h"
#include "home_elastic_visual.h"
#endif

typedef struct {
    hp_elastic_t spring;
    hp_elastic_cfg_t cfg;
    uint8_t on, track, row, slot, engine;
    uint16_t id;
} hp_elastic_p1_t;
#if CARAVEL_ELASTIC_PLAYABLE
/* One independent arm per track: all four can run at the same time. */
static hp_elastic_p1_t hp_el1_track[NTRK];
static hp_er_t hp_er_track[NTRK];
static hp_et_t hp_el_cell_time_track[NTRK];
static hp_et_t hp_el_row_time_track[NTRK][HP_ER_MAX];
/* Track-local volatile SETUP, one page, no project format or new UI pages.
 * Three small arrays use only ~21 B more than the prior global settings. */
static hp_elastic_cfg_t hp_el_cfg_track[NTRK];
static hp_et_cfg_t hp_el_times_track[NTRK];
static uint8_t hp_el_scope_track[NTRK]; /* 0=CELL, 1=ROW */
/* Page is UI-only; live COUPLE strength ramps on the control thread.
 * Never add per-spring linkage state or DSP-side coupling matrices. */
static uint8_t hp_el_setup_page;
static uint8_t hp_el_couple_live[NTRK];
static uint32_t hp_el_couple_clock[NTRK];
static uint8_t hp_el_pick_track[NTRK];
static uint8_t hp_el_row_held_track[NTRK];
static uint32_t hp_el_ui_next;           /* throttle redraw, not control-rate motion */
#define hp_el1 (hp_el1_track[song.sel])
#define hp_er (hp_er_track[song.sel])
#define hp_el_cell_time (hp_el_cell_time_track[song.sel])
#define hp_el_row_time (hp_el_row_time_track[song.sel])
#define hp_el1_cfg (hp_el_cfg_track[song.sel])
#define hp_el_times (hp_el_times_track[song.sel])
#define hp_el_scope (hp_el_scope_track[song.sel])
#define hp_el_pick (hp_el_pick_track[song.sel])
#define hp_el_row_held (hp_el_row_held_track[song.sel])
#else
static hp_elastic_p1_t hp_el1;
#define hp_el1_cfg hp_el1.cfg
#endif
static volatile uint32_t hp_el1_audio[NTRK]; /* bit31 valid | engine 24..30 | id 16..23 | signed delta 0..15 */
static hp_elo_frame_t hp_el1_frame[NTRK];     /* audio-render-owned, never written by UI */
#define HP_EL1_VALID 0x80000000u
#define HP_EL1_ID(w) ((uint32_t)(w) >> 16 & 0xFFu)
#define HP_EL1_ENG(w) ((uint32_t)(w) >> 24 & 0x7Fu)
#if CARAVEL_ELASTIC_PLAYABLE
static volatile uint32_t hp_er_audio[NTRK][HP_ER_MAX];
static hp_elo_frame_t hp_er_frame[NTRK][HP_ER_MAX];
/* Resume fade after MOTION released a target: 64 = full overlay immediately,
 * 0 = suppressed this block; +16 per block over ~15 ms. Normal arming starts
 * at 64, so only the MOTION->ELASTIC handover is ramped. */
static uint8_t hp_el_fade[NTRK][HP_ER_MAX];
static uint16_t hp_el_fade_id[NTRK][HP_ER_MAX];
static uint8_t hp_el_fade_eng[NTRK][HP_ER_MAX];
static uint8_t hp_el_cell_fade[NTRK];
static uint16_t hp_el_cell_fade_id[NTRK];
static uint8_t hp_el_cell_fade_eng[NTRK];
#endif

/* Only continuous per-track parameters without a selector/slot side effect.
 * Enums, on/off, notes, steps and F_INT selectors stay out of P1. */
static int hp_elastic_p1_safe_fmt(uint32_t fmt)
{
    switch(fmt){
    case F_PCT: case F_BIPCT: case F_TIME: case F_LFOHZ:
    case F_CUTOFF: case F_DB: case F_SEMI: case F_BPM: case F_OCT:
        return 1;
    default:
        return 0;
    }
}
static int hp_elastic_p1_safe_desc(const param_desc_t *d)
{
    return d && d->max > d->min && hp_elastic_p1_safe_fmt(d->fmt);
}
static int hp_elastic_p1_safe_id(const track_t *t,uint32_t id)
{
    if(id>=P_COUNT)return 0;
    return hp_elastic_p1_safe_desc(track_desc(t,id));
}
#if CARAVEL_ELASTIC_PLAYABLE
/* P1.2 tier-1 performance destinations, distinct from permissive legacy P1.1.
 * Format alone is not sufficient: F_BPM and F_SEMI can refer to timing/pitch,
 * F_DB to gain; other common parameters may change sequencer/voice topology.
 * Engine EDIT controls remain conservative continuous *timbre* candidates.
 * Every engine still requires a device listening audit before release. */
static int hp_elastic_p12_safe_id(const track_t *t,uint32_t id)
{
    if(!t || id>=P_COUNT)return 0;
    const param_desc_t *d=track_desc(t,id);
    if(!hp_elastic_p1_safe_desc(d))return 0;
    if(id>=P_E0){
        /* E0..E7 mean different things on each engine. Curated against the
         * engine edit descriptors, NOT a generic F_PCT test. Keep this stable
         * with ENGINES[] order (engines.c) and check it in the host suite.
         * "Allowed" means control-safe pilot, not hardware-auditioned. */
        static const uint8_t timbre_bits[NENGINES]={
            0xFCu, /*  0 ANALOG: MIX NOIS CUT RES DRV KTR */
            0x00u, /*  1 DIGITAL: retired/hidden */
            0xCCu, /*  2 PHASE: DCW ENV SUB FOLD */
            0x38u, /*  3 LOFI: CRSH SWP VIB, not chip/loop */
            0x40u, /*  4 SAMPLE: DRV only, no slot/tune/playback edits */
            0xF0u, /*  5 VOICE: BUZZ BRTH Q RAND */
            0xE0u, /*  6 TRIO: CUT RES PW */
            0x60u, /*  7 WHEEL: CLICK DRV */
            0xE8u, /*  8 GRAIN: DENS SPRD RAND TONE; no SRC/POS/PTCH */
            0xDEu, /*  9 PHYS: STRC BRIT DAMP POS BOW EXC */
            0xDCu, /* 10 DRUM: TONE DECY SNAP FOLD DRV; no TUNE/ACC */
            0xCEu, /* 11 NOISE: COLR FREQ RES DRFT CRSH */
            0x14u, /* 12 FM6: MLVL MEG; no ALGO/DTUN/SLOT */
            0x00u, /* 13 SLICE: common filter/FX only for now */
            0xF8u, /* 14 SEM: MIX CUT RES STATE VINT */
            0xFEu, /* 15 HADES: PW SUB1 SUB2 CUT RES DRV BODY */
            0xB4u, /* 16 DRONE: COUPLE CHAR DRIFT TONE */
            0xDEu  /* 17 ENSEMBLE: MORPH TONE DRIFT ENS TEXT MIX */
        };
        uint32_t engine=t->engine,slot=id-P_E0;
        if(engine>=NENGINES || slot>=8u ||
           !(timbre_bits[engine]&(1u<<slot)))return 0;
        return d->fmt==F_PCT || d->fmt==F_BIPCT ||
               d->fmt==F_CUTOFF; /* no pitch/gain/time/rate types */
    }
    switch(id){
    case P_FILT:  /* per-track bipolar high/low-pass sweep */
    case P_PAN:   /* stereo placement */
    case P_CHOR: case P_DLY: case P_REV: /* FX sends, not delay timing */
    case P_DIST: /* distortion amount */
    case P_IMIX: /* insert wet mix, not effect algorithm */
    case P_LD_FLT: case P_LD_SHP: /* filter/shape modulation amount */
    case P_SLDEPTH: /* slicer depth, never its rate/pattern */
        return d->fmt==F_PCT || d->fmt==F_BIPCT ||
               d->fmt==F_CUTOFF;
    default:
        return 0; /* LEVEL, pitch/tempo, ENV times, mode/voice/ARP/SEQ */
    }
}
/* Active step locks and recorded motion must be authoritative in PLAY.
 * The audio consumer consults only these existing ISR-owned bitsets, not the
 * mutable event list, and never writes the motion state. */
static int hp_elastic_p12_motion_owns(uint32_t track,uint32_t id)
{
    if(!song.playing || track>=NTRK || id>=P_COUNT)return 0;
    uint32_t bit=1u<<(id&31u),word=id>>5;
    return ((motion_active[track][word]|motion_locked[track][word])&bit)!=0u;
}
#endif
static int hp_elastic_p1_targets_track(uint32_t track)
{
#if CARAVEL_ELASTIC_PLAYABLE
    return track<NTRK && hp_el1_track[track].on;
#else
    return hp_el1.on && hp_el1.track==track;
#endif
}
#if CARAVEL_ELASTIC_PLAYABLE
static void hp_elastic_p1_clear_track(uint32_t t)
{
    if(t>=NTRK)return;
    for(uint32_t k=0;k<HP_ER_MAX;k++)hp_er_audio[t][k]=0u;
    hp_er_reset(&hp_er_track[t],255u,255u,255u);
    hp_et_reset(&hp_el_cell_time_track[t]);
    for(uint32_t k=0;k<HP_ER_MAX;k++)hp_et_reset(&hp_el_row_time_track[t][k]);
    hp_el_pick_track[t]=0u;hp_el_row_held_track[t]=0u;
    hp_el_couple_live[t]=0u;hp_el_couple_clock[t]=fm1_ms;
    for(uint32_t k=0;k<HP_ER_MAX;k++){
        hp_el_fade[t][k]=64u;hp_el_fade_id[t][k]=0u;hp_el_fade_eng[t][k]=0u;
    }
    hp_el_cell_fade[t]=64u;hp_el_cell_fade_id[t]=0u;hp_el_cell_fade_eng[t]=0u;
    hp_el1_audio[t]=0;
    hp_elastic_cancel(&hp_el1_track[t].spring);
    hp_el1_track[t].on=0;hp_el1_track[t].track=255u;
    hp_el1_track[t].row=255u;hp_el1_track[t].slot=255u;
    hp_el1_track[t].engine=255u;hp_el1_track[t].id=0u;
}
static void hp_elastic_p1_clear(void)
{
    for(uint32_t t=0;t<NTRK;t++)hp_elastic_p1_clear_track(t);
}
static void hp_elastic_p1_clear_current(void)
{
    hp_elastic_p1_clear_track(song.sel);
}
static uint32_t hp_elastic_p1_armed_count(uint32_t *first)
{
    uint32_t n=0u;
    for(uint32_t t=0;t<NTRK;t++)
        if(hp_el1_track[t].on){if(!n&&first)*first=t;n++;}
    return n;
}
/* Four-track HUD and WebAssembly smoke use the same source of truth.
 * No persistent/UI-only shadow copy that could disagree after A1 OFF. */
static uint32_t hp_elastic_p1_armed_mask(void)
{
    uint32_t mask=0u;
    for(uint32_t t=0;t<NTRK;t++)
        if(hp_el1_track[t].on)mask|=1u<<t;
    return mask;
}
#else
static void hp_elastic_p1_clear(void)
{
    if(hp_el1.track<NTRK)hp_el1_audio[hp_el1.track]=0;
    hp_elastic_cancel(&hp_el1.spring);
    hp_el1.on=0;hp_el1.track=255u;hp_el1.row=255u;hp_el1.slot=255u;
    hp_el1.engine=255u;hp_el1.id=0u;
}
static void hp_elastic_p1_clear_current(void){hp_elastic_p1_clear();}
#endif
static void hp_elastic_p1_init(void)
{
#if CARAVEL_ELASTIC_PLAYABLE
    hp_el_ui_next=0u;hp_el_setup_page=0u;
    for(uint32_t t=0;t<NTRK;t++){
        hp_el_cfg_track[t]=HP_ELASTIC_DEFAULT;
        hp_el_times_track[t]=HP_ET_DEFAULT;
        hp_el_scope_track[t]=0u;
        hp_el_couple_live[t]=0u;hp_el_couple_clock[t]=fm1_ms;
    }
#endif
    hp_elastic_p1_clear();
#if !CARAVEL_ELASTIC_PLAYABLE
    hp_el1_cfg=HP_ELASTIC_DEFAULT; /* P1.1 legacy unchanged */
#endif
}
#if CARAVEL_ELASTIC_PLAYABLE
/* For ROW mode use other visible, armed springs as bounded signed influences.
 * Unlike a MOD matrix this is internal to the four eligible ELASTIC cells.
 * The output and HOME readback share the exact same coupled value. */
static int32_t hp_elastic_p13_row_x(uint32_t t,uint32_t k)
{
    if(t>=NTRK||k>=HP_ER_MAX)return 0;
    const hp_er_t *row=&hp_er_track[t];
    if(!(row->valid&(1u<<k)))return 0;
    const hp_elastic_t *target=&row->spring[k];
    /* COUPLE=0 is the default. Return the exact P1.2 Q8 trajectory,
     * avoiding four cell scans and link-weight math on every UI tick. */
    const uint8_t strength=hp_el_couple_live[t];
    if(!strength)return target->x;
    const hp_elastic_cfg_t cfg=hp_el_cfg_track[t];
    int32_t travel[HP_ER_MAX],pos[HP_ER_MAX];
    for(uint32_t j=0;j<HP_ER_MAX;j++){
        const hp_elastic_t *e=&row->spring[j];
        travel[j]=(row->valid&(1u<<j))?
            hp_elastic_limit(e,cfg)*256:0;
        pos[j]=(row->valid&(1u<<j))?e->x:0;
    }
    int32_t x=pos[k];
    {
        x+=hp_ec_coupling_delta(pos,travel,k,hp_el_times_track[t].seed,
                               strength);
        int32_t span=travel[k];
        int32_t lo=hp_elastic_clamp(((int32_t)target->low-target->anchor)*256,
                                   -span,span);
        int32_t hi=hp_elastic_clamp(((int32_t)target->high-target->anchor)*256,
                                   -span,span);
        x=hp_ec_reflect_x(x,lo,hi,hp_el_times_track[t].reflect);
    }
    return x;
}
static int16_t hp_elastic_p13_row_value(uint32_t t,uint32_t k)
{
    if(t>=NTRK||k>=HP_ER_MAX)return 0;
    const hp_er_t *row=&hp_er_track[t];
    if(!(row->valid&(1u<<k)))return 0;
    const hp_elastic_t *e=&row->spring[k];
    int32_t v=e->anchor+hp_elastic_div_round(hp_elastic_p13_row_x(t,k));
    return (int16_t)hp_elastic_clamp(v,e->low,e->high);
}
/* 16ms tick and <=6 coupling units per tick: predictable fade on/off
 * regardless of UI framerate, bounded catch-up after stalls. */
static int hp_elastic_p13_coupling_poll(uint32_t t,uint32_t now)
{
    uint32_t elapsed=now-hp_el_couple_clock[t],ticks=elapsed/16u;
    if(!ticks)return 0;
    if(ticks>8u)ticks=8u;
    hp_el_couple_clock[t]+=ticks*16u;
    int32_t curr=hp_el_couple_live[t],want=hp_el_times_track[t].couple;
    int32_t step=(int32_t)ticks*6;
    if(curr<want)curr=curr+step>want?want:curr+step;
    else if(curr>want)curr=curr-step<want?want:curr-step;
    int changed=curr!=hp_el_couple_live[t];
    hp_el_couple_live[t]=(uint8_t)curr;
    return changed;
}
static void hp_elastic_p1_setup_page_reset(void){hp_el_setup_page=0u;}
static void hp_elastic_p1_setup_page_toggle(void)
{
    hp_el_setup_page^=1u;ui.force=1;
    ui_message(hp_el_setup_page?"ELASTIC PHYSICS: K1-K4":"ELASTIC TIMES: K1-K4");
}
static void hp_elastic_p1_publish_track(uint32_t t)
{
    if(t>=NTRK)return;
    for(uint32_t k=0;k<HP_ER_MAX;k++)hp_er_audio[t][k]=0u;
    if(hp_el_scope_track[t] && hp_el1_track[t].on && hp_er_track[t].track==t) {
        hp_el1_audio[t]=0u;
        for(uint32_t k=0;k<HP_ER_MAX;k++){
            if(!(hp_er_track[t].valid&(1u<<k)))continue;
            const hp_elastic_t*e=&hp_er_track[t].spring[k];
            int32_t delta=(int32_t)hp_elastic_p13_row_value(t,k)-e->anchor;
            /* Passive K2 can move from coupled K1; no extra held voices.
             * With COUPLE=0 the original P1.2 publication is identical. */
            if(delta && (e->active || hp_el_couple_live[t]))
                hp_er_audio[t][k]=HP_EL1_VALID|
                    ((uint32_t)hp_er_track[t].engine<<24)|
                    ((uint32_t)hp_er_track[t].id[k]<<16)|
                    (uint16_t)(int16_t)delta;
        }
        return;
    }
    hp_el1_audio[t]=0u;
    const hp_elastic_p1_t *s=&hp_el1_track[t];
    if(s->on && s->id<P_COUNT && s->spring.active){
        int32_t delta=(int32_t)hp_elastic_value(&s->spring)-s->spring.anchor;
        if(delta)hp_el1_audio[t]=HP_EL1_VALID|((uint32_t)s->engine<<24)|
            ((uint32_t)s->id<<16)|(uint16_t)(int16_t)delta;
    }
}
static void hp_elastic_p1_publish(void)
{
    for(uint32_t t=0;t<NTRK;t++)hp_elastic_p1_publish_track(t);
}
#else
static void hp_elastic_p1_publish(void)
{
    uint32_t t=hp_el1.track;
    if(t>=NTRK||hp_el1.id>=P_COUNT)return;
    uint32_t word=0;
    if(hp_el1.on && hp_el1.spring.active){
        int32_t delta=(int32_t)hp_elastic_value(&hp_el1.spring)-hp_el1.spring.anchor;
        if(delta)word=HP_EL1_VALID|((uint32_t)hp_el1.engine<<24)|
                        ((uint32_t)hp_el1.id<<16)|(uint16_t)(int16_t)delta;
    }
    hp_el1_audio[t]=word; /* atomic-width value on the target, no separate pointer */
}
#endif
static void hp_elastic_p1_manual(uint32_t t,uint32_t id)
{
#if CARAVEL_ELASTIC_PLAYABLE
    if(t>=NTRK)return;
    if(hp_el_scope_track[t] && hp_el1_track[t].on && hp_er_track[t].track==t){
        for(uint32_t k=0;k<HP_ER_MAX;k++)
            if((hp_er_track[t].valid&(1u<<k))&&hp_er_track[t].id[k]==id){
                hp_er_unbind(&hp_er_track[t],k);hp_er_audio[t][k]=0u;
                hp_et_reset(&hp_el_row_time_track[t][k]);
                if(!hp_er_count(&hp_er_track[t]))hp_elastic_p1_clear_track(t);
                ui.force=1;return;
            }
        return;
    }
    if(hp_el1_track[t].on && id==hp_el1_track[t].id)
        hp_elastic_p1_clear_track(t);
#else
    if(hp_el1.on&&t==hp_el1.track&&id==hp_el1.id)hp_elastic_p1_clear();
#endif
}
/* A continuous knob card of the selected part, in the current engine. Pair
 * tracks and engine-fade states are excluded; an engine change invalidates. */
static int hp_elastic_p1_ref_target(hp_ref_t r,uint32_t track)
{
    if(r.kind!=HP_TRACK||hp_owner(r)!=track)return 0;
    if(track>=NTRK||trk[track].engine!=trk[track].eng_req)return 0;
    if(pair_member(&trk[track]))return 0;
#if CARAVEL_ELASTIC_PLAYABLE
    return hp_elastic_p12_safe_id(&trk[track],hp_tid(r));
#else
    return hp_elastic_p1_safe_id(&trk[track],hp_tid(r));
#endif
}
/* Exact preferred matches must not silently fall through to the first
 * eligible cell: that previously shadowed a last-touched knob on another row. */
/* The remembered exact source must beat any fallback. Do not interpret
 * "first safe cell" as a preferred-id match: that selected the wrong knob.
 * Match a HOME ref to the original track + engine + parameter before fallback.
 */
static int hp_elastic_p1_find_exact(uint32_t row,hp_ref_t want,uint32_t *slot)
{
    const hp_track_t *h=&hp.track[song.sel];
    if(row>=h->count||want.kind!=HP_TRACK)return 0;
    for(uint32_t k=0;k<4u;k++){
        hp_ref_t r=h->cell[row][k];
        if(r.kind==HP_TRACK && hp_owner(r)==hp_owner(want) &&
           hp_tid(r)==hp_tid(want) &&
           hp_elastic_p1_ref_target(r,song.sel)){
            *slot=k;return 1;
        }
    }
    return 0;
}
static int hp_elastic_p1_find_row(uint32_t row,uint32_t prefer,uint32_t *slot)
{
    const hp_track_t *h=&hp.track[song.sel];
    if(row>=h->count)return 0;
    if(prefer!=0xFFFFu)
        for(uint32_t k=0;k<4u;k++)
            if(hp_tid(h->cell[row][k])==prefer&&
               hp_elastic_p1_ref_target(h->cell[row][k],song.sel)){
                *slot=k;return 1;
            }
    for(uint32_t k=0;k<4u;k++)
        if(hp_elastic_p1_ref_target(h->cell[row][k],song.sel)){
            *slot=k;return 1;
        }
    return 0;
}
/* Explicit last touch anywhere beats any default; active row TONE next,
 * then an eligible visible active cell, then eligible fallback row.
 * This does NOT silently modify or save the target parameter.
 */
static int hp_elastic_p1_find(uint32_t *row,uint32_t *slot)
{
    const hp_track_t *h=&hp.track[song.sel];
    uint32_t active=h->active;
    if(hp_last_valid && hp_last_track==song.sel){
        if(active<h->count&&hp_elastic_p1_find_exact(active,hp_last,slot)){
            *row=active;return 1;
        }
        for(uint32_t r=0;r<h->count;r++){
            if(r!=active && hp_elastic_p1_find_exact(r,hp_last,slot)){
                *row=r;return 1;
            }
        }
    }
    if(active<h->count && hp_elastic_p1_find_row(active,P_E7,slot)){
        *row=active;return 1;
    }
    for(uint32_t r=0;r<h->count;r++){
        if(r!=active && hp_elastic_p1_find_row(r,P_E7,slot)){
            *row=r;return 1;
        }
    }
    return 0;
}
static int hp_elastic_p1_target(uint32_t track,uint32_t row,uint32_t slot,uint32_t id,uint32_t engine)
{
    if(track>=NTRK)return 0;
    const hp_track_t *h=&hp.track[track];
    if(row>=h->count||slot>=4u)return 0;
    hp_ref_t r=h->cell[row][slot];
    return hp_tid(r)==id && trk[track].engine==engine &&
           hp_elastic_p1_ref_target(r,track);
}
#if CARAVEL_ELASTIC_PLAYABLE
/* ROW owns only the four visible, safe and distinct DRONE timbre cells. */
static int hp_elastic_p12_arm(void)
{
    uint32_t track=song.sel;
    hp_track_t *h=&hp.track[track];
    uint32_t row=h->active;
    if(track>=NTRK || row>=h->count)return 0;
    hp_er_reset(&hp_er,(uint8_t)track,(uint8_t)row,trk[track].engine);
    for(uint32_t k=0;k<HP_ER_MAX;k++) {
        hp_ref_t r=h->cell[row][k];
        if(!hp_elastic_p1_ref_target(r,track))continue;
        uint32_t id=hp_tid(r);
        const param_desc_t *d=track_desc(&trk[track],id);
        if(!d || id>=P_COUNT)continue;
        if(hp_er_bind(&hp_er,k,(uint16_t)id,trk[track].p[id],
                      d->min,d->max,fm1_ms)){
            hp_et_reset(&hp_el_row_time[k]);
#if FELUCCA_RANDOM_MORPH
            hp_rm_disengage(r);
#endif
        }
    }
    if(!hp_er_count(&hp_er))return 0;
    hp_el1.on=1;hp_el1.track=(uint8_t)track;hp_el1.row=(uint8_t)row;
    hp_el1.slot=255u;hp_el1.id=0u;hp_el1.engine=trk[track].engine;
    hp_el_pick=0u;hp_el_row_held=0u;
    hp_elastic_p1_publish();
    return 1;
}
static uint8_t hp_elastic_p12_isheld(void)
{
    return hp_el_scope ? hp_el_row_held : hp_el1.spring.paused;
}
static int hp_elastic_p12_card(uint32_t k)
{
    return hp_el_scope && hp_el1.on && hp_er.track==song.sel &&
        hp_er.row==hp.track[song.sel].active &&
        k<HP_ER_MAX && (hp_er.valid&(1u<<k));
}
static int32_t hp_elastic_p12_card_value(uint32_t k,int32_t base)
{
    return hp_elastic_p12_card(k)?hp_elastic_p13_row_value(song.sel,k):base;
}
static int32_t hp_elastic_p12_base_value(uint32_t k,int32_t base)
{
    return hp_elastic_p12_card(k)?hp_er.spring[k].anchor:
         (!hp_el_scope && hp_el1.on && !hp_el_pick && hp_el1.track==song.sel &&
          hp_el1.row==hp.track[song.sel].active && hp_el1.slot==k)?
          hp_el1.spring.anchor:base;
}
/* Second, short line for the real 159px ACTION-strip space. */
static void hp_elastic_p12_detail(char *buf,uint32_t n)
{
    if(!hp_el1.on){
        uint32_t first=0u,count=hp_elastic_p1_armed_count(&first);
        if(count==1u)perf_format(buf,n,"T%u ARMED / A1 ARM",first+1u);
        else if(count)perf_format(buf,n,"%u TRACKS ARMED",count);
        else str_cpy(buf,"A1 ARM  /  PLAY K1-K4",n);
        return;
    }
    if(hp_el_scope){
        uint32_t k=hp_er.last;
        if(k>=HP_ER_MAX || !(hp_er.valid&(1u<<k))){
            str_cpy(buf,"TURN K1-K4  /  A2 PULSE",n);return;
        }
        const hp_elastic_t*e=&hp_er.spring[k];
        const param_desc_t*d=track_desc(&trk[hp_er.track],hp_er.id[k]);
        const char *phase=hp_elastic_p12_motion_owns(hp_er.track,hp_er.id[k])?
            "MOTION":hp_el_row_held?"HOLD":
            hp_el_row_time[k].phase==1u?"PULL":hp_el_row_time[k].phase==2u?"BACK":"REST";
        perf_format(buf,n,"K%u %s %+d %s",k+1u,d?d->label:"PAR",
             hp_elastic_value(e)-e->anchor,phase);
    }else if(hp_el_pick){
        str_cpy(buf,"PICK: TURN SAFE DRONE KNOB",n);
    }else{
        const param_desc_t*d=track_desc(&trk[hp_el1.track],hp_el1.id);
        const char *phase=hp_elastic_p12_motion_owns(hp_el1.track,hp_el1.id)?
            "MOTION":hp_el1.spring.paused?"HOLD":
            hp_el_cell_time.phase==1u?"PULL":
            hp_el_cell_time.phase==2u?"BACK":"REST";
        perf_format(buf,n,"K%u %s %+d %s",hp_el1.slot+1u,d?d->label:"PAR",
            hp_elastic_value(&hp_el1.spring)-hp_el1.spring.anchor,phase);
    }
}
#endif
static void hp_elastic_p1_action(uint32_t key)
{
    if(key==0u){
        if(hp_el1.on){hp_elastic_p1_clear_current();ui_message("ELASTIC OFF");ui.force=1;return;}
#if !CARAVEL_ELASTIC_PLAYABLE
        if(song.playing){ui_message("ELASTIC P1: STOP SEQ FIRST");ui.force=1;return;}
#endif
#if CARAVEL_ELASTIC_PLAYABLE
        if(hp_el_scope){
            if(!hp_elastic_p12_arm()){
                ui_message("ELASTIC ROW: PIN A SAFE HOME KNOB");ui.force=1;return;
            }
            ui_message("ELASTIC ROW: TURN K1-K4");ui.force=1;return;
        }
        /* CELL pick is explicit: arming must not edit the stored parameter. */
        hp_el1.on=1u;hp_el1.track=(uint8_t)song.sel;
        hp_el1.row=hp.track[song.sel].active;hp_el1.slot=255u;
        hp_el1.engine=trk[song.sel].engine;hp_el1.id=0u;
        hp_el_pick=1u;ui_message("ELASTIC: TURN SAFE DRONE KNOB");
        ui.force=1;return;
#endif
        uint32_t row,k;
        if(!hp_elastic_p1_find(&row,&k)){ui_message("PIN A CONTINUOUS KNOB ON HOME");ui.force=1;return;}
        hp.track[song.sel].active=(uint8_t)row;
        hp_ref_t r=hp.track[song.sel].cell[row][k];
        const param_desc_t *d=track_desc(&trk[song.sel],hp_tid(r));
        if(!hp_elastic_p1_safe_desc(d)){ui_message("ELASTIC: PARAMETER NOT SAFE");ui.force=1;return;}
#if FELUCCA_RANDOM_MORPH
        hp_rm_disengage(r); /* a new manual performance owns this target */
#endif
        hp_el1.id=(uint16_t)hp_tid(r);
        hp_el1.track=(uint8_t)song.sel;hp_el1.row=(uint8_t)row;hp_el1.slot=(uint8_t)k;
        hp_el1.engine=trk[song.sel].engine;
        hp_elastic_reset(&hp_el1.spring,trk[song.sel].p[hp_el1.id],d->min,d->max,fm1_ms);
        hp_el1.on=1;
        hp_elastic_p1_publish();
        ui_message("ELASTIC ARMED: TURN KNOB");ui.force=1;return;
    }
    if(!hp_el1.on){ui_message("ELASTIC: A1 FIRST");ui.force=1;return;}
#if CARAVEL_ELASTIC_PLAYABLE
    if(hp_el_scope){
        if(key==1u){
            if(hp_el_row_held){ui_message("ELASTIC: HOLD");ui.force=1;return;}
            uint32_t k=hp_er.last;
            if(k<HP_ER_MAX && (hp_er.valid&(1u<<k))){
                hp_et_touch(&hp_er.spring[k],&hp_el_row_time[k],1,
                            fm1_ms,hp_el_times,hp_el1_cfg);
                ui_message("ELASTIC PULSE");
            }else ui_message("TURN A ROW KNOB FIRST");
        }else if(key==2u){
            uint32_t moving=0;
            for(uint32_t k=0;k<HP_ER_MAX;k++)
                if((hp_er.valid&(1u<<k))&&hp_er.spring[k].active)moving++;
            if(!moving){ui_message("ELASTIC: TURN KNOB FIRST");ui.force=1;return;}
            hp_el_row_held=!hp_el_row_held;
            for(uint32_t k=0;k<HP_ER_MAX;k++)
                if((hp_er.valid&(1u<<k))&&hp_er.spring[k].active)
                    hp_et_hold(&hp_er.spring[k],&hp_el_row_time[k],
                               hp_el_row_held,fm1_ms);
            ui_message(hp_el_row_held?"ELASTIC HOLD":"ELASTIC RESUME");
        }else if(key==3u){
            hp_er_return(&hp_er);
            for(uint32_t k=0;k<HP_ER_MAX;k++)hp_et_reset(&hp_el_row_time[k]);
            hp_el_row_held=0u;
            ui_message("ELASTIC RESET"); /* immediate safety action */
        }
        hp_elastic_p1_publish();ui.force=1;return;
    }
    if(hp_el_pick){ui_message("TURN A DRONE KNOB FIRST");ui.force=1;return;}
#endif
    if(key==1u){
        if(hp_el1.spring.paused){ui_message("ELASTIC: HOLD");ui.force=1;return;}
#if CARAVEL_ELASTIC_PLAYABLE
        hp_et_touch(&hp_el1.spring,&hp_el_cell_time,1,fm1_ms,
                    hp_el_times,hp_el1_cfg);
#else
        hp_elastic_touch(&hp_el1.spring,1,fm1_ms,hp_el1_cfg);
#endif
        ui_message("ELASTIC EXCITE");}
    else if(key==2u){
        if(!hp_el1.spring.active){ui_message("ELASTIC: TURN KNOB FIRST");ui.force=1;return;}
#if CARAVEL_ELASTIC_PLAYABLE
        hp_et_hold(&hp_el1.spring,&hp_el_cell_time,
                   !hp_el1.spring.paused,fm1_ms);
#else
        hp_elastic_hold(&hp_el1.spring,!hp_el1.spring.paused,fm1_ms);
#endif
        ui_message(hp_el1.spring.paused?"ELASTIC HOLD":"ELASTIC RESUME");}
    else if(key==3u){
        const param_desc_t*d=track_desc(&trk[hp_el1.track],hp_el1.id);
        hp_elastic_reset(&hp_el1.spring,hp_el1.spring.anchor,
                         d?d->min:0,d?d->max:127,fm1_ms);
#if CARAVEL_ELASTIC_PLAYABLE
        hp_et_reset(&hp_el_cell_time);
#endif
        ui_message("ELASTIC RESET"); /* P1 deliberately does NOT implement SET/commit */
    }
    hp_elastic_p1_publish();ui.force=1;
}
static int hp_elastic_p1_turn(hp_ref_t r,uint32_t slot,int32_t raw)
{
    if(ui.ml.on)return 0; /* MIDI Learn keeps its original HOME knob behavior */
#if CARAVEL_ELASTIC_PLAYABLE
    if(hp_el1.on && hp_el_scope){
        if(hp_er.track!=song.sel || hp_er.row!=hp.track[song.sel].active)
            return 0;
        if(slot>=HP_ER_MAX || !(hp_er.valid&(1u<<slot))){
            ui_message("NOT ELASTIC: USE DRONE TIMBRE");ui.force=1;return 1;
        }
        hp_ref_t v=hp.track[song.sel].cell[hp_er.row][slot];
        if(hp_tid(r)!=hp_er.id[slot] || !hp_elastic_p1_ref_target(v,song.sel)){
            ui_message("ELASTIC TARGET CHANGED");ui.force=1;return 1;
        }
        if(!hp_el_row_held){
            hp_et_touch(&hp_er.spring[slot],&hp_el_row_time[slot],raw,
                        fm1_ms,hp_el_times,hp_el1_cfg);
            hp_er.last=(uint8_t)slot;
        }
        hp_elastic_p1_publish();ui.force=1;return 1;
    }
    if(hp_el1.on && hp_el_pick){
        if(song.sel!=hp_el1.track || hp_el1.row!=hp.track[song.sel].active)
            return 0;
        if(!hp_elastic_p1_ref_target(r,song.sel)){
            ui_message("NOT ELASTIC: USE DRONE TIMBRE");ui.force=1;return 1;
        }
        uint32_t id=hp_tid(r);
        const param_desc_t*d=track_desc(&trk[song.sel],id);
        if(!d){ui_message("ELASTIC TARGET UNAVAILABLE");ui.force=1;return 1;}
#if FELUCCA_RANDOM_MORPH
        hp_rm_disengage(r);
#endif
        hp_el1.id=(uint16_t)id;hp_el1.slot=(uint8_t)slot;
        hp_el1.engine=trk[song.sel].engine;hp_el_pick=0u;
        hp_elastic_reset(&hp_el1.spring,trk[song.sel].p[id],
                         d->min,d->max,fm1_ms);
        hp_et_reset(&hp_el_cell_time);
    }
#endif
    if(!hp_el1.on||hp_el1.track!=song.sel||hp_el1.row!=hp.track[song.sel].active||
       hp_el1.slot!=slot||hp_tid(r)!=hp_el1.id||
       hp_owner(r)!=song.sel||!hp_elastic_p1_ref_target(r,song.sel))return 0;
#if CARAVEL_ELASTIC_PLAYABLE
    hp_et_touch(&hp_el1.spring,&hp_el_cell_time,raw,fm1_ms,
                hp_el_times,hp_el1_cfg); /* no immediate parameter jump */
#else
    hp_elastic_touch(&hp_el1.spring,raw,fm1_ms,hp_el1_cfg);
#endif
    hp_elastic_p1_publish();ui.force=1;return 1;
}
#if CARAVEL_ELASTIC_PLAYABLE
/* Returns 0 no visible change, 1 changed, 2 hard invalidation. */
static uint32_t hp_elastic_p1_poll_track(uint32_t t,uint32_t now)
{
    hp_elastic_p1_t *st=&hp_el1_track[t];
    if(!st->on)return 0u;
    hp_er_t *er=&hp_er_track[t];
    const hp_elastic_cfg_t cfg=hp_el_cfg_track[t];
    const hp_et_cfg_t times=hp_el_times_track[t];
    int coupled_changed=hp_elastic_p13_coupling_poll(t,now);
    if(hp_el_scope_track[t] && er->track<NTRK){
        if(trk[er->track].engine!=er->engine ||
           trk[er->track].eng_req!=er->engine){
            hp_elastic_p1_clear_track(t);return 2u;
        }
        uint32_t changed=coupled_changed;
        for(uint32_t k=0;k<HP_ER_MAX;k++){
            if(!(er->valid&(1u<<k)))continue;
            hp_ref_t v=hp.track[er->track].cell[er->row][k];
            if(hp_tid(v)!=er->id[k] ||
               !hp_elastic_p1_ref_target(v,er->track)){
                hp_er_unbind(er,k);hp_er_audio[t][k]=0u;
                hp_et_reset(&hp_el_row_time_track[t][k]);
                changed=1u;continue;
            }
            int16_t before=hp_elastic_p13_row_value(t,k);
            int32_t travel=hp_elastic_limit(&er->spring[k],cfg)*256;
            int32_t shift=hp_evis_shift(hp_elastic_p13_row_x(t,k),travel);
            uint8_t phase=hp_el_row_time_track[t][k].phase;
            hp_et_poll_bpm(&er->spring[k],&hp_el_row_time_track[t][k],now,
                           times,cfg,(uint32_t)song.g[G_BPM]);
            /* The 3px spring can move before the rounded parameter does.
             * Refresh the display at the existing capped 80ms cadence. */
            if(before!=hp_elastic_p13_row_value(t,k) ||
               shift!=hp_evis_shift(hp_elastic_p13_row_x(t,k),travel) ||
               phase!=hp_el_row_time_track[t][k].phase)changed=1u;
        }
        if(!hp_er_count(er)){hp_elastic_p1_clear_track(t);return 2u;}
        hp_elastic_p1_publish_track(t);
        return changed;
    }
    if(hp_el_pick_track[t]){
        if(hp.track[st->track].active!=st->row ||
           trk[st->track].engine!=st->engine){
            hp_elastic_p1_clear_track(t);return 2u;
        }
        return 0u;
    }
    if(!hp_elastic_p1_target(st->track,st->row,st->slot,st->id,st->engine)){
        hp_elastic_p1_clear_track(t);return 2u;
    }
    int16_t before=hp_elastic_value(&st->spring);
    int32_t travel=hp_elastic_limit(&st->spring,cfg)*256;
    int32_t shift=hp_evis_shift(st->spring.x,travel);
    uint8_t moving=st->spring.active,phase=hp_el_cell_time_track[t].phase;
    hp_et_poll_bpm(&st->spring,&hp_el_cell_time_track[t],now,
                   times,cfg,(uint32_t)song.g[G_BPM]);
    hp_elastic_p1_publish_track(t);
    return (before!=hp_elastic_value(&st->spring)||
            shift!=hp_evis_shift(st->spring.x,travel) ||
            phase!=hp_el_cell_time_track[t].phase ||
            (moving&&!st->spring.active))?1u:0u;
}
static void hp_elastic_p1_poll(uint32_t now)
{
    uint32_t changed=0u,force=0u;
    for(uint32_t t=0;t<NTRK;t++){
        uint32_t r=hp_elastic_p1_poll_track(t,now);
        if(r>=2u)force=1u;
        else if(r && t==song.sel)changed=1u; /* offscreen motion never forces LCD redraw */
    }
    if(force)ui.force=1;
    else if(changed && (int32_t)(now-hp_el_ui_next)>=0){
        hp_el_ui_next=now+80u;ui.force=1;
    }
}
#else
static void hp_elastic_p1_poll(uint32_t now)
{
    if(!hp_el1.on)return;
    if(song.playing||song.sel!=hp_el1.track||
       !hp_elastic_p1_target(hp_el1.track,hp_el1.row,hp_el1.slot,hp_el1.id,hp_el1.engine)){
        hp_elastic_p1_clear();ui.force=1;return;
    }
    int16_t before=hp_elastic_value(&hp_el1.spring);
    uint8_t moving=hp_el1.spring.active;
    hp_elastic_poll(&hp_el1.spring,now,hp_el1_cfg);
    hp_elastic_p1_publish();
    if(before!=hp_elastic_value(&hp_el1.spring)||(moving&&!hp_el1.spring.active))
        ui.force=1;
}
#endif
static int hp_elastic_p1_card(hp_ref_t r,uint32_t slot)
{
#if CARAVEL_ELASTIC_PLAYABLE
    if(hp_el_scope) return hp_elastic_p12_card(slot) &&
                        hp_er.id[slot]==hp_tid(r);
    if(hp_el_pick)return 0;
#endif
    return hp_el1.on && hp_el1.track==song.sel &&
           hp_el1.row==hp.track[song.sel].active && hp_el1.slot==slot &&
           hp_tid(r)==hp_el1.id && hp_owner(r)==song.sel;
}
static void hp_elastic_p1_setup_caption(uint32_t k,char *name,char *value)
{
#if CARAVEL_ELASTIC_PLAYABLE
    if(hp_el_setup_page){
        const hp_et_cfg_t p=hp_el_times;
        if(k==0u){str_cpy(name,"BOUNCE",16);
            if(hp_es_is_sync(p.bounce))str_cpy(value,hp_es_label(p.bounce),24);
            else perf_format(value,24,"%u",p.bounce);return;}
        if(k==1u){str_cpy(name,"REFLECT",16);perf_format(value,24,"%u%%",p.reflect);return;}
        if(k==2u){str_cpy(name,"COUPLE",16);
            if(!p.couple)str_cpy(value,"OFF",24);
            else perf_format(value,24,"%u%%",p.couple);
            return;
        }
        str_cpy(name,"SEED",16);perf_format(value,24,"%u",p.seed);return;
    }
    if(k==0u){
        str_cpy(name,"SCOPE",16);str_cpy(value,hp_el_scope?"ROW":"CELL",24);
        return;
    }
    if(k==1u || k==2u){
        uint32_t duration=k==1u?hp_et_ms(hp_el_times.pull):
                                  hp_et_return_ms(hp_el_times.back);
        str_cpy(name,k==1u?"PULL":"RETURN",16);
        if(k==2u && hp_el_times.back==HP_ES_RETURN_INF){str_cpy(value,"INF",24);return;}
        if(duration<1000u)perf_format(value,24,"%ums",duration);
        else if(duration>=60000u){
            /* 5m is short and unmistakable; 1m30 fits the 48px value row.
             * No tiny seconds decimals once the user is in the slow zone. */
            uint32_t mins=duration/60000u,secs=(duration%60000u)/1000u;
            if(secs>=10u)perf_format(value,24,"%um%u",mins,secs);
            else if(secs)perf_format(value,24,"%um0%u",mins,secs);
            else perf_format(value,24,"%um",mins);
        }else {
            uint32_t tenths=(duration+50u)/100u;
            if(!(tenths%10u))perf_format(value,24,"%us",tenths/10u);
            else perf_format(value,24,"%u.%us",tenths/10u,tenths%10u);
        }
        return;
    }
    str_cpy(name,"RANGE",16);fmt_int(value,hp_el1_cfg.range);
#else
    static const char*const names[4]={"RESPONSE","FALL","BOUNCE","RANGE"};
    uint8_t v=k==0?hp_el1_cfg.response:k==1?hp_el1_cfg.decay:
              k==2?hp_el1_cfg.bounce:hp_el1_cfg.range;
    str_cpy(name,names[k&3u],16);fmt_int(value,v);
#endif
}
static void hp_elastic_p1_setup_turn(uint32_t k,int32_t steps)
{
#if CARAVEL_ELASTIC_PLAYABLE
    if(!steps||k>=4u)return;
    if(hp_el_setup_page){
        hp_et_cfg_t *p=&hp_el_times;
        if(k==0u)p->bounce=hp_es_step(p->bounce,steps);
        else if(k==1u)p->reflect=(uint8_t)clamp((int32_t)p->reflect+steps,0,100);
        else if(k==2u)p->couple=(uint8_t)clamp((int32_t)p->couple+steps,0,100);
        else{
            /* Seed changes while COUPLE is active would jump the effective
             * pair network. Turn COUPLE off and let it fade to 0 first. */
            if(hp_el1.on && (p->couple||hp_el_couple_live[song.sel])){
                ui_message("COUPLE OFF BEFORE SEED");ui.force=1;return;
            }
            p->seed=(uint8_t)clamp((int32_t)p->seed+steps,0,255);
        }
        hp_dirty=1u;ui.force=1;return;
    }
    if(k==0u){
        uint8_t new_scope=(uint8_t)(steps>0?1u:0u);
        if(new_scope!=hp_el_scope){
            hp_elastic_p1_clear_current();hp_el_scope=new_scope;
            hp_dirty=1u;
            ui_message(hp_el_scope?"ELASTIC ROW: 4 KNOBS":"ELASTIC CELL: PICK A KNOB");
        }
        ui.force=1;return;
    }
    if(k==1u||k==2u){
        uint8_t *v=k==1u?&hp_el_times.pull:&hp_el_times.back;
        *v=(uint8_t)clamp((int32_t)*v+steps,0,k==2u?HP_ES_RETURN_INF:100u);
        /* Legacy v4 count playback cannot have an infinite envelope.
         * Selecting INF intentionally switches its next gesture to tempo mode. */
        if(k==2u && *v==HP_ES_RETURN_INF &&
           !hp_es_is_sync(hp_el_times.bounce))
            hp_el_times.bounce=HP_ES_DEFAULT;
        hp_dirty=1u;
        /* In-flight gesture durations are snapshotted; new settings
         * apply to the next turn without jumping a sounding parameter. */
        ui.force=1;return;
    }
    if(k!=3u)return;
    hp_el1_cfg.range=(uint8_t)clamp((int32_t)hp_el1_cfg.range+steps,0,100);
    hp_dirty=1u;
    /* Only the selected track's current spring is clamped. A change to
     * T2 RANGE must not change T1/T3/T4 trajectories or audio words. */
    uint32_t tr=song.sel;
    if(hp_el1_track[tr].on){
        if(hp_el_scope){
            hp_er_t *er=&hp_er_track[tr];
            for(uint32_t i=0;i<HP_ER_MAX;i++){
                if(!(er->valid&(1u<<i)))continue;
                hp_elastic_t *e=&er->spring[i];
                hp_et_t *t=&hp_el_row_time_track[tr][i];
                if(!hp_elastic_limit(e,hp_el1_cfg))hp_et_cancel(e,t);
                else{
                    e->x=hp_et_clamp_x(e,e->x,hp_el1_cfg);
                    t->start_q8=hp_et_clamp_x(e,t->start_q8,hp_el1_cfg);
                    t->goal_q8=hp_et_clamp_x(e,t->goal_q8,hp_el1_cfg);
                }
            }
        }else if(!hp_el_pick_track[tr]){
            hp_elastic_t *e=&hp_el1_track[tr].spring;
            hp_et_t *t=&hp_el_cell_time_track[tr];
            if(!hp_elastic_limit(e,hp_el1_cfg))hp_et_cancel(e,t);
            else{
                e->x=hp_et_clamp_x(e,e->x,hp_el1_cfg);
                t->start_q8=hp_et_clamp_x(e,t->start_q8,hp_el1_cfg);
                t->goal_q8=hp_et_clamp_x(e,t->goal_q8,hp_el1_cfg);
            }
        }
    }
    hp_elastic_p1_publish();ui.force=1;
#else
    uint8_t *v=k==0?&hp_el1_cfg.response:k==1?&hp_el1_cfg.decay:
               k==2?&hp_el1_cfg.bounce:&hp_el1_cfg.range;
    *v=(uint8_t)clamp((int32_t)*v+steps,0,100);
    int32_t maxq=hp_elastic_limit(&hp_el1.spring,hp_el1_cfg)*256;
    hp_el1.spring.x=hp_elastic_clamp(hp_el1.spring.x,-maxq,maxq);
    hp_el1.spring.v=hp_elastic_clamp(hp_el1.spring.v,-maxq,maxq);
    if(!maxq)hp_elastic_cancel(&hp_el1.spring);
    hp_elastic_p1_publish();ui.force=1;
#endif
}
static void hp_elastic_p1_status(char *buf,uint32_t length)
{
#if CARAVEL_ELASTIC_PLAYABLE
    if(!hp_el1.on){
        uint32_t first=0u,count=hp_elastic_p1_armed_count(&first);
        if(count==1u)perf_format(buf,length,"A1 ARM / T%u ARMED",first+1u);
        else if(count)perf_format(buf,length,"A1 ARM / %u ARMED",count);
        else str_cpy(buf,hp_el_scope?"A1 ARM ROW":"A1 ARM / TURN KNOB",length);
        return;
    }
    if(hp_el_scope){
        perf_format(buf,length,"T%u ROW %u/4 %s",hp_er.track+1u,
                    hp_er_count(&hp_er),hp_el_row_held?"HOLD":"PLAY");
        return;
    }
    if(hp_el_pick){str_cpy(buf,"TURN K1-K4 TO PICK",length);return;}
#endif
    if(!hp_el1.on){str_cpy(buf,"A1: ARM KNOB",length);return;}
    const hp_elastic_t*e=&hp_el1.spring;
    const param_desc_t*d=track_desc(&trk[hp_el1.track],hp_el1.id);
    perf_format(buf,length,"T%u %s %d / %d%s",hp_el1.track+1u,
                d?d->label:"PARAM",hp_elastic_value(e),e->anchor,e->paused?" HOLD":"");
}
/* Called only from hp_begin/hp_end, inside the existing per-track audio bracket,
 * after the HOME macro and before mod_begin / after mod_end. This preserves
 * the original parameter before project capture and after each render block. */
static void hp_elastic_p1_audio_begin(track_t*t)
{
    uint32_t i=trk_index(t);
#if CARAVEL_ELASTIC_PLAYABLE
    /* UI state is not read by the audio core. Only its published 32-bit words
     * are sampled once per block; audio keeps its own restore frames. */
    uint32_t words[HP_ER_MAX],row_mask=0u;
    for(uint32_t k=0;k<HP_ER_MAX;k++){
        words[k]=hp_er_audio[i][k];
        row_mask|=words[k]&HP_EL1_VALID;
    }
    if(row_mask){
        /* Multiword publication is not a coherent transaction across cores.
         * Audio frame snapshots are bounded, but this still needs a full
         * dual-core/SAVE race audit before any release approval. */
        hp_el1_frame[i].applied=0u;
        for(uint32_t k=0;k<HP_ER_MAX;k++){
            uint32_t w=words[k];
            hp_elo_frame_t*f=&hp_er_frame[i][k];
            f->applied=0u;
            if(!(w&HP_EL1_VALID))continue;
            uint32_t id=HP_EL1_ID(w),eng=HP_EL1_ENG(w);
            int16_t delta=(int16_t)(w&0xFFFFu);
            if(!delta||id>=P_COUNT||t->engine!=eng){hp_el_fade[i][k]=64u;continue;}
            if(hp_el_fade_id[i][k]!=id || hp_el_fade_eng[i][k]!=eng){
                hp_el_fade_id[i][k]=(uint16_t)id;
                hp_el_fade_eng[i][k]=(uint8_t)eng;
                hp_el_fade[i][k]=64u;
            }
            /* MOTION/step lock wins over the ELASTIC transient layer.
             * Spring trajectory continues in the UI, but may be inaudible
             * while that target belongs to the sequencer. On release, the
             * overlay fades back in instead of stepping to the full delta. */
            if(hp_elastic_p12_motion_owns(i,id) ||
               !hp_elastic_p12_safe_id(t,id)){
                hp_el_fade[i][k]=0u;
                continue;
            }
            uint8_t fade=hp_el_fade[i][k];
            if(fade<64u){
                delta=(int16_t)(((int32_t)delta*fade)/64);
                if(hp_el_fade[i][k]<64u)hp_el_fade[i][k]=(uint8_t)(fade+16u>64u?64u:fade+16u);
            }
            const param_desc_t*d=track_desc(t,id);
            hp_elo_begin(&t->p[id],delta,d->min,d->max,f);
            if(f->applied)f->id=(uint16_t)id;
        }
        return;
    }
    for(uint32_t k=0;k<HP_ER_MAX;k++){
        hp_er_frame[i][k].applied=0u;
        hp_el_fade[i][k]=64u;
    }
#endif
    uint32_t word=hp_el1_audio[i];
    hp_elo_frame_t *f=&hp_el1_frame[i];
    uint32_t id=HP_EL1_ID(word),eng=HP_EL1_ENG(word);
    int16_t delta=(int16_t)(word&0xFFFFu);
    if(!(word&HP_EL1_VALID)||!delta||id>=P_COUNT||t->engine!=eng){
        f->applied=0;
#if CARAVEL_ELASTIC_PLAYABLE
        hp_el_cell_fade[i]=64u;
#endif
        return;
    }
#if CARAVEL_ELASTIC_PLAYABLE
    if(hp_el_cell_fade_id[i]!=id || hp_el_cell_fade_eng[i]!=eng){
        hp_el_cell_fade_id[i]=(uint16_t)id;
        hp_el_cell_fade_eng[i]=(uint8_t)eng;
        hp_el_cell_fade[i]=64u;
    }
    if(hp_elastic_p12_motion_owns(i,id) || !hp_elastic_p12_safe_id(t,id)){
        hp_el_cell_fade[i]=0u;
        f->applied=0;return;
    }
    uint8_t fade=hp_el_cell_fade[i];
    if(fade<64u){
        delta=(int16_t)(((int32_t)delta*fade)/64);
        hp_el_cell_fade[i]=(uint8_t)(fade+16u>64u?64u:fade+16u);
    }
#endif
    const param_desc_t*d=track_desc(t,id);
    if(!hp_elastic_p1_safe_desc(d)){f->applied=0;return;}
    hp_elo_begin(&t->p[id],delta,d->min,d->max,f);
    if(f->applied)f->id=(uint16_t)id;
}
static void hp_elastic_p1_audio_end(track_t*t)
{
    uint32_t i=trk_index(t);
#if CARAVEL_ELASTIC_PLAYABLE
    /* Always restore all audio-owned frames even if UI disarmed/switched mode
     * between begin and end. Never rely on current mutable UI state to undo. */
    for(uint32_t k=HP_ER_MAX;k>0u;k--){
        hp_elo_frame_t *g=&hp_er_frame[i][k-1u];
        if(g->applied)hp_elo_end(&t->p[g->id],g);
    }
#endif
    hp_elo_frame_t *f=&hp_el1_frame[i];
    if(f->applied)hp_elo_end(&t->p[f->id],f);
    else f->applied=0;
}
#endif
