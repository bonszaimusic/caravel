/* SPDX-License-Identifier: GPL-3.0-only */
/* Emulator-only, combined A/B project capsule in EXTENDED virtual NOR.
 * Not a hardware flash allocation. Legacy native objects remain readable. */
#include "home_store_types.h"
#include "home_store_codec.h"
#define HP_EXT 0x100000u
#define HP_COPY 16384u
#define HP_PREF 0x13F000u
_Static_assert(sizeof(hp_store_t)<=HP_COPY,"HOME capsule exceeds virtual sector");
static hp_store_t hp_io,hp_pending[4];
static uint8_t hp_pending_mask;
static uint32_t hp_as_seen,hp_as_saved,hp_as_time,hp_as_poll;
#include "home_store_common.c"





static const hp_store_t*hp_slot_disk(uint32_t slot)
{
 if(slot>4)return 0;const hp_store_t*a=(const void*)(nor+HP_EXT+slot*2*HP_COPY),*b=(const void*)((const uint8_t*)a+HP_COPY);
 int va=hp_capsule_ok(a),vb=hp_capsule_ok(b);if(!va)return vb?b:0;if(!vb)return a;return (int32_t)(b->seq-a->seq)>0?b:a;
}
static const hp_store_t*hp_slot(uint32_t slot){if(slot<4&&(hp_pending_mask&(1u<<slot)))return &hp_pending[slot];return hp_slot_disk(slot);}

static void hp_commit(uint32_t slot,hp_store_t*p)
{
 const hp_store_t*old=hp_slot_disk(slot);uint32_t off=HP_EXT+slot*2*HP_COPY;
 if(old==(const hp_store_t*)(nor+off))off+=HP_COPY;
 p->seq=old?old->seq+1:1;p->crc=hp_hash((const uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);
 memset(nor+off,255,HP_COPY);memcpy(nor+off+4,(const uint8_t*)p+4,p->size-4);memcpy(nor+off,&p->magic,4);web_flash_writes++;
}

static int hp_project_save(uint32_t slot,const char*name)
{
 if(project_compat_save_blocked(slot))return 1;
 if(slot>=4||chain_busy()){ui_message("STOP SONG FIRST");return 1;}
 if(!hp_capture(&hp_io)){ui_message("PROJECT TOO LARGE / NOT SAVED");return 2;}
 if(name){memset(proj_scratch.name,0,sizeof proj_scratch.name);memcpy(proj_scratch.name,name,str_len(name)<PROJ_NAME_LEN?str_len(name):PROJ_NAME_LEN);proj_scratch.sum=proj_sum(&proj_scratch);if(!proj_pack(&hp_io.native,&proj_scratch))return 2;}
 memcpy(&proj_slot[slot],&hp_io.native,sizeof hp_io.native);proj_cur=slot;pat_from_set(slot+1);proj_name_get(proj_name,(uint8_t*)proj_scratch.name);
 if(transport_busy()){
  hp_io.seq=0;hp_io.crc=hp_hash((const uint8_t*)&hp_io+32,hp_io.size-32);hp_io.reserved[0]=hp_hash(&hp_io,20);
  hp_pending[slot]=hp_io;hp_pending_mask|=1u<<slot;project_ram_set(project_ram_mask()|(1u<<slot));ui_message("SAVED RAM / STOP TO COMMIT");project_compat_saved();return 0;
 }
 if(st_save(OBJ_PROJECT0+slot,&hp_io.native,sizeof hp_io.native)){ui_message("SAVE ERROR");return 2;}
 hp_commit(slot,&hp_io);hp_pending_mask&=~(1u<<slot);project_ram_set(project_ram_mask()&~(1u<<slot));
 ui_message("SAVED: SOUND + HOME + PAIR");hp_dirty=0;project_compat_saved();return 0;
}
static int hp_project_load(uint32_t slot)
{
 const hp_store_t*p=hp_slot(slot);if(!p)return 0;
 if(!proj_import(&proj_scratch,&p->native,sizeof p->native))return 1;
 if(project_compat_request(&proj_scratch,slot))return 1;
 if(!hp_restore(p)){ui_message("PERF PROJECT INVALID");return 1;}
 proj_cur=slot;pat_from_set(slot+1);ui_message(project_compat_state.trial?"TEST LOADED / SAVE A COPY":"LOADED: SOUND + HOME + PAIR");go_home();return 1;
}

static void hp_autosave_poll(void)
{
 if(hp_pending_mask&&!transport_busy()){for(uint32_t slot=0;slot<4;slot++)if(hp_pending_mask&(1u<<slot)){hp_commit(slot,&hp_pending[slot]);hp_pending_mask&=~(1u<<slot);}}
 if(project_compat_state.hold_autosave)return;
 if(fm1_ms-hp_as_poll<250)return;hp_as_poll=fm1_ms;
 if(nor[HP_PREF]!=hp_default||nor[HP_PREF+1]!=(uint8_t)~hp_default){nor[HP_PREF]=hp_default;nor[HP_PREF+1]=(uint8_t)~hp_default;web_flash_writes++;}
 uint32_t sig=hp_signature();if(sig!=hp_as_seen||!autosave_quiet()){hp_as_seen=sig;hp_as_time=fm1_ms;return;}
 if(sig==hp_as_saved||(ui_prefs&PREF_RESTORE_OFF)||fm1_ms-hp_as_time<10000)return;
 if(!hp_capture(&hp_io))return;hp_commit(4,&hp_io);hp_as_saved=sig;hp_dirty=0;
}
static int hp_autosave_boot(int allowed)
{
 hp_pending_mask=0;
 if(nor[HP_PREF]<3&&nor[HP_PREF+1]==(uint8_t)~nor[HP_PREF])hp_default=nor[HP_PREF];
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,hp_default);
 for(uint32_t slot=0;slot<4;slot++){const hp_store_t*p=hp_slot(slot);if(p)memcpy(&proj_slot[slot],&p->native,sizeof p->native);}
 int restored=0;const hp_store_t*p=hp_slot(4);if(p&&allowed&&!(ui_prefs&PREF_RESTORE_OFF)&&proj_import(&proj_scratch,&p->native,sizeof p->native)&&!project_compat_request(&proj_scratch,4))restored=hp_restore(p);
 hp_as_seen=hp_as_saved=hp_signature();hp_as_time=fm1_ms;return restored;
}

static int hp_forget(uint32_t slot){if(slot>=4)return 1;hp_pending_mask&=~(1u<<slot);memset(nor+HP_EXT+slot*2*HP_COPY,255,2*HP_COPY);web_flash_writes++;return 0;}
