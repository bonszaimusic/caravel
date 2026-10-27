/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-1 test backend: 5 slots x 2 atomic copies x 2 sectors, E8000..FBFFF.
 * This deliberately uses the STOCK updater's VM-backup margin. Custom OTA staging,
 * user samples, native projects, FM6 bank and settings retain their existing ranges.
 * Sector tails F00..FFF stay erased so boot/update record scans cannot see payload.
 */
#include "home_store_types.h"
#define HP_HW_BASE 0xE8000u
#define HP_HW_COPY 8192u
#define HP_HW_MAGIC 0x32504648u
#define HP_HW_ROW 0u
#define HP_HW_CELL 8u
#define HP_HW_MAC 2056u
#define HP_HW_ROUTE 2072u
#define HP_HW_NEVENT 2392u
#define HP_HW_EVENT 2394u
#define HP_HW_BASEVAL 2778u
#define HP_HW_PAIR 2782u
#define HP_HW_META (HP_HW_PAIR+sizeof(hp_pair_store_t))
#define HP_HW_DATA (sizeof(project_store_t)+HP_HW_META)
_Static_assert(HP_HW_DATA<=7168u,"Performance project exceeds two protected sectors");
_Static_assert(UP_SLOTS<64u,"User preset origin must fit codec high bits");
_Static_assert(NPAGES<63u,"Revisit packed page field before adding more pages");
_Static_assert(NTRK==4&&HP_ROWS==32&&HP_EVENTS==128,"Revisit hardware codec dimensions");
_Static_assert(sizeof(proj_wire_u.raw)>=sizeof(hp_store_t),"HPM1 scratch exceeds native transfer workspace");
#define hp_io (*(hp_store_t *)(void *)proj_wire_u.raw)
static uint8_t hp_pending[4][HP_HW_META] __attribute__((section(".pool")));
static uint8_t hp_pending_mask;
static uint32_t hp_as_seen,hp_as_saved,hp_as_time,hp_as_poll,hp_retry;
#include "home_store_common.c"

typedef struct {uint32_t magic,version,size,seq,crc,flags,rsv,hcrc;} hp_hw_header_t;
static void hp_put(uint8_t*p,uint32_t v,uint32_t n){while(n--){*p++=(uint8_t)v;v>>=8;}}
static uint32_t hp_take(const uint8_t*p,uint32_t n){uint32_t v=0;while(n)v=(v<<8)|p[--n];return v;}
static uint32_t hp_ref_pack(hp_ref_t r){return r.id|(uint32_t)r.aux<<11|(uint32_t)(r.engine==255?31:r.engine)<<15|(uint32_t)(r.track==255?7:r.track)<<20|(uint32_t)r.kind<<23|(uint32_t)(r.page<63?r.page:63)<<26;}
static hp_ref_t hp_ref_unpack(uint32_t v){hp_ref_t r={0};r.id=v&2047;r.aux=(v>>11)&15;r.engine=(v>>15)&31;if(r.engine==31)r.engine=255;r.track=(v>>20)&7;if(r.track==7)r.track=255;r.kind=(v>>23)&7;r.page=v>>26;if(r.page==63)r.page=0xFFFF;return r;}
/* Encode into independent pending RAM. Decode backwards also supports expansion
 * in-place from hp_io.model; destination fields always lie at or above the input. */
static void hp_meta_pack(uint8_t*out,const hp_model_t*m,const hp_pair_store_t*p){
 memset(out,0,HP_HW_META);
 for(uint32_t t=0;t<4;t++){
  hp_put(out+2*t,(m->track[t].count-1)|m->track[t].active<<5|(uint32_t)m->track[t].pad[0]<<10,2);
  for(uint32_t r=0;r<32;r++)for(uint32_t k=0;k<4;k++)hp_put(out+HP_HW_CELL+4*(t*128+r*4+k),hp_ref_pack(m->track[t].cell[r][k]),4);
  for(uint32_t c=0;c<4;c++){out[HP_HW_MAC+t*4+c]=m->macro[t][c].base;for(uint32_t j=0;j<4;j++){uint8_t*q=out+HP_HW_ROUTE+5*(t*16+c*4+j);hp_put(q,hp_ref_pack(m->macro[t][c].route[j].ref),4);q[4]=(uint8_t)m->macro[t][c].route[j].amount;}}
 }
 hp_put(out+HP_HW_NEVENT,m->events,2);
 for(uint32_t i=0;i<m->events;i++){const hp_event_t*e=&m->event[i];hp_put(out+HP_HW_EVENT+3*i,e->track|(uint32_t)e->step<<2|(uint32_t)e->control<<8|(uint32_t)e->lock<<11|(uint32_t)e->value<<12,3);}
 hp_put(out+HP_HW_BASEVAL,(uint16_t)m->pair_base,2);out[HP_HW_BASEVAL+2]=m->pair_motion;out[HP_HW_BASEVAL+3]=m->pair_valid;
 memcpy(out+HP_HW_PAIR,p,sizeof*p);
}
static void hp_meta_unpack(hp_model_t*m,hp_pair_store_t*p,const uint8_t*in){
 memcpy(p,in+HP_HW_PAIR,sizeof*p);
 m->pair_valid=in[HP_HW_BASEVAL+3];m->pair_motion=in[HP_HW_BASEVAL+2];m->pair_base=(int16_t)hp_take(in+HP_HW_BASEVAL,2);m->pad=0;
 for(int32_t i=127;i>=0;i--){uint32_t v=hp_take(in+HP_HW_EVENT+3*i,3);m->event[i]=(hp_event_t){v&3,(v>>2)&63,(v>>8)&7,(v>>11)&1,(v>>12)&127,0};}
 m->events=hp_take(in+HP_HW_NEVENT,2);
 for(int32_t t=3;t>=0;t--)for(int32_t c=3;c>=0;c--){
  for(int32_t j=3;j>=0;j--){const uint8_t*q=in+HP_HW_ROUTE+5*(t*16+c*4+j);uint32_t v=hp_take(q,4);int16_t amount=(int8_t)q[4];m->macro[t][c].route[j]=(hp_route_t){hp_ref_unpack(v),amount,0};}
  m->macro[t][c].base=in[HP_HW_MAC+t*4+c];m->macro[t][c].pad=0;
 }
 for(int32_t t=3;t>=0;t--){for(int32_t r=31;r>=0;r--)for(int32_t k=3;k>=0;k--){uint32_t v=hp_take(in+HP_HW_CELL+4*(t*128+r*4+k),4);m->track[t].cell[r][k]=hp_ref_unpack(v);}uint32_t v=hp_take(in+2*t,2);m->track[t].count=(v&31)+1;m->track[t].active=(v>>5)&31;m->track[t].pad[0]=(v>>10)&63;m->track[t].pad[1]=0;}
}
static uint32_t hp_hw_addr(uint32_t slot,uint32_t copy){return HP_HW_BASE+slot*4*ST_SECTOR+copy*HP_HW_COPY;}
static uint32_t hp_hw_piece(uint32_t logical){return 256u+(logical/3584u)*4096u+logical%3584u;}
static int hp_hw_read(uint32_t base,uint32_t off,void*dst,uint32_t n){uint8_t*d=dst;while(n){uint32_t k=3584u-off%3584u;if(k>n)k=n;if(st_read(base+hp_hw_piece(off),d,k))return -1;off+=k;d+=k;n-=k;}return 0;}
static int hp_hw_head(uint32_t slot,uint32_t copy,hp_hw_header_t*h){
 if(slot>4||st_read(hp_hw_addr(slot,copy),h,sizeof*h)||h->magic!=HP_HW_MAGIC||h->version!=1||h->size!=(h->flags?0:HP_HW_DATA)||h->flags>1||h->hcrc!=hp_hash(h,28))return 0;
 uint8_t b[256];uint32_t hash=2166136261u;
 for(uint32_t off=0;off<h->size;){uint32_t k=h->size-off;if(k>sizeof b)k=sizeof b;if(hp_hw_read(hp_hw_addr(slot,copy),off,b,k))return 0;for(uint32_t j=0;j<k;j++){hash^=b[j];hash*=16777619u;}off+=k;}
 return hash==h->crc;
}
static int hp_hw_current(uint32_t slot,hp_hw_header_t*h){hp_hw_header_t a,b;int va=hp_hw_head(slot,0,&a),vb=hp_hw_head(slot,1,&b);if(!va&&!vb)return -1;if(vb&&(!va||(int32_t)(b.seq-a.seq)>0)){*h=b;return 1;}*h=a;return 0;}
static void hp_hw_seal(void){hp_io.magic=HP_MAGIC;hp_io.version=1;hp_io.size=sizeof hp_io;hp_io.crc=hp_hash((uint8_t*)&hp_io+32,sizeof hp_io-32);hp_io.reserved[0]=hp_hash(&hp_io,20);}
static const hp_store_t*hp_slot_disk(uint32_t slot){
 hp_hw_header_t h;int copy=hp_hw_current(slot,&h);if(copy<0||h.flags)return 0;
 for(uint32_t pass=0;pass<2;pass++){
  if(pass){copy^=1;if(!hp_hw_head(slot,copy,&h)||h.flags)return 0;}
  proj_wire_gen++;memset(&hp_io,0,sizeof hp_io);
  if(hp_hw_read(hp_hw_addr(slot,copy),0,&hp_io.native,sizeof hp_io.native)||hp_hw_read(hp_hw_addr(slot,copy),sizeof hp_io.native,&hp_io.model,HP_HW_META))continue;
  hp_meta_unpack(&hp_io.model,&hp_io.pair,(const uint8_t*)&hp_io.model);hp_io.seq=h.seq;hp_hw_seal();if(hp_capsule_ok(&hp_io))return &hp_io;
 }return 0;
}
static const hp_store_t*hp_slot(uint32_t slot){if(slot<4&&(hp_pending_mask&(1u<<slot))){proj_wire_gen++;memset(&hp_io,0,sizeof hp_io);hp_io.native=proj_slot[slot];hp_meta_unpack(&hp_io.model,&hp_io.pair,hp_pending[slot]);hp_hw_seal();return &hp_io;}return hp_slot_disk(slot);}
static int hp_hw_commit(uint32_t slot,const uint8_t*meta,const project_store_t*native,int erase){
 hp_hw_header_t old,h={0};int cur=hp_hw_current(slot,&old);uint32_t to=cur==0?1:0,base=hp_hw_addr(slot,to);
 h.magic=HP_HW_MAGIC;h.version=1;h.size=erase?0:HP_HW_DATA;h.seq=cur<0?1:old.seq+1;h.flags=!!erase;h.crc=2166136261u;
 for(uint32_t off=0;off<h.size;off++){uint8_t v=off<sizeof*native?((const uint8_t*)native)[off]:meta[off-sizeof*native];h.crc=(h.crc^v)*16777619u;}
 h.hcrc=hp_hash(&h,28);
 if(st_erase(base)||st_erase(base+4096))return 0;
 for(uint32_t off=0;off<h.size;){uint8_t b[256];uint32_t physical=base+hp_hw_piece(off),k=256u-(physical&255u);if(k>h.size-off)k=h.size-off;for(uint32_t j=0;j<k;j++){uint32_t p=off+j;b[j]=p<sizeof*native?((const uint8_t*)native)[p]:meta[p-sizeof*native];}if(st_prog(physical,b,k))return 0;off+=k;}
 if(st_prog(base,&h,sizeof h))return 0;hp_hw_header_t check;return hp_hw_head(slot,to,&check)&&!memcmp(&h,&check,sizeof h);
}
static uint32_t hp_pref_value(void){uint32_t v=(ui_prefs3>>3)&3;return v==1?0:v==2?2:1;}
static int hp_project_save(uint32_t slot,const char*name){
 if(project_compat_save_blocked(slot))return 1;
 if(slot>=4||chain_busy()){ui_message("STOP SONG FIRST");return 1;}if(!flash_ok){ui_message("FLASH UNAVAILABLE");return 2;}
 proj_wire_gen++;hp_capture(&hp_io);
 if(name){memset(proj_scratch.name,0,sizeof proj_scratch.name);str_cpy(proj_scratch.name,name,sizeof proj_scratch.name);proj_scratch.sum=proj_sum(&proj_scratch);if(!proj_pack(&hp_io.native,&proj_scratch))return 2;}
 memcpy(&proj_slot[slot],&hp_io.native,sizeof hp_io.native);hp_meta_pack(hp_pending[slot],&hp_io.model,&hp_io.pair);hp_pending_mask|=1u<<slot;project_ram_set(project_ram_mask()|(1u<<slot));proj_cur=slot;pat_from_set(slot+1);proj_name_get(proj_name,(uint8_t*)proj_scratch.name);
 if(transport_busy()){ui_message("SAVED RAM / STOP TO COMMIT");project_compat_saved();return 0;}
 if(st_save(OBJ_PROJECT0+slot,&proj_slot[slot],sizeof proj_slot[slot])||!hp_hw_commit(slot,hp_pending[slot],&proj_slot[slot],0)){ui_message("RAM: SAVE ERROR / RETRY");hp_retry=fm1_ms;return 2;}
 hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));ui_message("SAVED: SOUND + HOME + PAIR");project_compat_saved();return 0;
}
static int hp_project_load(uint32_t slot){const hp_store_t*p=hp_slot(slot);if(!p)return 0;
 if(!proj_import(&proj_scratch,&p->native,sizeof p->native))return 1;
 if(project_compat_request(&proj_scratch,slot))return 1;if(!hp_restore(p)){ui_message("PERF PROJECT INVALID");return 1;}proj_cur=slot;pat_from_set(slot+1);go_home();ui_message(project_compat_state.trial?"TEST LOADED / SAVE A COPY":"LOADED: SOUND + HOME + PAIR");return 1;}
/* USB restore writes a validated saved project, without replacing the working sound. */
static int hp_project_import(uint32_t slot,const hp_store_t*p){
 if(slot>=4||transport_busy()||!flash_ok||project_compat_save_blocked(slot))return 0;
 if(!hp_capsule_ok(p)||!proj_import(&proj_scratch,&p->native,sizeof p->native)||!proj_engines_ok(&proj_scratch))return 0;
 if(p->pair.on)for(uint32_t m=0;m<2;m++){proj_trk_t*t=&proj_scratch.t[m?p->pair.b:p->pair.a];if(!pair_engine_ok(t->engine)||t->p[P_SPRD]||(uint32_t)t->p[P_VOICE]>V_UNISON)return 0;}
 hp_meta_pack(hp_pending[slot],&p->model,&p->pair);proj_slot[slot]=p->native;
 hp_pending_mask|=1u<<slot;project_ram_set(project_ram_mask()|(1u<<slot));
 if(st_save(OBJ_PROJECT0+slot,&proj_slot[slot],sizeof proj_slot[slot])||!hp_hw_commit(slot,hp_pending[slot],&proj_slot[slot],0)){hp_retry=fm1_ms;return 0;}
 hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));return 1;
}
/* Rename the stored snapshot, including any pending RAM snapshot. */
static int hp_project_rename(uint32_t slot,const char*name){
 if(transport_busy()){ui_message("STOP TO SAVE");return 1;}
 const hp_store_t*p=hp_slot(slot);
 if(!p)return -1;
 if(!proj_import(&proj_scratch,&p->native,sizeof p->native))return 2;
 memset(proj_scratch.name,0,sizeof proj_scratch.name);str_cpy(proj_scratch.name,name,sizeof proj_scratch.name);
 if(!proj_pack(&hp_io.native,&proj_scratch))return 2;
 hp_meta_pack(hp_pending[slot],&hp_io.model,&hp_io.pair);proj_slot[slot]=hp_io.native;
 hp_pending_mask|=1u<<slot;project_ram_set(project_ram_mask()|(1u<<slot));
 if(st_save(OBJ_PROJECT0+slot,&proj_slot[slot],sizeof proj_slot[slot])||!hp_hw_commit(slot,hp_pending[slot],&proj_slot[slot],0)){ui_message("RAM: SAVE ERROR / RETRY");hp_retry=fm1_ms;return 2;}
 hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));
 if(proj_cur==slot)proj_name_get(proj_name,(const uint8_t*)proj_scratch.name);ui_message("RENAMED");return 0;
}
static void hp_autosave_poll(void){
 if(!flash_ok)return;
 if(hp_pref_value()!=hp_default){uint32_t v=hp_default==0?1:hp_default==2?2:0;ui_prefs3=(ui_prefs3&~24u)|(v<<3);settings_save();}
 if(hp_pending_mask&&!transport_busy()&&(!hp_retry||fm1_ms-hp_retry>=1000)){
  uint32_t slot=0;while(!(hp_pending_mask&(1u<<slot)))slot++;
  if(st_save(OBJ_PROJECT0+slot,&proj_slot[slot],sizeof proj_slot[slot])||!hp_hw_commit(slot,hp_pending[slot],&proj_slot[slot],0)){hp_retry=fm1_ms;ui_message("RAM: SAVE ERROR / RETRY");return;}
  hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));hp_retry=0;ui_message("PERF PROJECT SAVED");
 }
 if(project_compat_state.hold_autosave)return;
 if(fm1_ms-hp_as_poll<250)return;hp_as_poll=fm1_ms;uint32_t sig=hp_signature();
 if(sig!=hp_as_seen||!autosave_quiet()){hp_as_seen=sig;hp_as_time=fm1_ms;return;}
 if(sig==hp_as_saved||(ui_prefs&PREF_RESTORE_OFF)||fm1_ms-hp_as_time<10000||fm1_ms-as.t<10000||(hp_retry&&fm1_ms-hp_retry<1000))return;
 proj_wire_gen++;hp_capture(&hp_io);hp_meta_pack((uint8_t*)&hp_io.model,&hp,&hp_io.pair);
 if(st_save(OBJ_AUTOSAVE,&hp_io.native,sizeof hp_io.native)||!hp_hw_commit(4,(const uint8_t*)&hp_io.model,&hp_io.native,0)){hp_retry=fm1_ms;return;}hp_as_saved=sig;hp_retry=0;
}
static int hp_autosave_boot(int allowed){
 hp_pending_mask=0;hp_default=hp_pref_value();for(uint32_t t=0;t<4;t++)hp_apply(t,hp_default);
 for(uint32_t slot=0;slot<4;slot++){const hp_store_t*p=hp_slot_disk(slot);if(p)proj_slot[slot]=p->native;}
 int restored=0;const hp_store_t*p=hp_slot_disk(4);if(p&&allowed&&!(ui_prefs&PREF_RESTORE_OFF)&&proj_import(&proj_scratch,&p->native,sizeof p->native)&&!project_compat_request(&proj_scratch,4))restored=hp_restore(p);
 hp_as_seen=hp_as_saved=hp_signature();hp_as_time=fm1_ms;return restored;
}
static int hp_forget(uint32_t slot){if(slot>=4)return 1;if(!hp_hw_commit(slot,0,0,1))return 1;hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));return 0;}
