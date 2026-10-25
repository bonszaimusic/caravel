/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define WEB_FLASH_TEST 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
static uint8_t reply[700];static uint32_t reply_n;
static void ed_b(uint32_t n){CHECK(reply_n<sizeof reply);reply[reply_n++]=n&127;}
static void ed_bk_u32(uint32_t n){for(unsigned i=0;i<5;i++)ed_b(n>>(7*i));}
#include "editor_diag.c"
static void command(unsigned op,unsigned seconds){uint8_t a[3]={op,42,seconds};reply_n=0;CHECK(ed_diag_handle(91,a,op==1?3:2));}
int main(int argc,char **argv){
 web_nor_erase();web_boot();run_ms(1400);
 if(argc>1){
  CHECK(web_pair_prepare(0,2,ENGI_DRONE,ENGI_HADES));trk[0].p[P_E3]=3;run_ms(10);web_keys(1u);run_ms(10);
  char line[100];while(fgets(line,sizeof line,stdin)){
   unsigned op,token,seconds,ms;if(sscanf(line,"%u %u %u %u",&op,&token,&seconds,&ms)!=4||ms>10000||op>127||token>127||seconds>127)return 2;
   run_ms(ms);uint8_t a[3]={op,token,seconds};reply_n=0;ed_diag_handle(91,a,op==1?3:2);
   for(unsigned i=0;i<reply_n;i++)printf("%02x",reply[i]);printf("\n");fflush(stdout);
  }return 0;
 }

 command(0,0);CHECK(reply[0]==0&&reply[1]==0&&reply[2]==42&&reply[3]==PD_WORDS);CHECK(reply_n==4+5*PD_WORDS);CHECK(pd_wire[0]==PD_MAGIC&&pd_wire[2]==0&&!pd_wire[7]);
 for(unsigned bad=0;bad<3;bad++){uint8_t a[3]={1,1,bad==0?0:bad==1?61:127};reply_n=0;ed_diag_handle(91,a,3);CHECK(reply[1]==1&&!pd_request);}
 command(1,1);CHECK(!reply[1]&&pd_request==1);run_ms(10);CHECK(pd_active&&pd.state==1&&pd.halves);
 unsigned id=pd.session;command(1,30);CHECK(reply[1]==2&&pd.session==id);
 run_ms(1010);CHECK(!pd_active&&pd.state==3&&pd.session==id);command(0,0);CHECK(pd_wire[2]==0&&pd_wire[8]==0&&pd_wire[9]==0);CHECK(pd_wire[6]>=1000&&pd_wire[7]>300);
 command(1,30);run_ms(10);command(2,0);run_ms(5);CHECK(!pd_active&&pd.state==2&&pd.session==id+1);
 /* Exclusive engine/track counters, wrap arithmetic, deadline and shed attribution. */
 memset(&pd,0,sizeof pd);pd_active=1;pd.seconds=60;pd.shed_base=10;
 pd_finish_half(20,PD_PERIOD+100,240,1,12);CHECK(pd.halves==1&&pd.total==PD_PERIOD+100&&pd.maximum==pd.total);CHECK(pd.nested==240&&pd.late==1&&pd.shed==2&&pd.over85==1);
 pd_finish_half(23,1000,100,0,12);CHECK(pd.halves==2&&pd.maximum==PD_PERIOD+100&&pd.over85==1);
 /* No musical, preset, project or flash mutation by the wire handler. */
 pd_active=0;CHECK(web_pair_prepare(0,2,ENGI_DRONE,ENGI_HADES));trk[0].p[P_E3]=3;run_ms(10);web_keys(1u);run_ms(20);
 uint8_t flash[1024];memcpy(flash,nor+0xE8000,sizeof flash);int16_t params[NTRK][P_COUNT];for(unsigned t=0;t<NTRK;t++)memcpy(params[t],trk[t].p,sizeof params[t]);
 command(0,0);CHECK(pd_wire[38]==5&&pd_wire[44]%256==ENGI_DRONE&&(pd_wire[80]&255)==ENGI_HADES);
 CHECK((pd_wire[45]&255)==1&&((pd_wire[45]>>8)&255)==1);CHECK(pd_wire[2]==0);
 for(unsigned t=0;t<NTRK;t++)CHECK(!memcmp(params[t],trk[t].p,sizeof params[t]));CHECK(!memcmp(flash,nor+0xE8000,sizeof flash));CHECK(gates(0)==1&&gates(2)==1);
 seq_start();run_ms(10);CHECK(song.playing);
 command(1,1);run_ms(150);CHECK(song.playing);CHECK(pd.engine[0].calls>0&&pd.engine[1].calls>0);command(2,0);run_ms(10);CHECK(!pd_active&&song.playing);seq_stop();run_ms(10);
 /* Shared trace for a real JS decoder/wire client test (all fields, high bits included). */
 command(0,0);FILE*f=fopen("build/diag-reply.bin","wb");CHECK(f);fwrite(reply,1,reply_n,f);fclose(f);
 printf("USB DIAGNOSTICS: %u checks passed; framing, bounds, timeout/stop, no musical writes, hardware-only timing flag, counters and held Pair.\n",checks);return 0;
}
