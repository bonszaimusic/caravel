/* SPDX-License-Identifier: GPL-3.0-only */
#define BACKUP_TEST_MAIN backup_baseline_main
#include "backup_test.c"
#undef main
static hp_rm_snap_t expected[4],saved_slots[4][4];
static hp_store_t capsule,legacy;
static uint8_t flash_before[sizeof nor];
static void seal(hp_store_t*p){p->crc=hp_hash((uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);}
static void fill(unsigned variant){
 hp_rm_snapshot_clear();unsigned n=0;
 for(unsigned t=0;t<4;t++)for(unsigned id=0;id<P_COUNT&&n<128;id++){
  hp_ref_t r=hp_track_ref(t,id,0xFFFF);const param_desc_t*d=hp_desc(r);
  if(!hp_rm_safe(r,d))continue;
  for(unsigned slot=0;slot<4;slot++){
   hp_rm_snapshot_write_ref(&hp_rm_snap[slot],n,r);
   hp_rm_snap[slot].values[n]=d->min+(variant+slot+n)%((unsigned)(d->max-d->min)+1u);
  }n++;
 }
 CHECK(n==128);
 for(unsigned slot=0;slot<4;slot++){hp_rm_snap[slot].count=n;hp_rm_snap[slot].scope=slot%3;hp_rm_snap[slot].origin_track=slot;hp_rm_snap[slot].origin_row=slot*7;}
 CHECK(hp_snapshots_valid(hp_rm_snap));
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);fill(0);memcpy(expected,hp_rm_snap,sizeof expected);
 CHECK(hp_capture(&hp_io));seal(&hp_io);CHECK(hp_io.version==2&&hp_io.size==7200&&hp_capsule_ok(&hp_io));
 printf("v2 metadata: %u/%u bytes, 4x128 targets\n",hp_io.reserved[1],HP_V2_META);
 capsule=hp_io;hp_init();CHECK(!hp_rm_snap[0].count);CHECK(hp_restore(&capsule));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Full HOME rows, macro routes and 128 automation events coexist with 4x128. */
 for(unsigned t=0;t<4;t++){
  hp.track[t].count=32;hp.track[t].active=31;
  for(unsigned i=0;i<128;i++)hp.track[t].cell[i/4][i%4]=hp_rm_snapshot_ref(&hp_rm_snap[0],i);
  for(unsigned m=0;m<4;m++){hp.macro[t][m].base=20+m;for(unsigned j=0;j<4;j++)hp.macro[t][m].route[j]=(hp_route_t){hp_rm_snapshot_ref(&hp_rm_snap[0],t*16+m*4+j),(int)(j*32)-64,0};}
 }
 hp.events=128;for(unsigned i=0;i<128;i++)hp.event[i]=(hp_event_t){i%4,i/4,i%4,i%2,(i*17)%128,0};
 CHECK(hp_capture(&hp_io));seal(&hp_io);CHECK(hp_capsule_ok(&hp_io));
 printf("full HOME + 128 events + 4x128 snapshots: %u/%u bytes\n",hp_io.reserved[1],HP_V2_META);
 CHECK(!hp_project_save(0,0));hp_init();CHECK(hp_project_load(0));CHECK(hp.track[3].count==32&&hp.events==128);CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Incompressible input fails closed, and decompression rejects malformed spans. */
 uint8_t noise[HP_RAW_META],out[HP_V2_META];uint32_t seed=0xDEADBEEFu;
 for(unsigned i=0;i<sizeof noise;i++){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;noise[i]=(uint8_t)seed;}
 CHECK(!hp_zip(out,sizeof out,noise,sizeof noise));
 uint8_t invalid[]={128,0,0};CHECK(!hp_unzip(noise,sizeof noise,invalid,sizeof invalid));
 for(unsigned t=0;t<4;t++)hp_apply(t,2);hp.events=0;memset(hp.macro,0,sizeof hp.macro);
 /* A-D own independent snapshot sets, not the currently selected live slots. */
 for(unsigned s=0;s<4;s++){fill(s*17);memcpy(saved_slots[s],hp_rm_snap,sizeof hp_rm_snap);CHECK(!hp_project_save(s,0));}
 hp_init();CHECK(!hp_autosave_boot(0));
 for(unsigned s=0;s<4;s++){CHECK(hp_project_load(s));CHECK(!memcmp(saved_slots[s],hp_rm_snap,sizeof hp_rm_snap));}
 CHECK(!hp_project_rename(0,"SNAPSHOTS"));CHECK(hp_project_load(0));CHECK(!memcmp(saved_slots[0],hp_rm_snap,sizeof hp_rm_snap));
 /* Live save retains its original set while the live texture is edited. */
 fill(87);memcpy(expected,hp_rm_snap,sizeof expected);song.playing=1;CHECK(!hp_project_save(2,0));CHECK(hp_pending_mask&4);fill(11);song.playing=0;hp_autosave_poll();CHECK(!(hp_pending_mask&4));CHECK(hp_project_load(2));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Stable USB WORK, independent slot GETs, exact length and full import. */
 CHECK(!list(0,&ed_bk_len,&ed_bk_crc));CHECK(ed_bk_len==7200);
 for(unsigned s=0;s<4;s++){CHECK(!get(2+s,0,256));CHECK(!get(0,0,256));}
 uint32_t n=7200;memcpy(&capsule,ed_bk_performance,n);CHECK(hp_capsule_ok(&capsule));
 fill(2);CHECK(!put_all(0,&capsule,n,st_crc32(&capsule,n)));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 CHECK(!put_all(3,&capsule,n,st_crc32(&capsule,n)));CHECK(hp_project_load(1));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 /* Legacy HPM1 remains accepted and explicitly empties the four new slots. */
 FILE*f=fopen("experiments/home-perf/fixtures/home-v1.hpm","rb");CHECK(f&&fread(&legacy,1,sizeof legacy,f)==sizeof legacy);fclose(f);
 CHECK(hp_capsule_ok(&legacy));CHECK(hp_restore(&legacy));for(unsigned s=0;s<4;s++)CHECK(!hp_rm_snap[s].count);
 CHECK(hp_project_import(3,&legacy));CHECK(hp_project_load(3));for(unsigned s=0;s<4;s++)CHECK(!hp_rm_snap[s].count);
 /* Validate checksums AND compressed lengths, refs, owners and value ranges. */
 hp_store_t bad=capsule;bad.crc^=1;CHECK(!hp_restore(&bad));bad=capsule;bad.reserved[1]=HP_V2_META+1;seal(&bad);CHECK(!hp_restore(&bad));
 uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));CHECK(hp_v2_decode(&capsule,raw));hp_rm_snap_t*sn=(void*)(raw+HP_HW_META);
 sn[0].count=129;bad=capsule;bad.reserved[1]=hp_zip((uint8_t*)&bad.model,HP_V2_META,raw,sizeof raw);seal(&bad);CHECK(!hp_restore(&bad));
 CHECK(hp_v2_decode(&capsule,raw));sn[0].origin_track=4;bad=capsule;bad.reserved[1]=hp_zip((uint8_t*)&bad.model,HP_V2_META,raw,sizeof raw);seal(&bad);CHECK(!hp_restore(&bad));
 CHECK(hp_v2_decode(&capsule,raw));sn[0].values[0]=32767;bad=capsule;bad.reserved[1]=hp_zip((uint8_t*)&bad.model,HP_V2_META,raw,sizeof raw);seal(&bad);CHECK(!hp_restore(&bad));
 CHECK(hp_v2_decode(&capsule,raw));sn[0].refs[0][2]|=128;bad=capsule;bad.reserved[1]=hp_zip((uint8_t*)&bad.model,HP_V2_META,raw,sizeof raw);seal(&bad);CHECK(!hp_restore(&bad));
 /* Every interruption keeps the previous complete combined copy. */
 CHECK(hp_restore(&capsule));CHECK(!hp_project_save(0,0));memcpy(flash_before,nor,sizeof nor);fill(39);hp_capture(&hp_io);uint8_t meta[HP_PENDING_BYTES];CHECK(hp_pending_pack(meta,&hp_io));project_store_t native=hp_io.native;
 unsigned successes=0;
 for(int cut=0;cut<40;cut++){
  memcpy(nor,flash_before,sizeof nor);web_flash_fail_after=cut;int ok=hp_hw_commit(0,meta,&native,0);web_flash_fail_after=-1;
  const hp_store_t*p=hp_slot_disk(0);CHECK(p&&hp_restore(p));CHECK(hp_rm_snap[0].values[0]==(ok?39:expected[0].values[0]));successes+=ok;
 }
 CHECK(successes>0);
 /* First native -> v2 migration: interrupted writes leave native original. */
 web_nor_erase();web_boot();run_ms(1400);project_capture(&proj_scratch);CHECK(proj_pack(&native,&proj_scratch));CHECK(!st_save(OBJ_PROJECT0,&native,sizeof native));proj_slot[0]=native;
 memcpy(flash_before,nor,sizeof nor);fill(9);
 for(int cut=0;cut<40;cut++){
  memcpy(nor,flash_before,sizeof nor);hp_pending_mask=0;proj_slot[0]=native;web_flash_fail_after=cut;int rc=hp_project_save(0,0);web_flash_fail_after=-1;
  hp_pending_mask=0;hp_init();hp_autosave_boot(0);project_load(0);
  if(rc){CHECK(!hp_rm_snap[0].count);project_store_t disk;CHECK(st_load(OBJ_PROJECT0,&disk,sizeof disk)==sizeof disk);CHECK(!memcmp(&native,&disk,sizeof disk));}
  else CHECK(hp_rm_snap[0].count==128);
  fill(9);
 }
 /* Autosave detects snapshot-only changes and restores after boot. */
 hp_pending_mask=0;fill(27);memcpy(expected,hp_rm_snap,sizeof expected);uint32_t sig=hp_signature();hp_rm_snap[0].values[0]++;CHECK(hp_signature()!=sig);hp_rm_snap[0].values[0]--;
 hp_as_saved=0;hp_as_seen=hp_signature();hp_as_time=as.t=0;fm1_ms+=12000;hp_autosave_poll();CHECK(hp_slot_disk(4));hp_init();CHECK(hp_autosave_boot(1));CHECK(!memcmp(expected,hp_rm_snap,sizeof expected));
 for(unsigned sec=0xE8000;sec<0xFC000;sec+=4096)for(unsigned i=0xF00;i<0x1000;i++)CHECK(nor[sec+i]==255);
 printf("SNAPSHOT PERSISTENCE: %u checks PASS; 4x128, A-D, reboot, live-save, USB, legacy, malformed, interruption and first migration.\n",checks);
 return 0;
}
