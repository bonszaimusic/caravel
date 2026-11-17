/* SPDX-License-Identifier: GPL-3.0-only */
/* A retriggered live owner must not release a pitch CAPTURE still owns. */
#define CARAVEL_UX_CAPTURE 1
#define FORMAT2_EDITOR_MAIN capture_owner_fixture_main
#include "format2_editor_test.c"
static int capture_gate(unsigned note) {
 for(unsigned v=0;v<NVOICE;v++)if(trk[0].v[v].active&&trk[0].v[v].gate&&trk[0].v[v].note==note)return 1;
 return 0;
}
static void settle(void) { for(unsigned i=0;i<20;i++)web_render(44); }
static void capture_owner_case(unsigned ext,unsigned midi) {
 web_nor_erase();web_boot();for(unsigned i=0;i<1200;i++)web_render(44);
 song.sel=0;seq_stop();pair_disable();settle();
 track_t *t=&trk[0];t->p[P_VOICE]=V_POLY;t->p[P_CHRD]=CH_OFF;t->p[P_LATCH]=LT_OFF;t->p[P_AMODE]=0;
 t->p[P_CSRC]=ext;t->p[P_CEXT]=1;t->p[P_COCT]=0;
 t->p[P_QUANT]=QN_OFF;t->p[P_TRANS]=0;t->p[P_SCALE]=1;t->p[P_ROOT]=0;
 harmony_on=1;harmony_bar=0;hroot_set(0,0);harmony_follow[0]=1;link_retrig[0]=1;
 unsigned raw=ext?60:58,old=ext?59:60,next=ext?55:57,key=ext?7:5;
 cap_play_on(0,raw,70);assert(capture_gate(old));
 if(midi)web_midi(0x90,raw,100);else web_keys(1u<<key);
 settle();assert(capture_gate(old));
 hroot_set(0,9);ret_harm=1;link_retrigger(0);ret_harm=0;
 assert(capture_gate(old)&&capture_gate(next)&&cap_note_held(t,old));
 if(midi)web_midi(0x80,raw,0);else web_keys(0);
 settle();assert(capture_gate(old)&&!capture_gate(next));
 cap_play_off(0,raw);assert(!capture_gate(old));
}
int main(void) {
 for(unsigned ext=0;ext<2;ext++)for(unsigned midi=0;midi<2;midi++)capture_owner_case(ext,midi);
 puts("CHORD EXT CAPTURE ownership: loop pitch survives MIDI/key retrigger, with SOURCE OFF and HARM; both release exactly");
}
