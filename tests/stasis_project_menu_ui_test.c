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
static int hp_project_rename(uint32_t slot, const char *name);
static int hp_project_save(uint32_t slot, const char *name);
static int hp_project_load(uint32_t slot);
static int hp_autosave_boot(int allowed);
static int hp_forget(uint32_t slot);
#include "ui_harness.h"
#include "../experiments/action-banks/action_control.c"
/* Any call here means the fixture has escaped its stated scope.
 * Never return a false green from untested persistent storage. */
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
    uxl_draw_play();
    assert(stviz_has_last);
    memcpy(stviz_before,host_screen,sizeof stviz_before);
    background=swap16(T_BG);
    assert(host_screen[194u*240u+120u]!=background); /* MIDI band at y186..196 */
    assert(host_screen[185u*240u+67u]==background && host_screen[186u*240u+67u]!=background);
    assert(host_screen[196u*240u+67u]!=background && host_screen[197u*240u+67u]==background);
    for(i=197u;i<=200u;i++) assert(host_screen[i*240u+120u]==background); /* clear rows above the legend (y201) */
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
        static const uint8_t HOT[8][2]={{2,0},{2,1},{2,2},{2,3},{3,0},{3,1},{3,2},{4,1}};
        uint32_t c,rv;
        song.g[G_DRREV]=100;song.g[G_RSIZE]=127;st.drift_ph=0x40000000u;
        for(rv=0u;rv<2u;rv++){
            st_expand.reverse=(int16_t)rv;
            for(c=0u;c<9u;c++){
                uxl.hot_t=(uint8_t)(c<8u?4u:0u);
                if(c<8u){uxl.grp[UXL_SPACE]=HOT[c][0];uxl.hot_k=HOT[c][1];}
                else uxl.grp[UXL_SPACE]=3u;
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
                assert(memcmp(&stviz_before[137u*240u+110u],&stviz_expected[137u*240u+110u],20u*sizeof(uint16_t))!=0);
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
        st.auto_hold=hold_keep;uxl.hot_t=0u;uxl.grp[UXL_SPACE]=0u;
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
    static const char *const NAMES[] = {"GRAIN", "REGION", "REVERB"};
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
        uxl_ring_pt(ANG[k],65,&x[k],&y[k]);
        assert(x[k]>0 && x[k]<240 && y[k]>28 && y[k]<198);
    }
    for(j=0u;j<3u;j++) {
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
            assert(host_screen[(uint32_t)y[k]*240u+(uint32_t)x[k]]!=swap16(T_BG));
#if defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
        assert(stviz_enabled() && stviz_has_last);
        if(j<2u) {   /* HISTORY is K3 on GRAIN and K4 on REGION. */
            uint8_t qkeep=st.active_quality;
            ui.hot_col=(uint8_t)(j==0u?2u:3u);ui.hot_t=4u;
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
            /* The main STASIS page has CAPT/STR/MOTN and the same scope. */
            ui.hot_col=2u;ui.hot_t=4u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_STR);
            ui.hot_col=3u;ui.force=1u;uxl_ring_build();
            assert(stviz_frame.hot==STVIZ_HOT_MOTN);
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
    /* An earlier section of this fixture stops the STASIS transport
     * (st.stopping=1, as after the sequencer STOP). NEW SOURCE is rejected in
     * that state by design ("START BEFORE NEW SOURCE"), so the 800 ms trigger
     * below needs a started transport. The rejection itself is asserted first. */
    st.stopping=1u;st_new_source_request=0u;
    action_stasis(1u);
    assert(!st_new_source_request);
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
    puts("STASIS/AFTER PROJECT + PLAY SAVE + MENU jump host checks PASS");
    return 0;
}
