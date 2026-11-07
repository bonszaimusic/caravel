/* SPDX-License-Identifier: GPL-3.0-only */
/* Real firmware panel, musical transport and MIDI-event regression; no device acceptance. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN harmony_panel_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void hit(uint32_t k){key(1u<<k);key(0);}
static void bank_hold(uint32_t mask){web_keys(mask);run_ms(650);web_keys(0);run_ms(70);}
static uint32_t chord(void){return harmony_chord_at(harmony_bar);}
static void next_bar(void){uint32_t b=perf_bars_q16>>16,g=0;while((perf_bars_q16>>16)==b&&g++<5000u)run_ms(1);CHECK(g<5000u);}
static void roots(void){const uint8_t r[4]={0,9,5,7};for(uint32_t i=0;i<4;i++){hroot_set(i,r[i]);hdur_set(i,0);}}
static uint32_t note_mask(uint32_t t){uint32_t n=0;for(uint32_t v=0;v<NVOICE;v++)if(trk[t].v[v].gate)n|=1u<<(trk[t].v[v].note%12);return n;}
static void clock_packet(uint32_t st){midi_enqueue(15u|st<<8,1);}
static uint32_t pulse_us;
static void external_ms(uint32_t n){while(n--){pulse_us+=1000;while(pulse_us>=20833u){pulse_us-=20833u;clock_packet(0xF8);}run_ms(1);}}
static void screenshot(void){
 const char *path=getenv("HARM_SCREEN");if(!path)return;FILE *f=fopen(path,"wb");CHECK(f!=NULL);
 fprintf(f,"P6\n240 240\n255\n");for(uint32_t i=0;i<240u*240u;i++){uint16_t q=web_fb[i];q=(q>>8)|(q<<8);uint8_t rgb[3]={(q>>11)*255/31,((q>>5)&63)*255/63,(q&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
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
 printf("HARMONY PANEL PASS: %u checks.\n",checks);return 0;
}
