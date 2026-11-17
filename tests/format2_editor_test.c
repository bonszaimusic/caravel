/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
#define FORMAT2_PERFORMANCE_MAIN performance_test_main
#include "format2_performance_test.c"
#undef main
#include <stddef.h>
#define __attribute__(x)
#define SXQ 1024u
static uint32_t so_w,so_r;
static void fl_inval(uint32_t off,uint32_t n){(void)off;(void)n;}
static void audio_silence(void){}
static int fl_erase4k(uint32_t off,uint32_t *took){*took=0;return st_erase(off);}
static int fl_write(uint32_t off,const void *p,uint32_t n){return st_prog(off,p,n);}
static uint8_t reply[2048];static uint32_t reply_n;
static void ota_wire_send(const void *p,uint32_t n){memcpy(reply,p,n);reply_n=n;}
static const uint8_t *queued_frame;static uint32_t queued_n;
static int ota_frame_get(const uint8_t **p,uint32_t *n){if(!queued_n)return 0;*p=queued_frame;*n=queued_n;return 1;}
static void ota_frame_done(void){queued_n=0;}
#include "../firmware/src/editor.c"
static uint32_t request(uint8_t cmd,const uint8_t *a,uint32_t n){
 uint8_t f[512]={ED_HDR0,ED_HDR1,ED_HDR2,cmd};if(n)memcpy(f+4,a,n);reply_n=0;ed_handle(f,n+4);return reply_n;
}
#ifndef FORMAT2_EDITOR_MAIN
#define FORMAT2_EDITOR_MAIN main
#endif
int FORMAT2_EDITOR_MAIN(void){
 web_nor_erase();web_boot();for(uint32_t i=0;i<1200;i++)web_render(44);
 request(ED_INFO,0,0);assert(reply_n);uint32_t off=5;while(reply[off])off++;off++;
 assert(reply[off]==6);off+=5;
 const char *names[]={"ANALOG","PHYS","SEM","HADES","DRONE","ENSEMBLE"};
 for(uint32_t i=0;i<6;i++){assert(!strcmp((const char*)reply+off,names[i]));off+=(uint32_t)strlen(names[i])+1;}
 for(uint32_t e=0;e<6;e++){
  uint8_t a[]={e,0};request(ED_PRESET,a,2);assert(reply_n&&reply[5]==e&&TSEL->eng_req==eng_unstore(e));
  request(ED_NAMES,a,1);assert(reply_n&&reply[5]==e);
 }
 uint32_t before=web_flash_writes_count();uint8_t bad[]={6,0};request(ED_PRESET,bad,2);assert(!reply_n);
 request(ED_SMP_ERASE,bad,1);assert(!reply_n&&web_flash_writes_count()==before);
 uint8_t st[]={0,1,60,0,0,0,ST_NOTE,0,80,90,3,6,0};
 request(ED_STEP_SET,st,sizeof st);assert(reply_n&&reply_n==19u&&TSEL->step[0].note[0]==60&&step_ratchet(&TSEL->step[0])==3);
 st[10]=0;request(ED_STEP_SET,st,sizeof st);assert(!reply_n);
 ed_bk_id=0;ed_bk_len=ed_bk_pos=sizeof(project_store_t);project_capture(&proj_scratch);
 assert(proj_pack((project_store_t*)ED_BK_RAW,&proj_scratch));ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);
 assert(ed_bk_commit()==2); /* refuse native-only partial restoration */
 request(ED_BACKUP_LIST,0,0);assert(reply_n&&reply[5]==2&&reply[7]==8);
 /* Real handler + service boundary: old framing cannot mutate any command,
  * including after a modern client has already edited the same device. */
 uint8_t modern[]={0,P_LEVEL,37,64};request(ED_SET,modern,sizeof modern);
 assert(TSEL->p[P_LEVEL]==37);
 uint8_t tracks_before[sizeof trk],settings_before[sizeof settings];
 memcpy(tracks_before,trk,sizeof trk);memcpy(settings_before,&settings,sizeof settings);
 before=web_flash_writes_count();
 for(uint32_t cmd=0;cmd<128;cmd++) {
  uint8_t legacy[16]={0x7D,0x46,0x4C,(uint8_t)cmd,0,P_LEVEL,42,64};
  reply_n=0;ed_handle(legacy,sizeof legacy);assert(!reply_n);
  queued_frame=legacy;queued_n=sizeof legacy;ed_service();assert(!queued_n);
  assert(!memcmp(tracks_before,trk,sizeof trk)&&!memcmp(settings_before,&settings,sizeof settings));
  assert(web_flash_writes_count()==before);
 }
 uint8_t legacy_info[]={0x7D,0x46,0x4C,ED_INFO};queued_frame=legacy_info;queued_n=sizeof legacy_info;
 reply_n=0;ed_service();assert(reply_n==10&&reply[5]==0&&reply[6]==0x60&&reply[7]==2&&reply[8]==18);
 assert(web_flash_writes_count()==before);
 if(getenv("FORMAT2_LAYOUT"))printf("LAYOUT {\"native\":%u,\"motion\":%zu,\"motionOffset\":%u,\"lfo2Offset\":%u,\"lfo2Bytes\":%u,\"midiOffset\":%u,\"nameOffset\":%u,\"container\":%zu,\"metaOffset\":%zu,\"metaRaw\":%zu,\"snapOffset\":%zu,\"snapBytes\":%zu,\"elasticOffset\":%zu,\"setOffset\":%zu,\"settings\":%zu,\"learnOffset\":%zu,\"panelBytes\":%zu,\"bankBytes\":%zu}\n",
 (unsigned)PROJ_STORE_SIZE,sizeof(motion_store_t),(unsigned)(68+NTRK*(P_COUNT+2+NSTEP*7)+24),(unsigned)PROJ_LFO2_OFF,(unsigned)PROJ_LFO2_SIZE,(unsigned)PROJ_MIDI_OFF,(unsigned)PROJ_NAME_OFF,(size_t)HP_V2_SIZE,(size_t)(32+PROJ_STORE_SIZE),(size_t)HP_RAW_META,(size_t)HP_HW_META,sizeof hp_rm_snap,(size_t)HP_RAW_META_V3,(size_t)(HP_RAW_META-HP_F2_SET_BYTES),sizeof(persist_t),offsetof(persist_t,learn_base),sizeof(panel_t),sizeof(up_bank_t));
 puts("FORMAT 2 protocol 18: six engines, strict steps, complete restore, 128 Classic commands rejected without RAM/flash mutation passed");
}
