/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY TRANSPORT on an EXTERNAL MIDI clock (USB), simulated through the real MIDI queue and the real firmware: the transport's musical time
 * follows the pulses and not the BPM setting, a Stop freezes it, Continue resumes it where it stood, a Start counts, and the journey legs and WAIT follow.
 * This is a host simulation with ideal pulse spacing; jitter, a real cable and a real sequencer on the device are not covered. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_extclock_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t k){key(1u<<(k?action_keys[k-1u]:ACT_BANK_NEXT));key(0);}
static int32_t absd(int32_t a){return a<0?-a:a;}
static uint32_t pulse_acc;                                   /* the 24 ppqn clock at `ext_bpm`, in microseconds */
static uint32_t ext_bpm=100;
static void mpkt(uint32_t status){midi_enqueue(0xFu|status<<8,1);}
static void clock_ms(uint32_t ms){                            /* run ms of wall time, sending the pulses that fall in it */
 uint32_t period_us=60000000u/(ext_bpm*24u);
 while(ms--){pulse_acc+=1000u;while(pulse_acc>=period_us){pulse_acc-=period_us;mpkt(0xF8u);}run_ms(1);}
}
static uint32_t bars_x1000(void){return (uint32_t)(((uint64_t)perf_bars_q16*1000u)>>16);}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 song.g[G_BPM]=120;                                            /* the setting is NOT what the external clock runs at */
 hp_eng[0].scope=HP_RM_TRACK;hp_eng[0].bars=2;hp_eng[0].shape=HP_RM_LINEAR;hp_jn[0].wait=0;
 hp_rm_snapshot_clear();hp_rm_reset_all();
 trk[0].p[P_REV]=10;trk[0].p[P_PAN]=-40;action_snap_slot=0;CHECK(hp_rm_snapshot_store(0));
 trk[0].p[P_REV]=90;trk[0].p[P_PAN]=40;action_snap_slot=1;CHECK(hp_rm_snapshot_store(1));
 while(action_bank!=ACT_BANK_JOURNEY)tap(0);

 song.g[G_CLOCK]=1;run_ms(100);                                /* external clock, USB */
 mpkt(0xFAu);clock_ms(1500);                                   /* Start, then 1.5 s of clock at 100 BPM (1 bar = 2.4 s) */
 CHECK(song.playing);
 uint32_t s0=perf_starts;
 {uint32_t b=bars_x1000();CHECK(b>400u&&b<750u);}              /* about 0.6 bar, as the pulses say: not the 0.75 bar the 120 BPM setting would give */
 /* CONTINUE: a leg of 2 bars is 192 pulses, whatever the tempo */
 hp_j.tp=1;trk[0].p[P_REV]=50;trk[0].p[P_PAN]=0;action_snap_slot=0;tap(1);run_ms(40);
 CHECK(hp_j.on&&hp_rm.beat&&hp_rm.state==HP_RM_RECALL);
 clock_ms(2400);                                               /* one bar at 100 BPM: half the leg */
 CHECK(hp_rm.phase>32768u-3500u&&hp_rm.phase<32768u+3500u);
 /* a tempo change of the external clock speeds the leg up */
 {uint32_t p0=hp_rm.phase;ext_bpm=200;clock_ms(600);        /* 0.6 s at 200 BPM is half a bar: a quarter of the leg (a quarter bar, 8192, at the old tempo) */
  uint32_t d=hp_rm.phase-p0;CHECK(d>12000u&&d<20500u);ext_bpm=100;}
 hp_j.on=hp_j.paused=0;hp_rm_reset();
 /* Stop freezes, Continue resumes where it stood, Start counts */
 hp_j.tp=1;trk[0].p[P_REV]=50;trk[0].p[P_PAN]=0;action_snap_slot=0;tap(1);run_ms(40);
 clock_ms(1200);uint32_t at_stop=hp_rm.phase;CHECK(at_stop>8000u&&at_stop<40000u);
 mpkt(0xFCu);clock_ms(100);CHECK(!song.playing);
 {uint32_t b=perf_bars_q16;clock_ms(1500);CHECK(perf_bars_q16==b&&hp_rm.phase<=at_stop+300u);}             /* clock pulses keep coming, the transport is stopped */
 mpkt(0xFBu);clock_ms(1000);                                    /* Continue: the position is kept, no new Start */
 CHECK(song.playing&&perf_starts==s0&&hp_rm.phase>at_stop+8000u);
 CHECK(hp_j.on&&hp_j.tgt==0u);
 /* START: a new Start begins the journey again */
 hp_j.tp=2;mpkt(0xFCu);clock_ms(100);mpkt(0xFAu);clock_ms(300);
 CHECK(perf_starts==s0+1u&&hp_j.on&&hp_j.tgt==0u&&hp_rm.phase<15000u&&strstr(ui.msg,"START")!=NULL);
 hp_j.on=hp_j.paused=0;hp_rm_reset();
 printf("JOURNEY EXTERNAL CLOCK PASS: %u checks.\n",checks);
 return 0;
}
