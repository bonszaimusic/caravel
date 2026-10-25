/* SPDX-License-Identifier: GPL-3.0-only */
#include "captured_controls.h"
static void shot(const char*path){
 FILE*f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);unsigned char p[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);
}
int main(int argc,char**argv){
 CHECK(argc==2);web_nor_erase();web_boot();run_ms(1400);normal_off();
 CHECK(web_pair_prepare(0,1,ENGI_HADES,ENGI_DRONE));apply(&trk[0],&captured[0]);apply(&trk[1],&captured[1]);
 hp_apply(0,2);go_home();ui.msg_t=ui.hot_t=0;run_ms(80);shot(argv[1]);
 CHECK(hp.track[0].count==2&&hp.track[0].active==0);
 hp_row_turn(1);ui.msg_t=ui.hot_t=0;run_ms(80);CHECK(hp.track[0].active==1);char path[256];snprintf(path,sizeof path,"%s-row2.ppm",argv[1]);shot(path);
 return 0;
}
