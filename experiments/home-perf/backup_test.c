/* SPDX-License-Identifier: GPL-3.0-only */
#define HARDWARE_TEST_MAIN hardware_baseline_main
#include "hardware_test.c"
enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t rep[4096];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
static int ed_flash_stop(void) { return transport_busy(); }
#include "../../firmware/src/editor_backup.c"

static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
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

static hp_store_t received,saved,working;
static void get_all(uint32_t id,hp_store_t*out){
 uint32_t len=0,crc=0;CHECK(!list(id,&len,&crc)&&len==10000);
 for(uint32_t off=0;off<len;off+=256){uint32_t n=len-off;if(n>256)n=256;CHECK(!get(id,off,n));CHECK(ed_unpack7(rep+9,rep_n-9,(uint8_t*)out+off,n)==n);}
 CHECK(st_crc32(out,len)==crc&&hp_capsule_ok(out));
}
static int bridge(void){
 web_nor_erase();web_boot();run_ms(1400);hp_apply(0,2);hp.macro[0][0].base=66;hp_event_put(0,4,0,91,1);CHECK(!hp_project_save(0,0));
 uint8_t a[1024],head[3];while(fread(head,1,3,stdin)==3){uint32_t n=head[1]|(uint32_t)head[2]<<8;if(n>sizeof a||fread(a,1,n,stdin)!=n)return 3;rep_n=0;
  if(head[0]==14){ed_b(a[0]);ed_b(0);}else ed_backup_handle(head[0],a,n);
  uint8_t size[2]={rep_n&255,rep_n>>8};fwrite(size,1,2,stdout);fwrite(rep,1,rep_n,stdout);fflush(stdout);
 }return 0;
}
int main(int argc,char**argv){
 if(argc>1)return bridge();
 web_nor_erase();web_boot();run_ms(1400);CHECK(web_pair_prepare(0,2,ENGI_SEM,ENGI_DRONE));run_ms(40);
 hp_apply(0,2);hp.macro[0][0].base=66;hp_event_put(0,4,0,91,1);hp.macro[0][0].route[0]=(hp_route_t){hp_track_ref(0,P_FILT,0xFFFF),24,0};CHECK(!hp_project_save(0,0));
 hp_apply(0,0);hp.macro[0][0].base=12;get_all(2,&saved);CHECK(saved.model.track[0].count==2&&saved.model.macro[0][0].base==66&&saved.pair.on);
 get_all(0,&working);CHECK(working.model.track[0].count==1&&working.model.macro[0][0].base==12);
 /* Listing all objects and reading any slot cannot overwrite the captured working project. */
 uint32_t len=0,crc=0;CHECK(!list(0,&len,&crc));CHECK(!get(2,0,256));CHECK(!get(0,0,256));CHECK(ed_unpack7(rep+9,rep_n-9,(uint8_t*)&received,256)==256);CHECK(!memcmp(&received,&working,256));
 CHECK(!put_all(3,&saved,sizeof saved,st_crc32(&saved,sizeof saved)));CHECK(hp.macro[0][0].base==12);get_all(3,&received);CHECK(received.model.macro[0][0].base==66&&received.pair.on);
 CHECK(!put_all(0,&saved,sizeof saved,st_crc32(&saved,sizeof saved)));CHECK(hp.macro[0][0].base==66&&ep.on&&hp.events==1);
 uint32_t writes=web_flash_writes;saved.crc^=1;CHECK(put_all(4,&saved,sizeof saved,st_crc32(&saved,sizeof saved))==2);CHECK(web_flash_writes==writes);saved.crc^=1;
 CHECK(put_all(4,&saved,sizeof saved,st_crc32(&saved,sizeof saved)^1)==2);CHECK(web_flash_writes==writes);
 CHECK(!put_begin(4,sizeof saved,st_crc32(&saved,sizeof saved)));CHECK(!put_chunk(4,0,(uint8_t*)&saved,256));usb.resets++;CHECK(put_end(4,2)==5);CHECK(web_flash_writes==writes);
 CHECK(!put_begin(4,sizeof saved,st_crc32(&saved,sizeof saved)));fm1_ms+=15001;CHECK(put_end(4,2)==5);
 CHECK(!put_all(3,&saved.native,sizeof saved.native,st_crc32(&saved.native,sizeof saved.native)));CHECK(!hp_slot(1));project_load(1);CHECK(hp.macro[0][0].base==0&&!ep.on);
 CHECK(!put_all(0,&saved.native,sizeof saved.native,st_crc32(&saved.native,sizeof saved.native)));CHECK(hp.events==0);
 persist_t prefs;settings_export(&prefs);prefs.favorites.factory[15][13]=(prefs.favorites.factory[15][13]&~24u)|16u;hp_default=0;CHECK(!put_all(1,&prefs,sizeof prefs,st_crc32(&prefs,sizeof prefs)));CHECK(hp_default==2);
 printf("USB PERFORMANCE BACKUP: %u checks passed: LIST/GET chunking and CRC, stable working snapshot, slot import without changing sound, full working restore, corruption, USB reset, timeout and legacy native restore.\n",checks);return 0;
}
