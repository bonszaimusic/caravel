/* SPDX-License-Identifier: GPL-3.0-only
 * FORMAT 2 archive transport and atomic NOR recovery, real production backend. */
#define WEB_FLASH_TEST 1
#define FORMAT2_EDITOR_MAIN backup_editor_base_main
#include "format2_editor_test.c"
#define rep (ed_out+5)
static uint32_t call(uint32_t cmd,const uint8_t *a,uint32_t n) {
 ed_begin(cmd);assert(ed_backup_handle(cmd,a,n));return ed_n-5u;
}
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }

/* PUT begin: rc */
static uint32_t put_begin(uint32_t id, uint32_t len, uint32_t crc)
{
    uint8_t a[12] = {0, (uint8_t)id};
    put32(a + 2, len);
    put32(a + 7, crc);
    call(ED_BACKUP_PUT, a, sizeof a);
    return rep[2];
}
/* PUT one chunk of data at off: rc */
static uint32_t put_chunk(uint32_t id, uint32_t off, const uint8_t *p, uint32_t n)
{
    uint8_t a[16 + 300];
    uint32_t k = 7;
    a[0] = 1;
    a[1] = (uint8_t)id;
    put32(a + 2, off);
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        a[k++] = (uint8_t)m;
        for (i = 0; i < g; i++) a[k++] = p[i] & 127u;
        p += g;
        n -= g;
    }
    call(ED_BACKUP_PUT, a, k);
    return rep[2];
}
static uint32_t put_end(uint32_t id, uint32_t op)   /* op 2 commit, 3 abort */
{
    uint8_t a[2] = {(uint8_t)op, (uint8_t)id};
    call(ED_BACKUP_PUT, a, 2);
    return rep[2];
}
/* a whole object: begin, 256-byte chunks, commit; the first failing rc or the commit's */
static uint32_t put_all(uint32_t id, const void *v, uint32_t len, uint32_t crc)
{
    const uint8_t *p = v;
    uint32_t off, rc = put_begin(id, len, crc);
    for (off = 0; !rc && off < len; off += 256u)
        rc = put_chunk(id, off, p + off, len - off > 256u ? 256u : len - off);
    return rc ? rc : put_end(id, 2);
}
/* LIST: rc; the length and CRC of object id */
static uint32_t list(uint32_t id, uint32_t *len, uint32_t *crc)
{
    uint32_t i;
    call(ED_BACKUP_LIST, 0, 0);
    if (rep[1])
        return rep[1];
    for (i = 0; i < rep[2]; i++) {
        const uint8_t *o = rep + 3 + i * 11u;
        if (o[0] == id) {
            *len = ed_bk_r32(o + 1);
            *crc = ed_bk_r32(o + 6);
        }
    }
    return 0;
}
static uint32_t get(uint32_t id, uint32_t off, uint32_t count)
{
    uint8_t a[8] = {(uint8_t)id};
    put32(a + 1, off);
    a[6] = (uint8_t)(count & 127u);
    a[7] = (uint8_t)(count >> 7);
    call(ED_BACKUP_GET, a, sizeof a);
    return rep[1];
}

static hp_store_t candidate,original;
static uint8_t baseline_flash[sizeof nor];
static void boot_backup(void) {
 web_flash_fail_after=-1;web_nor_erase();web_boot();
 for(uint32_t i=0;i<1200;i++)web_render(44);
}
int main(void) {
 boot_backup();uint32_t n=0,crc=0;
 assert(!list(0,&n,&crc)&&rep[0]==2&&rep[2]==8&&n==6144);
 memcpy(&original,&ed_bk_performance,n);assert(crc==st_crc32(&original,n));
 assert(!get(0,0,256));
 /* WORK remains a stable snapshot while slot reads reuse project scratch. */
 assert(!hp_project_save(0,0));assert(!get(2,0,64));assert(!get(0,0,256));
 assert(!memcmp(&original,&ed_bk_performance,n));
 usb.resets++;assert(get(0,0,16)==5);assert(!list(0,&n,&crc));
 persist_t settings_wire;settings_export(&settings_wire);
 settings_wire.lowcut=2;settings_wire.palette=palette_to_stored(6);
 assert(!put_all(1,&settings_wire,sizeof settings_wire,st_crc32(&settings_wire,sizeof settings_wire)));
 assert(fx_lowcut==2&&settings.palette==6);
 uint32_t writes=web_flash_writes;
 assert(put_all(1,&settings_wire,sizeof settings_wire,0)==2&&web_flash_writes==writes);
 settings_wire.lowcut=4;
 assert(put_all(1,&settings_wire,sizeof settings_wire,st_crc32(&settings_wire,sizeof settings_wire))==2&&web_flash_writes==writes);
 settings_wire.lowcut=2;settings_wire.magic=0x50455237u;
 assert(put_all(1,&settings_wire,sizeof settings_wire,st_crc32(&settings_wire,sizeof settings_wire))==2&&web_flash_writes==writes);
 assert(put_begin(0,sizeof(project_store_t),0)==1);
 for(uint32_t id=8;id<=9;id++)assert(put_begin(id,0,0)==1);
 for(uint32_t id=32;id<=34;id++)assert(put_begin(id,0,0)==1);
 assert(!put_begin(0,6144,0));
 assert(put_chunk(0,1,(const uint8_t*)&original,64)==1);
 assert(!put_chunk(0,0,(const uint8_t*)&original,64));
 assert(put_end(0,2)==2&&web_flash_writes==writes);
 assert(!put_begin(0,6144,crc));fm1_ms+=15001;assert(put_chunk(0,0,(const uint8_t*)&original,64)==5);
 assert(!put_begin(0,6144,crc));usb.resets++;assert(put_end(0,2)==5);
 assert(!put_begin(0,6144,crc));assert(!put_end(0,3));assert(put_end(0,2)==5);
 transport_req=1;assert(!put_begin(0,6144,crc)&&transport_req==2&&!song.playing);assert(!put_end(0,3));transport_req=0;
 /* Reject old generations and malformed metadata before any write. */
 candidate=original;candidate.version=5;seal(&candidate);writes=web_flash_writes;
 assert(put_all(2,&candidate,6144,st_crc32(&candidate,6144))==2&&web_flash_writes==writes);
 candidate=original;candidate.native.raw[80]^=1;seal(&candidate);
 assert(put_all(2,&candidate,6144,st_crc32(&candidate,6144))==2&&web_flash_writes==writes);
 candidate=original;candidate.reserved[2]--;seal(&candidate);
 assert(put_all(2,&candidate,6144,st_crc32(&candidate,6144))==2&&web_flash_writes==writes);
 /* A complete round-trip includes the own-set flag and all sixteen slots. */
 performance_test_main();assert(hp_capture(&candidate));seal(&candidate);
 assert(!put_all(0,&candidate,6144,st_crc32(&candidate,6144)));
 assert(!put_all(3,&candidate,6144,st_crc32(&candidate,6144)));
 assert(hp_project_load(1));for(uint32_t t=0;t<4;t++)for(uint32_t k=0;k<4;k++)assert(hp_rm_snap[t][k].count==32);
 /* Strict bank generation/engine/version validation, no silent repair. */
 up_bank_t bank={.magic=UP_BANK_MAGIC,.rsize=sizeof(up_rec_t),.nslot=UP_PER_BANK};
 bank.r[0].used=UP_USED;bank.r[0].ver=99;bank.r[0].engine=0;bank.r[0].np=P_COUNT;writes=web_flash_writes;
 assert(put_all(6,&bank,sizeof bank,st_crc32(&bank,sizeof bank))==2&&web_flash_writes==writes);
 bank.r[0].ver=UP_VER;bank.r[0].engine=6;
 assert(put_all(6,&bank,sizeof bank,st_crc32(&bank,sizeof bank))==2&&web_flash_writes==writes);
 memset(bank.r,0,sizeof bank.r);assert(!put_all(6,&bank,sizeof bank,st_crc32(&bank,sizeof bank)));
 bank.magic=0x31425055u;writes=web_flash_writes;
 assert(put_all(6,&bank,sizeof bank,st_crc32(&bank,sizeof bank))==2&&web_flash_writes==writes);
 /* Every erase/program failure before the commit header preserves the last
  * complete copy. No hardware timing is simulated. */
 boot_backup();trk[0].step[0]=(step_t){.note={60},.n=1,.time=ST_NOTE,.vel=96};
 assert(!hp_project_save(0,0));assert(hp_capture(&original));seal(&original);
 trk[0].step[0].note[0]=72;assert(hp_capture(&candidate));seal(&candidate);
 memcpy(baseline_flash,nor,sizeof nor);uint32_t cut,success=0;
 for(cut=0;cut<128;cut++) {
  memcpy(nor,baseline_flash,sizeof nor);web_flash_fail_after=(int)cut;
  uint32_t rc=put_all(2,&candidate,6144,st_crc32(&candidate,6144));web_flash_fail_after=-1;
  assert(rc==0||rc==4);
  if(rc)assert(hp_pending_mask&1u);
  hp_pending_mask=0;project_ram_set(0); /* power loss discards the retry RAM */
  assert(hp_project_load(0));
  assert(trk[0].step[0].note[0]==(rc?60:72));
  for(uint32_t off=0xE8000;off<0xFC000;off+=4096)for(uint32_t k=0xF00;k<4096;k++)assert(nor[off+k]==255);
  if(!rc){success=1;break;}
 }
 assert(success&&cut>2);
 /* A tombstone stays erased after another boot; a failed erase keeps data. */
 web_flash_fail_after=0;assert(put_all(2,0,0,st_crc32(0,0))==4);web_flash_fail_after=-1;
 assert(hp_project_load(0));assert(!put_all(2,0,0,st_crc32(0,0)));assert(!hp_slot_disk(0));
 web_boot();assert(!hp_slot_disk(0));
 printf("FORMAT 2 backup: strict archives, stale/reset/timeout/order/CRC guards, complete round-trip, %u power-cut positions and durable erase passed\n",cut+1);
}
