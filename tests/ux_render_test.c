/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Extend ui_render.c's original layout/ink/coverage hooks to the UX screens.
 * Compile with the same UX/profile defines as ux_round3.sh. OUT/ppm must exist. */
#include <stdint.h>
static void action_refresh(void);
static void action_dispatch(uint32_t);
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"
#include "../experiments/action-banks/action_control.c"

static void screen_begin(const char *name)
{
    cur_name=name;nscr=npend=nrules=0;ntight=0;tight[0]=0;
    memset(host_screen,0,sizeof host_screen);cv_oy=0;
    ui.force=1;ui.msg_t=0;
}
static void play_scene(void)
{
    uxl.play=1;ui.layer=ui.menu=ui.confirm=0;ui.msg_t=0;
    uxl.hot_t=uxl.cap_view=uxl.pv=0;uxl_draw_play();
}
int main(int argc,char **argv)
{
    char path[600],name[80];const char *out=argc>1?argv[1]:"build/ux-render";
    uint32_t p,e,a,g,i,k,scenes=0,bank_fail=0;
    snprintf(path,sizeof path,"%s/layout.txt",out);rep=fopen(path,"w");if(!rep)return 2;al_rep=rep;
    for(p=0;p<NPALETTES;p++) {
        state();hp_init();pal(p);memset(&uxl,0,sizeof uxl);
        for(e=0;e<NENGINES;e++) {
            if(!eng_ok(e))continue;eng(e);
            for(a=0;a<UXL_N;a++)for(g=0;g<uxl_area_ng(a);g++) {
                snprintf(name,sizeof name,"play_e%u_a%u_g%u_%s",e,a,g,UI_PALETTES[p].name);
                screen_begin(name);uxl.area=a;uxl.grp[a]=g;play_scene();lint();scenes++;
                if(e==0 && p==UI_GREY_INDEX)write_ppm(out,"GREY",name);
            }
        }
        state();hp_init();pal(p);eng(0);memset(&uxl,0,sizeof uxl);
        screen_begin("play_message_mute_loop_rec");uxl.play=1;trk[0].p[P_MUTE]=1;song.rec=1;
        cap_st[0]=1;cap_len[0]=65536;cap_full[0]=1;cap_ln[0]=2;
        cap_loop[0][0]=(cap_ev_t){1000,60,100};cap_loop[0][1]=(cap_ev_t){9000,60,0};
        cap_rn[0]=cap_rh[0]=cap_dn[0]=cap_dh[0]=2;
        cap_ring[0][0]=(cap_ev_t){1000,60,100};cap_ring[0][1]=(cap_ev_t){9000,60,0};
        cap_dring[0][0]=(cap_ev_t){3000,67,100};cap_dring[0][1]=(cap_ev_t){11000,67,0};
        perf_bars_q16=20000;str_cpy(ui.msg,"LOOP FULL",sizeof ui.msg);ui.msg_t=60;
        uxl_draw_play();lint();scenes++;
        if(p==UI_GREY_INDEX)write_ppm(out,"GREY","play_message_mute_loop_rec");
        cap_st[0]=cap_full[0]=cap_ln[0]=cap_dn[0]=cap_rn[0]=0;song.rec=0;trk[0].p[P_MUTE]=0;
        screen_begin("play_harmony_pending_stasis");harmony_on=1;harmony_pending=H_NEXT;
        hroot_set(0,9);harmony_bar=0;song.g[G_RTYPE]=3;uxl.play=1;
        { int16_t saved[NTRK][P_COUNT];for(k=0;k<NTRK;k++)memcpy(saved[k],trk[k].p,sizeof saved[k]);
          uxl_draw_play();for(k=0;k<NTRK;k++)if(memcmp(saved[k],trk[k].p,sizeof saved[k]))bank_fail++; }
        lint();scenes++;if(p==UI_GREY_INDEX)write_ppm(out,"GREY",cur_name);
        harmony_on=harmony_pending=0;song.g[G_RTYPE]=2;
        for(i=LAYER_FX;i<=LAYER_SCL;i++) {
            snprintf(name,sizeof name,"play_peek_%u_%s",i,UI_PALETTES[p].name);screen_begin(name);
            ui.layer=ui.ly=i;ui.home=0;ui.force=1;uxl.play=1;ui_draw();lint();scenes++;
            if(p==UI_GREY_INDEX)write_ppm(out,"GREY",name);
        }
        for(i=0;i<2;i++) {
            screen_begin(i?"play_entry_help_second":"play_entry_help_first");
            ui.layer=ui.ly=0;uxl.play=1;uxl.area=i?UXL_SPACE:UXL_SOUND;
            uxl.hint_until=fm1_ms+(i?2000u:4500u);
            uxl_draw_play();lint();scenes++;
            if(p==UI_GREY_INDEX)write_ppm(out,"GREY",cur_name);
            uxl.hint_until=0;
        }
        /* Two-banks invariant: every existing dual page keeps EDIT and VIEW;
         * no new grouping, page or engine is substituted by this test. */
        for(e=0;e<NENGINES;e++)for(i=0;i<NPAGES;i++) {
            state();hp_init();pal(p);eng(e);memset(&uxl,0,sizeof uxl);
            if(!eng_ok(e)||!page_visible(i))continue;
            ui.home=0;ui.page=i;page_entered();
            snprintf(name,sizeof name,"build_e%u_p%u_%s",e,i,UI_PALETTES[p].name);screen_begin(name);
            ui_draw();lint();scenes++;
            if(dual_page(cur_page()) && dual_of(cur_page(),0)->kind!=DK_HARMONY) {
                int edit=0,view=0;
                for(k=0;k<nscr;k++){edit|=!strncmp(scr[k].s,"EDIT K1",7);view|=!strcmp(scr[k].s,"VIEW");}
                if(dual_of(cur_page(),0)->kind!=DK_GRAPH && (!edit||!view))bank_fail++;
            }
            if(e==0 && p==UI_GREY_INDEX && (i==page_first(FAM_ENV)||i==page_first(FAM_FX)||i==page_first(FAM_EDIT)))
                write_ppm(out,"GREY",name);
        }
    }
    snprintf(path,sizeof path,"%s/system_menu.tsv",out);
    {FILE *f=fopen(path,"w");if(f){fprintf(f,"entry\ttab\n");for(i=0;i<MI_COUNT;i++)fprintf(f,"%s\t%s\n",MI_NAME[i],MTAB_NAME[MI_TAB[i]]);fclose(f);}}
    /* Frame cost, including the real canvas/blit stubs, 50 forced frames.
     * Warmed process CPU time on this host; never a JieLi timing claim. */
    state();hp_init();pal(UI_GREY_INDEX);memset(&uxl,0,sizeof uxl);uxl.play=1;uxl.area=UXL_SPACE;
    cur_name="cost";al_off=1;
    for(k=0;k<2;k++) {
        clock_t start;uint64_t px0=px_visited,bl0=blit_px;
        start=clock();
        for(i=0;i<50;i++) {
            npend=nscr=nrules=0;
            if(!k){ui.force=1;uxl_draw_play();}
            else for(p=0;p<2;p++) {cv_begin(240,120,T_BG);cv_oy=-(int32_t)p*120;uxl_y0=p*120;uxl_paint_widget();cv_blit(0,p*120);}
        }
        printf("cost %s: %.3f us/frame; gfx pixels=%llu blit pixels=%llu\n",k?"capture":"play",
            (double)(clock()-start)*1e6/CLOCKS_PER_SEC/50,
            (unsigned long long)(px_visited-px0)/50,(unsigned long long)(blit_px-bl0)/50);
    }
    fprintf(rep,"scenes=%u findings=%u ink=%u alignment=%u bank_fail=%u\n",scenes,nfind,nink,al_fail,bank_fail);
    printf("ux render: scenes=%u findings=%u ink=%u alignment=%u bank_fail=%u\n",scenes,nfind,nink,al_fail,bank_fail);
    fclose(rep);return nfind||nink||al_fail||bank_fail?1:0;
}
