/* SPDX-License-Identifier: GPL-3.0-only */
/* Real firmware panel, musical transport and MIDI-event regression; no device acceptance. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN harmony_panel_baseline_main
/* The text the real renderer draws, through gfx.c's host hook: the usability checks read what the screen says. */
static char hk_txt[96][40];
static unsigned hk_n;
static void hk_text(const char *s) { if (hk_n < 96u && s) { unsigned i = 0; while (s[i] && i < 39u) { hk_txt[hk_n][i] = s[i]; i++; } hk_txt[hk_n++][i] = 0; } }
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hk_text(s)
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "../engine-pair-morph/pair_test.c"
#ifndef HARM_BAR
#define HARM_BAR() (perf_bars_q16 >> 16)   /* (the sources before round 7 count HARMONY bars with perf_bars_q16 itself: the groups run against them as negative controls) */
#endif
static void hit(uint32_t k){key(1u<<k);key(0);}
static void bank_hold(uint32_t mask){web_keys(mask);run_ms(650);web_keys(0);run_ms(70);}
static uint32_t chord(void){return harmony_chord_at(harmony_bar);}
static void next_bar(void){uint32_t b=HARM_BAR(),g=0;while(HARM_BAR()==b&&g++<5000u)run_ms(1);CHECK(g<5000u);}
static void roots(void){const uint8_t r[4]={0,9,5,7};for(uint32_t i=0;i<4;i++){hroot_set(i,r[i]);hdur_set(i,0);}}
static uint32_t note_mask(uint32_t t){uint32_t n=0;for(uint32_t v=0;v<NVOICE;v++)if(trk[t].v[v].gate)n|=1u<<(trk[t].v[v].note%12);return n;}
static void clock_packet(uint32_t st){midi_enqueue(15u|st<<8,1);}
static uint32_t pulse_us;
static void external_ms(uint32_t n){while(n--){pulse_us+=1000;while(pulse_us>=20833u){pulse_us-=20833u;clock_packet(0xF8);}run_ms(1);}}
static void screenshot(void){
 const char *path=getenv("HARM_SCREEN");if(!path)return;FILE *f=fopen(path,"wb");CHECK(f!=NULL);
 fprintf(f,"P6\n240 240\n255\n");for(uint32_t i=0;i<240u*240u;i++){uint16_t q=web_fb[i];q=(q>>8)|(q<<8);uint8_t rgb[3]={(q>>11)*255/31,((q>>5)&63)*255/63,(q&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);
}

/* ---- Usability fixes U01-U04, U06, U07, U14 (the 0.12.0 HARMONY audit, 7b99c43): the screen's own text, the real knobs, the
 * real note paths. Every group below fails on the unfixed sources. */
static int hk_has(const char *s){for(unsigned i=0;i<hk_n;i++){unsigned j=0;while(s[j]&&hk_txt[i][j]==s[j])j++;if(!s[j]&&!hk_txt[i][j])return 1;}return 0;}
static void hk_dump(const char *tag){fprintf(stderr,"%s texts:",tag);for(unsigned i=0;i<hk_n;i++)fprintf(stderr," [%s]",hk_txt[i]);fprintf(stderr,"\n");}
static void ux_page(uint32_t graph){for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].graph==graph){ui.page=i;ui.home=0;page_entered();ui.msg_t=0;ui.force=1;hk_n=0;run_ms(40);return;}CHECK(0);}
static void ux_redraw(void){ui.msg_t=0;ui.force=1;hk_n=0;run_ms(40);}
static void ux_reset(void){
 normal_off();pair_disable();song.sel=0;song.g[G_CLOCK]=0;seq_stop();action_enabled=0;action_setup=0;harmony_perf_clear();
 harmony_on=1;roots();memset(link_src,0,sizeof link_src);memset(link_retrig,0,sizeof link_retrig);memset(harmony_follow,1,sizeof harmony_follow);
 trk[0].p[P_ROOT]=0;trk[0].p[P_SCALE]=1;trk[0].p[P_QUANT]=QN_OFF;trk[0].p[P_CHRD]=CH_BUS;trk[0].p[P_VOIC]=VC_CLOSE;trk[0].p[P_VOICE]=V_POLY;
 harmony_bar=0;harmony_edit_dur=0;link_setup_row=0;ui_prefs2|=(uint8_t)(PREF_SEL_PAGE>>16);
}
static uint32_t ux_pcs(const uint8_t *nn,uint32_t n){uint32_t m=0;for(uint32_t i=0;i<n;i++)m|=1u<<(nn[i]%12u);return m;}
static int ux_on(const char *g){const char *o=getenv("UX_GROUP");size_t n=strlen(g);if(!o)return 1;   /* UX_GROUP=U02 or a comma list U02,R7g */
 for(const char *p=o;*p;){const char *e=strchr(p,',');size_t l=e?(size_t)(e-p):strlen(p);if(l==n&&!strncmp(p,g,n))return 1;if(!e)break;p=e+1;}
 return 0;}   /* UX_GROUP=U02: one group only (negative controls) */
static void usability_fixes(void){
 if(ux_on("U01")){
 /* U01: C - Am - F - G in C major reads C, Am, F, G (the editor passed an absolute mask where a root-relative one belongs) */
 ux_reset();ux_page(GR_HARMONY);
 if(!(hk_has("Am")&&!hk_has("A6")&&!hk_has("Fsus4")&&!hk_has("Gsus2")))hk_dump("U01");
 CHECK(hk_has("Am")&&hk_has("C")&&hk_has("F")&&hk_has("G"));
 CHECK(!hk_has("A6")&&!hk_has("Fsus4")&&!hk_has("Gsus2"));
 { const uint8_t r[4]={2,4,11,7};for(uint32_t i=0;i<4;i++)hroot_set(i,r[i]);ux_redraw();   /* D E B G: Dm Em Bdim G */
   if(!(hk_has("Dm")&&hk_has("Em")&&hk_has("Bdim")))hk_dump("U01b");
   CHECK(hk_has("Dm")&&hk_has("Em")&&hk_has("Bdim")&&hk_has("G"));roots();}
 }
 if(ux_on("U03")){
 ux_reset();ux_page(GR_HARMONY);
 /* U03: with SHIFT the editor names the PREPARED chords, says so, and marks the sounding slot LIVE; never a mixed name */
 harmony_shift=harmony_shift_want=1;harmony_shift_pending=0;ux_redraw();
 if(!(hk_has("Am")&&hk_has("PREP")&&hk_has("LIVE Dm")&&!hk_has("Csus4")))hk_dump("U03");
 CHECK(hk_has("C")&&hk_has("Am")&&hk_has("F")&&hk_has("G")&&!hk_has("Csus4"));
 CHECK(hk_has("PREP")&&hk_has("LIVE Dm"));
 CHECK(hroot_get(0)==0&&hroot_live(0)==2);                              /* the stored progression is untouched */
 }
 if(ux_on("U01c")){
 /* U01/U03 render cache: T1's scale / ROOT change the names, so they are part of the cache signature (no forced redraw) */
 ux_reset();ux_page(GR_HARMONY);
 hk_n=0;run_ms(40);CHECK(hk_n==0);                                      /* nothing changed: cached */
 trk[0].p[P_ROOT]=2;hk_n=0;run_ms(40);CHECK(hk_n>0);                    /* D major: the names are rebuilt */
 hk_n=0;run_ms(40);CHECK(hk_n==0);trk[0].p[P_SCALE]=2;hk_n=0;run_ms(40);CHECK(hk_n>0);   /* another scale: rebuilt again */
 }
 if(ux_on("U03c")){
 /* the render cache follows what the names depend on: SHIFT, T1's scale and ROOT (no forced redraw) */
 ux_reset();ux_page(GR_HARMONY);
 harmony_shift=harmony_shift_want=1;harmony_shift_pending=0;ux_redraw();CHECK(hk_has("PREP"));
 harmony_shift=harmony_shift_want=0;hk_n=0;run_ms(40);
 CHECK(hk_n>0&&!hk_has("PREP"));                                        /* SHIFT 0: redrawn without PREP / LIVE */
 hk_n=0;run_ms(40);CHECK(hk_n==0);                                      /* nothing changed: cached */
 trk[0].p[P_ROOT]=2;hk_n=0;run_ms(40);                                  /* D major: C is outside, plays as the scale note below */
 CHECK(hk_n>0);trk[0].p[P_ROOT]=0;hk_n=0;run_ms(40);CHECK(hk_n>0&&hk_has("Am"));
 }
 if(ux_on("U02")){
 /* U02: an off-scale root is one effective root everywhere: C# in C major sounds C - E - G from CHRD BUS, and is
  * edited only along the scale */
 ux_reset();hroot_set(0,1);harmony_bar=0;
 { uint8_t out[CHORD_MAX];int32_t rt;uint16_t rel;uint32_t n=chord_make(&trk[0],60,out,&rt,&rel);
   CHECK(n==3&&out[0]==60&&out[1]==64&&out[2]==67);                       /* was C# E G C (61 64 67 72) */CHECK(ux_pcs(out,n)==((1u<<0)|(1u<<4)|(1u<<7)));
   CHECK(rt%12==0&&rel==((1u<<0)|(1u<<4)|(1u<<7)));
   rt=0;n=chord_make(&trk[0],64,out,&rt,&rel);CHECK(ux_pcs(out,n)==((1u<<0)|(1u<<4)|(1u<<7)));}
 }
 if(ux_on("U02e")){
 ux_reset();hroot_set(0,1);harmony_bar=0;
 ux_page(GR_HARMONY);CHECK(hk_has("C")&&!hk_has("C#"));                   /* the card shows what sounds */
 }
 if(ux_on("U02s")){
 ux_reset();hroot_set(0,1);harmony_bar=0;ux_page(GR_HARMONY);
 {
   web_enc(EN_K1,1);run_ms(30);CHECK(hroot_get(0)==2);                  /* from the stored C#, shown as C: next scale degree D */
   web_enc(EN_K1,-1);run_ms(30);CHECK(hroot_get(0)==0);                 /* back to C */
   web_enc(EN_K1,-1);run_ms(30);CHECK(hroot_get(0)==11);                /* below C: B, never B-flat (11 is in the scale) */
   web_enc(EN_K1,3);run_ms(30);CHECK(hroot_get(0)==4);                  /* B -> C D E in one burst: three degrees */
   for(int i=0;i<7;i++){web_enc(EN_K1,1);run_ms(10);}CHECK(hroot_get(0)==4);   /* a full turn of seven degrees is the same root */
   hroot_set(0,1);web_enc(EN_K1,-1);run_ms(30);CHECK(hroot_get(0)==11); }  /* stored C# going down: below its effective C */
 }
 if(ux_on("U02c")){
 /* a scale change after the roots were set leaves foreign roots: still one effective root for the bus and the editor */
 ux_reset();trk[0].p[P_SCALE]=1;trk[0].p[P_ROOT]=2;                        /* D major: C is outside, B is outside; C plays as B...C# */
 { uint8_t out[CHORD_MAX];int32_t rt;uint16_t rel;hroot_set(0,0);harmony_bar=0;
   uint32_t n=chord_make(&trk[0],60,out,&rt,&rel);uint16_t bm=harmony_mask_root(0);
   CHECK(n>=3);CHECK((ux_pcs(out,n)&~(uint32_t)bm)==0);}                  /* every tone CHRD BUS plays is a tone the snapping uses */
 }
 if(ux_on("U04")){
 /* U04: K4 FOLLOW is direction based: right ON, left OFF, the burst size does not matter */
 ux_reset();ux_page(GR_LINK);link_setup_row=0;ux_redraw();
 harmony_follow[0]=0;
 web_enc(EN_K4,1);run_ms(30);CHECK(harmony_follow[0]==1);
 web_enc(EN_K4,1);run_ms(30);CHECK(harmony_follow[0]==1);              /* another click right stays ON (it toggled OFF) */
 web_enc(EN_K4,3);run_ms(30);CHECK(harmony_follow[0]==1);
 web_enc(EN_K4,-1);run_ms(30);CHECK(harmony_follow[0]==0);
 web_enc(EN_K4,-1);run_ms(30);CHECK(harmony_follow[0]==0);
 web_enc(EN_K4,2);run_ms(30);CHECK(harmony_follow[0]==1);              /* one impulse of +2 */
 web_enc(EN_K4,-4);run_ms(30);CHECK(harmony_follow[0]==0);
 harmony_follow[0]=1;web_enc(EN_K4,0);run_ms(30);CHECK(harmony_follow[0]==1);
 }
 if(ux_on("U06")){
 ux_reset();ux_page(GR_LINK);link_setup_row=0;
 /* U06: the track's switch is FOLLOW, the bus is HARMONY; both states are on the page together */
 harmony_follow[0]=1;harmony_on=0;ux_redraw();
 if(!(hk_has("FOLLOW")&&hk_has("HARMONY")&&!hk_has("HARM")))hk_dump("U06");
 CHECK(hk_has("FOLLOW")&&hk_has("HARMONY")&&!hk_has("HARM"));
 CHECK(hk_has("ON")&&hk_has("OFF"));
 harmony_on=1;harmony_follow[0]=0;ux_redraw();CHECK(hk_has("ON")&&hk_has("OFF")&&hk_has("FOLLOW")&&hk_has("HARMONY"));
 }
 if(ux_on("U07")){
 ux_reset();ux_page(GR_LINK);
 /* U07: HARMONY SETUP: only K1 is a control; K2..K4 are no cards (hint text instead) and do nothing */
 link_setup_row=1;harmony_on=1;ux_redraw();
 if(!(hk_has("HARMONY")&&!hk_has("ROOTS")&&!hk_has("DUR")&&!hk_has("NEXT")&&!hk_has("SCL")))hk_dump("U07");
 CHECK(hk_has("HARMONY")&&!hk_has("ROOTS")&&!hk_has("DUR")&&!hk_has("NEXT")&&!hk_has("SCL")&&!hk_has("1-8"));
 CHECK(hk_has("ONLY K1 WORKS HERE"));
 { uint8_t rr=hroot_get(0),dd=hdur_get(0),fo=harmony_follow[0],on=harmony_on;
   web_enc(EN_K2,1);web_enc(EN_K3,1);web_enc(EN_K4,1);run_ms(30);
   CHECK(link_setup_row==1&&hroot_get(0)==rr&&hdur_get(0)==dd&&harmony_follow[0]==fo&&harmony_on==on);}
 harmony_on=0;ux_redraw();CHECK(hk_has("ONLY K1 WORKS HERE")&&hk_has("THEN SELECT"));      /* OFF: say how to reach the roots */
 }
 if(ux_on("U14")){
 /* U14: Harmony never moves a note that picks a drum: DRUM track, FOLLOW ON, GM 49 stays 49 (SEQ note step, ARP, ARP CHORD) */
 ux_reset();
 trk[1].eng_req=ENGI_DRUM;run_ms(80);CHECK(drum_track(&trk[1])&&chord_kit(&trk[1]));
 trk[1].p[P_QUANT]=QN_OFF;harmony_follow[1]=1;harmony_on=1;harmony_bar=0;
 CHECK(harmony_mask(&trk[1])!=0);
 { step_t st;memset(&st,0,sizeof st);st.time=ST_NOTE;st.n=1;st.note[0]=49;st.vel=100;
   seq_step(&trk[1],&st,0,10000,SEQ_ROLLED);CHECK(trk[1].seq_n==1);CHECK(trk[1].seq_notes[0]==49);       /* was 48 */
   seq_release(&trk[1]);
   st.note[0]=38;seq_step(&trk[1],&st,4,10000,SEQ_ROLLED);CHECK(trk[1].seq_notes[0]==38);seq_release(&trk[1]);   /* 38 is no C-major chord tone either */
   memset(&st,0,sizeof st);st.time=ST_NOTE;st.hit=1;st.vel=100;
   seq_step(&trk[1],&st,0,10000,SEQ_ROLLED);CHECK(trk[1].seq_notes[0]==DRUM_LANE_NOTE[0]);seq_release(&trk[1]);}   /* grid hit unchanged */
 CHECK(harmony_snap(&trk[1],49)==49&&harmony_snap(&trk[1],38)==38);
 CHECK(harmony_snap(&trk[0],61)!=61);                                     /* a pitched track still snaps: C# -> C or E */
 }
 if(ux_on("U05")){
 /* U05 (review of 4662c97): the bus rows (HARMONY, DURATION, the SETUP row's switch) are global and keyed from T1: ALL + the key, never the
  * selected track's T1/T3 badge. The LINK row stays the selected track's. */
 for(uint32_t sel=0;sel<3u;sel+=2u){
   char tb[4]={'T',(char)('1'+sel),0};
   ux_reset();song.sel=sel;ux_page(GR_HARMONY);harmony_edit_dur=0;ux_redraw();
   if(!(hk_has("ALL")&&!hk_has(tb)&&hk_has("C MAJ T1")))hk_dump("U05 roots");
   CHECK(hk_has("ALL")&&!hk_has(tb)&&hk_has("C MAJ T1"));
   harmony_edit_dur=1;ux_redraw();CHECK(hk_has("ALL")&&!hk_has(tb)&&hk_has("C MAJ T1"));
   ux_page(GR_LINK);link_setup_row=1;ux_redraw();                                  /* SETUP row (bottom): global */
   if(!(hk_has("ALL")&&hk_has(tb)&&hk_has("C MAJ T1")))hk_dump("U05 setup");
   CHECK(hk_has("ALL")&&hk_has("C MAJ T1"));                                       /* ALL on the SETUP row, the key under its switch */
   CHECK(hk_has(tb));                                                              /* the LINK row above keeps the track's badge */
 }
 /* the key follows T1's own ROOT and SCALE, and the cache redraws for it */
 ux_reset();ux_page(GR_HARMONY);trk[0].p[P_ROOT]=2;trk[0].p[P_SCALE]=2;ux_redraw();CHECK(hk_has("D MIN T1"));
 ux_page(GR_LINK);link_setup_row=1;ux_redraw();CHECK(hk_has("D MIN T1"));trk[0].p[P_ROOT]=5;hk_n=0;run_ms(40);CHECK(hk_has("F MIN T1"));
 }
 if(ux_on("P3")){
 /* P3 (review of 4662c97): on the SETUP row only K1 works, so its header must not claim K1-4 */
 ux_reset();ux_page(GR_LINK);link_setup_row=1;ux_redraw();
 if(!(hk_has("EDIT K1")&&!hk_has("EDIT K1-4")))hk_dump("P3");
 CHECK(hk_has("EDIT K1")&&!hk_has("EDIT K1-4")&&hk_has("ONLY K1 WORKS HERE"));
 link_setup_row=0;ux_redraw();CHECK(hk_has("EDIT K1-4"));                           /* the LINK row: K1..K4 all work */
 }
 if(ux_on("TL")){
 /* tl_chord_tone (ui_tools.c G5 GENERATE): the strong steps take tones of the bus chord, a pitch-class mask that is absolute (keyed from T1);
  * the generating track's own ROOT must not be added again. Suspected from the source in round 1, not reproduced until now. */
 for(uint32_t g=0;g<2u;g++){                           /* g0: T1 itself in D major; g1: T2 with ROOT F under T1's D major */
   uint32_t tn=g,i,strong=0,bad=0,mask;track_t *t;
   ux_reset();trk[1].eng_req=trk[0].eng_req;run_ms(80);                   /* (T2 as T1's melodic engine: an earlier group may have made it a kit) */
   trk[0].p[P_ROOT]=2;trk[0].p[P_SCALE]=1;hroot_set(0,2);hroot_set(1,9);hroot_set(2,7);hroot_set(3,4);   /* D, A, G, E: all in D major */
   trk[1].p[P_ROOT]=5;trk[1].p[P_SCALE]=1;harmony_follow[1]=1;
   song.sel=tn;t=&trk[tn];t->p[P_SLEN]=16;harmony_on=1;harmony_bar=0;
   for(uint32_t r=0;r<8u;r++){
     for(i=0;i<NSTEP;i++)step_clear(&t->step[i]);
     CHECK(tl_do(t,TL_GENERATE,0)!=0);
     mask=harmony_mask(t);CHECK(mask!=0u);
     for(i=0;i<16u;i+=4u)if(tl_gate(&t->step[i])){strong++;if(!((mask>>(t->step[i].note[0]%12u))&1u)){bad++;if(bad==1)fprintf(stderr,"TL g%u: step %u note %u not in the bus chord mask 0x%x\n",g,i,t->step[i].note[0],mask);}}
   }
   CHECK(strong>=8u);CHECK(bad==0u);
 }
 }
 if(ux_on("U13")){
 /* U13: the four slots are CHORD 1..4 (their duration is 1/2/4/8 bars); HARMONY off says where to switch it on; the ARP/SEQ effect is stated (U09); SHIFT is in degrees (U12) */
 ux_reset();ux_page(GR_HARMONY);harmony_edit_dur=0;ux_redraw();
 if(!(hk_has("CHORD 1")&&hk_has("CHORD 4")&&!hk_has("BAR 1")))hk_dump("U13");
 CHECK(hk_has("CHORD 1")&&hk_has("CHORD 2")&&hk_has("CHORD 3")&&hk_has("CHORD 4")&&!hk_has("BAR 1")&&!hk_has("BAR 4"));
 CHECK(hk_has("ARP EVERY NOTE, SEQ 1 5 9 13"));                                        /* U09: what HARMONY does per note source */
 harmony_on=1;harmony_shift=harmony_shift_want=2;harmony_shift_pending=0;ux_redraw();
 CHECK(hk_has("SHIFT +2 DEG (SCALE STEPS)"));
 harmony_shift=harmony_shift_want=-3;ux_redraw();CHECK(hk_has("SHIFT -3 DEG (SCALE STEPS)"));
 harmony_shift=harmony_shift_want=0;ux_redraw();CHECK(hk_has("ARP EVERY NOTE, SEQ 1 5 9 13"));
 }
 if(ux_on("U10")){
 /* U10: CHRD BUS on a track that has no bus (HARMONY off, or FOLLOW off) plays its own in-key triad: the card says OWN DIA3 */
 ux_reset();trk[0].p[P_CHRD]=CH_BUS;ux_page(GR_CHORD);harmony_on=1;harmony_follow[0]=1;ux_redraw();
 CHECK(!hk_has("OWN DIA3")&&!hk_has("OWN DIA3"));
 harmony_follow[0]=0;ux_redraw();
 if(!hk_has("OWN DIA3"))hk_dump("U10");
 CHECK(hk_has("OWN DIA3"));
 harmony_follow[0]=1;harmony_on=0;ux_redraw();CHECK(hk_has("OWN DIA3"));
 harmony_on=1;ux_redraw();CHECK(!hk_has("OWN DIA3"));                                  /* the cache follows both switches */
 trk[0].p[P_CHRD]=CH_OFF;harmony_follow[0]=0;ux_redraw();CHECK(!hk_has("OWN DIA3"));    /* only the BUS value says it */
 }
 if(ux_on("U08")){
 /* U08/U11/U12: the HARM bank spells out what is queued for the next bar and the SHIFT in degrees; the other banks and ACTION off keep a HARMONY chip while HOLD/SHIFT/a command is on */
 ux_reset();go_home();action_enabled=1;action_bank=ACT_BANK_HARMONY;action_setup=0;action_refresh();harmony_on=1;harmony_hold=0;harmony_pending=H_NONE;harmony_shift=harmony_shift_want=0;harmony_shift_pending=0;ux_redraw();
 CHECK(hk_has("RUN SHIFT 0 DEG"));
 harmony_on=0;ux_redraw();CHECK(hk_has("OFF: SCL > HARMONY SETUP"));harmony_on=1;harmony_hold=0;       /* U13: OFF says where to switch it on */
 harmony_hold=1;harmony_shift=harmony_shift_want=1;ux_redraw();CHECK(hk_has("HOLD SHIFT +1 DEG"));
 harmony_pending=H_NEXT;ux_redraw();CHECK(hk_has("NEXT BAR SHIFT +1 DEG"));
 harmony_pending=H_PREV;ux_redraw();CHECK(hk_has("PREV BAR SHIFT +1 DEG"));
 harmony_pending=H_RESUME;ux_redraw();CHECK(hk_has("RESUME: NEXT BAR +1 DEG"));                       /* (the longest queued text: SHIFT without its word, it must fit the bank) */
 harmony_pending=H_RESET;ux_redraw();CHECK(hk_has("RESET: CHORD 1, SHIFT 0"));
 harmony_pending=H_NONE;harmony_shift_pending=1;ux_redraw();CHECK(hk_has("SHIFT NEXT BAR +1 DEG"));
 harmony_shift_pending=0;
 action_bank=0;ux_redraw();CHECK(hk_has("HARM HOLD SHIFT +1 DEG"));                       /* another bank: the chip */
 harmony_hold=0;harmony_shift=harmony_shift_want=0;ux_redraw();CHECK(!hk_has("HARM RUN")&&!hk_has("HARM HOLD"));   /* nothing going on: no chip */
 harmony_hold=1;ux_redraw();CHECK(hk_has("HARM HOLD"));
 action_enabled=0;ux_redraw();CHECK(hk_has("HARM HOLD"));                                 /* ACTION off: the chip replaces the hint */
 harmony_on=0;harmony_hold=0;ux_redraw();CHECK(hk_has("HOME x2: ACTION")&&!hk_has("HARM HOLD"));
 harmony_perf_clear();
 }
 if(ux_on("R3P3")){      /* (R3P3 is also run as R3P3b: the RESET line and the tutorial) */
 /* review of dbeac55: the T1 key label's cache signature carries T1's own ROOT and SCALE (CHR and the periodic scales keep one absolute mask
  * under every root); the persistent RESET line keeps its HARM identity; the tutorial no longer talks of stars */
 ux_reset();ux_page(GR_HARMONY);trk[0].p[P_SCALE]=0;trk[0].p[P_ROOT]=0;ux_redraw();CHECK(hk_has("C CHR T1"));
 trk[0].p[P_ROOT]=2;hk_n=0;run_ms(40);CHECK(hk_has("D CHR T1"));                    /* CHR, root C -> D: the mask is 0xFFF both times */
 trk[0].p[P_SCALE]=13;trk[0].p[P_ROOT]=0;hk_n=0;run_ms(40);CHECK(hk_has("T1 C"));      /* WHOLE at root C: too wide for the line, the scale is dropped, the root stays */
 trk[0].p[P_ROOT]=2;hk_n=0;run_ms(40);CHECK(hk_has("D WHOLE T1"));   /* WHOLE at D (a narrower root): it fits; one absolute mask at C and D, the label follows either way */
 ux_page(GR_LINK);link_setup_row=1;trk[0].p[P_SCALE]=0;trk[0].p[P_ROOT]=0;ux_redraw();CHECK(hk_has("C CHR T1"));
 trk[0].p[P_ROOT]=2;hk_n=0;run_ms(40);CHECK(hk_has("D CHR T1"));                    /* the SETUP row's key line, too */
 }
 if(ux_on("R3P3b")){
 ux_reset();go_home();action_enabled=1;action_bank=0;action_setup=0;action_refresh();harmony_on=1;harmony_pending=H_RESET;ux_redraw();
 if(!hk_has("HARM RESET: CHORD 1, SHIFT 0"))hk_dump("R3P3 reset");
 CHECK(hk_has("HARM RESET: CHORD 1, SHIFT 0"));action_enabled=0;ux_redraw();CHECK(hk_has("HARM RESET: CHORD 1, SHIFT 0"));
 harmony_perf_clear();
 {FILE *f=fopen("docs/tutorials/harmony-live.md","rb");char b[16384];size_t n=f?fread(b,1,sizeof b-1,f):0;if(f)fclose(f);b[n]=0;
  CHECK(n>0&&!strstr(b,"starred")&&!strstr(b,"the star "));}
 }
 if(ux_on("U14a")||ux_on("U14c")){
 ux_reset();trk[1].eng_req=ENGI_DRUM;run_ms(80);CHECK(drum_track(&trk[1])&&chord_kit(&trk[1]));
 trk[1].p[P_QUANT]=QN_OFF;harmony_follow[1]=1;harmony_on=1;harmony_bar=0;
 { uint32_t list[1]={49};
   trk[1].held[0]=49;trk[1].nheld=1;held_key[1][0]=0;trk[1].p[P_AMODE]=AM_UP;trk[1].p[P_AOCT]=1;trk[1].p[P_APROB]=127;
   if(ux_on("U14a")){trk[1].arp_note=0;arp_fire(&trk[1],5000);CHECK(trk[1].arp_note==49);arp_off(&trk[1]);}   /* ARP step */
   if(ux_on("U14c")){trk[1].p[P_AMODE]=AM_CHORD;trk[1].arp_note=0;arp_chord(&trk[1],list,1);CHECK(trk[1].arp_note==49);arp_off(&trk[1]);}   /* ARP CHORD */
   trk[1].p[P_AMODE]=AM_OFF;trk[1].nheld=0;}
 }
}

/* ---- Round 7: the findings of the internal review of b8152a6. Every group fails on b8152a6 and passes now. ---- */
static uint32_t r7_gates(uint32_t t){uint32_t n=0;for(uint32_t v=0;v<NVOICE;v++)n+=trk[t].v[v].gate;return n;}
static uint32_t r7_until_bar_change(uint32_t lim){uint32_t b=harmony_bar,g=0;while(harmony_bar==b&&g<lim){run_ms(1);g++;}return g;}
/* every round-7 group starts from the same clean panel, so it also runs alone (UX_GROUP=R7x) and in any order */
static void r7_clean(void){web_keys(0);web_buttons(0);run_ms(20);transport_req=2;run_ms(30);transport_req=0;panic_req=(1u<<NTRK)-1u;run_ms(60);normal_off();pair_disable();go_home();ui.menu=0;ui.layer=0;ui.confirm=0;ui.msg_t=0;song.sel=0;song.g[G_BPM]=120;song.g[G_CLOCK]=0;
 for(uint32_t t=0;t<NTRK;t++){memset(trk[t].step,0,sizeof trk[t].step);trk[t].nheld=0;trk[t].arp_phys=0;trk[t].p[P_AMODE]=AM_OFF;arp_forget(&trk[t]);trk_all_off(&trk[t]);}
 memset(held_key,0,sizeof held_key);for(uint32_t t=1;t<NTRK;t++)trk[t].eng_req=trk[0].eng_req;   /* (an earlier group made a track a DRUM kit) */
 latch_req=(1u<<NTRK)-1u;run_ms(40);for(uint32_t t=0;t<NTRK;t++){latch_forget(t);chord_panic(t);chord_live_sync(t);memset(&chord_last[t],0,sizeof chord_last[t]);}   /* LATCH and chord ownership of an earlier group */
 run_ms(100);ux_reset();}
static void round7_fixes(void){
 if(ux_on("R7a")){   /* A-F1: a start is a bar line of the HARMONY clock, whatever the stop left behind */
  r7_clean();song.g[G_BPM]=120;hdur_set(3,0);
  seq_start();CHECK(r7_until_bar_change(5000)>=1950u);   /* (the fresh start: a whole bar) */
  run_ms(500);seq_stop();run_ms(10);seq_start();
  { uint32_t g=r7_until_bar_change(5000);CHECK(g>=1950u&&g<=2060u);CHECK(beat_n==0u);}   /* after a stop mid-bar: again a whole bar, and the change is on the pattern's bar line */
  seq_stop();
 }
 if(ux_on("R7b")){   /* A-F1: BAR RESET while playing lands chord 1 on the pattern's bar line; never the last chord, never an underflowed bar */
  r7_clean();song.g[G_BPM]=120;
  seq_start();run_ms(2500);seq_stop();run_ms(10);seq_start();run_ms(100);bar_reset_req=1;
  { uint32_t g=0,under=0;while(bar_reset_req&&g++<2600u){run_ms(1);under+=harmony_bar>=0x10000u;}
    CHECK(!bar_reset_req&&!under);CHECK(chord()==0u&&beat_n==0u);
    CHECK(r7_until_bar_change(5000)>=1950u);}
  seq_stop();run_ms(10);
  ux_reset();song.g[G_BPM]=120;seq_start();run_ms(700);song.g[G_BPM]=100;run_ms(300);bar_reset_req=1;   /* a tempo change first */
  { uint32_t g=0,under=0;while(bar_reset_req&&g++<3000u){run_ms(1);under+=harmony_bar>=0x10000u;}
    CHECK(!bar_reset_req&&!under);CHECK(chord()==0u&&beat_n==0u);
    { uint32_t d=r7_until_bar_change(5000);CHECK(d>=2330u&&d<=2470u);}}   /* (a bar at 100 BPM: 2400 ms) */
  seq_stop();song.g[G_BPM]=120;
 }
 if(ux_on("R7c")){   /* A-F2: STOP leaves a plain chord a player still holds (as 0.11.0); a CHRD BUS chord is the engine's and ends */
  r7_clean();harmony_on=0;trk[0].p[P_CHRD]=CH_MAJ;trk[0].p[P_AMODE]=0;trk[0].p[P_LATCH]=LT_OFF;
  seq_start();run_ms(50);key(1u<<7);
  CHECK(note_mask(0)!=0u);seq_stop();run_ms(60);CHECK(note_mask(0)!=0u);   /* the held MAJ chord sounds on */
  key(0);run_ms(100);CHECK(note_mask(0)==0u);
  r7_clean();harmony_on=0;trk[0].p[P_CHRD]=CH_MAJ;trk[0].p[P_AMODE]=0;trk[0].p[P_LATCH]=LT_OFF;   /* two keys: A, A+B, B: the chord of B is still held when A is let go (chord_live counts it) */
  seq_start();run_ms(50);key(1u<<7);key((1u<<7)|(1u<<10));key(1u<<10);
  { uint32_t before=note_mask(0);CHECK(before!=0u);seq_stop();run_ms(60);CHECK(note_mask(0)==before);}
  key(0);run_ms(100);CHECK(note_mask(0)==0u);
  ux_reset();harmony_on=1;seq_start();run_ms(50);key(1u<<7);CHECK(note_mask(0)!=0u);seq_stop();run_ms(60);CHECK(note_mask(0)==0u);   /* a held BUS chord ends with the transport */
  key(0);run_ms(60);
 }
 if(ux_on("R7q")){   /* STOP still ends a note of the last plain chord that nobody holds any more (an orphan), as before round 7 */
  r7_clean();harmony_on=0;trk[0].p[P_CHRD]=CH_MAJ;trk[0].p[P_AMODE]=0;trk[0].p[P_LATCH]=LT_OFF;
  seq_start();run_ms(50);
  trk_note_on(&trk[0],60,100);run_ms(20);CHECK(note_mask(0)!=0u);
  memset(&chord_last[0],0,sizeof chord_last[0]);chord_last[0].n=1;chord_last[0].note[0]=60;   /* chord_last names it; no key list, MIDI chord or latch holds it */
  seq_stop();run_ms(60);CHECK(note_mask(0)==0u);
 }
 if(ux_on("R7r")){   /* CHRD BUS + ARP: a key let go after STOP, or after a retrigger moved its chord, leaves the ARP's list too (the arpeggio must not outlive its key) */
  r7_clean();harmony_on=0;trk[0].p[P_AMODE]=AM_UP;trk[0].p[P_LATCH]=LT_OFF;trk[0].p[P_CHRD]=CH_BUS;
  seq_start();run_ms(50);key(1u<<7);CHECK(trk[0].nheld>0u);
  seq_stop();run_ms(60);key(0);run_ms(100);
  CHECK(trk[0].nheld==0u&&trk[0].arp_phys==0u);
  r7_clean();song.g[G_BPM]=240;harmony_on=1;memset(link_retrig,0,4);link_retrig[0]=1;   /* RETRIG CHORD */
  trk[0].p[P_AMODE]=AM_UP;trk[0].p[P_LATCH]=LT_OFF;trk[0].p[P_CHRD]=CH_BUS;
  seq_start();run_ms(50);key(1u<<7);CHECK(trk[0].nheld>0u);next_bar();run_ms(40);   /* the bus chord changes while the key is held */
  key(0);run_ms(150);
  CHECK(trk[0].nheld==0u&&trk[0].arp_phys==0u);
  seq_stop();trk[0].p[P_AMODE]=AM_OFF;memset(link_retrig,0,4);song.g[G_BPM]=120;
 }
 if(ux_on("R7d")){   /* A-F3 / B-B1: a slot without a performance capsule loads its own HARMONY / LINK state, and a probe changes nothing */
  r7_clean();harmony_on=1;
  { const uint8_t r[4]={7,9,5,0};for(uint32_t i=0;i<4;i++){hroot_set(i,r[i]);hdur_set(i,(uint8_t)i);} }
  link_src[1]=1;link_retrig[1]=3;harmony_follow[2]=0;
  { project_t cap;project_capture(&cap);project_store_t st;CHECK(proj_pack(&st,&cap));proj_slot[1]=st;}
  CHECK(hp_slot(1)==NULL);
  harmony_on=0;for(uint32_t i=0;i<4;i++){hroot_set(i,2);hdur_set(i,0);}link_src[1]=0;link_retrig[1]=0;harmony_follow[2]=1;
  { char nb[24];uint16_t l48[NTRK];uint32_t den[NTRK];uint8_t nn[NTRK];(void)project_used(1);(void)project_info(1,nb,l48,den,nn);
    CHECK(!harmony_on&&hroot_get(0)==2&&link_src[1]==0&&harmony_follow[2]==1);}   /* the probes leave the live state alone */
  project_load(1);run_ms(100);
  CHECK(harmony_on==1&&hroot_get(0)==7&&hroot_get(1)==9&&hroot_get(2)==5&&hroot_get(3)==0);
  CHECK(hdur_get(0)==0&&hdur_get(1)==1&&hdur_get(2)==2&&hdur_get(3)==3&&link_src[1]==1&&link_retrig[1]==3&&harmony_follow[2]==0);
  harmony_on=1;harmony_follow[2]=1;link_src[1]=0;link_retrig[1]=0;
 }
 if(ux_on("R7e")){   /* B-B2: a HARMONY-only edit is autosaved and comes back with RESTORE LAST */
  static uint8_t before[0x100000];uint32_t i;
  r7_clean();song.g[G_RTYPE]=2;trk[0].step[3].n=1;trk[0].step[3].note[0]=60;trk[0].step[3].time=ST_NOTE;
  run_ms(25000);run_ms(90000);memcpy(before,nor,sizeof nor);   /* (the autosave of everything the group itself changed has been written by now) */
  { uint32_t sg=autosave_sig(),sg2;   /* every HARMONY / LINK field is part of what the autosave watches (the transient bar and SHIFT are not) */
    harmony_on=!harmony_on;sg2=autosave_sig();CHECK(sg2!=sg);harmony_on=!harmony_on;
    harmony_root[1]^=1u;CHECK(autosave_sig()!=sg);harmony_root[1]^=1u;harmony_dur[2]^=1u;CHECK(autosave_sig()!=sg);harmony_dur[2]^=1u;
    link_src[1]^=1u;CHECK(autosave_sig()!=sg);link_src[1]^=1u;link_retrig[2]^=1u;CHECK(autosave_sig()!=sg);link_retrig[2]^=1u;harmony_follow[3]^=1u;CHECK(autosave_sig()!=sg);harmony_follow[3]^=1u;
    CHECK(autosave_sig()==sg);harmony_bar+=1u;harmony_shift_want=1;CHECK(autosave_sig()==sg);harmony_bar-=1u;harmony_shift_want=0;}
  harmony_on=1;for(i=0;i<4;i++){harmony_root[i]=(uint8_t)(2+i);harmony_dur[i]=(uint8_t)i;link_src[i]=i?1:0;link_retrig[i]=3;harmony_follow[i]=0;}
  run_ms(300000);
  CHECK(memcmp(before,nor,sizeof nor)!=0);   /* an autosave was written */
  harmony_on=0;memset(harmony_root,0,4);memset(harmony_dur,0,4);memset(link_src,0,4);memset(link_retrig,0,4);memset(harmony_follow,1,4);
  hp_pending_mask=0;hp_init();CHECK(hp_autosave_boot(1)==1);
  CHECK(harmony_on==1&&harmony_root[0]==2&&harmony_root[3]==5&&harmony_dur[2]==2&&link_src[1]==1&&link_retrig[3]==3&&harmony_follow[0]==0);
  harmony_on=1;memset(harmony_follow,1,4);memset(link_src,0,4);memset(link_retrig,0,4);song.g[G_RTYPE]=0;
 }
 if(ux_on("R7g")){   /* A-F4 / D-F2: the UI only asks; the audio side retriggers a held bus chord after a LINK key change */
  r7_clean();harmony_on=0;memset(link_src,0,4);memset(link_retrig,0,4);link_src[1]=1;link_retrig[1]=2;
  trk[0].p[P_SCALE]=1;trk[1].p[P_SCALE]=1;trk[0].p[P_ROOT]=0;trk[1].p[P_ROOT]=0;trk[1].p[P_QUANT]=QN_OFF;trk[1].p[P_CHRD]=CH_BUS;trk[1].p[P_VOIC]=VC_CLOSE;trk[1].p[P_VOICE]=V_POLY;trk[1].p[P_LATCH]=LT_OFF;trk[1].p[P_AMODE]=0;trk[1].p[P_MUTE]=0;
  song.sel=1;key(1u<<7);
  { uint8_t before[CHORD_MAX];memcpy(before,chord_last[1].note,sizeof before);CHECK(chord_last[1].n>0u);
    trk[0].p[P_SCALE]=2;key_link_sync();
    CHECK(memcmp(before,chord_last[1].note,sizeof before)==0);   /* the UI call touched no voice or ownership list */
    run_ms(40);CHECK(memcmp(before,chord_last[1].note,sizeof before)!=0);}   /* the audio side did, at its next block */
  key(0);run_ms(100);CHECK(r7_gates(1)==0u);memset(link_src,0,4);memset(link_retrig,0,4);song.sel=0;
 }
 if(ux_on("R7h")){   /* D-F3: a stale 'this press ended it' mark must not swallow a later NoteOff (a stuck note) */
  r7_clean();normal_off();song.g[G_CLOCK]=0;song.g[G_BPM]=240;song.sel=0;
  { const uint8_t r[4]={0,5,7,9};for(uint32_t i=0;i<4;i++){hroot_set(i,r[i]);hdur_set(i,0);} }
  harmony_on=1;for(uint32_t i=0;i<4;i++){harmony_follow[i]=1;link_src[i]=0;link_retrig[i]=0;}link_retrig[0]=1;
  { track_t*t=&trk[0];t->p[P_SCALE]=1;t->p[P_ROOT]=0;t->p[P_QUANT]=QN_OFF;t->p[P_CHRD]=CH_BUS;t->p[P_VOIC]=VC_CLOSE;t->p[P_VOICE]=V_POLY;t->p[P_LATCH]=LT_STACK;t->p[P_AMODE]=0;t->p[P_MUTE]=0;
    { const char*ph=getenv("R7_PHASE");run_ms(ph?(uint32_t)atoi(ph):0u); }   /* (the audio block phase: the group must hold for any) */
    transport_req=1;run_ms(30);
    key(1u<<7);key(0);key(1u<<8);next_bar();run_ms(3);key(0);
    t->p[P_LATCH]=LT_OFF;run_ms(50);t->p[P_LATCH]=LT_STACK;run_ms(50);
    next_bar();next_bar();run_ms(3);key(1u<<7);next_bar();run_ms(3);key(0);
    t->p[P_LATCH]=LT_OFF;run_ms(100);
    if(r7_gates(0)){fprintf(stderr,"R7h gated:");for(uint32_t v=0;v<NVOICE;v++)if(trk[0].v[v].gate)fprintf(stderr," %u",trk[0].v[v].note);fprintf(stderr," harmony_on=%u hb=%u kb:",harmony_on,harmony_bar);for(uint32_t k=0;k<27;k++)if(kb_chn[k])fprintf(stderr," k%u(n%u,trk%u,live%u)",k,kb_chn[k],kb_trk[k],kb_live[k]);fprintf(stderr," lat_bus_last=%u stay=%x nheld=%u arp_note=%d arp_phys=%u amode=%u chord_n=%u live=%u seq_n=%u mute=%u latch=%u chrd=%u voice=%u eng=%u\n",lat_bus_last[0],(unsigned)lat_stay[0][0],(unsigned)t->nheld,(int)t->arp_note,(unsigned)t->arp_phys,(unsigned)t->p[P_AMODE],(unsigned)chord_last[0].n,(unsigned)chord_live[0],(unsigned)t->seq_n,(unsigned)t->p[P_MUTE],(unsigned)t->p[P_LATCH],(unsigned)t->p[P_CHRD],(unsigned)t->p[P_VOICE],(unsigned)t->eng_req);}
    CHECK(r7_gates(0)==0u);   /* nothing keeps sounding with no key, latch, arp or sequence owning it */
    transport_req=2;run_ms(800);t->p[P_LATCH]=LT_OFF;t->p[P_CHRD]=CH_OFF;}
 }
 if(ux_on("R7i")){   /* A-F5: a fresh project (T1 scale CHR) builds the bus on the major scale of ROOT, as CHRD DIA3 does */
  r7_clean();trk[0].p[P_SCALE]=0;trk[0].p[P_ROOT]=0;ux_page(GR_HARMONY);
  if(!(hk_has("C")&&hk_has("Am")&&hk_has("F")&&hk_has("G")&&!hk_has("Cm")))hk_dump("R7i");
  CHECK(hk_has("C")&&hk_has("Am")&&hk_has("F")&&hk_has("G")&&!hk_has("Cm")&&!hk_has("Fm"));
  trk[0].p[P_SCALE]=1;
 }
 if(ux_on("R7k")){   /* B-B3: a HOME cell stored before LINK and HARMONY joined PAGES[] names a page that moved: SOURCE must still open a page that holds the parameter */
  uint32_t ci=0,pi,tested=0;   /* (up to six different pages behind CHORD) */
  r7_clean();normal_off();hp_apply(0,2);
  for(pi=0;pi<NPAGES;pi++)if(PAGES[pi].graph==GR_CHORD)ci=pi;
  for(pi=ci+3;pi<NPAGES;pi++){
   uint32_t id=PAGES[pi].id[0];
   if(id==0xFFu||!page_visible(pi)||PAGES[pi].scope==SC_GLOBAL)continue;
   if(!web_home_assign(0,0,0,HP_TRACK,0,id))continue;
   { uint32_t stale=pi-2,k,has=0;hp_ref_t*c=&hp.track[0].cell[0][0];c->page=(uint16_t)stale;
     for(k=0;k<4;k++)has|=PAGES[stale].id[k]==id;
     if(has)continue;                                                    /* (need a stale page that does not hold it) */
     song.sel=0;hp_open_editor();hp_edit_row=0;hp_edit_col=0;hp_edit_action=0;hp_source();
     for(k=0,has=0;k<4;k++)has|=PAGES[ui.page].id[k]==id;
     CHECK(!ui.home&&has);tested++;go_home();if(tested>=6)break;}
  }
  CHECK(tested>=3);go_home();
  /* a cell of each mod slot: the MOD page holds its ids at run time (its table lists slot 1's; slots 2..4 are shifted), SOURCE must open it */
  { uint32_t mp=0;for(pi=0;pi<NPAGES;pi++)if(PAGES[pi].graph==GR_MOD)mp=pi;
    for(uint32_t sl=0;sl<4;sl++){
      mod_ui_slot=0;
      if(mp&&web_home_assign(0,0,0,HP_TRACK,0,P_M1SRC+3u*sl)){hp_ref_t*c=&hp.track[0].cell[0][0];c->page=(uint16_t)mp;song.sel=0;hp_open_editor();hp_edit_row=0;hp_edit_col=0;hp_edit_action=0;hp_source();
        CHECK(!ui.home&&PAGES[ui.page].graph==GR_MOD&&mod_ui_slot==sl);go_home();}
      else CHECK(0);}}
 }
 if(ux_on("R7l")){   /* A-F9: a track that others follow cannot follow one itself (no chains) */
  r7_clean();memset(link_src,0,4);link_src[1]=1;song.sel=0;ux_page(GR_LINK);link_setup_row=0;
  web_enc(EN_K1,1);run_ms(30);web_enc(EN_K1,1);run_ms(30);web_enc(EN_K1,-1);run_ms(30);
  CHECK(link_src[0]==0u&&link_src[1]==1u);memset(link_src,0,4);
 }
 if(ux_on("R7m")){   /* A-F10: a SHIFT edit while stopped leaves a queued PREV / NEXT / RESET queued */
  r7_clean();seq_stop();run_ms(20);harmony_pending=H_NEXT;harmony_shift_want=2;harmony_shift_dirty=1;run_ms(40);
  CHECK(harmony_shift==2&&harmony_pending==H_NEXT);
  harmony_pending=H_NONE;harmony_shift_want=0;harmony_shift=0;harmony_shift_pending=0;
 }
 if(ux_on("R7n")){   /* A-F7: the bar counter is 16 bits; the chord position must not jump when it wraps */
  r7_clean();song.g[G_BPM]=240;hdur_set(0,0);hdur_set(1,0);hdur_set(2,0);hdur_set(3,2);
  seq_stop();run_ms(10);perf_bars_q16=65534u<<16;perf_bars_rem=0;seq_start();
  { uint32_t g=0,big=0,wrapped=0,last=perf_bars_q16>>16;
    while(g++<2600u){run_ms(1);if((perf_bars_q16>>16)<last)wrapped=1;last=perf_bars_q16>>16;big+=harmony_bar>=0x10000u;}
    CHECK(wrapped&&!big);}
  seq_stop();hdur_set(3,0);song.g[G_BPM]=120;
 }
 if(ux_on("R7p")){   /* the ARP's bar sync runs on the same bars as HARMONY: after a real STOP / PLAY mid-bar its steps stay evenly spaced (no realign at a drifted bar line) */
  r7_clean();harmony_on=0;trk[0].p[P_AMODE]=AM_UP;trk[0].p[P_CHRD]=CH_OFF;trk[0].p[P_VOICE]=V_POLY;song.g[G_BPM]=120;
  key((1u<<7)|(1u<<9)|(1u<<11));
  seq_start();run_ms(2300);seq_stop();run_ms(10);seq_start();   /* stopped mid-bar, started again */
  { uint32_t last_idx=trk[0].arp_idx,last_t=0,n=0,lo=1000,hi=0;
    for(uint32_t ms=0;ms<4400u;ms++){run_ms(1);
      if(trk[0].arp_idx!=last_idx){if(last_t){uint32_t d=ms-last_t;n++;if(d<lo)lo=d;if(d>hi)hi=d;}last_t=ms;last_idx=trk[0].arp_idx;}}
    if(!(n>20u&&lo>=118u&&hi<=132u))fprintf(stderr,"R7p: %u steps, intervals %u..%u ms\n",n,lo,hi);
    CHECK(n>20u&&lo>=118u&&hi<=132u);}   /* 1/16 at 120 BPM: 125 ms */
  seq_stop();key(0);trk[0].p[P_AMODE]=AM_OFF;
  /* the same through a BAR RESET in the middle of a bar, and at another tempo */
  r7_clean();harmony_on=0;trk[0].p[P_AMODE]=AM_UP;trk[0].p[P_CHRD]=CH_OFF;trk[0].p[P_VOICE]=V_POLY;song.g[G_BPM]=120;
  key((1u<<7)|(1u<<9)|(1u<<11));seq_start();run_ms(1300);bar_reset_req=1;
  { uint32_t last_idx=trk[0].arp_idx,last_t=0,n=0,lo=1000,hi=0;
    for(uint32_t ms=0;ms<4400u;ms++){run_ms(1);
      if(trk[0].arp_idx!=last_idx){if(last_t){uint32_t d=ms-last_t;if(ms>200u){n++;if(d<lo)lo=d;if(d>hi)hi=d;}}last_t=ms;last_idx=trk[0].arp_idx;}}
    if(!(n>20u&&lo>=118u&&hi<=132u))fprintf(stderr,"R7p reset: %u steps, intervals %u..%u ms\n",n,lo,hi);
    CHECK(n>20u&&lo>=118u&&hi<=132u);}
  seq_stop();run_ms(10);song.g[G_BPM]=100;seq_start();run_ms(1900);seq_stop();run_ms(10);seq_start();
  { uint32_t last_idx=trk[0].arp_idx,last_t=0,n=0,lo=1000,hi=0;
    for(uint32_t ms=0;ms<4800u;ms++){run_ms(1);
      if(trk[0].arp_idx!=last_idx){if(last_t){uint32_t d=ms-last_t;n++;if(d<lo)lo=d;if(d>hi)hi=d;}last_t=ms;last_idx=trk[0].arp_idx;}}
    if(!(n>20u&&lo>=143u&&hi<=158u))fprintf(stderr,"R7p 100bpm: %u steps, intervals %u..%u ms\n",n,lo,hi);
    CHECK(n>20u&&lo>=143u&&hi<=158u);}   /* 1/16 at 100 BPM: 150 ms */
  seq_stop();key(0);trk[0].p[P_AMODE]=AM_OFF;song.g[G_BPM]=120;
 }
 if(ux_on("R7o")){   /* B-B5: an import of a format without a Harmony block leaves the staged state off, not stale */
  r7_clean();harmony_on=1;{project_t cap;project_capture(&cap);}   /* (stages the live state: on) */
  (void)proj_import(&proj_scratch,"\0\0\0\0",4);                   /* not a project: the staged state is reset first */
  CHECK(proj_harmony.on==0&&proj_harmony.root[0]==0&&proj_harmony.follow[0]==1&&proj_harmony.follow[3]==1);
 }
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 /* B-B4: a cold boot has every track following the bus, as INIT and every load do (the first autosave stored the zeros) */
 if(ux_on("R7f"))CHECK(harmony_follow[0]==1u&&harmony_follow[1]==1u&&harmony_follow[2]==1u&&harmony_follow[3]==1u);   /* R7f */
 /* Actual target-feature UI, starting with the bus OFF. Every SELECT
  * detent must reach the next editable row, including across page borders. */
 web_buttons(1u<<B_SCL);run_ms(70);web_buttons(0);run_ms(70);
 CHECK(!ui.home&&cur_page()->graph==GR_SCALE&&!harmony_on);
 web_enc(EN_SELECT,3);run_ms(30);CHECK(cur_page()->graph==GR_LINK&&link_setup_row&&!harmony_on);
 web_enc(EN_K1,1);run_ms(30);CHECK(harmony_on);
 web_enc(EN_SELECT,2);run_ms(30);CHECK(cur_page()->graph==GR_HARMONY&&harmony_edit_dur);
 web_enc(EN_SELECT,-1);run_ms(30);CHECK(cur_page()->graph==GR_HARMONY&&!harmony_edit_dur);
 web_enc(EN_SELECT,-1);run_ms(30);CHECK(cur_page()->graph==GR_LINK&&link_setup_row);
 web_enc(EN_SELECT,-1);run_ms(30);CHECK(cur_page()->graph==GR_LINK&&!link_setup_row);
 web_enc(EN_SELECT,-2);run_ms(30);CHECK(cur_page()->graph==GR_SCALE);
 web_enc(EN_SELECT,99);run_ms(30);CHECK(cur_page()->graph==GR_HARMONY&&harmony_edit_dur);
 web_enc(EN_SELECT,99);run_ms(30);CHECK(cur_page()->graph==GR_HARMONY&&harmony_edit_dur);
 web_enc(EN_SELECT,-99);run_ms(30);CHECK(cur_page()->graph==GR_SCALE);
 web_enc(EN_SELECT,3);run_ms(30);web_enc(EN_K1,-1);run_ms(30);CHECK(!harmony_on);
 web_enc(EN_SELECT,99);run_ms(30);CHECK(cur_page()->graph==GR_LINK&&link_setup_row);
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 /* Explicit physical pitches, independent of implementation lookup tables. */
 CHECK(ACT_KEY_MASK==((1u<<20)|(1u<<21)|(1u<<22)|(1u<<23)|(1u<<24)|(1u<<26)));
 hit(20);CHECK(action_bank==3u);hit(22);CHECK(action_bank==0u);
 hit(22);CHECK(action_bank==ACT_BANK_JOURNEY);hit(22);CHECK(action_bank==ACT_BANK_HARMONY);
 hit(20);CHECK(action_bank==ACT_BANK_JOURNEY);hit(22);CHECK(action_bank==ACT_BANK_HARMONY);
 uint8_t b=action_bank;bank_hold(1u<<20);CHECK(action_setup&&action_bank==b);
 bank_hold(1u<<22);CHECK(!action_setup&&action_bank==b);
 bank_hold((1u<<20)|(1u<<22));CHECK(action_setup&&action_bank==b);
 bank_hold((1u<<20)|(1u<<22));CHECK(!action_setup&&action_bank==b);
 /* All four actions remain white keys; the old F# action is playable. */
 action_bank=3u;for(uint32_t i=0;i<4;i++){static const uint8_t k[4]={21,23,24,26};uint8_t m=trk[i].p[P_MUTE];hit(k[i]);CHECK(trk[i].p[P_MUTE]!=m);hit(k[i]);}
 key(1u<<25);CHECK(kb_note[25]!=KB_SILENT);key(0);
 for(uint32_t k=20;k<=26;k++)if(k!=25){key(1u<<k);CHECK(kb_note[k]==KB_SILENT);key(0);}
 /* Activating ACTION over an already held note preserves its lifetime. */
 action_enabled=0;action_refresh();key(1u<<21);CHECK(kb_note[21]!=KB_SILENT);
 action_enabled=1;action_refresh();run_ms(20);CHECK(kb_note[21]!=KB_SILENT);key(0);hit(21);CHECK(kb_note[21]==KB_SILENT);
 action_bank=ACT_BANK_HARMONY;harmony_on=1;roots();song.g[G_CLOCK]=0;song.g[G_BPM]=120;
 trk[0].p[P_ROOT]=0;trk[0].p[P_SCALE]=1;
 seq_start();run_ms(20);CHECK(chord()==0u);
 /* NEXT is queued and advances exactly once, even at a natural transition. */
 hit(24);CHECK(chord()==0u&&harmony_pending==H_NEXT);next_bar();CHECK(chord()==1u);
 hit(23);CHECK(chord()==1u);next_bar();CHECK(chord()==0u);
 hit(23);next_bar();CHECK(chord()==3u); /* wrap backwards */
 hit(26);next_bar();CHECK(chord()==0u&&!harmony_hold&&!harmony_shift);
 /* HOLD freezes Harmony only; a manual NEXT while held remains held. */
 hit(21);CHECK(harmony_hold);uint32_t p=perf_bars_q16;next_bar();next_bar();CHECK(chord()==0u&&perf_bars_q16>p&&song.playing);
 hit(24);next_bar();CHECK(chord()==1u&&harmony_hold);
 hit(21);CHECK(harmony_hold&&harmony_pending==H_RESUME);next_bar();CHECK(chord()==2u&&!harmony_hold);
 /* Duration starts afresh at an explicit jump; no alteration of prepared data. */
 hdur_set(1,1);hit(26);next_bar();hit(24);next_bar();CHECK(chord()==1u);next_bar();CHECK(chord()==1u);next_bar();CHECK(chord()==2u);
 roots();hit(26);next_bar();
 /* SHIFT is a scale-degree overlay and the encoder has a true zero. */
 bank_hold(1u<<22);CHECK(action_setup);web_enc(EN_K1,1);run_ms(30);CHECK(harmony_shift_want==1&&!harmony_shift);
 next_bar();CHECK(harmony_shift==1&&hroot_live(0)==2u&&hroot_live(1)==11u&&hroot_live(2)==7u&&hroot_live(3)==9u);
 CHECK(hroot_get(0)==0&&hroot_get(1)==9&&hroot_get(2)==5&&hroot_get(3)==7);
 CHECK((harmony_mask_root(0)&((1u<<2)|(1u<<5)|(1u<<9)))==((1u<<2)|(1u<<5)|(1u<<9)));
 {project_t p;project_store_t st;project_capture(&p);CHECK(proj_pack(&st,&p));CHECK(hroot_get(0)==0&&proj_harmony.root[0]==0&&harmony_shift==1);}
 web_enc(EN_K1,-2);run_ms(30);next_bar();CHECK(harmony_shift==-1&&hroot_live(0)==11u);
 bank_hold(1u<<20);CHECK(!action_setup);
 /* New targets are real MIDI-learn picks; rising edges do not autorepeat. */
 ui.ml.on=1;hit(21);CHECK(ui.ml.pick&&ui.ml.id==HP_HFIRST);ml_off();
 CHECK(ml_learn(80,0,HP_HFIRST+3u)==1u&&ml_learn(81,0,HP_HFIRST+2u)==1u&&ml_learn(82,0,HP_HFIRST+4u)==1u);
 midi(0xB0,80,127);next_bar();CHECK(chord()==0u&&!harmony_shift);
 midi(0xB0,81,127);next_bar();CHECK(chord()==1u);
 midi(0xB0,81,127);run_ms(30);CHECK(harmony_pending==H_NONE);
 midi(0xB0,82,64);next_bar();CHECK(harmony_shift==0);
 /* A BUS owner is rebuilt on SHIFT; both panel and MIDI NoteOff release it. */
 seq_stop();normal_off();roots();harmony_on=1;harmony_follow[0]=1;link_retrig[0]=1;
 trk[0].p[P_SCALE]=1;trk[0].p[P_ROOT]=0;trk[0].p[P_QUANT]=QN_OFF;trk[0].p[P_TRANS]=0;trk[0].p[P_CHRD]=CH_BUS;trk[0].p[P_VOIC]=VC_CLOSE;trk[0].p[P_LATCH]=LT_OFF;trk[0].p[P_VOICE]=V_POLY;trk[0].p[P_AMODE]=0;trk[0].p[P_MUTE]=0;
 seq_start();run_ms(20);hit(21); /* hold chord zero */
 key(1u<<7);CHECK(note_mask(0)==((1u<<0)|(1u<<4)|(1u<<7)));
 hp_midi(&trk[0],HP_HFIRST+4u,73);run_ms(30);next_bar();CHECK(note_mask(0)==((1u<<2)|(1u<<5)|(1u<<9)));
 ui.force=1;run_ms(30);screenshot();
 key(0);CHECK(!gates(0));midi(0x90,60,100);CHECK(gates(0));hp_midi(&trk[0],HP_HFIRST+4u,64);next_bar();midi(0x80,60,0);CHECK(!gates(0));
 /* A bank change never cancels an active HOLD; RESET clears all transient edits. */
 hit(22);CHECK(action_bank==1u&&harmony_hold);hit(20);CHECK(action_bank==ACT_BANK_HARMONY);
 hit(26);next_bar();CHECK(chord()==0u&&!harmony_hold&&!harmony_shift&&!harmony_shift_pending);
 /* Bus off clears queued work, and stopped edits never create notes. */
 seq_stop();hit(24);CHECK(chord()==1u&&!gates(0));hit(21);CHECK(harmony_hold);
 harmony_on=0;run_ms(30);CHECK(!harmony_hold&&!harmony_pending&&!harmony_shift);
 harmony_on=1;seq_start();run_ms(50);CHECK(chord()==0u);
 /* External USB clock uses the same bar boundary and Stop/Continue semantics. */
 song.g[G_CLOCK]=1;run_ms(50);clock_packet(0xFA);external_ms(200);hit(24);
 CHECK(chord()==0u&&harmony_pending==H_NEXT);external_ms(1900);CHECK(chord()==1u);
 hit(21);CHECK(harmony_hold);external_ms(2100);CHECK(chord()==1u);
 hit(24);CHECK(harmony_pending==H_NEXT);
 clock_packet(0xFC);external_ms(80);p=perf_bars_q16;external_ms(500);CHECK(!song.playing&&perf_bars_q16==p&&harmony_hold&&chord()==1u&&harmony_pending==H_NEXT);
 clock_packet(0xFB);external_ms(100);CHECK(song.playing&&harmony_hold&&harmony_pending==H_NEXT);
 clock_packet(0xFA);external_ms(100);CHECK(chord()==0u&&!harmony_hold);
 usability_fixes();
 round7_fixes();
 printf("HARMONY PANEL PASS: %u checks.\n",checks);return 0;
}
