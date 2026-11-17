/* SPDX-License-Identifier: GPL-3.0-only */
#define FORMAT2_EDITOR_MAIN preset_editor_base_main
#include "format2_editor_test.c"
int main(void) {
 web_nor_erase();web_boot();for(uint32_t i=0;i<1200;i++)web_render(44);
 static up_bank_t disk;
 for(uint32_t e=0;e<6;e++) {
  set_engine_of(TSEL,eng_unstore(e));TSEL->engine=TSEL->eng_req;
  for(uint32_t id=0;id<P_COUNT;id++) {
   const param_desc_t *d=track_desc(TSEL,id);TSEL->p[id]=(id&1)?d->min:d->max;
  }
  assert(!up_store(e,"F2 PRESET"));assert(up_used(e));
  const up_rec_t *r=up_rec(e);assert(r->engine==eng_unstore(e)&&r->np==114);
  for(uint32_t i=0;i<16;i++)assert(!r->note[i]);
 }
 assert(sizeof disk==3080&&st_load(OBJ_UPRESET0,&disk,sizeof disk)==sizeof disk);
 for(uint32_t e=0;e<6;e++)assert(disk.r[e].engine==e);
 ed_bk_id=6;ed_bk_len=ed_bk_pos=sizeof disk;
 memcpy(ED_BK_RAW,&disk,sizeof disk);ed_bk_crc=st_crc32(&disk,sizeof disk);
 memset(up_bank,0,sizeof up_bank);assert(!ed_bk_commit());
 for(uint32_t e=0;e<6;e++)assert(up_used(e)&&up_rec(e)->engine==eng_unstore(e));
 uint32_t before=web_flash_writes;disk.r[0].engine=6;
 memcpy(ED_BK_RAW,&disk,sizeof disk);ed_bk_crc=st_crc32(&disk,sizeof disk);
 assert(ed_bk_commit()==2&&web_flash_writes==before);
 puts("FORMAT 2 user presets: all six engines, parameter bounds, stored/runtime IDs and strict backup round-trip passed");
}
