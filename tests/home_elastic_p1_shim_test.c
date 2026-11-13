/* SPDX-License-Identifier: GPL-3.0-only */
/* Control/UI/render handshake shim. This tests the same P1 header as firmware;
 * it does not replace the real FM-1 build, dual-core or DSP audio tests. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#define CARAVEL_ELASTIC_P1 1
#define CARAVEL_ACTION_BANKS 1
#define FELUCCA_RANDOM_MORPH 1
#define NTRK 4
#define ENGI_DRONE 16u
#define P_E7 61u
#define P_FILT 7u
#define P_E4 58u
#define P_COUNT 128u
typedef struct {const char *label;uint8_t fmt;int16_t min,max,def;}param_desc_t;
enum {F_INT,F_PCT,F_BIPCT,F_TIME,F_LFOHZ,F_CUTOFF,F_DB,F_SEMI,F_ENUM,F_BPM,F_NOTE,F_ONOFF,F_OCT,F_STEPS};
#define HP_TRACK 1
static uint32_t fm1_ms;
typedef struct {uint8_t eng_req,engine;int16_t p[128];}track_t;
static track_t trk[NTRK];
static struct {uint32_t sel,playing;}song;
static struct {int force;struct {int on;}ml;}ui;
typedef struct {uint8_t kind,track;uint16_t id;}hp_ref_t;
typedef struct {uint8_t count,active,pad[2];hp_ref_t cell[32][4];}hp_track_t;
static struct {hp_track_t track[NTRK];}hp;
static hp_ref_t hp_last;
static uint8_t hp_last_valid,hp_last_track;
static hp_ref_t hp_current(uint32_t k){return hp.track[song.sel].cell[hp.track[song.sel].active][k&3u];}
static uint32_t hp_owner(hp_ref_t r){return r.track;}
static uint32_t hp_tid(hp_ref_t r){return r.id;}
static uint32_t trk_index(const track_t*t){return (uint32_t)(t-trk);}
static int pair_member(const track_t*t){(void)t;return 0;}
static const param_desc_t *track_desc(const track_t*t,uint32_t id)
{
 static const param_desc_t tone={"TONE",F_PCT,0,127,76};
 static const param_desc_t filt={"FILT",F_INT,-64,63,0};
 static const param_desc_t charv={"CHAR",F_PCT,0,127,30};
 (void)t;
 return id==P_E7?&tone:id==P_E4?&charv:id==P_FILT?&filt:0;
}
static void hp_rm_disengage(hp_ref_t r){(void)r;}
static int32_t clamp(int32_t v,int32_t lo,int32_t hi){return v<lo?lo:v>hi?hi:v;}
static void str_cpy(char*dest,const char*src,uint32_t n){snprintf(dest,n,"%s",src);}
static void fmt_int(char*buf,int32_t x){sprintf(buf,"%d",x);}
static void perf_format(char*b,uint32_t n,const char*fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(b,n,fmt,ap);va_end(ap);}
static void ui_message(const char*s){(void)s;}
#include "../experiments/home-perf/home_elastic_p1.h"
static void target(uint32_t tr,uint32_t row,uint32_t col)
{
 hp.track[tr].cell[row][col]=(hp_ref_t){HP_TRACK,(uint8_t)tr,P_E7};
 if(hp.track[tr].count<=row)hp.track[tr].count=(uint8_t)(row+1u);
}
int main(void){
 hp_elastic_p1_init();song.sel=0;trk[0].eng_req=ENGI_DRONE;trk[0].engine=ENGI_DRONE;trk[0].p[P_E7]=63;
 /* A target outside the active row must still arm: the fix moves the row to it. */
 target(0,1,2);hp.track[0].active=0;
 assert(!hp_el1.on);hp_elastic_p1_action(0);
 assert(hp_el1.on && hp_el1.row==1 && hp_el1.slot==2 && hp.track[0].active==1);
 hp_ref_t r=hp_current(2);fm1_ms=1000;assert(hp_elastic_p1_turn(r,2,5));
 assert(!hp_elastic_p1_turn(r,3,5)); /* a turn of another cell never drives the armed target */
 hp_elastic_p1_audio_begin(&trk[0]);assert(trk[0].p[P_E7]>63);hp_elastic_p1_audio_end(&trk[0]);assert(trk[0].p[P_E7]==63);
 int16_t stored=trk[0].p[P_E7];
 hp_elastic_p1_action(2);int16_t locked=hp_elastic_value(&hp_el1.spring);fm1_ms=3000;hp_elastic_p1_poll(fm1_ms);assert(hp_elastic_value(&hp_el1.spring)==locked);
 hp_elastic_p1_action(2);hp_elastic_p1_action(1);
 for(uint32_t i=1;i<4000;i++){fm1_ms=3000+i*16;hp_elastic_p1_poll(fm1_ms);if(!hp_el1.spring.active)break;}
 assert(trk[0].p[P_E7]==stored);
 hp_elastic_p1_setup_turn(1,5);char a[32],v[32],status[80];hp_elastic_p1_setup_caption(1,a,v);hp_elastic_p1_status(status,sizeof status);assert(a[0]=='F');
 assert(hp_elastic_p1_card(r,2));hp_elastic_p1_action(3);assert(hp_elastic_value(&hp_el1.spring)==stored);
 hp_elastic_p1_manual(0,P_E7);assert(!hp_el1.on);assert(hp_el1_audio[0]==0);
 hp_elastic_p1_action(0);ui.ml.on=1;assert(!hp_elastic_p1_turn(hp_current(2),2,1));ui.ml.on=0;
 /* A generic continuous parameter (CHAR, P_E4) is accepted; an F_INT
  * selector (FILT) is rejected. */
 hp_elastic_p1_clear();
 memset(hp.track[0].cell,0,sizeof hp.track[0].cell);
 hp.track[0].cell[1][0]=(hp_ref_t){HP_TRACK,0,P_E4};
 hp.track[0].count=2;hp.track[0].active=1;hp_last_valid=0;trk[0].p[P_E4]=30;
 hp_elastic_p1_action(0);
 assert(hp_el1.on&&hp_el1.id==P_E4&&hp_el1.row==1&&hp_el1.slot==0);
 assert(hp_elastic_p1_turn(hp_current(0),0,4));
 assert(HP_EL1_ID(hp_el1_audio[0])==P_E4);
 hp_elastic_p1_audio_begin(&trk[0]);assert(trk[0].p[P_E4]!=30);hp_elastic_p1_audio_end(&trk[0]);assert(trk[0].p[P_E4]==30);
 hp_elastic_p1_clear();
 hp.track[0].cell[0][0]=(hp_ref_t){HP_TRACK,0,P_FILT};
 hp.track[0].count=1;hp.track[0].active=0;hp_last_valid=0;
 hp_elastic_p1_action(0);assert(!hp_el1.on);
 song.sel=1;fm1_ms=90000;hp_elastic_p1_poll(fm1_ms);assert(!hp_el1.on);
 song.sel=0;song.playing=1;hp_elastic_p1_action(0);assert(!hp_el1.on);
 song.playing=0;target(0,0,0);hp.track[0].count=1;hp.track[0].active=0;hp_last_valid=0;hp_elastic_p1_action(0);assert(hp_el1.on);
 song.playing=1;hp_elastic_p1_poll(90016u);assert(!hp_el1.on);
 puts("home_elastic_p1_shim_test: PASS");return 0;
}
