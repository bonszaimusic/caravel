/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN layout_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"

static void tap(uint32_t b){web_buttons(1u<<b);run_ms(70);web_buttons(0);run_ms(420);}
static void expect_page(const char *name){if(ui.home||strcmp(cur_page()->title,name))fprintf(stderr,"expected %s got %s home=%u\n",name,cur_page()->title,ui.home);CHECK(!ui.home && !strcmp(cur_page()->title,name));}
static void image(const char *name)
{
    const char *dir=getenv("LAYOUT_SCREENSHOTS");if(!dir)return;
    char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);
    ui.force=1;ui_draw();FILE *f=fopen(path,"wb");CHECK(f);
    fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240u*240u;i++){
        uint16_t v=(web_fb[i]>>8)|(web_fb[i]<<8);
        uint8_t rgb[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
        fwrite(rgb,1,3,f);
    }fclose(f);
}
int main(void)
{
    web_nor_erase();web_boot();run_ms(1300);
    static const uint16_t ids[]={0,127,255,256,391,400,406,511};
    for(uint32_t k=1;k<8;k++)for(uint32_t t=0;t<NTRK;t++)
      for(uint32_t en=0;en<=NENGINES;en++)for(uint32_t a=0;a<16;a++)
        for(uint32_t id=0;id<sizeof ids/sizeof ids[0];id++){
          hp_ref_t r={0};r.id=ids[id];r.kind=k;r.track=t;r.engine=en==NENGINES?255:en;r.aux=a;
          CHECK(hp_rm_snapshot_packable(r));hp_rm_snapshot_write_ref(&hp_rm_snap[0],0,r);
          CHECK(hp_rm_same(r,hp_rm_snapshot_ref(&hp_rm_snap[0],0)));
        }
    CHECK(sizeof hp_rm_snap==2584u && HP_RM_MAX==128u && HP_RM_SNAP_SLOTS==4u);
    web_stasis_test_type(3);run_ms(120);
    ui.fam_last[FAM_FX]=0;go_home();tap(B_FX);expect_page("FX");
    tap(B_FX);expect_page("DLY");tap(B_FX);expect_page("CHORUS");
    CHECK(!dual_bottom(cur_page()));image("chorus-reverb");
    web_enc(0,1);run_ms(120);expect_page("REVERB");CHECK(dual_bottom(cur_page()));
    tap(B_FX);expect_page("GRAIN");
    const dual_t *grain=dual_of(cur_page(),0);CHECK(grain);
    CHECK(!strcmp(dual_row_page(grain,0)->title,"GRAIN"));
    CHECK(!strcmp(dual_row_page(grain,1)->title,"LAYER"));
    char title[20];page_title(title);CHECK(strstr(title,"3/5"));
    int mode=song.g[G_STMODE],scan=st_expand.scan;
    web_enc(4,9);run_ms(120);CHECK(st_expand.scan==scan+9);CHECK(song.g[G_STMODE]==mode);
    image("stasis-grain");
    web_enc(0,1);run_ms(120);expect_page("LAYER");
    CHECK(dual_of(cur_page(),0)==grain);page_title(title);CHECK(strstr(title,"3/5"));
    CHECK(ui.act==4 && act_cols()==8);
    int grain_size=st_expand.grain;
    web_enc(3,1);run_ms(120);CHECK(song.g[G_STMODE]==1);CHECK(st_expand.grain==grain_size);
    CHECK(st_layer.phase==STL_IDLE); /* editing never starts an overdub */
    web_stasis_test_hold(1);run_ms(1200);CHECK(st.auto_hold);
    tap(B_OCTUP);CHECK(st_layer.phase==STL_WAIT);image("stasis-layer-wait");
    tap(B_OCTUP);CHECK(st_layer.phase==STL_IDLE);
    image("stasis-layer");
    web_enc(0,-1);run_ms(120);expect_page("GRAIN");
    web_enc(0,-1);run_ms(120);expect_page("REVERB");
    web_stasis_test_hold(0);web_stasis_test_type(2);run_ms(120);
    web_enc(0,1);run_ms(120);expect_page("INSERT"); /* STASIS hidden for HALL */
    printf("STASIS layout PASS: %u checks; CHORUS -> REVERB -> GRAIN/LAYER in one view, native knobs and OCT+ preserved\n",checks);
    return 0;
}
