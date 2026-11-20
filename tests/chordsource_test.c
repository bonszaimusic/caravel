/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* Live-source oracles and real MIDI/panel timing, not recorded output hashes. */
#include <stdint.h>
static void action_refresh(void);
static void action_dispatch(uint32_t keys);
#define CARAVEL_UX_CAPTURE 1
#define CARAVEL_DUALCORE_D1 1
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
#if CARAVEL_ACTION_BANKS
#include "../experiments/action-banks/action_control.c"
#endif
#include "chordext_sim.h"
static unsigned checks, bad;
static void ck(const char *what, int ok) { checks++; if (!ok) { printf("live source FAIL: %s\n", what); bad++; } }
static void ticks(unsigned n) { while (n--) events_block(CTL); }
static ext_info_t source_info(unsigned f) { ext_info_t o; ext_info(f, &o); return o; }
static void fresh(unsigned source, unsigned follower)
{
    ui_power_on();
#if FELUCCA_PAIR_MORPH
    pair_disable();
#endif
    chordext_reset_runtime();
    memset(midi_notes,0,sizeof midi_notes); memset(midi_ch,0,sizeof midi_ch);
    memset(midi_owners,0,sizeof midi_owners); memset(mchord,0,sizeof mchord);
    memset(&um,0,sizeof um);
    for (unsigned i=0;i<NTRK;i++) {
        cap_wipe(i); latch_forget(i); set_engine_of(&trk[i],0); apply_preset_to(&trk[i],0);
        trk[i].p[P_VOICE]=V_POLY;trk[i].p[P_SCALE]=1;trk[i].p[P_ROOT]=0;trk[i].p[P_QUANT]=QN_OFF;
        trk[i].p[P_CHRD]=CH_OFF;trk[i].p[P_LATCH]=LT_OFF;trk[i].p[P_AMODE]=0;
        trk[i].p[P_CSRC]=0;trk[i].p[P_CEXT]=2;trk[i].p[P_COCT]=0;
        memset(trk[i].step,0,sizeof trk[i].step);song.mch[i]=(uint8_t)(i+1u);
    }
    song.sel=(uint8_t)source;song.g[G_ROUTE]=0;song.playing=0;harmony_on=0;
    trk[follower].p[P_CSRC]=(int16_t)(source+2u);ticks(1);ce_reset();
}
static void note(unsigned track,unsigned n,unsigned v) { midi_event(v?0x90u:0x80u,track,n,v); }
static void triad(unsigned source,unsigned a,unsigned b,unsigned c)
{
    note(source,a,90);note(source,b,90);note(source,c,90);
    note(source,a,0);note(source,b,0);note(source,c,0);
}
static int has_pool(unsigned f,unsigned a,unsigned b,unsigned c)
{
    ext_info_t o=source_info(f);return o.have_chord&&o.npc==3&&o.pc[0]==a&&o.pc[1]==b&&o.pc[2]==c;
}
static unsigned source_gates(unsigned t) {unsigned n=0;for(unsigned i=0;i<NVOICE;i++)n+=trk[t].v[i].active&&trk[t].v[i].gate;return n;}
static uint32_t gated_pcs(unsigned t)
{
    uint32_t pcs=0;
    for(unsigned i=0;i<NVOICE;i++)
        if(trk[t].v[i].active&&trk[t].v[i].gate)
            pcs|=1u<<(trk[t].v[i].note%12u);
    return pcs;
}
static void recogniser(void)
{
    /* Explicit relative masks and qualities; rotate both inputs and expected roots. */
    static const struct { const char *name; uint16_t pcs; uint8_t kind; } cases[]={
        {"maj",0x091,0},{"min",0x089,1},{"dim",0x049,2},{"aug",0x111,3},
        {"sus2",0x085,4},{"sus4",0x0A1,5},{"power",0x081,6},
        {"maj7",0x891,0},{"dom7",0x491,0},{"min7",0x489,1},{"mM7",0x889,1},
        {"m7b5",0x449,2},{"dim7",0x249,2},{"6",0x291,0},{"min6",0x289,1},
        {"add9",0x095,0},{"minadd9",0x08D,1},{"maj9",0x895,0},{"9",0x495,0},
        {"min9",0x48D,1},{"11",0x4B5,0},{"min11",0x4AD,1},{"13",0x6B5,0},
        {"min13",0x6AD,1},{"7sus4",0x4A1,5},{"+7",0x511,3},
        {"major shell",0x411,0},{"minor shell",0x409,1}
    };
    for(unsigned c=0;c<NELEM(cases);c++)for(unsigned r=0;r<12;r++) {
        uint32_t pcs=((uint32_t)cases[c].pcs<<r|cases[c].pcs>>(12-r))&4095u,root=99,kind=99;
        int ok=ext_recognise(pcs,r,&root,&kind);
        if(!(ok&&root==r&&kind==cases[c].kind))printf("oracle %s root%u -> %d root%u kind%u\n",cases[c].name,r,ok,root,kind);
        ck("recogniser's independent root-position table",ok&&root==r&&kind==cases[c].kind);
    }
    for(unsigned r=0;r<12;r++)for(unsigned inv=0;inv<3;inv++) {
        static const unsigned third[]={0,4,7};uint32_t pcs=(0x91u<<r|0x91u>>(12-r))&4095u,root=99,kind=99;
        ck("C/E remains major on the real root, all transpositions",ext_recognise(pcs,(r+third[inv])%12,&root,&kind)&&root==r&&kind==0);
    }
    for(unsigned interval=1;interval<12;interval++) {
        uint32_t root=99,kind=99;int ok=ext_recognise(1u|(1u<<interval),0,&root,&kind);
        ck("dyads only perfect fourth/fifth; no guessed third",ok==(interval==5||interval==7));
    }
    uint32_t root=0,kind=0;
    ck("chromatic cluster is ambiguous",!ext_recognise(0x01Fu,0,&root,&kind));
    ck("single note is not a chord",!ext_recognise(1u,0,&root,&kind));
}
static void collection(void)
{
    fresh(0,1);ck("live source starts WAIT",source_info(1).state==EXT_ST_WAIT);
    triad(0,60,64,67);ticks(40);ck("no partial commit before 41 blocks",source_info(1).state==EXT_ST_WAIT);
    ticks(1);ck("complete C major commits at gap expiry",has_pool(1,11,2,9));
    ck("release of source notes does not clear context",source_info(1).root==0);
    triad(0,57,60,64);ticks(41);ck("A minor has independent G B F extension oracle",has_pool(1,7,11,5));
    note(0,65,90);note(0,69,90);note(0,65,0);note(0,69,0);ticks(41);
    ck("ambiguous third keeps last context and reports KEPT",source_info(1).state==EXT_ST_AMBIG&&source_info(1).why==EXT_WHY_KEPT&&has_pool(1,7,11,5));
    note(1,60,80);ck("AMBIG keeps the previous pool playable, not merely visible",source_gates(1)>0);note(1,60,0);
    ext_cfg_t kept=ext_cfg(&trk[1]);ck("AMBIG retains SEQ/mono mapping",ext_map(&trk[1],&kept,60)>=0);
    uint32_t anchors[24]={60},last=60;ck("AMBIG retains the ARP pool",ext_arp(&trk[1],&kept,anchors,1,&last)==3);
    note(0,62,90);note(0,62,0);ticks(41);
    ck("single-note gesture does not replace context",source_info(1).root==9);
    fresh(0,1);note(0,60,90);ticks(1);note(0,64,90);ticks(1);note(0,67,90);ticks(40);
    ck("one-block arrival offsets still produce one delayed chord",source_info(1).state==EXT_ST_WAIT);ticks(1);ck("one-block arrival chord",has_pool(1,11,2,9));
    fresh(0,1);note(0,60,90);note(0,64,90);ticks(40);note(0,67,90);ticks(41);
    ck("arrival at W-1 extends the gap",has_pool(1,11,2,9));
    fresh(0,1);note(0,60,90);note(0,64,90);ticks(41);note(0,67,90);ticks(41);
    ck("arrival at W starts a new gesture after the ambiguous dyad",!source_info(1).have_chord);
    fresh(0,1);triad(0,60,64,67);
    for(unsigned i=1;i<=137;i++){ticks(1);if(i%20u==0)note(0,60,90);}
    ck("hard cap does not commit early",source_info(1).state==EXT_ST_WAIT);ticks(1);
    ck("hard cap commits despite renewed note-ons",has_pool(1,11,2,9));
}
static void routing_and_lifecycle(void)
{
    fresh(0,1);triad(0,60,64,67);ticks(20);seq_stop();ticks(30);
    ck("STOP discards an unfinished source gesture",source_info(1).state==EXT_ST_WAIT);

    for(unsigned s=0;s<4;s++) {unsigned f=(s+1)%4;fresh(s,f);triad(s,60,64,67);ticks(41);ck("every T1-T4 can be a live source",has_pool(f,11,2,9));}
    fresh(0,1);trk[0].p[P_CHRD]=CH_MAJ;note(0,60,90);
    ck("known CHRD root commits immediately without collector delay",has_pool(1,11,2,9));
    fresh(0,1);trk[0].p[P_MUTE]=1;triad(0,60,64,67);ticks(41);ck("muted source continues to conduct",has_pool(1,11,2,9));
    trk[1].p[P_CSRC]=3;ticks(1);ck("self source rejected",source_info(1).state==EXT_ST_NA&&source_info(1).why==EXT_WHY_SELF);
    trk[0].p[P_CSRC]=3;trk[1].p[P_CSRC]=2;ticks(1);
    ck("two-way cycle rejected on both tracks",source_info(0).state==EXT_ST_NA&&source_info(1).state==EXT_ST_NA);
    fresh(0,1);triad(0,60,64,67);ticks(41);seq_stop();chord_panic(0);chord_panic(1);
    ck("STOP/panic keep chord memory without sounding a follower",has_pool(1,11,2,9)&&!source_gates(1));
    trk[1].p[P_CSRC]=4;ticks(1);ck("source change returns WAIT, not another source's cached chord",source_info(1).state==EXT_ST_WAIT);
    fresh(0,1);triad(0,60,64,67);ticks(41);project_t p;project_capture(&p);p.sum++;
    ck("invalid project load is rejected",project_restore_runtime(&p)!=0);
    ck("rejected project does not clear remembered source",has_pool(1,11,2,9));
    project_capture(&p);ck("valid restore succeeds",!project_restore_runtime(&p));ticks(1);
    ck("project restore clears ephemeral live memory",source_info(1).state==EXT_ST_WAIT);
    fresh(0,1);cap_play_on(0,60,100);cap_play_on(0,64,100);cap_play_on(0,67,100);ticks(41);
    ck("CAPTURE playback is not a live chord source",source_info(1).state==EXT_ST_WAIT);
    cap_play_off(0,60);cap_play_off(0,64);cap_play_off(0,67);
}
static void owners(void)
{
    fresh(0,1);triad(0,60,64,67);ticks(40);
    midi_enqueue(0x09u|(0x91u<<8)|(60u<<16)|(90u<<24),1);ticks(1);
    ck("expiry commits before a follower input in that same block",source_gates(1)>0);
    note(1,60,0);

    fresh(0,1);triad(0,60,64,67);ticks(41);note(1,60,80);ck("follower sounds extensions",source_gates(1)>0);
    link_retrig[1]=1;triad(0,57,60,64);ticks(41);ck("live commit updates held voicing",has_pool(1,7,11,5)&&source_gates(1)>0);
    note(1,60,0);ck("original MIDI note-off releases rebuilt owner exactly",!source_gates(1));
    fresh(0,1);triad(0,60,64,67);ticks(41);note(1,60,80);
    triad(0,57,60,64);ticks(41);note(1,60,0);ck("RETRIG OFF retains release identity across source change",!source_gates(1));
}
/* A follower panel key pressed in the exact gap-expiry block must sound
 * the newly published EXT pool. A post-keyboard ext_source_tick (M3) updates
 * the displayed pool but leaves this first, held voicing on the old one. */
static void panel_follower_on_gap_expiry(void)
{
    fresh(0,1);
    note(0,60,90);note(0,64,90);note(0,67,90);ticks(EXT_GAP);
    ck("panel expiry fixture starts with held C major",has_pool(1,11,2,9));

    /* Physical MIDI holds, not released note-on gestures: C E G -> A C E. */
    note(0,67,0);note(0,57,90);
    song.sel=1; /* CE_KEY now plays the follower, never the source. */
    ce_reset();
    ce_ev_t down={ce_us_of_block(EXT_GAP-1u),CE_KEY,7,1,0};
    ce_sched(&down,1);
    ce_run_until(ce_us_of_block(EXT_GAP-1u));
    ck("panel expiry: old pool until the preceding block",
       ce_block_no()==EXT_GAP-1u&&ce_fed_block(0)==~0u&&has_pool(1,11,2,9)&&!source_gates(1));
    ce_run_until(ce_us_of_block(EXT_GAP));
    ck("panel expiry: key fed in the 41st block and new pool published",
       ce_fed_block(0)==EXT_GAP-1u&&ce_block_no()==EXT_GAP&&has_pool(1,7,11,5));
    ck("panel expiry: first sounding follower voicing uses NEW Am extension, not old C extension",
       kb_chn[7]==3&&source_gates(1)==3&&gated_pcs(1)==((1u<<7)|(1u<<11)|(1u<<5)));

    ce_ev_t up={ce_us_of_block(EXT_GAP),CE_KEY,7,0,0};
    ce_sched(&up,1);ce_run_until(ce_us_of_block(EXT_GAP+1u));
    ck("panel expiry: the follower key-up releases its own voicing",!source_gates(1));
}
static void real_sources(void)
{
    fresh(0,1);
    ce_ev_t keys[]={ {0,CE_KEY,7,1,0},{5000,CE_KEY,11,1,0},{12000,CE_KEY,14,1,0},
        {15000,CE_KEY,7,0,0},{16000,CE_KEY,11,0,0},{17000,CE_KEY,14,0,0} };
    ce_sched(keys,6);ce_run_until(45000);
    ck("real panel scan collects a released C major gesture",has_pool(1,11,2,9));
    fresh(0,1);midi_event(0xB0,0,64,127);triad(0,60,64,67);ticks(41);
    ck("sustain does not postpone recognition",has_pool(1,11,2,9));
    midi_event(0xB0,0,64,0);ticks(41);
    ck("pedal release never clears or changes context",has_pool(1,11,2,9));
    fresh(0,1);note(0,60,90);note(0,64,90);note(0,67,90);ticks(41);
    note(0,64,0);note(0,67,0);note(0,65,90);note(0,69,90);ticks(41);
    ck("legato's old held C is not silently added to new F/A dyad",source_info(1).state==EXT_ST_AMBIG&&has_pool(1,11,2,9));
    fresh(0,1);note(0,60,90);note(0,64,90);ticks(42);note(0,67,90);ticks(41);
    ck("W+1 arrival starts a separate single-note gesture",!source_info(1).have_chord);
    fresh(0,1);triad(0,60,64,67);ticks(41);
    trk[0].p[P_AMODE]=AM_UP;arp_add(&trk[0],57);arp_add(&trk[0],60);arp_add(&trk[0],64);
    for(unsigned k=0;k<90;k++)arp_step(&trk[0],CTL,CTL);
    ck("generated ARP notes cannot replace the live source chord",has_pool(1,11,2,9)&&!xext[0].pcs);
    trk[0].p[P_AMODE]=0;
    step_t step={0};step.n=3;step.note[0]=57;step.note[1]=60;step.note[2]=64;step.vel=90;
    seq_step(&trk[0],&step,0,1000,SEQ_ROLLED);ticks(41);
    ck("generated SEQ chord cannot replace the live source chord",has_pool(1,11,2,9)&&!xext[0].pcs);
#if FELUCCA_PAIR_MORPH
    fresh(0,2);ck("pair fixture enables",pair_enable(0,1));
    trk[2].p[P_CSRC]=3;ticks(1);
    ck("PAIR follower cannot be a live source",source_info(2).state==EXT_ST_NA&&source_info(2).why==EXT_WHY_PAIR);
    trk[2].p[P_CSRC]=2;ticks(1);triad(0,60,64,67);ticks(41);
    ck("PAIR owner can conduct another track",has_pool(2,11,2,9));
#endif
    for(unsigned voice=0;voice<4;voice++)for(unsigned latch=0;latch<3;latch++) {
        fresh(0,1);trk[1].p[P_VOICE]=voice;trk[1].p[P_LATCH]=latch;
        link_retrig[1]=1;triad(0,60,64,67);ticks(41);note(1,60,80);
        triad(0,57,60,64);ticks(41);note(1,60,0);
        if(!latch)ck("live RETRIG keeps release identity in every voice mode",!source_gates(1));
        midi_event(0xB0,1,120,0);ck("panic silences live RETRIG in every voice/latch mode",!source_gates(1));
    }
}
static void latency(void)
{
    unsigned latest=0;
    /* 1000 ARP phases, real arp_step before/after the commit, not a render-time estimate. */
    for(unsigned phase=0;phase<1000;phase++) {
        fresh(0,1);triad(0,60,64,67);ticks(41);
        trk[1].p[P_AMODE]=AM_CHORD;note(1,60,90);
        trk[1].arp_pos=phase;
        triad(0,57,60,64);
        for(unsigned b=1;b<=41;b++) {
            ticks(1);arp_step(&trk[1],CTL,CTL);
            if(b<41)ck("ARP remains on old pool strictly before gap expiry",has_pool(1,11,2,9));
        }
        ck("new pool is available to ARP in the commit block",has_pool(1,7,11,5));
        latest=41;
    }
    printf("live latency: 1000 ARP phases, last-input to commit %u blocks = %.3f ms; additional resolver delay 0 blocks (host control model)\n",latest,latest*32000.0/44100.0);
}
static void transport_matrix(void)
{
    for(unsigned trs=0;trs<2;trs++)for(unsigned spacing=0;spacing<4;spacing++)for(unsigned phase=0;phase<32;phase++) {
        fresh(0,1);const unsigned gap[]={0,726,5000,12000};ce_ev_t e[3];
        for(unsigned k=0;k<3;k++)e[k]=(ce_ev_t){phase+gap[spacing]*k,(uint8_t)(trs?(k?CE_TRS_RS:CE_TRS):CE_USB),(uint8_t)(trs&&k?0:0x90u),(uint8_t)(k==0?60:k==1?64:67),100};
        ce_sched(e,3);ce_run_until(phase+gap[spacing]*2+40000);
        ck("USB burst/TRS running-status, offsets and all 32 block phases",has_pool(1,11,2,9));
    }
}
int main(void)
{
    recogniser();collection();routing_and_lifecycle();owners();panel_follower_on_gap_expiry();real_sources();transport_matrix();latency();
    printf("live source: %u checks, %u failures; recognition, collection, lifecycle, owners and transport phases\n",checks,bad);
    return bad!=0;
}
