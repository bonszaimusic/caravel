/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible release examples and screenshots from the actual firmware DSP/UI.
 * cc -O2 -w -DCARAVEL_REVERB_MINIMAL=1 -DFELUCCA_SLICE=0 \
 * -Ibuild/gen -Ifirmware/src tools/stasis_release_media.c -lm -o build/stasis-media
 * Run separately for each scene: build/stasis-media hold|add|blend
 */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN media_pair_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
static void page(const char *name)
{
    for (uint32_t i=0;i<NPAGES;i++) if (str_eq(PAGES[i].title,name)) {
        ui.home=0;ui.page=(uint8_t)i;page_entered();return;
    }
    CHECK(0);
}
static void screen(const char *name)
{
    char path[200];snprintf(path,sizeof path,"build/release-060-media/%s.ppm",name);
    ui.msg_t=ui.hot_t=0;ui.force=1;ui_draw();
    FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");
    for(uint32_t i=0;i<240u*240u;i++) {
        uint16_t p=(uint16_t)((web_fb[i]>>8)|(web_fb[i]<<8));
        uint8_t rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(rgb,1,3,f);
    }
    fclose(f);
}
static void sound(uint32_t tr,uint32_t eng,uint32_t pre)
{
    host_preset(&trk[tr],eng,pre);
    trk[tr].p[P_LEVEL]=88;trk[tr].p[P_REV]=116;trk[tr].p[P_DLY]=trk[tr].p[P_CHOR]=0;
    trk[tr].p[P_ITYPE]=trk[tr].p[P_DIST]=0;
}
static void chord(uint32_t tr,uint32_t root,int on)
{
    const uint8_t offsets[]={0,7,12,15};
    for(uint32_t i=0;i<4;i++)if(on)trk_note_on(&trk[tr],root+offsets[i],100);
        else trk_note_off(&trk[tr],root+offsets[i]);
}
int main(int argc,char **argv)
{
    CHECK(argc==2);int add=str_eq(argv[1],"add"),blend=str_eq(argv[1],"blend");
    CHECK(add||blend||str_eq(argv[1],"hold"));
    web_nor_erase();web_boot();run_ms(1400);web_master(850);
    song.g[G_RTYPE]=3;song.g[G_RSIZE]=112;song.g[G_DRREV]=45;
    song.g[G_STAMT]=42;song.g[G_STTIME]=6;
    sound(0,blend?ENGI_DRONE:ENGI_SEM,blend?9:1);
    sound(1,blend?ENGI_SEM:ENGI_DRONE,blend?2:11);sound(2,ENGI_HADES,7);
    page("REVERB");
    char path[200];snprintf(path,sizeof path,"build/release-060-media/%s.wav",argv[1]);
    FILE *f=fopen(path,"wb");CHECK(f);uint32_t seconds=add||blend?24:20,frames=seconds*FS;
    wav_hdr(f,frames);double peak=0;uint64_t energy=0;uint32_t done=0;
    for(uint32_t ms=0;done<frames;ms++) {
        if(ms==0) {if(blend)trk_note_on(&trk[0],43,100);else chord(0,48,1);}
        if(ms==2300)screen("stasis-live");
        if(ms==2500)song.g[G_DRLVL]=1;
        if(ms==2800)screen("stasis-hold");
        if(ms==3000) {if(blend)trk_note_off(&trk[0],43);else chord(0,48,0);}
        if(ms==4200)trk[0].p[P_MUTE]=1;
        if((add||blend)&&ms==5200) {
            page("LAYER");song.g[G_STMODE]=blend?1:0;screen(blend?"blend-ready":"add-ready");
        }
        if((add||blend)&&ms==6000) {
            if(blend)chord(1,60,1);else trk_note_on(&trk[1],36,100);
        }
        if((add||blend)&&ms==7000)CHECK(stasis_layer_command(0));
        if(add&&ms==7100)screen("add-mix");
        if(blend&&ms==9000)screen("blend-mix");
        if((add||blend)&&ms==7800) {
            if(blend)chord(1,60,0);else trk_note_off(&trk[1],36);
        }
        if((add||blend)&&ms==9000)trk[1].p[P_MUTE]=1;
        if(add&&ms==12000)trk_note_on(&trk[2],67,100);
        if(add&&ms==13000)CHECK(stasis_layer_command(0));
        if(add&&ms==13800)trk_note_off(&trk[2],67);
        if(add&&ms==15000)trk[2].p[P_MUTE]=1;
        if(!add&&!blend&&ms==6500)song.g[G_DRREV]=100;
        if(ms==16000) {song.g[G_DRREV]=85;if(add||blend)screen("layer-finished");}
        if(ms==(add||blend?19000u:15000u))stasis_transport_stop();
        uint32_t n=frames-done<44u?frames-done:44u;
        web_render(n);
        for(uint32_t i=0;i<n;i++) {
            CHECK(isfinite(out_l[i])&&isfinite(out_r[i]));
            double l=fabs(out_l[i]),r=fabs(out_r[i]);if(l>peak)peak=l;if(r>peak)peak=r;
            int32_t a=(int32_t)(out_l[i]*32767.),b=(int32_t)(out_r[i]*32767.);
            CHECK(a>=-32768&&a<=32767&&b>=-32768&&b<=32767);
            wav_put(f,a,b);energy+=(uint64_t)abs(a)+(uint64_t)abs(b);
        }
        done+=n;
    }
    fclose(f);CHECK(peak>0.001&&peak<1&&energy>1000000);
    printf("scene=%s frames=%u seconds=%u peak=%.8f sample_rate=%u\n",argv[1],done,seconds,peak,FS);
    return 0;
}
