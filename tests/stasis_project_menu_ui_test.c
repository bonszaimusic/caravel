/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Narrow UI regression: modal SYSTEM exits, PLAY SAVE opens real SAVE
 * workflow, AFTER/TONE data survive native project encoding.
 *
 * Build with the existing tests/ui_test.c host flags and libraries.
 * Host-only; no FM-1 timings/firmware-package assertions. */
/* Use the actual project/UI integration harness. The legacy ui_test.c
 * fixture omits FELUCCA_PAIR_MORPH/HOME_PERF in default builds and therefore
 * never includes ui_uxl.c (and its C5 edge detector). */
#define FELUCCA_CPU2 0
#define UI_TEST_NO_MAIN 1
#ifndef CARAVEL_UX_LAYERS
#define CARAVEL_UX_LAYERS 1
#endif
#ifndef CARAVEL_STASIS_EXPAND
#define CARAVEL_STASIS_EXPAND 1
#endif
#ifndef CARAVEL_REVERB_MINIMAL
#define CARAVEL_REVERB_MINIMAL 1
#endif
#ifndef CARAVEL_AFTERBURNER
#define CARAVEL_AFTERBURNER 1
#endif
#if !CARAVEL_STASIS2
#error "STASIS2 integration test must be built with CARAVEL_STASIS2=1"
#endif
#include <stdint.h> /* uint32_t must precede forward declarations for ui_input.c */
#include <assert.h>
static void action_refresh(void);
static void action_dispatch(uint32_t keys);
/* A5 is a UI/FORMAT-2 *in-memory* integration test, not a NOR-flash
 * persistence test. The real HOME hardware storage backend needs the
 * physical flash transport; reject unexpected calls rather than faking
 * a successful save, load or erase. Declare first: the UI and project
 * sources are included as a single translation unit by ui_harness.h. */
static int hp_hw_erase_old(uint32_t slot);
static int hp_hw_old(uint32_t slot);
static int hp_project_rename(uint32_t slot, const char *name);
static int hp_project_save(uint32_t slot, const char *name);
static int hp_project_load(uint32_t slot);
static int hp_autosave_boot(int allowed);
static int hp_forget(uint32_t slot);
#include "ui_harness.h"
#include "../experiments/action-banks/action_control.c"
/* Any call here means the fixture has escaped its stated scope.
 * Never return a false green from untested persistent storage. */
static int hp_hw_old(uint32_t slot)
{ (void)slot; assert(!"unexpected HP NOR legacy lookup in UI test"); return 0; }
static int hp_hw_erase_old(uint32_t slot)
{ (void)slot; assert(!"unexpected HP NOR erase in UI test"); return 1; }
static int hp_project_rename(uint32_t slot, const char *name)
{ (void)slot; (void)name; assert(!"unexpected HP NOR rename in UI test"); return 1; }
static int hp_project_save(uint32_t slot, const char *name)
{ (void)slot; (void)name; assert(!"unexpected HP NOR save in UI test"); return 1; }
static int hp_project_load(uint32_t slot)
{ (void)slot; assert(!"unexpected HP NOR load in UI test"); return 1; }
static int hp_autosave_boot(int allowed)
{ (void)allowed; assert(!"unexpected HP NOR boot in UI test"); return 1; }
static int hp_forget(uint32_t slot)
{ (void)slot; assert(!"unexpected HP NOR forget in UI test"); return 1; }

#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
static uint16_t stviz_before[240u*240u],stviz_expected[240u*240u];
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE && !FELUCCA_FM4
static void path_test_frame(const stviz_view_t *v, int scope)
{
    for(uint32_t half=0;half<2u;half++) {
        cv_begin(240,120,T_BG);cv_oy=-(int32_t)(half*120u);
        if(scope)stviz_paint(v,0,0);
        stviz_sides(v);cv_oy=0;cv_blit(0,half*120u);
    }
}
/* The hot state comes from a real PLAY/BUILD render. The assertions do not
 * name the new enum, so this test also compiles against the unfixed source. */
static void stviz_rev_hot_pixels(uint8_t hot)
{
    typeof(ux) saved_ux=ux;
    assert(hot!=STVIZ_HOT_NONE);
    for(uint32_t pal=0;pal<4;pal++) {
        palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
        for(uint32_t active=0;active<2;active++) {
            stviz_view_t v={0};v.pgm=1u;v.source_state=UXL_ST_LIVE;
            v.rsend=active?64u:0u;v.hot=STVIZ_HOT_NONE;
            uint16_t ink=swap16(uxl_path_ink(3u,active));
            path_test_frame(&v,0);
            assert(host_screen[93u*240u+28u]==ink);
            assert(host_screen[81u*240u+219u]==ink);
            assert(host_screen[90u*240u+26u]==swap16(T_BG));
            assert(host_screen[83u*240u+217u]==swap16(T_BG));
            v.hot=hot;path_test_frame(&v,0);
            assert(host_screen[90u*240u+26u]==ink);
            assert(host_screen[83u*240u+217u]==ink);
            path_test_frame(&v,1);memcpy(stviz_before,host_screen,sizeof stviz_before);
            stviz_partial(&v);assert(!memcmp(stviz_before,host_screen,sizeof stviz_before));
        }
    }
    ux=saved_ux;
}
/* Independent expected stroke masks, not derived from the renderer.
 * The same fixture compiles with the pre-F2 production code. */
static void stviz_style_regression(void)
{
    typeof(ux) saved_ux=ux;
    for(uint32_t pal=0;pal<4;pal++) {
        palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
        for(uint32_t pg=1;pg<=2;pg++) {
            stviz_view_t v={0};v.pgm=pg;v.source_state=UXL_ST_LIVE;
            v.send=v.rsend=v.dry=v.feed=v.hall=v.chorus=64;v.layer=1;
            path_test_frame(&v,0);
            /* STS horizontal stem: i=1..13, clear of head and other routes. */
            for(uint32_t i=1;i<=13;i++) {
                uint32_t on=pg==1u?i%5u<3u:(i%8u<3u || i%8u==5u);
                uint16_t ink=pg==1u?T_THEME:T_TEXT;
                assert(host_screen[98u*240u+28u+i]==swap16(on?ink:T_BG));
            }
            /* K4 REV left stem: dotted on SENDS, dash-dot on TANK. */
            for(uint32_t i=1;i<=7;i++) {
                uint32_t on=pg==1u?i%3u==0u:(i%8u<3u || i%8u==5u);
                assert(host_screen[(96u-i)*240u+28u]==swap16(on?T_TEXT:T_BG));
            }
        }
    }
    ux=saved_ux;
}
/* Drive real UI snapshots from mixer/transport state. This test uses only
 * pre-F3 types and functions, so the negative variant still compiles. */
static void stviz_gate_regression(void)
{
    stviz_view_t v;
    uint32_t k=(uint32_t)(TSEL-trk);
    uint8_t mute=pf.mute, ready=st.gr_ready, hold=st.auto_hold, stop=st.stopping,
            release=st.releasing, fresh=st.new_source;
    int32_t mg=pf.mg[k], sg=st.stop_gain, rg=st.release_gain;
#if CARAVEL_STASIS_EXPAND
    uint16_t xf=st.grain_xfade;
    st.grain_xfade=0;
#endif
    int16_t strength=mod_stasis.strength;
    uint32_t paths[]={UXP_DRY,UXP_CHO,UXP_REV,UXP_STS};
    st.auto_hold=st.stopping=st.releasing=st.new_source=0;
    st.gr_ready=1;st.stop_gain=st.release_gain=32768;mod_stasis.strength=64;
    pf.mute|=(uint8_t)(1u<<k);pf.mg[k]=1;
    assert(stviz_snapshot(&v));
    for(uint32_t i=0;i<NELEM(paths);i++) assert(stviz_path_value(&v,paths[i],64)==64);
    uint32_t play_before=uxl_play_sig();
    pf.mg[k]=0;
    assert(uxl_play_sig()!=play_before);
    assert(stviz_snapshot(&v));
    for(uint32_t i=0;i<NELEM(paths);i++) assert(stviz_path_value(&v,paths[i],64)==0);
    assert(stviz_path_value(&v,UXP_HALL,64)==64);
    assert(stviz_path_value(&v,UXP_STR,64)==64 && stviz_path_value(&v,UXP_FEED,64)==64);
    pf.mute=mute;pf.mg[k]=mg;
    st.stopping=1;st.stop_gain=1;assert(stviz_snapshot(&v));
    assert(stviz_path_value(&v,UXP_STS,64)==0);
    assert(stviz_path_value(&v,UXP_STR,64)==64 && stviz_path_value(&v,UXP_FEED,64)==64);
    play_before=uxl_play_sig();st.stop_gain=0;
    assert(uxl_play_sig()!=play_before);
    assert(stviz_snapshot(&v));
    assert(stviz_path_value(&v,UXP_STR,64)==0 && stviz_path_value(&v,UXP_FEED,64)==0);
    assert(stviz_path_value(&v,UXP_HALL,64)==64);
    st.stopping=0;st.auto_hold=1;assert(stviz_snapshot(&v));
    assert(stviz_path_value(&v,UXP_STS,64)==0 && stviz_path_value(&v,UXP_STR,64)==64);
    st.auto_hold=0;st.gr_ready=0;assert(stviz_snapshot(&v));
    assert(stviz_path_value(&v,UXP_STR,64)==0 && stviz_path_value(&v,UXP_FEED,64)==0);
#if CARAVEL_STASIS_EXPAND
    st.grain_xfade=1;assert(stviz_snapshot(&v));
    assert(stviz_path_value(&v,UXP_STR,64)==64);
    st.grain_xfade=xf;
#endif
    st.gr_ready=ready;st.auto_hold=hold;st.stopping=stop;st.releasing=release;st.new_source=fresh;
    st.stop_gain=sg;st.release_gain=rg;mod_stasis.strength=strength;
}
/* Extended F2/F3 acceptance: independent masks, real mixer ramps and
 * actual PLAY/BUILD redraws. This is host-only test code. */
static uint32_t path_expected_mask(uint32_t knob)
{
    /* 14 pixels, bit zero is the first pixel. */
    return knob==1u?0x3fffu:knob==2u?0x1ce7u:knob==3u?0x1249u:0x2727u;
}
static void path_style_acceptance(void)
{
    static const uint8_t paths[]={UXP_STS,UXP_REV,UXP_DRY,UXP_FEED,UXP_HALL,UXP_STR,UXP_CHO};
    static const uint8_t knobs[2][7]={{2,3,255,255,255,255,1},{255,255,2,1,3,255,255}};
    static const uint8_t palettes[]={2,0,9,2};
    typeof(ux) keep=ux;uint8_t style=ui_style;
    for(uint32_t pal=0;pal<4;pal++) {
        ui_style=ux.style=pal==3?ST_LINE:ST_FLAT;palette_set(palettes[pal]);
        for(uint32_t k=1;k<=3;k++) {
            cv_begin(240,120,T_BG);uxl_path_swatch(k,100,40);cv_blit(0,0);
            int32_t x0=(k&1u)?86:100;
            for(uint32_t i=0;i<14;i++)for(uint32_t y=35;y<37;y++)
                assert(host_screen[y*240u+x0+i]==swap16(path_expected_mask(k)&(1u<<i)?uxl_path_col(k):T_BG));
        }
        assert(path_expected_mask(3)!=path_expected_mask(255));
        for(uint32_t pg=1;pg<=2;pg++)for(uint32_t p=0;p<7;p++)
        for(uint32_t active=0;active<2;active++)for(uint32_t hot=0;hot<2;hot++)
        for(uint32_t vertical=0;vertical<2;vertical++) {
            stviz_view_t v={0};v.pgm=pg;v.layer=1;
            uint32_t k=knobs[pg-1][p],mask=path_expected_mask(k);
            uint16_t ink=k==255u?(active?T_TEXT:T_DIM):
                active?uxl_path_col(k):ux_mix(uxl_path_col(k),T_BG,55);
            cv_begin(240,120,T_BG);
            stviz_pathline(&v,paths[p],active?64:0,hot,80,50,vertical?80:93,vertical?63:50,0);
            cv_blit(0,0);
            for(uint32_t i=0;i<14;i++)for(uint32_t thick=0;thick<2;thick++) {
                uint32_t x=80+(vertical?thick:i),y=50+(vertical?i:thick);
                assert(host_screen[y*240+x]==swap16((mask&(1u<<i))&&(!thick||active)?ink:T_BG));
            }
            /* Short stems retain their assigned pattern; the head is solid. */
            cv_begin(240,120,T_BG);stviz_pathline(&v,paths[p],64,hot,80,50,80,62,1);cv_blit(0,0);
            for(uint32_t i=0;i<6;i++)
                assert(host_screen[(50+i)*240+80]==swap16(mask&(1u<<i)?uxl_path_ink(k,1):T_BG));
            assert(host_screen[62u*240+80]==swap16(uxl_path_ink(k,1)));
        }
    }
    ux=keep;ui_style=style;
    puts("F2 acceptance: four palettes, seven paths, three 14px swatches, independent masks, hot/cold, zero values, short stems/solid heads PASS");
}
static void path_acceptance_setup(uint32_t context,uint32_t selected)
{
    song.sel=selected;song.playing=song.rec=0;
    ui.home=ui.menu=ui.confirm=ui.uboot=ui.layer=ui.msg_t=ui.hot_t=ui.bpm_t=0;
    nm.kind=NK_NONE;page_over=0;link_setup_row=0;
    uxl.play=context<2;uxl.area=UXL_SPACE;uxl.grp[UXL_SPACE]=context==1?4:0;
    uxl.inter=uxl.cap_view=uxl.lvl=uxl.pv=uxl.hot_t=uxl.cap_auto=0;
    uxl.hint_until=0;uxl_path_hint_seen=1;uxl_path_hint_until=0;
    const char *name=context==3?"REVERB":"FX";
    for(uint32_t i=0;i<NPAGES;i++)if(str_eq(PAGES[i].title,name)){ui.page=i;break;}
    song.g[G_RTYPE]=3;song.g[G_RSIZE]=64;song.g[G_DRLVL]=0;
    mod_stasis.strength=64;mod_stasis.capt=mod_stasis.motion=0;st_ring_src=0;st_expand.scan=st_expand.reverse=0;st_expand.length=64;st_hall_feed=st_hall_ret=127;
    for(uint32_t k=0;k<NTRK;k++) {
        trk[k].p[P_MUTE]=0;trk[k].p[P_DRY]=trk[k].p[P_REV]=trk[k].p[P_CHOR]=64;trk[k].p[P_SSEND]=64;
    }
    memset(&st,0,sizeof st);st.gr_ready=1;st.valid=ST_RING_LEN;st.w=32768;
    st.region_status=STREG_FIT;st.region_length=28000;st.region_young=256;
    st.active_quality=st_expand.quality=0;st.active_grain=st_expand.grain=ST_GRAIN_DEFAULT;
    st.stop_gain=st.release_gain=32768;
    st.gr_l[0]=12000;st.gr_l[1]=22000;
    memset(&pf,0,sizeof pf);for(uint32_t k=0;k<NTRK;k++)pf.mg[k]=32768;
    perf_held=perf_latched=perf_solo=perf_kill=0;memset((void *)perf_motion,0,sizeof perf_motion);
    memset((void *)perf_k,0,sizeof perf_k);ep.on=0;
    ui.force=1;
    if(uxl.play)uxl_draw_play();else uxl_ring_build();
    ui.force=0;
}
static void path_acceptance_draw(int full)
{
    uint32_t before=host_blit_rows;assert(!ui.force);
    if(uxl.play)uxl_draw_play();else uxl_ring_build();
    if(host_blit_rows-before!=(full?(uxl.play?240u:212u):0u))
        fprintf(stderr,"redraw mismatch play=%u page=%u sel=%u want=%d rows=%u gate=%u state=%u mute=%u mg=%d force=%u inter=%u\n",uxl.play,ui.page,song.sel,full,host_blit_rows-before,uxl_path_gate_sig(),uxl_stasis_source_state(),pf.mute,pf.mg[song.sel],ui.force,uxl.inter);
    assert(host_blit_rows-before==(full?(uxl.play?240u:212u):0u));
}
static void path_acceptance_pixels(int track_on,int writer_on,int grains_on)
{
    static const uint8_t paths[]={UXP_STS,UXP_REV,UXP_DRY,UXP_FEED,UXP_HALL,UXP_STR,UXP_CHO};
    static const uint8_t xs[]={38,28,28,196,219,201,40},ys[]={98,96,110,98,107,138,128};
    for(uint32_t p=0;p<7;p++) {
        int active=p==0?track_on&&writer_on:p==1||p==2||p==6?track_on:p==3||p==5?grains_on:1;
        uint32_t k=stviz_path_knob(&stviz_frame,paths[p]);
        uint16_t color=k==255u?(active?T_TEXT:T_DIM):active?uxl_path_col(k):ux_mix(uxl_path_col(k),T_BG,55);
        assert(host_screen[ys[p]*240u+xs[p]]==swap16(color));
    }
}
static void path_mixer_redraw_acceptance(void)
{
    palette_set(2);fm1_ms=1000;
    for(uint32_t context=0;context<4;context++)for(uint32_t selected=0;selected<4;selected++)
    for(uint32_t mode=0;mode<4;mode++) {
        path_acceptance_setup(context,selected);
        uint32_t pair=(selected+1u)%4u;
        if(mode==3){ep.on=1;ep.a=selected;ep.b=pair;ui.force=1;if(uxl.play)uxl_draw_play();else uxl_ring_build();ui.force=0;}
        path_acceptance_draw(0);path_acceptance_pixels(1,1,1);
        if(mode==0)trk[selected].p[P_MUTE]=1;
        if(mode==1)perf_held=PF_BIT(PF_M1+selected);
        if(mode==2)perf_solo=(uint8_t)(15u&~(1u<<selected));
        if(mode==3){ep.on=1;ep.a=selected;ep.b=pair;trk[pair].p[P_MUTE]=1;}
        assert(perf_begin(1));assert(pf.mute&(1u<<selected));
        /* Existing PLAY mute badges invalidate once at the command edge. */
        path_acceptance_draw(uxl.play&&(mode==0||mode==3));path_acceptance_pixels(1,1,1);
        /* Both ordinary and SPREAD mixer ramps are the real production ones. */
        for(uint32_t k=0;k<4;k++)if(pf.mute&(1u<<k)) {
            int32_t b[127],sd[127];for(uint32_t i=0;i<127;i++)b[i]=sd[i]=65536;
            if(mode&1u)perf_mute_sd(k,b,sd,127);else perf_mute(k,b,127);
        }
        assert(pf.mg[selected]>0);path_acceptance_draw(0);path_acceptance_pixels(1,1,1);
        for(uint32_t k=0;k<4;k++)if(pf.mute&(1u<<k)) {
            int32_t b=65536,sd=65536;if(mode&1u)perf_mute_sd(k,&b,&sd,1);else perf_mute(k,&b,1);
            assert(!b && (!(mode&1u)||!sd));
        }
        assert(!pf.mg[selected]);path_acceptance_draw(1);path_acceptance_pixels(0,1,1);path_acceptance_draw(0);
        for(uint32_t k=0;k<4;k++)if(k!=selected && !(mode==3 && k==pair)) {
            assert(pf.mg[k]==32768);song.sel=k;stviz_view_t v;assert(stviz_snapshot(&v));
            assert(stviz_path_value(&v,UXP_CHO,64)==64);song.sel=selected;
        }
        if(mode==3)assert(!pf.mg[pair]);
        trk[selected].p[P_MUTE]=trk[pair].p[P_MUTE]=0;perf_held=perf_solo=0;assert(perf_begin(1));
        for(uint32_t k=0;k<4;k++)if(pf.mute&(1u<<k)) {
            int32_t b=65536,sd=65536;if(mode&1u)perf_mute_sd(k,&b,&sd,1);else perf_mute(k,&b,1);assert(b>0);
        }
        path_acceptance_draw(1);path_acceptance_pixels(1,1,1);path_acceptance_draw(0);
        /* Read-only UI helper never changes audio state, even on repeated reads. */
        typeof(st) saved_st=st;typeof(pf) saved_pf=pf;
        for(uint32_t i=0;i<16;i++)(void)uxl_path_gate_sig();
        assert(!memcmp(&st,&saved_st,sizeof st)&&!memcmp(&pf,&saved_pf,sizeof pf));
    }
    puts("F3 acceptance: PLAY SENDS/TANK + BUILD FX/REVERB, T1-T4, persistent/performance/SOLO/Pair mute, real 128-sample mono/stereo ramps, isolated tracks, unforced binary redraw and no stable refresh PASS");
}
static void path_dsp_gate_assert(void)
{
    stviz_view_t v;assert(stviz_snapshot(&v));
    int grains=(st.gr_ready||st.grain_xfade)&&!(st.stopping?st.stop_gain<=0:st.releasing&&st.release_gain<=0);
    assert((stviz_path_value(&v,UXP_FEED,64)!=0)==grains);
    assert((stviz_path_value(&v,UXP_STR,64)!=0)==grains);
    assert((stviz_path_value(&v,UXP_STS,64)!=0)==!(st.auto_hold||st.stopping));
    assert(stviz_path_value(&v,UXP_HALL,64)==64);
}
static void path_dsp_step(void)
{
    int32_t bus[CTL],wet[CTL]={0};for(uint32_t i=0;i<CTL;i++)bus[i]=2048;
    stasis_block(bus,wet,CTL);path_dsp_gate_assert();
}
static void path_transport_acceptance(void)
{
    for(uint32_t context=0;context<4;context++) {
        path_acceptance_setup(context,context);stasis_transport_stop();assert(st.stopping&&st.stop_gain>0);
        path_acceptance_draw(1);path_acceptance_pixels(1,0,1);
        uint32_t guard=0;
        while(st.stop_gain>0){path_dsp_step();assert(++guard<20000u);}
        path_acceptance_draw(1);path_acceptance_pixels(1,0,0);path_acceptance_draw(0);
        stasis_transport_start();assert(!st.gr_ready && !st.grain_xfade);assert(uxl_stasis_source_state()==UXL_ST_FILL);
        path_acceptance_draw(1);path_acceptance_pixels(1,1,0);
        guard=0;while(!st.gr_ready){path_dsp_step();assert(++guard<10000u);}
        path_acceptance_draw(1);path_acceptance_pixels(1,1,1);
        song.g[G_DRLVL]=mod_stasis.capt=1;path_dsp_step();assert(st.auto_hold);assert(uxl_stasis_source_state()==UXL_ST_FROZEN);
        path_acceptance_draw(1);path_acceptance_pixels(1,0,1);
        song.g[G_DRLVL]=mod_stasis.capt=0;path_dsp_step();assert(st.releasing);assert(uxl_stasis_source_state()==UXL_ST_RELEASE);
        path_acceptance_draw(1);path_acceptance_pixels(1,0,1);
        guard=0;while(st.releasing){path_dsp_step();assert(++guard<10000u);}
        path_acceptance_draw(1);path_acceptance_pixels(1,1,!!(st.gr_ready||st.grain_xfade));
        while(!st.gr_ready){path_dsp_step();assert(++guard<10000u);}
        stasis_new_source_request();path_dsp_step();assert(st.new_source&&st.releasing);assert(uxl_stasis_source_state()==UXL_ST_NEW);
        path_acceptance_draw(1);path_acceptance_pixels(1,1,1);
        guard=0;while(st.new_source){path_dsp_step();assert(++guard<10000u);}
        assert(!st.gr_ready);path_acceptance_draw(1);path_acceptance_pixels(1,1,!!st.grain_xfade);
        /* Real fresh capture arms while the writer fills, then freezes. */
        stasis_transport_start();song.g[G_DRLVL]=mod_stasis.capt=1;path_dsp_step();assert(st.cap_pending);
        assert(uxl_stasis_source_state()==UXL_ST_ARM);path_acceptance_draw(1);path_acceptance_pixels(1,1,0);
        guard=0;while(!st.auto_hold){path_dsp_step();assert(++guard<10000u);}
        assert(uxl_stasis_source_state()==UXL_ST_FROZEN);path_acceptance_draw(1);
        /* WAIT is a region-planner result: explicitly test no heads, and a surviving crossfade. */
        st.auto_hold=st.stopping=st.releasing=st.cap_pending=st.gr_ready=0;st.grain_xfade=0;st.region_status=STREG_WAIT;
        assert(uxl_stasis_source_state()==UXL_ST_WAIT);path_acceptance_draw(1);path_acceptance_pixels(1,1,0);
        st.grain_xfade=1;path_acceptance_draw(1);path_acceptance_pixels(1,1,1);
        mod_stasis.strength=0;path_acceptance_draw(1);path_acceptance_pixels(1,1,0);path_acceptance_draw(0);
    }
    puts("F3 DSP/UI transitions: actual STOP fade/end, START/FILL/LIVE, FREEZE/RELEASE, NEW, ARM; injected planner WAIT/crossfade, STR0 with FEED/HALL300%, HALL independent PASS");
}

/* Test-only negative control: old constant-width renderer fails the max
 * third-pixel assertion. Expectations independent of production helper. */
static void stviz_intensity_regression(void)
{
    static const uint8_t paths[]={UXP_STS,UXP_REV,UXP_DRY,UXP_FEED,UXP_HALL,UXP_STR,UXP_CHO};
    static const uint8_t values[]={0,1,42,43,63,64,84,85,95,96,127};
    typeof(ux) keep=ux;
    for(uint32_t pal=0;pal<4;pal++) {
        palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
        for(uint32_t pg=1;pg<=2;pg++)for(uint32_t p=0;p<NELEM(paths);p++)
        for(uint32_t j=0;j<NELEM(values);j++)for(uint32_t hot=0;hot<2;hot++) {
            stviz_view_t v={0};v.pgm=pg;v.layer=127;
            uint32_t val=values[j],path=paths[p],k=stviz_path_knob(&v,path);
            uint32_t w=(path==UXP_FEED||path==UXP_HALL)?(val<64?1:val<96?2:3):(val<43?1:val<85?2:3);
            uint16_t ink=swap16(uxl_path_ink(k,val!=0));
            cv_begin(240,120,T_BG);stviz_pathline(&v,path,val,hot,80,50,93,50,0);cv_blit(0,0);
            assert(host_screen[50u*240u+80u]==ink);
            assert(host_screen[51u*240u+80u]==(w>=2?ink:swap16(T_BG)));
            assert(host_screen[49u*240u+80u]==(w>=3?ink:swap16(T_BG)));
            v.blocked=UXP_GATE_TRACK|UXP_GATE_RECORD|UXP_GATE_GRAINS;
            if(path!=UXP_HALL) {
                cv_begin(240,120,T_BG);stviz_pathline(&v,path,val,hot,80,50,93,50,0);cv_blit(0,0);
                assert(host_screen[50u*240u+80u]==swap16(uxl_path_ink(k,0)));
                assert(host_screen[49u*240u+80u]==swap16(T_BG)&&host_screen[51u*240u+80u]==swap16(T_BG));
            }
        }
    }
    /* Maximal actual route geometry, every thumb/shadow position. */
    static const int32_t start[]={277,7,187,97};
    static const uint8_t focus[]={STVIZ_HOT_NONE,STVIZ_HOT_CHO,STVIZ_HOT_SEND,STVIZ_HOT_REV,STVIZ_HOT_FEED,STVIZ_HOT_DRY,STVIZ_HOT_HALL};
    static const uint8_t widths[]={1,64,127};
    for(uint32_t pal=0;pal<4;pal++) {
    palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
    for(uint32_t pg=1;pg<=2;pg++)for(uint32_t h=0;h<NELEM(focus);h++)for(uint32_t w=0;w<NELEM(widths);w++){
        stviz_view_t v={0};v.pgm=pg;v.hot=focus[h];
        v.send=v.rsend=v.dry=v.feed=v.hall=v.chorus=v.layer=widths[w];
        path_test_frame(&v,0);
        for(uint32_t k=0;k<4;k++)for(uint32_t val=0;val<128;val++){
            int32_t x,y;uxl_ring_pt(start[k]+(int32_t)(val*76u/127u),61,&x,&y);
            for(int32_t part=0;part<2;part++)for(int32_t dy=-8;dy<=8;dy++)for(int32_t dx=-8;dx<=8;dx++){
                int32_t r=part?7:8;if(dx*dx+dy*dy>r*r+r/2)continue;
                assert(host_screen[(y+dy+2*part)*240+x+dx+part]==swap16(T_BG));
            }
        }
    }
    }
    ux=keep;
}
static void stviz_path_regression(uint8_t rev_hot)
{
    static const uint8_t paths[]={UXP_STS,UXP_REV,UXP_DRY,UXP_FEED,UXP_HALL,UXP_STR,UXP_CHO};
    static const uint8_t xs[]={38,28,28,196,219,201,40},ys[]={98,96,110,98,107,138,128};
    static const uint8_t knobs[2][7]={{2,3,255,255,255,255,1},{255,255,2,1,3,255,255}};
    typeof(ux) saved_ux=ux;
    for(uint32_t pal=0;pal<4;pal++) {
        palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
        for(uint32_t pg=1;pg<=2;pg++)for(uint32_t state=0;state<=UXL_ST_LIMIT;state++)
            for(uint32_t enabled=0;enabled<2;enabled++)for(uint32_t str=0;str<2;str++) {
                stviz_view_t v={0};v.pgm=pg;v.source_state=state;
                v.send=v.rsend=v.dry=v.feed=v.hall=v.chorus=enabled?64:0;v.layer=str;
                /* Synthetic steady states: fade transitions tested separately. */
                if(state==UXL_ST_STOP || state==UXL_ST_FROZEN || state==UXL_ST_RELEASE) v.blocked|=UXP_GATE_RECORD;
                if(state==UXL_ST_STOP || state==UXL_ST_WAIT || state==UXL_ST_FILL || state==UXL_ST_NEW) v.blocked|=UXP_GATE_GRAINS;
                path_test_frame(&v,1);
                for(uint32_t p=0;p<7;p++) {
                    uint32_t active=p==5?str:enabled;
                    if(p==0 && (state==UXL_ST_STOP || state==UXL_ST_FROZEN || state==UXL_ST_RELEASE))active=0;
                    if((p==3 || p==5) && (!str || state==UXL_ST_STOP || state==UXL_ST_WAIT || state==UXL_ST_FILL || state==UXL_ST_NEW))active=0;
                    uint32_t k=knobs[pg-1][p];
                    uint16_t on=k==255u?T_TEXT:k==1u?T_ACCENT:k==2u?T_THEME:T_TEXT;
                    uint16_t off=k==255u?T_DIM:ux_mix(on,T_BG,55);
                    assert(on!=off); /* GREY/MONO retain an active/off cue. */
                    assert(stviz_path_knob(&v,paths[p])==k);
                    assert(host_screen[ys[p]*240u+xs[p]]==swap16(active?on:off));
                }
                memcpy(stviz_before,host_screen,sizeof stviz_before);
                stviz_partial(&v);
                assert(!memcmp(stviz_before,host_screen,sizeof stviz_before));
            }
    }
    /* Independent pixel masks and thumb/shadow clearance in all four palettes/styles. */
    for(uint32_t pal=0;pal<4;pal++) {
    palette_set(pal==0||pal==3?2u:pal==1?0u:9u);ux.style=pal==3?ST_LINE:ST_FLAT;
    stviz_view_t v={0};v.pgm=1;v.layer=1;v.source_state=UXL_ST_LIVE;
    for(uint32_t hot=0;hot<2;hot++) {
        v.hot=hot?STVIZ_HOT_DRY:STVIZ_HOT_NONE;v.dry=v.chorus=0;
        path_test_frame(&v,0);memcpy(stviz_before,host_screen,sizeof stviz_before);
        v.dry=127;path_test_frame(&v,0);
        for(uint32_t i=0;i<240u*240u;i++)stviz_expected[i]=host_screen[i]!=stviz_before[i];
        v.dry=v.chorus=0;v.hot=hot?STVIZ_HOT_CHO:STVIZ_HOT_NONE;path_test_frame(&v,0);
        memcpy(stviz_before,host_screen,sizeof stviz_before);
        v.chorus=127;path_test_frame(&v,0);
        for(uint32_t i=0;i<240u*240u;i++)
            if(stviz_expected[i])assert(host_screen[i]==stviz_before[i]);
    }
    /* All actual arrow/label pixels clear every thumb and shadow setting. */
    static const int32_t start[4]={277,7,187,97};
    const uint8_t hot[]={STVIZ_HOT_NONE,STVIZ_HOT_CHO,STVIZ_HOT_SEND,STVIZ_HOT_DRY,STVIZ_HOT_FEED,STVIZ_HOT_HALL,rev_hot};
    for(uint32_t pg=1;pg<=2;pg++)for(uint32_t h=0;h<NELEM(hot);h++) {
        v.pgm=pg;v.hot=hot[h];v.dry=v.chorus=v.send=v.rsend=v.feed=v.hall=64;
        path_test_frame(&v,1);memcpy(stviz_before,host_screen,sizeof stviz_before);
        stviz_partial(&v);assert(!memcmp(stviz_before,host_screen,sizeof stviz_before));
        path_test_frame(&v,0);
        for(uint32_t k=0;k<4;k++)for(uint32_t val=0;val<128;val++) {
            int32_t x,y;uxl_ring_pt(start[k]+(int32_t)(val*76u/127u),61,&x,&y);
            for(int32_t part=0;part<2;part++)for(int32_t dy=-8;dy<=8;dy++)for(int32_t dx=-8;dx<=8;dx++) {
                int32_t r=part?7:8;if(dx*dx+dy*dy>r*r+r/2)continue;
                assert(host_screen[(y+dy+2*part)*240+x+dx+part]==swap16(T_BG));
            }
        }
    }
    }
    /* Mixer gate transitions are exercised by stviz_gate_regression. */
    ux=saved_ux;
    puts("STASIS paths: seven ways x nine states x four palettes/styles, STR/FEED gating, CHO/DRY separation, right STR, REV hot, thumb clearance and full/strip parity PASS");
}
#endif
static void stviz_ui_regression(void)
{
    uint32_t i,grp,blits;
    uint16_t background;
    uxl.play=1u;uxl.area=UXL_SPACE;
    uxl.cap_view=uxl.lvl=uxl.pv=0u;uxl.hot_t=0u;
    ui.force=1u;ui.msg_t=0u;
    song.g[G_RTYPE]=3u; song.playing=1u;
    st.w=32768u;st.valid=ST_RING_LEN;st.phase=100u;
    st.active_quality=0u;
#if CARAVEL_STASIS2
    st.active_grain=2u;   /* 93 ms (index 1 was 93 ms before the 23 ms step): 2048 phase steps are not a whole grain */
#else
    st.active_grain=1u;
#endif
    st.auto_hold=st.releasing=st.stopping=st.new_source=0u;
    st.gr_ready=1u;st.region_status=STREG_FIT;
    st.region_young=256u;st.region_length=28000u;
    st.gr_l[0]=12000u;st.gr_l[1]=22000u;
    st.reverse_mask=0u;
    for(i=0u;i<32u;i++)st_scope_peak[i]=0u;
    st_scope_peak[8]=40u;st_scope_peak[22]=72u;
    for(grp=0u;grp<5u;grp++){
        uxl.grp[UXL_SPACE]=(uint8_t)grp;
        assert(stviz_enabled()); /* SENDS, FILTER, TEXTURE, REGION, TANK */
    }
    song.g[G_RTYPE]=2u;assert(!stviz_enabled());
    song.g[G_RTYPE]=3u;uxl.cap_view=1u;assert(!stviz_enabled());
    uxl.cap_view=0u;uxl.grp[UXL_SPACE]=0u;
    /* C5's four timer ticks are visible in the scope, but the main
     * C5 physical legend owns the gesture and is not rate-limited. */
    stviz_view_t pending={0};
    uxl_new_held=1u;uxl_new_fired=0u;uxl_new_step=2u;
    assert(stviz_snapshot(&pending));
    assert(pending.new_pending && pending.new_step==2u);
    uxl_new_held=uxl_new_step=0u;
    /* FREEZE halts the source writer, NOT either audible grain reader.
     * Their real read addresses must move with phase while the held source
     * peaks remain unchanged. */
    st.auto_hold=1u;st.phase=100u;
    stviz_view_t frozen0={0},frozen1={0};
    assert(stviz_snapshot(&frozen0) && frozen0.frozen);
    st.phase+=2048u;
    assert(stviz_snapshot(&frozen1) && frozen1.frozen);
    assert(frozen0.read[0]!=frozen1.read[0]);
    assert(!memcmp(frozen0.peak,frozen1.peak,sizeof frozen0.peak));
    st.auto_hold=0u;st.phase=100u;
    stviz_has_last=stviz_has_time=0u;
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE && !FELUCCA_FM4
    {   uint32_t saved_ms=fm1_ms,saved_hint=uxl.hint_until;
        uxl.hint_until=0;uxl_path_hint_seen=0;fm1_ms=1000u;
        uxl_path_hint_tick();assert(uxl_path_help());
        fm1_ms=6000u;assert(!uxl_path_help());
        uxl_path_hint_tick();assert(!uxl_path_help());
        fm1_ms=saved_ms;uxl.hint_until=saved_hint;
        uxl_path_hint_seen=0;uxl_path_hint_until=0;
    }
#endif
    uxl_draw_play();
    assert(stviz_has_last);
    memcpy(stviz_before,host_screen,sizeof stviz_before);
    background=swap16(T_BG);
    assert(host_screen[194u*240u+120u]!=background); /* MIDI band at y190..200 */
    assert(host_screen[188u*240u+67u]==background && host_screen[190u*240u+67u]!=background);
    assert(host_screen[200u*240u+67u]!=background && host_screen[201u*240u+67u]==background);
    for(i=201u;i<=202u;i++) assert(host_screen[i*240u+120u]==background); /* clear rows above message (y203) */
    {   /* Conservative thumb/ELASTIC envelope, all 512 settings: no arrow or band collision. */
        static const int32_t start[4]={277,7,187,97};
        uint32_t k,v;int32_t x,y;
        assert(uxl_orbit_radius()==61);
        for(k=0;k<4u;k++)for(v=0;v<128u;v++){
            uxl_ring_pt(start[k]+(int32_t)(v*76u/127u),uxl_orbit_radius(),&x,&y);
            assert(x-9>49 && x+9<191);
            assert(y+10<=186 && y-9>=45);
            {   /* Exact disc and shadow pixels must be outside every partial LCD strip. */
                int32_t part,dx,dy;
                for(part=0;part<2;part++){
                    int32_t r=part?7:8;
                    for(dy=-r;dy<=r;dy++)for(dx=-r;dx<=r;dx++){
                        int32_t px=x+dx+part,py=y+dy+2*part;
                        if(dx*dx+dy*dy>r*r+r/2)continue;
                        if(py>=68 && py<168){
                            int32_t half=STVIZ_HALF[(py-68)/4];
                            assert(px<120-half || px>120+half);
                        }
                    }
                }
            }
        }
    }
    /* Only recorded audio, not MIDI CAPTURE, has changed. Full-frame
     * reference and the strip-wise partial redraw must be pixel-identical. */
    st_scope_peak[8]=96u;
    ui.force=1u;
    uxl_draw_play();
    memcpy(stviz_expected,host_screen,sizeof stviz_expected);
    memcpy(host_screen,stviz_before,sizeof stviz_before);
    blits=host_blit_rows;
    stviz_partial(&stviz_frame);
    assert(host_blit_rows-blits==(uint32_t)(STVIZ_STRIPS*STVIZ_STRIP_H));
    assert(!memcmp(host_screen,stviz_expected,sizeof stviz_expected));
    /* V7 elements: STR ring, MOTN dots, REVERSE arrows, both grain footprints, the region arc,
     * the writer cursor and every "hot" highlight must also be pixel-identical in the strips. */
    {
        static const uint8_t HOT[9][2]={{2,0},{2,1},{2,2},{2,3},{3,0},{3,1},{3,2},{4,1},{0,2}};   /* (0,2): SENDS K3, the STASIS send */
        uint32_t c,rv;
        song.g[G_DRREV]=100;song.g[G_RSIZE]=127;st.drift_ph=0x40000000u;
        for(rv=0u;rv<2u;rv++){
            st_expand.reverse=(int16_t)rv;
            for(c=0u;c<11u;c++){                           /* 9 hot states, REGION at rest (3), TANK at rest (4: the signal path) */
                uxl.hot_t=(uint8_t)(c<9u?4u:0u);
                if(c<9u){uxl.grp[UXL_SPACE]=HOT[c][0];uxl.hot_k=HOT[c][1];}
                else uxl.grp[UXL_SPACE]=c==9u?3u:4u;
                ui.force=1u;uxl_draw_play();
                memcpy(stviz_expected,host_screen,sizeof stviz_expected);
                memset(host_screen,0x5A,sizeof stviz_before);       /* nothing may rely on old pixels inside the scope */
                memcpy(host_screen,stviz_expected,sizeof stviz_before);
                for(i=0u;i<STVIZ_STRIPS;i++){                       /* wipe the strip rectangles, repaint only them */
                    uint32_t y0=STVIZ_STRIP_Y0+i*STVIZ_STRIP_H,x0;
                    for(x0=120u-STVIZ_HALF[i];x0<=120u+STVIZ_HALF[i];x0++){
                        uint32_t yy;for(yy=y0;yy<y0+STVIZ_STRIP_H;yy++)host_screen[yy*240u+x0]=0x1234;
                    }
                }
                stviz_partial(&stviz_frame);
                assert(!memcmp(host_screen,stviz_expected,sizeof stviz_expected));
            }
        }
        uxl.hot_t=0u;uxl.grp[UXL_SPACE]=0u;st_expand.reverse=0;
        song.g[G_DRREV]=0;
    }
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE && CARAVEL_STASIS2 && !FELUCCA_FM4
    {   /* the signal path beside the ring (SENDS and TANK), in the colours of the knobs: K2 cyan, K3 orange, K4 white; the ways of other pages grey, a way set to 0 its colour dimmed */
        typeof(ux) ux_keep=ux;
        uint16_t cy,og,wh,gr,ogd,whd;
        uint32_t px,py,lit=0u;
        palette_set(2u);                                                       /* AMBER: the hues of the knobs exist (the host's default palette has none) */
        cy=swap16(T_ACCENT);og=swap16(T_THEME);wh=swap16(T_TEXT);gr=swap16(T_DIM);
        ogd=swap16(ux_mix(T_THEME,T_BG,55));whd=swap16(ux_mix(T_TEXT,T_BG,55));
        uxl.hot_t=0u;
        TSEL->p[P_REV]=40;TSEL->p[P_SSEND]=40;TSEL->p[P_DRY]=127;st_hall_feed=64;st_hall_ret=64;mod_stasis.strength=64;
        uxl.grp[UXL_SPACE]=0u;ui.force=1u;uxl_draw_play();                    /* SENDS: STS = K3 orange, REV = K4 white */
        assert(host_screen[98u*240u+38u]==og);                                 /* Independent STS 40 into the ring */
        assert(host_screen[96u*240u+28u]==wh);                                 /* REV: the dotted hint */
        assert(host_screen[110u*240u+28u]==wh && host_screen[98u*240u+196u]==wh);   /* Active other-page paths are neutral bright. */
        {   uint32_t px2,py2,o3=0u;                                          /* the VALUE of STASIS (K3) in its knob's orange (its default is white), the label by the old rule */
            for(py2=171u;py2<186u;py2++)for(px2=4u;px2<30u;px2++){   /* (the text is anti-aliased: warm = red above blue) */
                uint16_t q=swap16(host_screen[py2*240u+px2]);
                o3+=((q>>11)&31u)>(q&31u)+8u;
            }
            assert(o3>5u);
        }
        uint8_t old_hot_k=uxl.hot_k;
        uxl.hot_k=3u;uxl.hot_t=4u;ui.force=1u;uxl_draw_play();
        assert(stviz_frame.pgm==1u);
        uint8_t rev_hot=stviz_frame.hot;
        stviz_rev_hot_pixels(rev_hot);
        uxl.hot_k=old_hot_k;uxl.hot_t=0u;
        TSEL->p[P_REV]=0;ui.force=1u;uxl_draw_play();
        assert(host_screen[96u*240u+28u]==whd && host_screen[98u*240u+38u]==og);    /* REV 0 leaves STS active */
        TSEL->p[P_REV]=40;TSEL->p[P_SSEND]=0;ui.force=1u;uxl_draw_play();
        assert(host_screen[98u*240u+38u]==ogd && host_screen[96u*240u+28u]==wh);     /* STS 0 does not dim REV */
        TSEL->p[P_SSEND]=127;
        uxl.grp[UXL_SPACE]=4u;ui.force=1u;uxl_draw_play();                    /* TANK: FEED = K2 cyan, DRY = K3 orange, HALL = K4 white */
        assert(host_screen[98u*240u+196u]==cy);
        int32_t saved_stop_gain=st.stop_gain;
        st.stopping=1u;st.stop_gain=0;ui.force=1u;uxl_draw_play();
        assert(host_screen[98u*240u+196u]==swap16(ux_mix(T_ACCENT,T_BG,55)));
        assert(host_screen[107u*240u+219u]==wh); /* independent REV/HALL */
        st.stopping=0u;st.stop_gain=saved_stop_gain;ui.force=1u;uxl_draw_play();
        assert(host_screen[110u*240u+28u]==og);
        assert(host_screen[107u*240u+219u]==wh);
        {   uint32_t px2,py2,c2=0u;                                          /* FEED's value (K2) in cyan: its default is white */
            for(py2=67u;py2<82u;py2++)for(px2=205u;px2<236u;px2++){
                uint16_t q=swap16(host_screen[py2*240u+px2]);
                c2+=(q&31u)>((q>>11)&31u)+8u;                                   /* cool = blue above red */
            }
            assert(c2>5u);
        }
        assert(host_screen[98u*240u+38u]==wh && host_screen[96u*240u+28u]==wh);      /* STS and REV: active, neutral on TANK. */
        TSEL->p[P_DRY]=0;ui.force=1u;uxl_draw_play();
        assert(host_screen[110u*240u+28u]==ogd);
        {   stviz_view_t v={0};v.layer=1;
            v.source_state=UXL_ST_STOP;v.blocked=UXP_GATE_RECORD|UXP_GATE_GRAINS;
            assert(!stviz_path_value(&v,UXP_STS,40) && !stviz_path_value(&v,UXP_FEED,64) && !stviz_path_value(&v,UXP_STR,64));
            assert(stviz_path_value(&v,UXP_REV,40)==40 && stviz_path_value(&v,UXP_DRY,127)==127 && stviz_path_value(&v,UXP_HALL,64)==64);
            v.source_state=UXL_ST_FROZEN;v.blocked=UXP_GATE_RECORD;
            assert(!stviz_path_value(&v,UXP_STS,40) && stviz_path_value(&v,UXP_FEED,64)==64);
            v.source_state=UXL_ST_ARM;v.blocked=0;assert(stviz_path_value(&v,UXP_STS,40)==40); /* ARM keeps recording until ready */
            v.source_state=UXL_ST_FILL;v.blocked=UXP_GATE_GRAINS;assert(!stviz_path_value(&v,UXP_FEED,64) && !stviz_path_value(&v,UXP_STR,64));
            v.source_state=UXL_ST_WAIT;assert(!stviz_path_value(&v,UXP_FEED,64));
        }
        for(py=91u;py<106u;py++)for(px=205u;px<234u;px++)lit+=host_screen[py*240u+px]!=swap16(T_BG);
        assert(lit>20u);                                                       /* the word TANK on the right */
        TSEL->p[P_REV]=12;TSEL->p[P_SSEND]=127;TSEL->p[P_DRY]=127;uxl.grp[UXL_SPACE]=0u;
        ux=ux_keep;ui.force=1u;
        stviz_gate_regression();
        stviz_style_regression();
        stviz_intensity_regression();
        stviz_path_regression(rev_hot);
    }
#endif
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    {   /* Each new control must change actual rendered pixels even frozen.
         * Test endpoint differences and full/partial equality, not only
         * the existence of snapshot fields. */
        static const uint8_t GROUP[4]={4,4,4,2}, KNOB[4]={1,3,2,3};   /* TANK K2 FEED, K4 HALL, K3 DRY; TEXTURE K4 TONE */
        static const uint8_t HOT[4]={STVIZ_HOT_FEED,STVIZ_HOT_HALL,STVIZ_HOT_DRY,STVIZ_HOT_TONE};
        uint8_t feed_keep=st_hall_feed,hall_keep=st_hall_ret;
        int16_t dry_keep=TSEL->p[P_DRY],tone_keep=stasis_tone;
        uint8_t hold_keep=st.auto_hold;
        uint32_t k;
        st.auto_hold=1u;
        for(k=0u;k<4u;k++){
            uxl.grp[UXL_SPACE]=GROUP[k];uxl.hot_k=KNOB[k];uxl.hot_t=4u;
            if(k==0u)st_hall_feed=0u;
            if(k==1u)st_hall_ret=0u;
            if(k==2u)TSEL->p[P_DRY]=0;
            if(k==3u)stasis_tone=0;
            ui.force=1u;uxl_draw_play();
            assert(stviz_frame.hot==HOT[k]);
            memcpy(stviz_before,host_screen,sizeof stviz_before);
            if(k==0u)st_hall_feed=127u;
            if(k==1u)st_hall_ret=127u;
            if(k==2u)TSEL->p[P_DRY]=127;
            if(k==3u)stasis_tone=127;
            uxl.hot_t=4u;ui.force=1u;uxl_draw_play();
            memcpy(stviz_expected,host_screen,sizeof stviz_expected);
            /* Compare just the center, not the changed outer knob. */
            assert(memcmp(&stviz_before[129u*240u+105u],&stviz_expected[129u*240u+105u],30u*sizeof(uint16_t))!=0 ||
                   k==3u);
            if(k==3u)
                assert(memcmp(&stviz_before[132u*240u+107u],&stviz_expected[132u*240u+107u],27u*sizeof(uint16_t))!=0);
            /* Keep the new outer controls; erase every scope rectangle. */
            for(i=0u;i<STVIZ_STRIPS;i++){
                uint32_t yy,x,y0=STVIZ_STRIP_Y0+i*STVIZ_STRIP_H;
                for(yy=y0;yy<y0+STVIZ_STRIP_H;yy++)
                    for(x=120u-STVIZ_HALF[i];x<=120u+STVIZ_HALF[i];x++)
                        host_screen[yy*240u+x]=0x1234;
            }
            stviz_partial(&stviz_frame);
            assert(!memcmp(host_screen,stviz_expected,sizeof stviz_expected));
        }
        st_hall_feed=feed_keep;st_hall_ret=hall_keep;TSEL->p[P_DRY]=dry_keep;stasis_tone=tone_keep;
        st.auto_hold=hold_keep;
        /* Distinct edit focus for REGION controls and actual full/strip parity. */
        uxl.grp[UXL_SPACE]=3u;
        { static const uint8_t focus[]={STVIZ_HOT_POS,STVIZ_HOT_LEN,STVIZ_HOT_REVERSE};
          for(k=0;k<3;k++){
            uxl.hot_k=k;uxl.hot_t=4u;ui.force=1u;uxl_draw_play();
            assert(stviz_frame.hot==focus[k]);
            memcpy(stviz_before,host_screen,sizeof stviz_before);
            stviz_partial(&stviz_frame);
            assert(!memcmp(stviz_before,host_screen,sizeof stviz_before));
          }
        }
        uxl.hot_t=0u;uxl.grp[UXL_SPACE]=0u;
    }
#endif
    puts("STASIS V7 scope: STR ring, MOTN dots, footprints, region arc and every hot highlight pixel-identical in the strips");
    ui.force=0u;
    /* Background V6 must NEVER stall the main UI on an active DMA.
     * Software lcd_busy can remain set after DMA has actually finished:
     * the production lcd_scope_ready() probes hardware status before
     * retiring an already-completed transaction. */
    stviz_last=stviz_frame;stviz_has_last=stviz_has_time=1u;
    stviz_last_ms=1000u;fm1_ms=1200u;
    st_scope_peak[8]=0u;
    host_scope_ready=0u;blits=host_blit_rows;
    stviz_tick();
    assert(host_blit_rows==blits && stviz_last_ms==1000u);
    host_scope_ready=1u;
    stviz_tick();
    assert(host_blit_rows-blits==(uint32_t)(STVIZ_STRIPS*STVIZ_STRIP_H) && stviz_last_ms==1200u);
    blits=host_blit_rows;
    stviz_tick();
    assert(host_blit_rows==blits); /* not another blit before 167 ms */
    puts("STASIS V6 real-scope PLAY: five pages, lower CAP notes, pixel-identical strip redraw and DMA busy skip PASS");
}
#endif

#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
/* Host framebuffer proof that BUILD reuses the four existing PLAY orbit
 * quadrants, never the old GR_STGRAIN text panel. Test both source pages
 * under STASIS2; the compile flags keep the standard build untouched. */
static void build_fx_orbit_regression(void)
{
    static const char *const NAMES[] = {"TEXTURE", "REGION", "REVERB", "SETUP"};   /* PLAY's TEXTURE, REGION, TANK and what PLAY leaves out */
    uint32_t j,i;
    int32_t x[4],y[4];
    static const int16_t ANG[4]={315,45,225,135};
    uint32_t k;
    uxl.play=0u; uxl.cap_view=uxl.lvl=uxl.pv=0u;
    ui.home=ui.menu=ui.confirm=ui.uboot=ui.layer=0u;
    ui.msg_t=0u; ui.force=1u;
    song.g[G_RTYPE]=3u;
    st.w=32768u; st.valid=ST_RING_LEN; st.phase=100u;
    st.active_quality=0u; st.active_grain=ST_GRAIN_DEFAULT; /* 186 ms */
    st.auto_hold=st.releasing=st.stopping=st.new_source=0u;
    st.gr_ready=1u; st.region_status=STREG_FIT;
    st.region_young=256u; st.region_length=28000u;
    st.gr_l[0]=12000u; st.gr_l[1]=22000u;
#if defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
    for(i=0u;i<32u;i++)st_scope_peak[i]=0u;
    st_scope_peak[8]=40u; st_scope_peak[22]=72u;
#endif
    for(k=0u;k<4u;k++) {
        uxl_ring_pt(ANG[k],61,&x[k],&y[k]);
        assert(x[k]>0 && x[k]<240 && y[k]>28 && y[k]<198);
    }
    for(j=0u;j<4u;j++) {
        uint8_t found=0u;
        for(i=0u;i<NPAGES;i++)
            if(str_eq(PAGES[i].title,NAMES[j])) {
                ui.page=(uint8_t)i;found=1u;break;
            }
        assert(found && page_visible(ui.page));
        ui.force=1u;ui.graph_sig=~0u;
        assert(uxl_ring_page());
        uxl_ring_build();
        for(k=0u;k<4u;k++)
            if(!(j==3u && k==3u))   /* SETUP's K4 is empty: no arc there */
                assert(host_screen[(uint32_t)y[k]*240u+(uint32_t)x[k]]!=swap16(T_BG));
#if defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
        assert(stviz_enabled() && stviz_has_last);
        if(j==3u) {   /* HISTORY is K3 on SETUP. */
            uint8_t qkeep=st.active_quality;
            ui.hot_col=2u;ui.hot_t=4u;
            st.active_quality=0u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_HISTORY);
            memcpy(stviz_before,host_screen,sizeof stviz_before);
            st.active_quality=2u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.quality==2u);
            assert(memcmp(&stviz_before[116u*240u+95u],&host_screen[116u*240u+95u],8u*sizeof(uint16_t))!=0);
            st.active_quality=qkeep;ui.hot_t=0u;
            ui.force=1u;uxl_ring_build();
        }
        if(j==2u) {
            /* The REVERB page under STASIS is PLAY's TANK (TYPE, FEED, DRY, HALL) with the same scope and the same hot elements. */
            ui.hot_col=1u;ui.hot_t=4u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_FEED);
            ui.hot_col=2u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_DRY);
            ui.hot_col=3u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_HALL);
            st.reverse_mask=1u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.rev==1u);
            st.reverse_mask=2u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.rev==2u);
            st.reverse_mask=0u;ui.hot_t=0u;
            st.cap_pending=1u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.source_state==UXL_ST_ARM);
            st.cap_pending=0u;st.releasing=1u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.source_state==UXL_ST_RELEASE);
            st.releasing=0u;ui.force=1u;uxl_ring_build();
        }
        if(j==0u) {   /* TEXTURE: K2 STR, K3 MOTN, K4 TONE light the elements PLAY lights */
            ui.hot_t=4u;
            ui.hot_col=1u;ui.force=1u;uxl_ring_build();assert(stviz_frame.hot==STVIZ_HOT_STR);
            ui.hot_col=2u;ui.force=1u;uxl_ring_build();assert(stviz_frame.hot==STVIZ_HOT_MOTN);
            ui.hot_col=3u;ui.force=1u;uxl_ring_build();assert(stviz_frame.hot==STVIZ_HOT_TONE);
            ui.hot_t=0u;ui.force=1u;uxl_ring_build();
        }
        if(j==0u) {
            static uint16_t previous[240u*240u],expected[240u*240u];
            memcpy(previous,host_screen,sizeof previous);
            st_scope_peak[8]=96u;ui.force=1u;
            uxl_ring_build();
            memcpy(expected,host_screen,sizeof expected);
            memcpy(host_screen,previous,sizeof previous);
            stviz_partial(&stviz_frame);
            assert(!memcmp(host_screen,expected,sizeof expected));
            st_scope_peak[8]=40u;
        }
#endif
    }
    {   /* Runtime hall gain is not a normalized encoder percentage. */
        char gain[12];
        uxl_stasis_gain_fmt(gain,sizeof gain,0);assert(str_eq(gain,"0%"));
        uxl_stasis_gain_fmt(gain,sizeof gain,64);assert(str_eq(gain,"100%"));
        uxl_stasis_gain_fmt(gain,sizeof gain,127);assert(str_eq(gain,"300%"));
        st.cap_pending=1u;assert(str_eq(uxl_stasis_source_label(),"ARM"));
        st.cap_pending=0u;st.auto_hold=1u;assert(str_eq(uxl_stasis_source_label(),"FROZEN"));
        st.releasing=1u;assert(str_eq(uxl_stasis_source_label(),"RELEASE"));
        st.new_source=1u;assert(str_eq(uxl_stasis_source_label(),"NEW"));
        st.new_source=st.releasing=st.auto_hold=0u;
        {   /* The HISTORY lock must mirror the actual BUILD edit gate. */
            uint32_t saved_capt=mod_stasis.capt;
            int16_t saved_request=song.g[G_DRLVL];
            song.g[G_DRLVL]=0;mod_stasis.capt=0u;
            assert(!uxl_stasis_history_locked());
            st.auto_hold=1u;assert(uxl_stasis_history_locked());
            st.auto_hold=0u;st.cap_pending=1u;assert(uxl_stasis_history_locked());
            st.cap_pending=0u;st.releasing=1u;assert(uxl_stasis_history_locked());
            st.releasing=0u;mod_stasis.capt=1u;assert(uxl_stasis_history_locked());
            mod_stasis.capt=0u;song.g[G_DRLVL]=1;assert(uxl_stasis_history_locked());
            mod_stasis.capt=saved_capt;song.g[G_DRLVL]=saved_request;
        }
        song.g[G_RTYPE]=2u;assert(!uxl_stasis_build_page(cur_page()));
#if defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
        assert(!stviz_enabled());
#endif
        song.g[G_RTYPE]=3u;
    }
#if CARAVEL_STASIS2 && !FELUCCA_FM4
    {   /* the STASIS send takes the delay's slot (SENDS K3 in PLAY, FX slot 3 in BUILD) while the reverb is STASIS; the stored DLY stays */
        uxl_tg_t tg[4];
        uint32_t pg_fx=NPAGES,pi;
        int16_t *vp=0,dly=trk[0].p[P_DLY];
        for(pi=0u;pi<NPAGES;pi++)if(PAGES[pi].graph==GR_FX){pg_fx=pi;break;}
        assert(pg_fx<NPAGES);
        song.g[G_RTYPE]=3u;
        uxl_group(UXL_SPACE,0u,tg);
        assert(tg[0].id==P_DIST && tg[1].id==P_CHOR && tg[2].id==P_SSEND && tg[3].id==P_REV);
        assert(page_desc(&PAGES[pg_fx],2u,&vp)==&TP[P_SSEND] && vp==&TSEL->p[P_SSEND]);
        assert(!fx_delay_suspended(&PAGES[pg_fx],2u));
#if defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
        {   uint8_t old_page=ui.page;
            ui.page=(uint8_t)pg_fx;ui.hot_col=2u;ui.hot_t=4u;ui.force=1u;
            assert(stviz_enabled());uxl_ring_build();
            assert(stviz_frame.pgm==1u && stviz_frame.hot==STVIZ_HOT_SEND);
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
            ui.hot_col=3u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.pgm==1u);
            stviz_rev_hot_pixels(stviz_frame.hot);
            ui.hot_col=2u;
#endif
            ui.page=old_page;ui.hot_t=0u;
        }
#endif
        song.g[G_RTYPE]=2u;
        uxl_group(UXL_SPACE,0u,tg);
        assert(tg[2].id==P_DLY);
        assert(page_desc(&PAGES[pg_fx],2u,&vp)==&TP[P_DLY]);
        assert(trk[0].p[P_DLY]==dly);                    /* switching the type never touches the delay's value */
        {   char vb[8];const char *un;
            TSEL->p[P_REV]=40;param_format(&TP[P_SSEND],127,vb,&un);assert(str_eq(vb,"100")&&str_eq(un,"%"));   /* absolute maximum */
            param_format(&TP[P_SSEND],64,vb,&un);assert(str_eq(vb,"50"));
            TSEL->p[P_REV]=40;assert(param_turn(&TP[P_SSEND],127,-1)==126);   /* ordinary numeric step */
            TSEL->p[P_REV]=127;assert(param_turn(&TP[P_SSEND],127,-1)==126);
            assert(param_turn(&TP[P_SSEND],126,1)==127);                      /* absolute maximum again */
            assert(TP[P_SSEND].def==0);                  /* independent fresh send */
        }
        song.g[G_RTYPE]=3u;
    }
#endif
#if CARAVEL_STASIS2 && CARAVEL_STASIS_EXPAND && !FELUCCA_FM4
    {   /* BUILD's STASIS pages carry PLAY's targets at PLAY's knobs: TEXTURE = GRAIN STR MOTN TONE, REGION = POS LENGTH REVERSE SOURCE,
         * TANK (the REVERB page) = TYPE FEED DRY HALL, SETUP = TYPE CAPT HISTORY */
        uint32_t pt=NPAGES,pr=NPAGES,pv=NPAGES,ps=NPAGES,pi;
        int16_t *vp=0;char vb[8];const char *un;
        for(pi=0u;pi<NPAGES;pi++){
            if(str_eq(PAGES[pi].title,"TEXTURE"))pt=pi;
            if(str_eq(PAGES[pi].title,"REGION"))pr=pi;
            if(str_eq(PAGES[pi].title,"SETUP"))ps=pi;
            if(PAGES[pi].graph==GR_NONE&&PAGES[pi].id[0]==G_RTYPE&&str_eq(PAGES[pi].title,"REVERB"))pv=pi;
        }
        assert(pt<NPAGES&&pr<NPAGES&&pv<NPAGES&&ps<NPAGES);
        song.g[G_RTYPE]=3u;
        assert(page_desc(&PAGES[pt],0u,&vp)==&ST_GRAIN_DESC[0] && vp==&st_expand.grain);
        assert(page_desc(&PAGES[pt],1u,&vp)==&ST_STR_DESC && vp==&song.g[G_RSIZE]);
        assert(page_desc(&PAGES[pt],2u,&vp)==&ST_MOTION_DESC && vp==&song.g[G_DRREV]);
        assert(page_desc(&PAGES[pt],3u,&vp)==&ST_TONE_DESC && vp==&stasis_tone);
        assert(page_desc(&PAGES[pr],0u,&vp)==&ST_REGION_DESC[0] && vp==&st_expand.scan);
        assert(page_desc(&PAGES[pr],1u,&vp)==&ST_REGION_DESC[1] && page_desc(&PAGES[pr],2u,&vp)==&ST_REGION_DESC[2]);
        assert(page_desc(&PAGES[pr],3u,&vp)==&ST_SOURCE_DESC && *vp==(int16_t)stasis_build_state());
        assert(page_desc(&PAGES[pv],0u,&vp)==&GP[G_RTYPE]);
        st_hall_feed=64;st_hall_ret=127;
        assert(page_desc(&PAGES[pv],1u,&vp)==&ST_FEED_DESC && *vp==64);
        assert(page_desc(&PAGES[pv],2u,&vp)==&TP[P_DRY] && vp==&TSEL->p[P_DRY]);
        assert(page_desc(&PAGES[pv],3u,&vp)==&ST_HALL_DESC && *vp==127);
        assert(page_desc(&PAGES[ps],1u,&vp)==&GP[G_DRLVL] && page_desc(&PAGES[ps],2u,&vp)==&ST_REGION_DESC[3]);
        assert(!page_desc(&PAGES[ps],3u,&vp));
        param_format(&ST_FEED_DESC,64,vb,&un);assert(str_eq(vb,"100")&&str_eq(un,"%"));   /* the gain law, as PLAY shows it */
        param_format(&ST_FEED_DESC,127,vb,&un);assert(str_eq(vb,"300"));
        param_format(&ST_HALL_DESC,0,vb,&un);assert(str_eq(vb,"0"));
        {   char pl[12];uxl_stasis_gain_fmt(pl,sizeof pl,100);param_format(&ST_HALL_DESC,100,vb,&un);
            assert(!memcmp(pl,vb,str_len(vb))&&pl[str_len(vb)]=='%');}                   /* and the two displays agree at any value */
        param_format(&ST_SOURCE_DESC,5,vb,&un);assert(str_eq(vb,"FRZ"));
        /* the turns */
        st_hall_feed=64;stasis_build_turn(&PAGES[pv],1u,1);assert(st_hall_feed>64);
        st_hall_ret=64;stasis_build_turn(&PAGES[pv],3u,-1);assert(st_hall_ret<64);
        TSEL->p[P_DRY]=100;stasis_build_turn(&PAGES[pv],2u,-1);assert(TSEL->p[P_DRY]<100);
        {   uint32_t track,id;uint8_t old_page=ui.page;
            assert(stasis_build_hp_target(SB_TANK,2u,&track,&id)&&track&&id==P_DRY);
            assert(!stasis_build_hp_target(SB_TANK,1u,&track,&id));
            assert(!stasis_build_hp_target(SB_TANK,3u,&track,&id));
            assert(!stasis_build_hp_target(SB_REGION,3u,&track,&id));
            assert(stasis_build_hp_target(SB_TEXTURE,1u,&track,&id)&&!track&&id==G_RSIZE);
#if FELUCCA_HOME_PERF
            hp_ref_t r;
            ui.page=(uint8_t)pv;
            assert(uxl_inter_ref(2u,&r)&&r.kind==HP_TRACK&&hp_tid(r)==P_DRY);
            assert(!uxl_inter_ref(1u,&r)&&!uxl_inter_ref(3u,&r));
            hp_touch(2u);assert(hp_last_valid&&hp_last.kind==HP_TRACK&&hp_tid(hp_last)==P_DRY);
            hp_touch(1u);assert(!hp_last_valid);
#endif
            ui.page=old_page;
            {   uint8_t on=ui.ml.on;
                ui.ml.on=1u;ui.ml.pick=0u;
                stasis_build_turn(&PAGES[pv],2u,1);
                assert(ui.ml.pick&&ui.ml.id==P_DRY&&ui.ml.trk==song.sel);
                ui.ml.on=on;ui.ml.pick=0u;ml_arm=0u;
            }
        }
        stasis_tone=64;stasis_build_turn(&PAGES[pt],3u,1);assert(stasis_tone>64);
        stasis_build_turn(&PAGES[pt],1u,1);assert(song.g[G_RSIZE]>=0);
        ui.msg_t=0u;stasis_build_turn(&PAGES[pr],3u,1);assert(str_eq(ui.msg,"HOLD FX: B4 C5 D5"));   /* SOURCE: only the message */
        song.g[G_RTYPE]=2u;assert(!stasis_build_kind(&PAGES[pv]));                         /* HALL: the REVERB page is the old one */
        song.g[G_RTYPE]=3u;
    }
#endif
    /* DLY stays unavailable while STASIS owns the ring, but a real rail
     * remains at each knob and no misleading stored delay value appears. */
    for(i=0u;i<NPAGES;i++)
        if(str_eq(PAGES[i].title,"DLY")) {
            ui.page=(uint8_t)i;break;
        }
    assert(i<NPAGES && fx_delay_suspended(cur_page(),0u));
    ui.force=1u;ui.graph_sig=~0u;assert(uxl_ring_page());
    uxl_ring_build();
    for(k=0u;k<4u;k++)
            assert(host_screen[(uint32_t)y[k]*240u+(uint32_t)x[k]]!=swap16(T_BG));
    puts("BUILD FX PLAY-style orbit: STASIS GRAIN/REGION and DLY disabled rails PASS");
    puts("STASIS shared REVERB scope, source states, gain labels PASS");
}
#endif

int main(void)
{
    ui_power_on();
    uxl.play = 0; uxl.lvl = 0; ui.home = 1;
    ui.menu = 1; ui.force = 1;
    press(B_FX);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_FX);
    ui.menu = 3; ui.force = 1; /* INFO: SAVE navigates without HOME */
    press(B_SAVE);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_SAVE);
    /* SAVE release must not inadvertently cycle the SAVE page. */
    uint32_t saved_page = ui.page;
    frame();
    assert(ui.page == saved_page);
    ui.menu = 1; ui.force = 1;
    press(B_ENV);
    assert(!ui.menu && !ui.home && cur_page()->fam == FAM_ENV);
    /* PLAY contains no destructive direct-save gesture: a tap opens
     * the fully featured BUILD SAVE screen. */
    uxl.play = 1; ui.menu = 0; ui.confirm = 0;
    uint8_t old_page = ui.page, old_home = ui.home;
    press(B_SAVE);
    assert(play_project_save_only && !uxl.play && !ui.home &&
           cur_page()->graph == GR_SLOTS);
    /* SAVE and SELECT cannot wander to USER / PRESETS / PHRASES. */
    press(B_SAVE);
    assert(play_project_save_only && cur_page()->graph == GR_SLOTS);
    page_turn(1);
    assert(play_project_save_only && cur_page()->graph == GR_SLOTS);
    ui.act = 3u; /* K2 INIT/ERASE is blocked in this shortcut */
    act_do();
    assert(!ui.confirm && cur_page()->graph == GR_SLOTS);
    press(B_HOME);
    assert(!play_project_save_only && uxl.play &&
           ui.page == old_page && ui.home == old_home);

    project_t p, q;
    project_store_t bytes;
    afterburner.on = 1; afterburner.time = 49;
    afterburner.amt = 88; afterburner.damp = 21;
    stasis_tone = 111;
#if CARAVEL_STASIS2
    /* These three old global slots still round-trip as opaque legacy values.
     * They cannot silently become REGION, RETURN or SWEEP parameters. */
    song.g[G_SYNC] = 1;
    song.g[G_INFO] = 117;
    song.g[G_ENGGO] = 29;
    st_expand.scan=77;st_expand.length=113;
    st_expand.grain=2;st_expand.quality=1;st_expand.reverse=1;
#endif
    project_capture(&p);
    assert(proj_pack(&bytes, &p));
#if CARAVEL_STASIS2
    assert(bytes.raw[65] == PROJ_STREG_MARK);
#else
    assert(bytes.raw[65] == PROJ_FX_EXT_MARK);
#endif
    assert(proj_import(&q, bytes.raw, sizeof bytes));
    afterburner.on = 0; afterburner.time = 30;
    afterburner.amt = 32; afterburner.damp = 70;
    stasis_tone = 64;
#if CARAVEL_STASIS2
    st_expand.scan=0;st_expand.length=64;
    st_expand.grain=1;st_expand.quality=0;st_expand.reverse=0;
#endif
    assert(!project_restore_runtime(&q));
    assert(afterburner.on && afterburner.time == 49 &&
           afterburner.amt == 88 && afterburner.damp == 21);
    assert(stasis_tone == 111);
#if CARAVEL_STASIS2
    assert(song.g[G_SYNC] == 1 && song.g[G_INFO] == 117 && song.g[G_ENGGO] == 29);
    assert(st_expand.scan==77 && st_expand.length==113 &&
           st_expand.grain==2 && st_expand.quality==1 && st_expand.reverse==1);
    project_t again;
    project_store_t bytes_again;
    project_capture(&again);
    assert(proj_pack(&bytes_again, &again));
    assert(!memcmp(bytes.raw + 8u + 2u * G_SYNC,
                   bytes_again.raw + 8u + 2u * G_SYNC, 2u));
    assert(!memcmp(bytes.raw + 8u + 2u * G_INFO,
                   bytes_again.raw + 8u + 2u * G_INFO, 2u));
    assert(!memcmp(bytes.raw + 8u + 2u * G_ENGGO,
                   bytes_again.raw + 8u + 2u * G_ENGGO, 2u));
    assert(bytes_again.raw[65] == PROJ_STREG_MARK);
    project_t prior;
    project_store_t old;
    project_capture(&prior);
    prior.st_region=streg_wire_default();prior.st_region_present=1u;
    assert(proj_pack(&old,&prior));assert(old.raw[65] == PROJ_FX_EXT_MARK);
    assert(proj_import(&q,old.raw,sizeof old));
    assert(!project_restore_runtime(&q));
    assert(st_expand.scan==0 && st_expand.length==64 &&
           st_expand.grain==ST_GRAIN_DEFAULT && st_expand.quality==0 && st_expand.reverse==0);
#endif
    project_init_runtime();
    assert(afterburner.on == 0 && stasis_tone == 64);
#if CARAVEL_STASIS2
    assert(st_expand.scan==0 && st_expand.length==64 &&
           st_expand.grain==ST_GRAIN_DEFAULT && st_expand.quality==0 && st_expand.reverse==0);
#endif
#if CARAVEL_STASIS2
    /* The five PLAY SPACE groups use the real source variables, not shadow
     * copies of BUILD or an imaginary DENS/FEED/SWEEP control. */
    assert(uxl_area_ng(UXL_SPACE)==5u);
    uxl_tg_t tg[4];
    song.g[G_RTYPE]=2u;
    assert(uxl_group(UXL_SPACE,2u,tg)==0u);
    assert(uxl_group(UXL_SPACE,3u,tg)==0u);
    assert(uxl_group(UXL_SPACE,4u,tg)==4u); /* TANK: TYPE, FEED, DRY, HALL stay reachable */
    assert(tg[2].kind==HP_TRACK && tg[2].id==P_DRY && tg[2].trk==255u);
    assert(tg[1].kind==UXL_KSTASIS && tg[1].id==UXL_ST_FEED);
    assert(tg[3].kind==UXL_KSTASIS && tg[3].id==UXL_ST_HALL);
    song.g[G_RTYPE]=3u;
    assert(uxl_group(UXL_SPACE,2u,tg)==4u);
    assert(tg[0].kind==UXL_KSTASIS && tg[0].id==UXL_ST_GRAIN);
    assert(uxl_group(UXL_SPACE,3u,tg)==4u);
    assert(tg[0].id==UXL_ST_POS && tg[1].id==UXL_ST_LENGTH &&
           tg[2].id==UXL_ST_REVERSE && tg[3].id==UXL_ST_STATE);
    assert(uxl_group(UXL_SPACE,4u,tg)==4u);
    assert(tg[1].id==UXL_ST_FEED && tg[2].id==P_DRY && tg[3].id==UXL_ST_HALL);
    assert(uxl_group(UXL_SPACE,2u,tg)==4u && tg[3].kind==UXL_KTONE);   /* TEXTURE K4 = TONE */
    uxl.play=1;uxl.area=UXL_SPACE;uxl.grp[UXL_SPACE]=3u;
    song.g[G_RTYPE]=3;fm1_in.buttons=0;
    int16_t before=st_expand.length;
    uxl_knob(1u,1);
    assert(st_expand.length>before);
    uxl.mom=0;fm1_in.buttons=1u<<panel.btn[B_LFO];
    before=st_expand.length;uxl_knob(1u,1);
    assert(st_expand.length>before && (uxl.mom & 2u));
    fm1_in.buttons=0;uxl_mom_poll();
    assert(st_expand.length==before && !uxl.mom);
    /* S0: growing source HISTORY is not a painted PLAY field. It must not
     * drive two full 240x120 blits on each 32-sample FX block. */
    uxl.play=1;uxl.area=UXL_SPACE;uxl.grp[UXL_SPACE]=3u;
    song.g[G_RTYPE]=3u;
    uint32_t previous_length=st.region_length;
    uint8_t previous_status=st.region_status;
    st.region_status=STREG_FIT;st.region_length=0u;
    uint32_t sig0=uxl_play_sig();
    st.region_length=65536u;
    assert(uxl_play_sig()==sig0);
    st.region_status=STREG_LIMIT;
    assert(uxl_play_sig()!=sig0); /* a real user-facing state changes */
    st.region_length=previous_length;st.region_status=previous_status;
#if CARAVEL_UX_CAPTURE
    /* Scope pages may refresh the MIDI clock at most every 1/8 bar. The
     * dedicated CAPTURE editor and all non-STASIS pages stay at 1/32 bar. */
    uint32_t old_bars=perf_bars_q16;
    uint8_t old_playing=song.playing, old_cap_view=uxl.cap_view;
    song.playing=1u;uxl.cap_view=0u;
    perf_bars_q16=2047u;uint32_t cs0=uxl_cap_sig();
    perf_bars_q16=2048u;assert(uxl_cap_sig()==cs0);
    perf_bars_q16=8192u;assert(uxl_cap_sig()!=cs0);
    uxl.area=UXL_HARM;perf_bars_q16=2047u;cs0=uxl_cap_sig();
    perf_bars_q16=2048u;assert(uxl_cap_sig()!=cs0);
    uxl.area=UXL_SPACE;uxl.cap_view=1u;
    perf_bars_q16=2047u;cs0=uxl_cap_sig();
    perf_bars_q16=2048u;assert(uxl_cap_sig()!=cs0);
    uxl.cap_view=old_cap_view;
    song.playing=old_playing;perf_bars_q16=old_bars;
#endif
    puts("STASIS2 PLAY repaint signature: raw source growth ignored, status and clock retained");
    puts("STASIS2 PLAY SPACE groups and shared REGION / momentary knobs PASS");

    /* The new feature must not restore C5's old PHASER or D5 VERB. */
    uxl.play=1;ui.ly=ui.layer=LAYER_FX;
    song.g[G_RTYPE]=3;song.g[G_DRLVL]=0;
    fx_map_sync();
    assert(str_eq(uxl_fx_label(10),"FRZ"));
    assert(str_eq(uxl_fx_label(11),"NEW"));
    assert(str_eq(uxl_fx_label(12),"REL"));
    assert(uxl_stasis_cell_state(10)==1u && uxl_stasis_cell_state(11)==1u);
    for(uint32_t pos=10u;pos<=15u;pos++) {
        if(pos==13u)continue; /* E5 remains its ordinary FX slot */
        assert(perf_map[pos]==PF_N);
    }
    song.g[G_DRLVL]=1;
    assert(uxl_stasis_cell_state(10)==2u);
    song.g[G_DRLVL]=0;
    /* After D5 RELEASE (st.stopping=1) STASIS used to leave STOP only with the sequencer's PLAY: B4 and C5 were ignored. Now both start
     * the ring again (a fresh source: LIVE, FILL), so SOURCE never stays on STOP. */
    st.stopping=1u;st_new_source_request=0u;
    action_stasis(0u);                                   /* B4 */
    assert(!st.stopping && st.region_status==STREG_FILL && !st.valid);
    st.stopping=1u;
    action_stasis(1u);                                   /* C5 */
    assert(!st.stopping && !st_new_source_request);
    st.stopping=1u;action_stasis(3u);assert(st.stopping);   /* D5 again stays stopped */
    st.stopping=0u;
    fm1_in.notes=0u;fm1_in.buttons=0u;
    uxl_stasis_poll(); /* synchronize the edge detector */
    fm1_ms=1000u;fm1_in.buttons=1u<<panel.btn[B_FX];
    fm1_in.notes=1u<<19u;uxl_stasis_poll();
    assert(uxl_new_held && !uxl_new_fired && !st_new_source_request);
    assert(uxl_stasis_cell_state(11)==3u && uxl_new_step==0u);
    fm1_ms=1199u;uxl_stasis_poll();
    assert(uxl_new_step==0u && !st_new_source_request);
    ui.force=0;fm1_ms=1200u;uxl_stasis_poll();
    assert(uxl_new_step==1u && ui.force && uxl_stasis_cell_state(11)==3u);
    fm1_ms=1400u;uxl_stasis_poll();assert(uxl_new_step==2u);
    fm1_ms=1600u;uxl_stasis_poll();assert(uxl_new_step==3u);
    fm1_ms=1700u;uxl_stasis_poll();
    assert(uxl_new_step==3u && !st_new_source_request);
    fm1_ms=1801u;uxl_stasis_poll();
    assert(uxl_new_fired && st_new_source_request && uxl_new_step==4u);
    assert(uxl_stasis_cell_state(11)==2u); /* active only after trigger */
    fm1_ms=1900u;uxl_stasis_poll();
    assert(st_new_source_request==1u); /* never fire a second time */
    fm1_in.notes=0u;uxl_stasis_poll();
    assert(!uxl_new_held && !uxl_new_step);
    /* Short tap cancels the pending UI state and never triggers NEW. */
    st_new_source_request=0u;fm1_ms=2000u;fm1_in.notes=1u<<19u;
    uxl_stasis_poll();assert(uxl_stasis_cell_state(11)==3u);
    fm1_ms=2100u;fm1_in.notes=0u;uxl_stasis_poll();
    assert(uxl_stasis_cell_state(11)==1u && !st_new_source_request);
    song.g[G_RTYPE]=2;fx_map_sync();
    assert(!uxl_fx_label(11) && !uxl_stasis_cell_state(11));
    assert(perf_map[11]==perf_map_of(fx_keys,11));
    puts("STASIS2 REGION / FX B4-C5-D5 and C5 long-press host UI PASS");
#endif
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
    stviz_ui_regression();
#endif
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    build_fx_orbit_regression();
#endif
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE && CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6 && !FELUCCA_FM4
    path_style_acceptance();path_mixer_redraw_acceptance();path_transport_acceptance();
#endif
    puts("STASIS/AFTER PROJECT + PLAY SAVE + MENU jump host checks PASS");
    return 0;
}
