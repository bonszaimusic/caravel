/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS2 V6 experimental central audio scope (included by ui_uxl.c).
 *
 * This is not MIDI CAPTURE. Actual PCM16 was written to the ONE STASIS
 * source ring before st_scope_peak[] was updated by the audio task.
 * Draw once for a full PLAY repaint and again with the SAME painter for
 * bounded non-overlapping 8-row LCD strips. No pixels are read from LCD.
 * Preview is behind CARAVEL_STASIS_SCOPE_V6 until A4/A5 and FM-1 QA.
 */
#include "stasis_scope_strips.h"
#include "stasis_scope_rate.h"
#include "stasis_scope_policy.h"

typedef struct {
    uint8_t peak[32], region[32], age[32], writer, read[2];
    uint8_t has_writer, has_heads, tone, status, frozen, new_source;
    uint8_t gr, quality, new_pending, new_step;
    /* V7: continuous REGION arc, grain footprints (settings, not jumping heads),
     * STR ring, MOTN dots and the element a knob is turning right now. */
    uint8_t has_region, has_feet, rev, foot_d, str_th, motn_amt, motn_pos, hot;
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    uint8_t feed, hall, dry, source_state;
#endif
    uint16_t reg_a0, reg_a1, foot_a[2];
} stviz_view_t;
enum { STVIZ_HOT_NONE, STVIZ_HOT_REGION, STVIZ_HOT_FEET, STVIZ_HOT_STR, STVIZ_HOT_MOTN, STVIZ_HOT_TONE
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    , STVIZ_HOT_HISTORY, STVIZ_HOT_FEED, STVIZ_HOT_HALL, STVIZ_HOT_DRY
#endif
};
static uint8_t stviz_ref=16u;     /* auto-gain reference (largest bin, companded units), slow decay */
static uint32_t stviz_ref_ms;
static stviz_view_t stviz_frame, stviz_last;
static uint32_t stviz_last_ms;
static uint8_t stviz_has_last, stviz_has_time;

#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
/* BUILD uses the same bounded PCM view as PLAY, but must never pass its
 * own page through PLAY's area/group policy (which deliberately hides it). */
static uint8_t stviz_build_fx(void)
{
    return (uint8_t)(!uxl.play && !ui.home && !ui.menu && !ui.confirm &&
                     !name_on() && !ui.uboot && !ui.layer &&
                     uxl_stasis_build_page(cur_page()));
}
#endif
static uint8_t stviz_enabled(void)
{
    return (uint8_t)((stasis_scope_show(uxl.play,uxl.area==UXL_SPACE,
                           uxl_group_now(),(uint32_t)song.g[G_RTYPE])
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
                       || stviz_build_fx()
#endif
                       ) && !uxl.cap_view && !uxl.lvl && !uxl.pv);
}
/* Use one bounded read-only view, never scan/alter the audio ring in the UI.
 * Audio can advance during the copy; >one bin of writer motion invalidates it.
 */
static uint32_t stviz_deg(uint32_t idx) { return ((idx & ST_RING_MASK) * 360u) >> 16u; }
static uint8_t stviz_hot_element(void)
{
    uint32_t g=uxl_group_now(),k=uxl.hot_k;
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    if(stviz_build_fx()) {
        if(!ui.hot_t) return STVIZ_HOT_NONE;
        k=ui.hot_col;
        if(cur_page()->id[0]==G_RTYPE)
            return k==2u?STVIZ_HOT_STR:k==3u?STVIZ_HOT_MOTN:STVIZ_HOT_NONE;
        if(str_eq(cur_page()->title,"REGION"))
            return k<2u ? STVIZ_HOT_REGION :
                   k==2u ? STVIZ_HOT_FEET : STVIZ_HOT_HISTORY;
        return k==0u || k==3u ? STVIZ_HOT_FEET :
               k==1u ? STVIZ_HOT_REGION : STVIZ_HOT_HISTORY;
    }
#endif
    if(!uxl.hot_t) return STVIZ_HOT_NONE;
    if(g==3u) return k<2u ? STVIZ_HOT_REGION : k==2u ? STVIZ_HOT_FEET : STVIZ_HOT_NONE;
    /* TEXTURE = GRAIN, STR, MOTN, TONE; TANK = TYPE, FEED, DRY, HALL */
    if(g==2u) return k==0u ? STVIZ_HOT_FEET : k==1u ? STVIZ_HOT_STR :
                     k==2u ? STVIZ_HOT_MOTN : STVIZ_HOT_TONE;
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    if(g==4u && k==1u) return STVIZ_HOT_FEED;
    if(g==4u && k==2u) return STVIZ_HOT_DRY;
    if(g==4u && k==3u) return STVIZ_HOT_HALL;
#endif
    return STVIZ_HOT_NONE;
}
static uint8_t stviz_snapshot(stviz_view_t *v)
{
    uint32_t w, w_after, g, phase, half, i, r0, r1, maxpk=0u, q, f, span, lag, amin, amax, a0, a1, base;
    memset(v,0,sizeof *v);
    w=st.w;
    v->frozen=(uint8_t)(st.auto_hold || st.releasing);
    v->new_source=st.new_source;
    v->new_pending=(uint8_t)(uxl_new_held && !uxl_new_fired);
    v->new_step=uxl_new_step;
    v->status=st.region_status;
    v->quality=st.active_quality;
    v->gr=st.active_grain;
    v->tone=stasis_scope_tone_tier((uint32_t)clamp(stasis_tone,0,127));
    v->writer=(uint8_t)((w>>11u)&31u);
    v->has_writer=(uint8_t)(!v->frozen && !st.stopping);
    v->has_heads=(uint8_t)(st.gr_ready && !st.new_source);
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    /* Actual latched direction of each grain, not the pending encoder value. */
    v->rev=st.reverse_mask&3u;
    v->str_th=(uint8_t)(1u+((uint32_t)clamp(mod_stasis.strength,0,127)*4u>>7u));
#else
    v->rev=(uint8_t)(st_expand.reverse!=0);
    v->str_th=(uint8_t)(1u+((uint32_t)clamp(song.g[G_RSIZE],0,127)*4u>>7u));
#endif
    {   /* two dots on the STR ring only while MOTN is audible; 16 positions keep a still page still */
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
        uint32_t actual=st.evolve_q8>>8u;
        uint32_t m=(actual+31u)>>5u;
#else
        uint32_t m=(uint32_t)clamp(song.g[G_DRREV],0,127)>>5u;
#endif
        v->motn_amt=(uint8_t)m;
        v->motn_pos=m?(uint8_t)(st.drift_ph>>28u):0u;
    }
    v->hot=stviz_hot_element();
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    /* Settings, not measured output levels. DRY is per selected track. */
    v->feed=(uint8_t)clamp(st_hall_feed,0,127);
    v->hall=(uint8_t)clamp(st_hall_ret,0,127);
    v->dry=(uint8_t)clamp(TSEL->p[P_DRY],0,127);
    /* Snapshot the state once, so the two full halves and all strips agree. */
    v->source_state=uxl_stasis_source_state();
#endif
    g=ST_GRAIN_BASE<<(v->gr>3u?3u:v->gr);
    half=g>>1u;
    phase=st.phase&(g-1u);
    r0=(st.reverse_mask&1u)?g-1u-phase:phase;
    phase=(phase+half)&(g-1u);
    r1=(st.reverse_mask&2u)?g-1u-phase:phase;
    v->read[0]=(uint8_t)(((st.gr_l[0]+(r0>>(v->quality>2u?2u:v->quality)))&
                          ST_RING_MASK)>>11u);
    v->read[1]=(uint8_t)(((st.gr_l[1]+(r1>>(v->quality>2u?2u:v->quality)))&
                          ST_RING_MASK)>>11u);
    /* HOLD freezes the *recording*, not the grain playback heads. */
    for(i=0u;i<32u;i++){ if(st_scope_peak[i]>maxpk) maxpk=st_scope_peak[i]; }
    /* Auto-gain: the loudest bin sets the scale (attack at once, one step per
     * 4 units per second down, never below 16 = |pcm| 512 = -36 dBFS) so a quiet source
     * still fills the ring. */
    if(maxpk>=stviz_ref){ stviz_ref=(uint8_t)(maxpk<16u?16u:maxpk); stviz_ref_ms=fm1_ms; }
    else if((uint32_t)(fm1_ms-stviz_ref_ms)>=1000u){ stviz_ref=(uint8_t)(stviz_ref>20u?stviz_ref-4u:16u); stviz_ref_ms=fm1_ms; }
    for(i=0u;i<32u;i++){
        uint32_t pk=st_scope_peak[i];
        uint32_t age=(w-((i<<11u)+1024u))&ST_RING_MASK;
        uint32_t rel=pk?(3u*pk)/stviz_ref:0u;
        v->peak[i]=(uint8_t)(pk?1u+(rel>3u?3u:rel):0u);
        v->age[i]=v->frozen?0u:(uint8_t)(age>>14u);
        if(st.region_status>=STREG_FIT &&
           age>=st.region_young &&
           age<st.region_young+st.region_length)
            v->region[i]=1u;
    }
    if(st.region_status>=STREG_FIT){
        v->has_region=1u;
        v->reg_a1=(uint16_t)stviz_deg(w-st.region_young);
        v->reg_a0=(uint16_t)stviz_deg(w-(st.region_young+st.region_length));
        if(v->has_heads){                                 /* where the grains read from: the same anchors as st_grain_start */
            q=v->quality>2u?2u:v->quality; f=1u<<q;
            span=(g+f-1u)/f+2u; lag=(g+4096u)>>q;
            amin=st.region_young+span; amax=st.region_young+st.region_length;
            base=st.region_young+lag+(256u>>q)-256u;
            a0=base<amin?amin:base>amax?amax:base;
            a1=base+(743u>>q); a1=a1<amin?amin:a1>amax?amax:a1;
            v->has_feet=1u;
            v->foot_a[0]=(uint16_t)stviz_deg(w-a0);
            v->foot_a[1]=(uint16_t)stviz_deg(w-a1);
            if(st.gr_ready){                              /* MOTN scatter: the real start of each head (ring index) */
                v->foot_a[0]=(uint16_t)stviz_deg(st.gr_l[0]);
                v->foot_a[1]=(uint16_t)stviz_deg(st.gr_l[1]);
            }
            v->foot_d=(uint8_t)((span*360u)>>16u);
        }
    }
    w_after=st.w;
    return (uint8_t)((uint32_t)(w_after-w)<=2048u);
}
/* Real REGION length / POS in seconds, from captured frames and the record factor (44.1 kHz). */
static void stviz_fmt_region_sec(char *b,uint32_t cap,uint32_t length)
{
    uint32_t frames,f=1u<<(st.active_quality>2u?2u:st.active_quality),t10;
    if(st.region_status<STREG_FIT || !st.region_length){ str_cpy(b,"--",cap); return; }
    frames=length?st.region_length:st.region_young;
    t10=(frames*f*10u+22050u)/44100u;
    fmt_int(b,(int32_t)(t10/10u));
    str_cpy(b+str_len(b),".",cap-str_len(b));
    { char d[2]={(char)('0'+t10%10u),0}; str_cpy(b+str_len(b),d,cap-str_len(b)); }
    str_cpy(b+str_len(b),"s",cap-str_len(b));
}
/* Exact same drawing code on 240x120 full paint and 8-row local strips.
 * ox/oy are the strip's screen origin; cv_oy already shifts the full screen
 * into its top/bottom canvas, while local strips set cv_oy=0.
 */
/* A circle arc as short chords of at most 4 degrees; `thick` concentric radii. */
static void stviz_arc(int32_t ox,int32_t oy,int32_t r,int32_t thick,int32_t a0,int32_t ext,uint16_t c)
{
    int32_t t,s;
    for(t=0;t<thick;t++){
        int32_t px,py;
        uxl_ring_pt(a0,r+t,&px,&py);
        for(s=4;;s+=4){
            int32_t d=s>ext?ext:s,x,y;
            uxl_ring_pt(a0+d,r+t,&x,&y);
            cv_line(px-ox,py-oy,x-ox,y-oy,c);
            px=x;py=y;
            if(d>=ext) break;
        }
    }
}
/* A radial bar about 3 px wide: seven lines 1 degree apart (solid, no checkerboard). */
static void stviz_bar(int32_t ox,int32_t oy,int32_t a,int32_t r0,int32_t r1,uint16_t c)
{
    int32_t k;
    for(k=-3;k<=3;k++){
        int32_t x0,y0,x1,y1;
        uxl_ring_pt(a+k,r0,&x0,&y0);
        uxl_ring_pt(a+k,r1,&x1,&y1);
        cv_line(x0-ox,y0-oy,x1-ox,y1-oy,c);
    }
}
/* The grain window drawn in the core: two overlapping raised-cosine humps; half width follows GRAIN. */
static void stviz_window(int32_t ox,int32_t oy,uint32_t gr,uint16_t c)
{
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    /* Keep previous time-to-width relationship for 46/93/186 ms. */
    static const uint8_t HW[4]={3u,4u,6u,10u};
#else
    static const uint8_t HW[4]={4u,6u,10u,16u};
#endif
    int32_t half=HW[gr>3u?3u:gr],k,i;
    for(k=-1;k<=1;k+=2){
        int32_t px=0,py=0;
        for(i=0;i<=24;i++){
            int32_t x=120+k*(half/2)-half+(2*half*i)/24;
            int32_t y=122-(11*(256-uxl_sin(i*15+90)))/512;     /* i*15 deg = 0..360, cos via sin(a+90) */
            if(i) cv_line(px-ox,py-oy,x-ox,y-oy,c);
            px=x;py=y;
        }
    }
}
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
/* Three short independent setting rails, shown only while their knob is
 * hot. Replace the grain-window text area temporarily, not the PCM ring.
 * All pixels remain within r<=24; no extra LCD rectangles or animation. */
static void stviz_setting(const stviz_view_t *v,int32_t ox,int32_t oy)
{
    uint32_t value=v->hot==STVIZ_HOT_FEED?v->feed:
                   v->hot==STVIZ_HOT_HALL?v->hall:v->dry;
    const char *label=v->hot==STVIZ_HOT_FEED?"FEED":
                      v->hot==STVIZ_HOT_HALL?"HALL":"DRY";
    int32_t n=(int32_t)((value*30u+63u)/127u);
    cv_text(120-ox-text_w(&AF_S,label)/2,112-oy,&AF_S,label,T_TEXT);
    cv_rect(105-ox,129-oy,30,3,T_LINE);
    if(n) cv_rect(105-ox,129-oy,n,3,T_ACCENT);
    /* Unity reference for the two hall gain controls (64 = x1). */
    if(v->hot!=STVIZ_HOT_DRY) cv_rect(120-ox,127-oy,1,7,T_TEXT);
    /* Input arrow, output arrow or direct-path double line; these are
     * setting cues, not an animated or measured hall signal. */
    if(v->hot==STVIZ_HOT_FEED) {
        cv_line(98-ox,130-oy,103-ox,130-oy,T_TEXT);
        cv_line(100-ox,127-oy,103-ox,130-oy,T_TEXT);
        cv_line(100-ox,133-oy,103-ox,130-oy,T_TEXT);
    } else if(v->hot==STVIZ_HOT_HALL) {
        cv_line(137-ox,130-oy,142-ox,130-oy,T_TEXT);
        cv_line(139-ox,127-oy,142-ox,130-oy,T_TEXT);
        cv_line(139-ox,133-oy,142-ox,130-oy,T_TEXT);
    } else {
        cv_line(113-ox,135-oy,127-ox,135-oy,T_TEXT);
    }
}
#endif
static void stviz_paint(const stviz_view_t *v,int32_t ox,int32_t oy)
{
    uint32_t i;
    /* 0 = dark/cool, 4 = bright/warm. In MONO ux_mix maps this to
     * luminance automatically; in LINE the distinct geometry remains. */
    /* TONE temperature, 5 tiers through a neutral middle (no muddy green): cool accent,
     * neutral text, warm theme colour. */
    uint16_t base=v->tone<=2u ? ux_mix(T_ACCENT,T_TEXT,(int32_t)v->tone*50)
                              : ux_mix(T_TEXT,T_THEME,((int32_t)v->tone-2)*50);
    uint16_t warm=ux.style==ST_LINE ? T_TEXT :
                  v->frozen ? ux_mix(T_BG,T_ACCENT,90) :
                  ux_mix(T_BG,base,75);
    uint16_t hotc=T_ACCENT, feetc=v->hot==STVIZ_HOT_FEET?hotc:T_TEXT;
    uint16_t regc=v->hot==STVIZ_HOT_REGION?hotc:(v->frozen?T_ACCENT:T_THEME);
    uint16_t strc=v->hot==STVIZ_HOT_STR?hotc:T_MID;
    static const uint8_t LEN[5]={2u,3u,5u,7u,9u};
    char sb[14];
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    const char *state=uxl_stasis_state_name(v->source_state);
#else
    const char *state=v->new_source?"NEW":
                      v->frozen?"FROZEN":
                      v->status==STREG_WAIT?"WAIT":
                      v->status==STREG_FILL?"FILL":
                      v->status==STREG_LIMIT?"LIMIT":"LIVE";
#endif
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    const char *gms=N_STGRAIN[v->gr>ST_GRAIN_MAX?ST_GRAIN_MAX:v->gr];
#else
    const char *gms=v->gr==0u?"23ms":v->gr==1u?"46ms":v->gr==2u?"93ms":"186ms";
#endif
    /* The 14 DMA rectangles cover EVERY pixel of r<=48 (and none beyond r56), so everything
     * is drawn inside r<=47: STR ring r27..30, heads r31, bars r33..42, footprints r43..44,
     * region arc r45..46 with end ticks r43..47. Full repaint and partial strips stay identical. */
    stviz_arc(ox,oy,27,(int32_t)v->str_th,0,360,strc);
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    /* History division marks refer to the ACTIVE capture rate, not a
     * requested setting that has not yet been applied to the source. */
    for(i=0u;i<(1u<<(v->quality>2u?2u:v->quality));i++)
        stviz_bar(ox,oy,(int32_t)(i*360u/(1u<<(v->quality>2u?2u:v->quality))),23,25,
                  v->hot==STVIZ_HOT_HISTORY?T_ACCENT:T_MID);
    /* TONE is a setting, not spectral metering. Five geometric steps
     * remain distinct even with frozen audio or a monochrome palette. */
    if(v->hot==STVIZ_HOT_TONE)
        for(i=0u;i<5u;i++)
            cv_rect(110+(int32_t)i*4-ox,135-oy,2,2+(int32_t)i,
                    i<=v->tone?T_TEXT:T_LINE);
#endif
    for(i=0u;i<32u;i++){
        int32_t a=(int32_t)((i*360u)/32u)+5;
        uint16_t c=v->region[i] ? warm : ux_mix(warm,T_BG,45);
        if(v->hot==STVIZ_HOT_TONE) c=hotc;
        if(!v->frozen && v->age[i]) c=ux_mix(c,T_BG,(int32_t)v->age[i]*12);
        if(v->peak[i]) stviz_bar(ox,oy,a,33,33+(int32_t)LEN[v->peak[i]],c);
        else stviz_bar(ox,oy,a,33,35,T_LINE);
    }
    if(v->has_region){
        int32_t a0=(int32_t)v->reg_a0,ext=((int32_t)v->reg_a1-a0+360)%360;
        int32_t x0,y0,x1,y1,k;
        if(ext<4) ext=4;
        stviz_arc(ox,oy,45,2,a0,ext,regc);
        for(k=0;k<2;k++){
            int32_t a=k?a0+ext:a0;
            uxl_ring_pt(a,43,&x0,&y0);uxl_ring_pt(a,47,&x1,&y1);
            cv_line(x0-ox,y0-oy,x1-ox,y1-oy,T_TEXT);
        }
    }
    if(v->has_feet){
        int32_t h;
        for(h=0;h<2;h++){
            int32_t a0=(int32_t)v->foot_a[h],ext=(int32_t)v->foot_d;
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
            uint8_t rev=(v->rev>>h)&1u;
#else
            uint8_t rev=v->rev;
#endif
            int32_t tip=rev?a0:a0+ext,dir=rev?-1:1;
            int32_t x0,y0,x1,y1,x2,y2;
            stviz_arc(ox,oy,43,2,a0,ext<4?4:ext,feetc);
            uxl_ring_pt(tip,44,&x0,&y0);                      /* arrow head at the reading end */
            uxl_ring_pt(tip-dir*5,42,&x1,&y1);
            uxl_ring_pt(tip-dir*5,46,&x2,&y2);
            cv_line(x0-ox,y0-oy,x1-ox,y1-oy,feetc);
            cv_line(x0-ox,y0-oy,x2-ox,y2-oy,feetc);
        }
    }
    if(v->motn_amt){
        for(i=0u;i<2u;i++){
            int32_t x,y,a=(int32_t)((v->motn_pos*360u)>>4u)+(int32_t)i*180;
            uxl_ring_pt(a,28,&x,&y);
            cv_rect(x-(int32_t)v->motn_amt/2-1-ox,y-(int32_t)v->motn_amt/2-1-oy,
                    (int32_t)v->motn_amt+2,(int32_t)v->motn_amt+2,
                    v->hot==STVIZ_HOT_MOTN?hotc:(i?T_ACCENT:T_THEME));
        }
    }
    if(v->new_pending){
        /* The C5 legend is the primary 800-ms feedback; these four
         * matching core ticks are UI-only (no manufactured audio state).
         * Input-driven C5 updates force a normal full repaint at 200 ms
         * boundaries, independently of the 6-Hz audio meter budget. */
        for(i=0u;i<4u;i++){
            int32_t a=210+(int32_t)i*40, x0,y0,x1,y1;
            uxl_ring_pt(a,24,&x0,&y0);
            uxl_ring_pt(a,27,&x1,&y1);
            cv_line(x0-ox,y0-oy,x1-ox,y1-oy,
                    i<(uint32_t)v->new_step?T_ACCENT:T_LINE);
        }
    }
    if(v->has_writer){
        int32_t x0,y0,x1,y1;
        uxl_ring_pt((int32_t)v->writer*360/32+5,32,&x0,&y0);
        uxl_ring_pt((int32_t)v->writer*360/32+5,47,&x1,&y1);
        cv_line(x0-ox,y0-oy,x1-ox,y1-oy,T_TEXT);
    }
    if(v->has_heads){
        for(i=0u;i<2u;i++){
            int32_t x,y;
            uxl_ring_pt((int32_t)v->read[i]*360/32+5,31,&x,&y);
            if(i==0u)cv_rect(x-1-ox,y-1-oy,3,3,T_ACCENT);
            else cv_frame(x-2-ox,y-2-oy,5,5,T_TEXT);
        }
    }
    /* Core: state word (never the only cue: shapes differ), the grain window, its length. */
    str_cpy(sb,state,sizeof sb);
    cv_text(120-ox-text_w(&AF_S,sb)/2,99-oy,&AF_S,sb,T_TEXT);
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
    if(v->hot==STVIZ_HOT_FEED || v->hot==STVIZ_HOT_HALL || v->hot==STVIZ_HOT_DRY) {
        /* Keep the source state above the setting; avoid text overlap. */
        stviz_setting(v,ox,oy);
    } else
#endif
    {
        stviz_window(ox,oy,v->gr,v->hot==STVIZ_HOT_FEET?hotc:warm);
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
        if(v->hot==STVIZ_HOT_HISTORY) {
            const char *h=v->quality==0u?"1.5s":v->quality==1u?"3.0s":"5.9s";
            cv_text(120-ox-text_w(&AF_S,h)/2,123-oy,&AF_S,h,T_TEXT);
        } else
#endif
        cv_text(120-ox-text_w(&AF_S,gms)/2,123-oy,&AF_S,gms,T_MID);
    }
}
/* STVIZ_STRIPS non-overlapping 4-row rectangles (stasis_scope_strips.h) that cover every
 * pixel of r<=48 and reach at most r51: no LCD fill/second overpaint, the orbit controls
 * (r>=53 at their nearest pixel) are never touched. cv_begin creates each complete strip.
 * 6 updates/s * 25 strips = 150 blits/s of 100 rows in all, not a measured FM-1 rate.
 */
static void stviz_partial(const stviz_view_t *v)
{
    uint32_t i;
    for(i=0u;i<STVIZ_STRIPS;i++){
        uint32_t h=STVIZ_HALF[i],x=120u-h,y=STVIZ_STRIP_Y0+i*STVIZ_STRIP_H;
        cv_begin(2u*h+1u,STVIZ_STRIP_H,T_BG);
        cv_bg=T_BG;
        cv_oy=0;
        stviz_paint(v,(int32_t)x,(int32_t)y);
        cv_blit(x,y);
    }
    lcd_sync(); /* count the completion of the last transfer, not its start */
}
static void stviz_tick(void)
{
    stviz_view_t next;
    if(!stviz_enabled()){
        stviz_has_last=0u;stviz_has_time=0u;
        return;
    }
    if(!stviz_has_last ||
#if CARAVEL_UX_BUILD_FX_PLAY_STYLE
       (stviz_build_fx() ? ui.hot_t : uxl.hot_t) ||
#else
       uxl.hot_t ||
#endif
       ui.force ||
       !stasis_scope_refresh_due(fm1_ms,stviz_last_ms,stviz_has_time))
        return;
    if(!stviz_snapshot(&next) || !memcmp(&next,&stviz_last,sizeof next))
        return;
    /* Nonblocking DMA ownership gate. A busy transfer drops the
     * background visual sample, never spins in the audio/UX critical
     * path and never advances last_ms. The next due frame retries. */
    if(!lcd_scope_ready()) return;
    stviz_partial(&next);
    stviz_last=next;
    stviz_last_ms=fm1_ms;stviz_has_time=1u;
}
