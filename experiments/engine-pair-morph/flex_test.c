/* SPDX-License-Identifier: GPL-3.0-only */
#define PAIR_TEST_MAIN pair_original_main
#include "pair_test.c"
#undef main
static void capture(const char *name)
{
    ui.msg_t=ui.bpm_t=0;run_ms(500);
    uint16_t before[240*240];memcpy(before,web_fb,sizeof before);
    ui.force=1;ui_draw();
    int clean=1;
    for(uint32_t y=Y_LABEL;y<Y_LABEL+CARD_H;y++)
        for(uint32_t x=CARD_X(2);x<CARD_X(2)+CARD_W;x++) clean &= before[y*240+x]==web_fb[y*240+x];
    CHECK(clean);
    char path[160]; snprintf(path,sizeof path,"build/pair/%s.ppm",name);
    FILE *f=fopen(path,"wb"); CHECK(f != 0); fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240*240;i++) {
        uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);
        uint8_t rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31}; fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(void)
{
    pair_original_main();
    unsigned combinations=0;
    for(uint32_t a=0;a<NENGINES;a++) for(uint32_t b=0;b<NENGINES;b++) {
        if(!pair_engine_ok(a)||!pair_engine_ok(b)) continue;
        CHECK(web_pair_prepare(0,2,a,b)); run_ms(60); combinations++;
        CHECK(ep.budget <= trk_nvoice(&trk[0]) && ep.budget <= trk_nvoice(&trk[2]));
        for(uint32_t note=60;note<66;note++) {
            trk_note_on(&trk[note&1?0:2],note,100); run_ms(4);
            CHECK(voices_busy()<=NVOICE);
        }
        trk_all_off(&trk[2]); CHECK(!gates(0)&&!gates(2));
        run_ms(80);
        uint32_t n=web_pair_snapshot_save(); CHECK(n==4032);
        CHECK(web_pair_snapshot_load(n)); CHECK(trk[0].engine==a&&trk[2].engine==b);
        CHECK(!gates(0)&&!gates(2));
    }
    CHECK(combinations==196); CHECK(!pair_engine_ok(ENGI_DRUM)); CHECK(!pair_engine_ok(13));
    prepare(0); web_pair_morph(0,8192); run_ms(30); web_pair_page(); run_ms(50);
    CHECK(cur_page()->graph==GR_PAIR&&!ui.home); capture("pair-25");
    web_enc(EN_K3,8);run_ms(50);CHECK(ep.base==8192+8*256);
    web_pair_select(2);web_enc(EN_K3,-4);run_ms(50);CHECK(ep.base==8192+4*256);
    web_enc(EN_ALGO,-1);run_ms(50);CHECK(song.sel==1&&cur_page()->graph==GR_PAIR);
    web_enc(EN_ALGO,1);run_ms(50);CHECK(song.sel==2&&cur_page()->graph==GR_PAIR);
    CHECK(pair_ui_other()==0);web_enc(EN_K4,1);run_ms(50);CHECK(ep.motion);
    web_pair_morph(2,16384);run_ms(50);capture("pair-motion");
    ui_prefs|=PREF_LARGE;ui.force=1;run_ms(50);capture("pair-large");
    web_enc(EN_K2,-1);run_ms(60);CHECK(!ep.on);capture("pair-off");
    web_enc(EN_K2,1);run_ms(60);CHECK(ep.on&&ep.a==2&&ep.b==0);
    ui_prefs &= ~PREF_LARGE;go_home();home_tap();run_ms(50);capture("pair-mixer");
    /* Automatic audio-side unpairing must clear cached mixer icons without a forced redraw. */
    pair_disable();run_ms(100);
    uint16_t old_screen[240*240];memcpy(old_screen,web_fb,sizeof old_screen);
    ui.force=1;ui_draw();int cleared=1;
    for(uint32_t t=0;t<NTRK;t++)for(uint32_t y=Y_GRAPH+5;y<Y_GRAPH+21;y++)
        for(uint32_t x=CARD_X(t)+4;x<CARD_X(t)+20;x++)cleared &= old_screen[y*240+x]==web_fb[y*240+x];
    CHECK(cleared);capture("pair-mixer-off");CHECK(web_pair_enable(2,0));
    ui_home_view=HV_TRACKS;go_home();run_ms(60);capture("pair-home-tracks");
    web_pair_page();run_ms(30);
    trk[2].p[P_REL]=trk[0].p[P_REL]=127;
    trk_note_on(&trk[2],60,100);run_ms(100);web_pair_off();run_ms(10);
    CHECK(web_pair_enable(2,0));CHECK(ep.on);web_pair_off();run_ms(10);
    trk[2].p[P_REL]=trk[0].p[P_REL]=0;
    web_pair_panic();run_ms(1000);CHECK(web_pair_enable(2,0));
    go_home();home_tap();CHECK(str_eq(cur_page()->title,"MIXER"));
    home_tap();CHECK(str_eq(cur_page()->title,"CLOCK"));home_tap();CHECK(cur_page()->graph==GR_PAIR);
    home_tap();CHECK(ui.home);
    printf("flex pair: %u total checks; %u ordered engine combinations; device menu, follower edits, gains, snapshots passed\n",checks,combinations);
    return 0;
}
