/* SPDX-License-Identifier: GPL-3.0-only */
#define PAIR_TEST_MAIN pair_baseline_test
#include "pair_test.c"
static void tap(uint32_t b) { web_buttons(1u<<b);run_ms(100);web_buttons(0);run_ms(400); }
static void screen(const char *name)
{
    char p[160];snprintf(p,sizeof p,"build/pair/%s.ppm",name);ui.msg_t=ui.bpm_t=0;run_ms(500);
    FILE *f=fopen(p,"wb");CHECK(f!=0);fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);uint8_t v[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(v,1,3,f);}fclose(f);
}
static void mode_pair(uint32_t a,uint32_t b,uint32_t mode,uint32_t topo)
{
    CHECK(web_pair_prepare(0,2,a,b));run_ms(30);web_pair_off();run_ms(10);
    trk[0].p[P_VOICE]=mode;trk[2].p[P_VOICE]=(mode+1)%4;
    if(a==ENGI_DRONE)trk[0].p[P_E3]=topo;
    if(b==ENGI_DRONE)trk[2].p[P_E3]=topo;
    int16_t saved[2][P_COUNT];memcpy(saved[0],trk[0].p,sizeof saved[0]);memcpy(saved[1],trk[2].p,sizeof saved[1]);
    CHECK(web_pair_enable(0,2));run_ms(3);
    CHECK(trk[0].p[P_VOICE]==mode && trk[2].p[P_VOICE]==mode);
    for(uint32_t m=0;m<2;m++)for(uint32_t id=P_E0;id<P_COUNT;id++)CHECK(trk[m?2:0].p[id]==saved[m][id]);
}
int main(void)
{
    web_nor_erase();web_boot();run_ms(1500);
    uint32_t original=trk[0].p[P_VOICE];CHECK(original==V_LEGATO);
    /* The actual default startup, buttons and encoders: no Prepare or manual mode conversion. */
    tap(B_HOME);tap(B_HOME);tap(B_HOME);CHECK(cur_page()->graph==GR_PAIR&&!ui.home);
    web_enc(EN_K2,1);run_ms(100);CHECK(ep.on&&ui.home&&ep.mode==original);
    CHECK(trk[2].p[P_VOICE]==original);screen("home-pair-v6");
    int16_t engine_before[8];memcpy(engine_before,&trk[0].p[P_E0],sizeof engine_before);
    int32_t previous=ep.base;
    web_enc(EN_K1,8);run_ms(100);CHECK(ep.base==previous+2048);
    CHECK(!memcmp(engine_before,&trk[0].p[P_E0],sizeof engine_before));
    tap(B_EDIT);CHECK(!ui.home);tap(B_HOME);CHECK(ui.home);
    web_pair_select(2);previous=ep.base;web_enc(EN_K1,-4);run_ms(80);CHECK(ep.base==previous-1024);
    web_pair_select(1);previous=ep.base;web_enc(EN_K1,1);run_ms(80);CHECK(ep.base==previous+256);
    CHECK(ep.on);web_pair_off();run_ms(100);
    pair_baseline_test();
    unsigned combinations=0;
    for(uint32_t a=0;a<NENGINES;a++)for(uint32_t b=0;b<NENGINES;b++)for(uint32_t mode=0;mode<4;mode++) {
        if(!pair_engine_ok(a)||!pair_engine_ok(b))continue;
        mode_pair(a,b,mode,0);combinations++;
        for(uint32_t n=60;n<66;n++) {trk_note_on(&trk[n&1?0:2],n,100);CHECK(voices_busy()<=NVOICE);run_ms(2);}
        if(mode!=V_POLY)CHECK(trk[0].v[0].note==65&&trk[2].v[0].note==65);
        for(uint32_t n=66;n-->60;) {trk_note_off(&trk[n&1?2:0],n);run_ms(2);CHECK(voices_busy()<=NVOICE);}
        CHECK(!gates(0)&&!gates(2)&&!ep.key_count);
        uint32_t n=web_pair_snapshot_save();CHECK(n==4032);CHECK(web_pair_snapshot_load(n));CHECK(ep.mode==mode);
    }
    /* All DRONE topologies retain their own sound; source voice mode is inherited. */
    for(uint32_t topo=0;topo<6;topo++)for(uint32_t mode=0;mode<4;mode++)for(uint32_t reverse=0;reverse<2;reverse++) {
        mode_pair(reverse?ENGI_DRONE:ENGI_SEM,reverse?ENGI_SEM:ENGI_DRONE,mode,topo);
        trk_note_on(&trk[2],60,100);run_ms(5);uint32_t age=trk[0].v[0].age;
        trk_note_on(&trk[0],67,100);run_ms(5);
        if(mode==V_LEGATO||mode==V_UNISON)CHECK(trk[0].v[0].age==age);
        if(mode==V_MONO)CHECK(trk[0].v[0].age!=age);
        trk_note_off(&trk[2],67);run_ms(5);
        if(mode!=V_POLY)CHECK(gates(0)==1&&gates(2)==1&&trk[0].v[0].note==60&&trk[2].v[0].note==60);
        else if(topo>=3) {
            /* A POLY cell network's host voice can retain the released note.
             * Its full held mask and per-cell pitches own the musical gesture. */
            track_t *network=&trk[reverse?0:2], *pitched=&trk[reverse?2:0];
            drone_part_t *state=drone_state(network);uint32_t held60=0;
            for(uint32_t i=0;i<NVOICE;i++)held60+=pitched->v[i].gate&&pitched->v[i].note==60;
            CHECK(gates(0)==1&&gates(2)==1&&held60==1);
            CHECK(drone_held(state,60)&&!drone_held(state,67));
            CHECK(state->owner[0]==60&&state->pitch16[0]==60*16);
        }
        trk_note_off(&trk[0],60);run_ms(5);CHECK(!gates(0)&&!gates(2));
        trk_all_off(&trk[0]);
    }
    for(uint32_t mode=0;mode<4;mode++)for(uint32_t priority=0;priority<3;priority++) {
        mode_pair(ENGI_SEM,ENGI_DRONE,mode,5);trk[0].p[P_PRIO]=priority;run_ms(3);
        for(uint32_t n=0;n<128;n++){trk_note_on(&trk[0],n,100);CHECK(ep.key_count==n+1);CHECK(voices_busy()<=NVOICE);}
        for(uint32_t n=128;n-->0;){
            trk_note_off(&trk[2],n);CHECK(ep.key_count==n);
            if(mode!=V_POLY) { if(n)CHECK(gates(0)==gates(2)); }
            else {
                /* POLY cells use one network voice and up to four pitched voices;
                 * stealing a pitched slot does not discard the network's keys. */
                CHECK(!!drone_any_held(drone_state(&trk[2]))==!!n);
                CHECK(!drone_held(drone_state(&trk[2]),n));
                CHECK(gates(0)<=ep.budget&&gates(2)<=1&&voices_busy()<=NVOICE);
                for(uint32_t v=0;v<NVOICE;v++)if(trk[0].v[v].gate)CHECK(trk[0].v[v].note<n);
            }
        }
        CHECK(!gates(0)&&!gates(2));
    }
    /* UNISON is paired within eight voices and protects the other parts' mono voices. */
    mode_pair(ENGI_SEM,0,V_UNISON,0);
    trk[1].p[P_VOICE]=trk[3].p[P_VOICE]=V_MONO;
    trk_note_on(&trk[1],48,100);trk_note_on(&trk[3],36,100);trk_note_on(&trk[0],60,100);
    CHECK(voices_busy()<=NVOICE&&gates(0)==gates(2)&&gates(1)&&gates(3));
    CHECK(ep.live_slots==3);voice_kill(&trk[0].v[1]);CHECK(!gates(0)&&!gates(2)&&!ep.key_count);
    printf("usability: %u total checks; %u engine/mode combinations + 48 DRONE topology/mode cases; startup, HOME morph, inheritance, 128-key release, UNISON budget passed\n",checks,combinations);
    return 0;
}

