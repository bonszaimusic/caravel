/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_PAIR_MORPH 1
#ifndef FELUCCA_CPU2
#define FELUCCA_CPU2 0
#endif
#include "../../web/emu/felucca_web.c"
#include <assert.h>
#if defined(__SANITIZE_ADDRESS__)
/* This sandbox cannot enumerate /proc for LeakSanitizer. Pair code allocates no heap memory. */
int __lsan_is_turned_off(void) { return 1; }
#endif
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static void run_ms(uint32_t ms) { while (ms--) web_render(44); }
static uint32_t gates(uint32_t t) { return web_pair_get(7,t); }
static void prepare(uint32_t e) { CHECK(web_pair_prepare(0,2,ENGI_SEM,e)); run_ms(50); }
static void key(uint32_t mask) { web_keys(mask); run_ms(50); }
static void midi(uint32_t st, uint32_t n, uint32_t v) { web_midi(st,n,v); run_ms(30); }
static void normal_off(void) { web_pair_panic(); run_ms(800); }
#ifndef PAIR_TEST_MAIN
#define PAIR_TEST_MAIN main
#endif
int PAIR_TEST_MAIN(void)
{
    web_nor_erase(); web_boot(); run_ms(1100);
    CHECK(P_COUNT == 114);
    prepare(ENGI_DRONE); CHECK(ep.budget == 1);
    CHECK(!pair_enable(0,0));
    key(1u<<7); CHECK(gates(0)==1 && gates(2)==1);
    uint32_t age = trk[0].v[0].age, ph = drone_state(&trk[2])->phase[0];
    web_pair_select(2); run_ms(50); CHECK(trk[0].v[0].age==age && gates(2)==1);
    CHECK(drone_state(&trk[2])->phase[0] != ph); /* both engines continue at endpoints */
    web_pair_morph(2,0); run_ms(30); CHECK(ep.effective==0 && gates(2)==1);
    web_pair_morph(0,32767); run_ms(30); CHECK(ep.effective==32767 && gates(0)==1);
    key(0); normal_off();
    /* Shared latch entered and released from either selected member. */
    trk[2].p[P_LATCH]=LT_STACK; run_ms(2); CHECK(trk[0].p[P_LATCH]==LT_STACK);
    key(1u<<7); key(0); CHECK(gates(0)==1 && gates(2)==1);
    web_pair_select(0); key(1u<<7); key(0); CHECK(!gates(0) && !gates(2));
    normal_off();
    /* Both member channels share one ownership/latch path; sustain, note-off and selection. */
    trk[0].p[P_LATCH]=LT_OFF; song.g[G_ROUTE]=ROUTE_TRACK; run_ms(4);
    midi(0x92,60,100); CHECK(gates(0)==1 && gates(2)==1);
    midi(0xB2,64,127); midi(0x82,60,0); CHECK(gates(0)==1 && gates(2)==1);
    midi(0xB2,64,0); CHECK(!gates(0) && !gates(2)); normal_off();
    /* Shared level/pan/mute edits from the follower do not alias engine/LFO parameters. */
    int16_t e0=trk[0].p[P_E0]; trk[2].p[P_E0]=37; trk[2].p[P_LRATE]=91;
    trk[2].p[P_PAN]=-64; trk[2].p[P_LEVEL]=80; run_ms(5);
    CHECK(ep.pan==-64 && ep.level==80 && trk[0].p[P_E0]==e0 && trk[0].p[P_LRATE]!=91);
    trk[2].p[P_MUTE]=1; run_ms(5); midi(0x90,62,100); CHECK(!gates(0) && !gates(2));
    trk[0].p[P_MUTE]=0; run_ms(5);
    /* Aggregate LFO1/LFO2 from both members exactly once, with independent phases/rates. */
    ep.base=16384;
    for (uint32_t m=0;m<2;m++) {
        track_t *t=&trk[m?2:0]; t->lfo_val=16384; t->lfo_fade=32767;
        lfo2_state[m?2:0].val=-8192; lfo2_state[m?2:0].fade=32767;
        CHECK(web_pair_lfo(m?2:0,0,MS_LFO,16)); CHECK(web_pair_lfo(m?2:0,1,MS_LFO2,16));
    }
    int32_t sum=ep.base;
    for(uint32_t m=0;m<2;m++) {
        track_t *t=&trk[m?2:0]; int32_t l=mulq15(t->lfo_val,t->lfo_fade);
        int32_t depth=mod_depth(t,l);
        sum+=mod_tsrc(t,MS_LFO,depth<32767 ? mulq15(l,depth) : l)*16/64;
        sum+=mod_tsrc(t,MS_LFO2,0)*16/64;
    }
    for(uint32_t i=0;i<30;i++) pair_mix_begin(CTL);
    CHECK(ep.effective==sum);
    CHECK(ep.weight[0][0]+ep.weight[1][0]==32767);
    int16_t before[P_COUNT]; memcpy(before,trk[0].p,sizeof before);
    mod_begin(&trk[0]); mod_end(&trk[0]); CHECK(!memcmp(before,trk[0].p,sizeof before));
    /* Pair slot exhaustion and legacy steals cannot leave one half gated. */
    prepare(0); CHECK(ep.budget==4);
    for(uint32_t n=60;n<72;n++) { trk_note_on(&trk[2],n,100); CHECK(gates(0)==gates(2)); CHECK(voices_busy()<=NVOICE); }
    CHECK(gates(0)==4); voice_kill(&trk[2].v[0]); CHECK(!trk[0].v[0].gate);
    for(uint32_t n=40;n<48;n++) { trk_note_on(&trk[1],n,100); CHECK(voices_busy()<=NVOICE); CHECK(gates(0)==gates(2)); }
    normal_off();
    /* One shared sequencer, recording from the selected follower, no follower playback. */
    prepare(0); web_pair_select(2); song.rec=1u<<2; seq_start();
    key(1u<<7); key(0); seq_stop();
    uint32_t na=0,nb=0;
    for(uint32_t s=0;s<NSTEP;s++){ na+=trk[0].step[s].n; nb+=trk[2].step[s].n; }
    CHECK(na>0 && nb==0); normal_off();
    /* Keyboard MIDI OUT remains one stream; the follower never owns a second ARP. */
    prepare(0); usb.config=1; mo_r=mo_w=0;
    web_pair_select(2); key(1u<<7); CHECK(mo_w==1); key(0); CHECK(mo_w==2);
    usb.config=0; normal_off();
    trk[2].p[P_AMODE]=1; run_ms(5); key(1u<<7); run_ms(300);
    CHECK(trk[0].nheld==1 && trk[2].nheld==0 && !trk[2].arp_note);
    CHECK(gates(0)==gates(2)); key(0); normal_off();
    trk[0].p[P_AMODE]=0; run_ms(5);
    /* Both performance-layer track mute keys apply to the whole pair. */
    perf_held=1u<<(PF_M1+2); perf_begin(CTL);
    CHECK((pf.act & ((1u<<PF_M1)|(1u<<(PF_M1+2))))==((1u<<PF_M1)|(1u<<(PF_M1+2))));
    perf_held=0; run_ms(50);
    web_pair_sequence(); ep.lane[0]=0; seq_start(); run_ms(25);
    CHECK(gates(0)==1 && gates(2)==1 && ep.effective==0);
    seq_stop(); normal_off();
    /* Silent endpoint has no new FX sends, but its real DSP still advances. */
    prepare(ENGI_DRONE); ep.base=0; run_ms(30);
    trk[2].p[P_CHOR]=trk[2].p[P_DLY]=trk[2].p[P_REV]=127;
    key(1u<<7);
    int32_t stereo[CTL*2]; mix_block(stereo,CTL);
    int64_t sends=0;
    for(uint32_t i=0;i<CTL;i++) sends+=llabs(send_c[i])+llabs(send_d[i])+llabs(send_r[i]);
    CHECK(sends==0 && gates(2)==1);
    ep.base=32767; run_ms(30); mix_block(stereo,CTL); sends=0;
    for(uint32_t i=0;i<CTL;i++) sends+=llabs(send_c[i])+llabs(send_d[i])+llabs(send_r[i]);
    CHECK(sends>0);
    trk[0].p[P_MUTE]=1; run_ms(5); mix_block(stereo,CTL); sends=0;
    for(uint32_t i=0;i<CTL;i++) sends+=llabs(send_c[i])+llabs(send_d[i])+llabs(send_r[i]);
    CHECK(sends==0); trk[2].p[P_MUTE]=0; key(0); normal_off();
    /* Versioned sidecar roundtrip; native project exports drop PAIR destinations. */
    CHECK(web_pair_lfo(2,1,MS_LFO2,-31)); ep.base=23456; ep.lane[12]=7654;
    trk[2].p[P_E1]=51; lfo2_config[2].p[L2_RATE]=73;
    uint32_t size=web_pair_snapshot_save(); CHECK(size==sizeof pair_snapshot);
    uint8_t saved[sizeof pair_snapshot]; memcpy(saved,pair_snapshot,size);
    project_capture(&proj_scratch); CHECK(proj_scratch.t[2].p[P_M2DST]==MD_OFF);
    web_pair_off(); CHECK(!ep.on);
    memcpy(pair_snapshot,saved,size); CHECK(web_pair_snapshot_load(size));
    CHECK(ep.on && ep.a==0 && ep.b==2 && ep.base==23456 && ep.lane[12]==7654);
    CHECK(trk[2].p[P_E1]==51 && lfo2_config[2].p[L2_RATE]==73 && trk[2].p[P_M2DST]==MD_PAIR);
    CHECK(!gates(0) && !gates(2));
    pair_snapshot[20]^=1; CHECK(!web_pair_snapshot_load(size)); CHECK(ep.on && ep.base==23456);
    /* Fixed reservations, unsupported mode and project init disable deterministically. */
    trk_note_on(&trk[0],60,100); voice_policy=VOICE_POLICY_FIXED; events_block(CTL);
    CHECK(!ep.on && !gates(0) && !gates(2));
    prepare(ENGI_DRONE); trk[2].p[P_E3]=3; events_block(CTL); CHECK(ep.on);
    prepare(0); trk_note_on(&trk[0],60,100); trk[2].p[P_VOICE]=V_LEGATO; events_block(CTL);
    CHECK(ep.on && ep.mode==V_LEGATO && !gates(0) && !gates(2));
    prepare(0); trk_note_on(&trk[2],60,100); project_init_runtime(); CHECK(!ep.on && !gates(0) && !gates(2));
    /* Optional character: default OFF, smooth switch, equal-power midpoint, endpoint identity. */
    prepare(ENGI_DRONE); CHECK(!ep.motion && !ep.motion_mix);
    ep.base=ep.effective=16384;
    CHECK(web_pair_motion(1));
    int32_t previous=16384;
    for(uint32_t block=0;block<20;block++) {
        pair_mix_begin(CTL);
        for(uint32_t i=0;i<CTL;i++) {
            CHECK(abs(ep.weight[1][i]-previous)<=20); previous=ep.weight[1][i];
        }
    }
    CHECK(ep.motion_mix==32767 && abs(ep.weight[0][0]-16384)<4 && abs(ep.weight[1][0]-16384)<4);
    for(int32_t m=0;m<32768;m+=257) {
        double reference=sin(1.5707963267948966*(double)m/32768.0)*32767;
        CHECK(fabs(pair_power_gain(m)-reference)<12.0);
    }
    CHECK(pair_power_gain(0)==0 && pair_power_gain(32767)==32767);
    int16_t saved_tone;
    uint32_t id=pair_tone_begin(&trk[0],&saved_tone);
    CHECK(id==P_E4 && trk[0].p[id]==saved_tone+12); trk[0].p[id]=saved_tone;
    id=pair_tone_begin(&trk[2],&saved_tone);
    CHECK(id==P_E7 && trk[2].p[id]==saved_tone-12); trk[2].p[id]=saved_tone;
    int16_t stored[P_COUNT]; int32_t dry[CTL];
    memcpy(stored,trk[0].p,sizeof stored); track_render(&trk[0],dry,CTL);
    CHECK(!memcmp(stored,trk[0].p,sizeof stored));
    trk[0].p[P_M1SRC]=MS_LFO; trk[0].p[P_M1DST]=MD_E1+4; trk[0].p[P_M1AMT]=30;
    memcpy(stored,trk[0].p,sizeof stored); mod_begin(&trk[0]); track_render(&trk[0],dry,CTL); mod_end(&trk[0]);
    CHECK(!memcmp(stored,trk[0].p,sizeof stored));
    trk[0].p[P_M1SRC]=MS_OFF;
    for(uint32_t m=0;m<2;m++) {
        ep.base=ep.effective=m?32767:0; pair_mix_begin(CTL);
        CHECK(ep.weight[m][0]==32767 && ep.weight[!m][0]==0);
        CHECK(!pair_tone_begin(&trk[0],&saved_tone) && !pair_tone_begin(&trk[2],&saved_tone));
    }
    uint32_t motion_size=web_pair_snapshot_save(); CHECK(motion_size==4032 && !memcmp(pair_snapshot,"EPM2",4));
    CHECK(web_pair_snapshot_load(motion_size) && ep.motion);
    memcpy(pair_snapshot,"EPM1",4); pair_snapshot[14]=0;
    uint32_t legacy_hash=proj_hash(pair_snapshot+12,sizeof pair_snapshot-12);
    for(uint32_t i=0;i<4;i++) pair_snapshot[8+i]=legacy_hash>>(8*i);
    CHECK(web_pair_snapshot_load(motion_size) && !ep.motion);
    ep.base=ep.effective=16384; web_pair_motion(1);
    for(uint32_t i=0;i<20;i++)pair_mix_begin(CTL);
    web_pair_motion(0); for(uint32_t i=0;i<20;i++)pair_mix_begin(CTL);
    CHECK(!ep.motion_mix && ep.weight[0][0]==16383 && ep.weight[1][0]==16384);
    CHECK(!pair_tone_begin(&trk[0],&saved_tone));
    printf("pair: %u checks passed\n",checks);
    return 0;
}
