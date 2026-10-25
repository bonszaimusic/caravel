/* SPDX-License-Identifier: GPL-3.0-only */
/* Logical keys must not be confused with the single DRONE CELL host voice. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define WEB_FLASH_TEST 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"

static unsigned cell, poly;
static void setup(unsigned engine, unsigned topology, unsigned reverse)
{
    web_keys(0);web_pair_panic();run_ms(900);
    CHECK(web_pair_prepare(0,2,reverse?ENGI_DRONE:engine,reverse?engine:ENGI_DRONE));
    cell=reverse?0:2;poly=reverse?2:0;
    trk[cell].p[P_E3]=topology;run_ms(10);
    CHECK(ep.cells==(reverse?1:2));CHECK(ep.budget==4);
    song.g[G_ROUTE]=ROUTE_TRACK;mch_set(0,0);mch_set(2,2);
}
static unsigned held(unsigned t, unsigned n)
{
    return (drone_state(&trk[t])->held[n>>5]>>(n&31))&1;
}
static void on(unsigned n){trk_note_on(&trk[2],n,100);}
static void off(unsigned n){trk_note_off(&trk[0],n);}
static void chord(unsigned count)
{
    for(unsigned n=60;n<60+count;n++) on(n);
    CHECK(gates(cell)==1);CHECK(gates(poly)==(count>4?4:count));
    CHECK(ep.key_count==count);CHECK(voices_busy()<=NVOICE);
}

int main(void)
{
    web_nor_erase();web_boot();run_ms(1400);CHECK(P_COUNT==114);CHECK(P_E3==109);
    for(unsigned e=0;e<2;e++)for(unsigned topo=3;topo<=5;topo++)for(unsigned rev=0;rev<2;rev++) {
        setup(e?ENGI_HADES:ENGI_SEM,topo,rev);chord(4);
        for(unsigned n=60;n<64;n++) CHECK(held(cell,n));
        /* Release the most recent host pitch first; three other keys stay held. */
        off(63);CHECK(gates(cell)==1);CHECK(gates(poly)==3);CHECK(!held(cell,63));
        on(62);CHECK(ep.key_count==3);CHECK(gates(poly)==3);
        for(unsigned n=60;n<63;n++) off(n);
        CHECK(!gates(cell)&&!gates(poly));CHECK(!ep.key_count);
        run_ms(30);chord(8);CHECK(ep.key_count==8);
        for(unsigned n=60;n<68;n++) CHECK(held(cell,n));
        for(unsigned n=67;n>=60;n--) off(n);
        CHECK(!gates(cell)&&!gates(poly));CHECK(!drone_any_held(drone_state(&trk[cell])));

        /* A finished ordinary slot is reused before a still-held one. */
        chord(4);trk[poly].v[2].active=0;trk[poly].v[2].gate=0;
        on(70);CHECK(trk[poly].v[2].note==70);CHECK(trk[poly].v[0].note==60);
        CHECK(ep.key_count==5&&held(cell,62));
        voice_kill(&trk[poly].v[1]);CHECK(ep.key_count==5);CHECK(gates(cell)==1);
        CHECK(held(cell,61));CHECK(trk[poly].v[0].gate&&trk[poly].v[2].gate);
        on(71);CHECK(trk[poly].v[1].note==71);CHECK(voices_busy()<=NVOICE);
        voice_kill(&trk[cell].v[0]);CHECK(!ep.key_count);CHECK(!gates(poly)&&!gates(cell));
        CHECK(!drone_any_held(drone_state(&trk[cell])));

        setup(e?ENGI_HADES:ENGI_SEM,topo,rev);
        /* Actual panel chord from the follower, both morph endpoints audible. */
        web_pair_select(2);web_keys((1u<<0)|(1u<<4)|(1u<<7));run_ms(30);
        CHECK(gates(poly)==3&&gates(cell)==1);
        for(unsigned end=0;end<2;end++) {
            web_pair_morph(2,end?32767:0);run_ms(40);web_render(128);
            float peak=0;for(unsigned i=0;i<128;i++) {
                CHECK(isfinite(out_l[i])&&isfinite(out_r[i]));
                if(fabsf(out_l[i])>peak) peak=fabsf(out_l[i]);
            }
            CHECK(peak>0.00001f);
        }
        web_keys(0);run_ms(40);CHECK(!gates(poly)&&!gates(cell));
        /* Follower MIDI uses the same latch/sustain owner and note lifecycle. */
        web_midi(0x92,60,100);web_midi(0x92,64,100);web_midi(0x92,67,100);run_ms(15);
        CHECK(gates(poly)==3&&gates(cell)==1);
        web_midi(0xB2,64,127);web_midi(0x82,60,0);web_midi(0x82,64,0);web_midi(0x82,67,0);run_ms(15);
        CHECK(gates(poly)==3&&gates(cell)==1);
        web_midi(0xB2,64,0);run_ms(15);CHECK(!gates(poly)&&!gates(cell));
        web_midi(0x92,60,100);web_midi(0x92,64,100);run_ms(15);
        web_midi(0xB2,123,0);run_ms(15);CHECK(!gates(poly)&&!gates(cell));

        trk[0].p[P_LATCH]=LT_STACK;run_ms(10);
        web_keys((1u<<0)|(1u<<4)|(1u<<7));run_ms(30);web_keys(0);run_ms(30);
        CHECK(gates(poly)==3&&gates(cell)==1);
        web_pair_select(0);web_keys((1u<<0)|(1u<<4)|(1u<<7));run_ms(30);web_keys(0);run_ms(30);
        if(gates(poly)||gates(cell)) fprintf(stderr,"LATCH engine=%u topology=%u reverse=%u poly=%u cell=%u keys=%u\n",e,topo,rev,gates(poly),gates(cell),ep.key_count);
        CHECK(!gates(poly)&&!gates(cell));trk[0].p[P_LATCH]=LT_OFF;run_ms(10);
        uint8_t notes[3]={60,64,67};
        put_step(&trk[0],0,3,notes,ST_NOTE,0);trk[0].p[P_SLEN]=16;trk[0].p[P_SGATE]=96;
        seq_start();run_ms(25);CHECK(gates(poly)==3&&gates(cell)==1);
        seq_stop();web_pair_panic();run_ms(900);
        memset(trk[0].step,0,sizeof trk[0].step);

        /* CELL family edits preserve gestures; layout changes safely release. */
        chord(3);trk[cell].p[P_E3]=3+(topo-2)%3;run_ms(10);
        CHECK(ep.key_count==3&&gates(poly)==3&&gates(cell)==1);
        trk[cell].p[P_E3]=0;run_ms(10);CHECK(!ep.cells&&ep.budget==1);
        CHECK(!ep.key_count&&!gates(poly)&&!gates(cell));
        trk[cell].p[P_E3]=topo;run_ms(10);CHECK(ep.budget==4);
        chord(3);trk[2].p[P_VOICE]=V_LEGATO;run_ms(10);
        CHECK(ep.mode==V_LEGATO&&!ep.cells&&!ep.key_count);
        CHECK(!gates(poly)&&!gates(cell));
        trk[0].p[P_VOICE]=V_POLY;run_ms(10);CHECK(ep.cells&&ep.budget==4);
        /* Stored capsule remains HPM1; derived runtime layout is rebuilt. */
        CHECK(!hp_project_save(0,0));pair_disable();run_ms(20);
        CHECK(hp_project_load(0));CHECK(ep.cells==(rev?1:2)&&ep.budget==4);
        CHECK(!ep.key_count&&!gates(poly)&&!gates(cell));
        chord(3);pair_disable();CHECK(!ep.on&&!gates(poly)&&!gates(cell));
    }
    /* Two shared networks retain all 128 legal held MIDI notes, including zero. */
    web_pair_panic();run_ms(900);CHECK(web_pair_prepare(0,2,ENGI_DRONE,ENGI_DRONE));
    trk[0].p[P_E3]=3;trk[2].p[P_E3]=5;run_ms(10);CHECK(ep.cells==3&&ep.budget==4);
    for(unsigned n=0;n<128;n++){on(n);CHECK(held(0,n)&&held(2,n));CHECK(voices_busy()<=NVOICE);}
    CHECK(ep.key_count==128&&gates(0)==1&&gates(2)==1);
    on(0);CHECK(ep.key_count==128);
    for(unsigned n=0;n<128;n++){off(n);CHECK(!held(0,n)&&!held(2,n));}
    CHECK(!gates(0)&&!gates(2)&&!ep.key_count);

    /* Pair admission cannot consume the protected mono notes of other tracks. */
    setup(ENGI_SEM,3,0);
    for(unsigned t=1;t<4;t+=2){trk[t].p[P_VOICE]=V_MONO;trk_note_on(&trk[t],40+t,100);}
    for(unsigned n=60;n<88;n++){on(n);CHECK(voices_busy()<=NVOICE);CHECK(gates(1)&&gates(3));}
    trk_all_off(&trk[0]);CHECK(!ep.key_count&&!gates(poly)&&!gates(cell));
    CHECK(gates(1)&&gates(3));web_pair_panic();run_ms(900);
    printf("CELL PAIR: %u checks passed; SEM/HADES, all CELL topologies/roles, lifecycle, pressure, MIDI/panel, HPM1.\n",checks);
    return 0;
}
