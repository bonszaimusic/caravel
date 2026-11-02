/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded main-loop codec. A failed fit never reaches a flash erase. */
#if HP_SNAPSHOT_STORE
#define HP_RAW_META (HP_HW_META+sizeof hp_rm_snap)
_Static_assert(HP_V2_SIZE<=sizeof(hp_store_t),"v2 exceeds existing USB staging");
_Static_assert(HP_RAW_META<=8192u,"review codec stack before enlarging snapshot state");
/* Literal 0..127: 1..128 bytes. Match 128..255: 3..130 bytes,
 * followed by a little-endian 1..4096 backward distance. */
static uint32_t hp_zip(uint8_t*out,uint32_t cap,const uint8_t*in,uint32_t n)
{
 uint16_t table[1024];memset(table,0,sizeof table);
 uint32_t pos=0,w=0,lit=0,head=0;
 while(pos<n){
  uint32_t len=0,dist=0;
  if(pos+2<n){
   uint32_t h=(in[pos]*251u+in[pos+1]*31u+in[pos+2])&1023u;
   uint32_t old=table[h];table[h]=(uint16_t)(pos+1);
   if(old&&pos+1-old<=4096u){uint32_t at=old-1;
    while(len<130u&&pos+len<n&&in[at+len]==in[pos+len])len++;
    dist=pos-at;
   }
  }
  if(len>=3u){
   if(w+3u>cap)return 0;
   out[w++]=(uint8_t)(128u+len-3u);out[w++]=(uint8_t)dist;out[w++]=(uint8_t)(dist>>8);
   pos+=len;lit=0;
  }else{
   if(!lit){if(w==cap)return 0;head=w++;}
   if(w==cap)return 0;out[w++]=in[pos++];out[head]=(uint8_t)lit++;
   if(lit==128u)lit=0;
  }
  if(!(pos&127u))fm1_wdt_feed();
 }
 return w;
}
static int hp_unzip(uint8_t*out,uint32_t size,const uint8_t*in,uint32_t n)
{
 uint32_t r=0,w=0;
 while(r<n){uint32_t tag=in[r++],k;
  if(tag<128u){k=tag+1;if(k>n-r||k>size-w)return 0;memcpy(out+w,in+r,k);r+=k;w+=k;}
  else {k=tag-128u+3u;if(n-r<2u||k>size-w)return 0;
   uint32_t d=in[r]|(uint32_t)in[r+1]<<8;r+=2;if(!d||d>w||d>4096u)return 0;
   while(k--){out[w]=out[w-d];w++;}
  }
 }
 return w==size;
}
/* The RANDOM / JOURNEY performance settings of a project travel in the spare pad bytes of the first slot of track 1's set (version 3; for now
 * one setting for all tracks), which are zero in every other slot:
 *   byte 0: bit 7 present, bits 0..2 BARS index (hp_rm_times), bits 3..5 DEPTH index (hp_rm_depths), bit 6 zero
 *   byte 1: bits 0..1 SCOPE, bits 2..3 SHAPE, bit 4 DIRECTION (1 = reverse), bits 5..7 WAIT index (0/1/2/4/8/16 bars)
 * Both zero: the defaults apply. A journey is never stored as running: a loaded project starts stopped, with the first slot selected. A
 * change that is waiting for the next leg is saved as the intended value.
 * Version 2 (0.7.1 .. 0.8.0) stored four slots of up to 128 targets for the whole project, each with an owner track and a scope; a version 2
 * project is read and migrated: every target goes to the set of the track it belongs to (a global or Pair control to the owner's), in the same
 * slot number, at most 32 per slot (the rest is dropped and counted). Its settings are kept, its skipped-track mask is gone. */
#define HP_PS_PRESENT 0x80u
typedef struct {                                   /* the version 2 slot: 648 bytes, four of them in the capsule */
 uint16_t count;uint8_t scope,origin_track,origin_row,pad;
 uint8_t refs[HP_RM_V2_MAX][3];int16_t values[HP_RM_V2_MAX];
} hp_rm_snap_v2_t;
#define HP_RAW_META_V2 (HP_HW_META+4u*sizeof(hp_rm_snap_v2_t))   /* a version 2 capsule's decompressed size */
_Static_assert(HP_RAW_META_V2<=HP_RAW_META,"a version 2 block fits the buffer of a version 3 one");
typedef const hp_rm_snap_t (*hp_snaps_t)[HP_RM_SNAP_SLOTS];
static int hp_ps_bits_ok(uint32_t a,uint32_t b)
{
 if(!a)return !b;
 return (a&HP_PS_PRESENT)&&!(a&0x40u)&&(a&7u)<7u&&((a>>3)&7u)<6u&&(b&3u)<3u&&((b>>2)&3u)<3u&&((b>>5)&7u)<6u;
}
static int hp_ps_valid(hp_snaps_t s)
{
 for(uint32_t t=0;t<NTRK;t++){
  for(uint32_t k=1;k<HP_RM_SNAP_SLOTS;k++)if(s[t][k].pad[0]||s[t][k].pad[1])return 0;      /* only the first slot of each track carries its settings */
  if(!hp_ps_bits_ok(s[t][0].pad[0],s[t][0].pad[1]))return 0;
 }
 return 1;
}
#define HP_PS_DEFAULT_WORD (HP_PS_PRESENT|3u|(2u<<3)|((1u|(1u<<2))<<8))   /* BARS 8, DEPTH 35, TRACK, SMOOTH; WAIT 0, forward */
static uint32_t hp_ps_idx(const uint8_t*tab,uint32_t n,uint32_t v,uint32_t fallback){for(uint32_t i=0;i<n;i++)if(tab[i]==v)return i;return fallback;}
static uint32_t hp_ps_word(void)                      /* byte 0 and 1: the settings pad bytes */
{
 uint16_t pend;uint8_t val[HP_JVN];
 {uint32_t irq=perf_irq_save();pend=hp_j_pend;for(uint32_t i=0;i<HP_JVN;i++)val[i]=hp_j_val[i];perf_irq_restore(irq);}
 if(hp_et()!=(song.sel&3u))pend=0;                 /* the MIDI mailbox belongs to the selected track */
 uint32_t bars=hp_j.set[0]?hp_j.set[0]-1u:hp_ps_idx(hp_rm_times,7u,hp_rm.bars,3u);
 uint32_t depth=hp_j.set[1]?hp_j.set[1]-1u:hp_ps_idx(hp_rm_depths,6u,hp_rm.depth,2u);
 uint32_t scope=hp_j.set[2]?hp_j.set[2]-1u:hp_rm.scope;
 uint32_t shape=hp_j.set[3]?hp_j.set[3]-1u:hp_rm.shape;
 uint32_t wait=hp_j.wait<6u?hp_j.wait:0u,rev=hp_j.rev&1u;
 /* A settings CC that arrived since the last frame is still in the MIDI mailbox (hp_journey_poll has not taken it): it is the latest
  * intended value and is saved, without taking it from the mailbox. A DIR button edge in the mailbox will toggle the direction. */
 if(pend&(1u<<HJ_BARS))bars=hp_j_band(val[HJ_BARS],7u);
 if(pend&(1u<<HJ_DEPTH))depth=hp_j_band(val[HJ_DEPTH],6u);
 if(pend&(1u<<HJ_SCOPE))scope=hp_j_band(val[HJ_SCOPE],3u);
 if(pend&(1u<<HJ_SHAPE))shape=hp_j_band(val[HJ_SHAPE],3u);
 if(pend&(1u<<HJ_WAIT))wait=hp_j_band(val[HJ_WAIT],6u);
 if(pend&(1u<<HJ_DIR))rev^=1u;
 uint32_t a=HP_PS_PRESENT|bars|depth<<3;
 uint32_t b=(scope&3u)|(shape&3u)<<2|rev<<4|wait<<5;
 return a|b<<8;
}
static void hp_ps_pack(hp_rm_snap_t(*s)[HP_RM_SNAP_SLOTS])   /* each track's settings word */
{
 for(uint32_t t=0;t<NTRK;t++){hp_eng_ovr=(uint8_t)t;uint32_t w=hp_ps_word();hp_eng_ovr=255u;s[t][0].pad[0]=(uint8_t)w;s[t][0].pad[1]=(uint8_t)(w>>8);}
}
static void hp_ps_apply(hp_rm_snap_t(*s)[HP_RM_SNAP_SLOTS])   /* s: the loaded sets (the pads are cleared again), 0 = a project without any */
{
 for(uint32_t t=0;t<NTRK;t++){
  uint32_t a=s?s[t][0].pad[0]:0,b=s?s[t][0].pad[1]:0;
  if(!a){a=HP_PS_DEFAULT_WORD&255u;b=HP_PS_DEFAULT_WORD>>8;}
  hp_eng_ovr=(uint8_t)t;
  hp_rm.bars=hp_rm_times[a&7u];hp_rm.depth=hp_rm_depths[(a>>3)&7u];hp_rm.scope=(uint8_t)(b&3u);hp_rm.shape=(uint8_t)((b>>2)&3u);
  hp_j.rev=(uint8_t)((b>>4)&1u);hp_j.wait=(uint8_t)((b>>5)&7u);
  hp_j.on=hp_j.paused=hp_j.held=0;hp_j.phase=0;hp_j.cur=hp_j.tgt=0;hp_j.set[0]=hp_j.set[1]=hp_j.set[2]=hp_j.set[3]=0;hp_j_arrived=0;
  hp_eng_ovr=255u;
  if(s){s[t][0].pad[0]=s[t][0].pad[1]=0;}
 }
}
static hp_ref_t hp_rm_snapshot_ref_raw(const uint8_t*b)
{
 uint32_t v=b[0]|((uint32_t)b[1]<<8u)|((uint32_t)b[2]<<16u);
 hp_ref_t r={0};r.page=0xFFFFu;r.id=v&511u;r.kind=(v>>9u)&7u;
 r.track=(v>>12u)&3u;r.engine=(v>>14u)&31u;r.aux=(v>>19u)&15u;
 if(r.engine==31u)r.engine=255u;
 return r;
}
/* the targets of one slot: valid refs, values in their range, no duplicate; owner < NTRK: a track's own controls must be its own */
static int hp_snap_refs_ok(const uint8_t(*refs)[3],const int16_t*vals,uint32_t n,uint32_t owner)
{
 for(uint32_t i=0;i<n;i++){
  hp_ref_t r=hp_rm_snapshot_ref_raw(refs[i]);const param_desc_t*d=0;
  if(refs[i][2]&128u||!hp_ref_valid(r)||!r.kind)return 0;
  if(owner<NTRK&&hp_snap_own_kind(r)&&r.track!=owner)return 0;
  if(r.kind==HP_TRACK){if(r.aux>1)return 0;d=param_desc_of(r.engine==255u?0:r.engine,hp_tid(r));}
  else if(r.kind==HP_LFO2)d=lfo2_desc(r.id);
  else if(r.kind==HP_DX)d=dx_desc(r.id);
  else if(r.kind==HP_PAIR&&r.id==2)d=&HP_PDESC[2];
  else if(r.kind==HP_FM6){if(vals[i]<0||vals[i]>fm6_max(r.id))return 0;continue;}
  if(!d||vals[i]<d->min||vals[i]>d->max)return 0;
  for(uint32_t j=0;j<i;j++)if(hp_rm_same(r,hp_rm_snapshot_ref_raw(refs[j])))return 0;
 }
 return 1;
}
static int hp_snapshots_valid(hp_snaps_t s)
{
 if(!hp_ps_valid(s))return 0;
 for(uint32_t t=0;t<NTRK;t++)for(uint32_t k=0;k<HP_RM_SNAP_SLOTS;k++){
  const hp_rm_snap_t*q=&s[t][k];
  if(q->count>HP_RM_SNAP_MAX||q->scope>HP_RM_TRACK||!hp_snap_refs_ok(q->refs,q->values,q->count,t))return 0;
 }
 return 1;
}
static int hp_snapshots_valid_v2(const hp_rm_snap_v2_t*s)
{
 if(s[3].pad||(s[2].pad&0xF0u)||!hp_ps_bits_ok(s[0].pad,s[1].pad)||(!s[0].pad&&s[2].pad))return 0;
 for(uint32_t k=0;k<4u;k++){
  const hp_rm_snap_v2_t*q=&s[k];
  if(q->count>HP_RM_V2_MAX||q->scope>HP_RM_ALL||q->origin_track>=NTRK||q->origin_row>=HP_ROWS||!hp_snap_refs_ok(q->refs,q->values,q->count,255u))return 0;
 }
 return 1;
}
/* A version 2 capsule's four slots become the sets of the tracks the targets belong to; returns how many targets did not fit 32 per slot. */
static uint32_t hp_snapshots_migrate(const hp_rm_snap_v2_t*old)
{
 uint32_t dropped=0;
 memset(hp_rm_snap,0,sizeof hp_rm_snap);
 for(uint32_t k=0;k<4u;k++)for(uint32_t i=0;i<old[k].count;i++){
  hp_ref_t r=hp_rm_snapshot_ref_raw(old[k].refs[i]);
  hp_rm_snap_t*q=&hp_rm_snap[hp_snap_own_kind(r)?r.track:old[k].origin_track][k];
  if(q->count>=HP_RM_SNAP_MAX){dropped++;continue;}
  memcpy(q->refs[q->count],old[k].refs[i],3);q->values[q->count]=old[k].values[i];q->count++;
  q->scope=old[k].scope==HP_RM_ROW?HP_RM_ROW:HP_RM_TRACK;
 }
 for(uint32_t t=0;t<NTRK;t++){hp_rm_snap[t][0].pad[0]=old[0].pad;hp_rm_snap[t][0].pad[1]=old[1].pad;}    /* the settings of 0.7.1-0.8.0 were global: every track starts with them */
 return dropped;
}
static int hp_meta_valid(const uint8_t*b,uint32_t version)
{
 for(uint32_t t=0;t<4;t++){
  uint32_t v=hp_take(b+2*t,2);if(((v>>5)&31u)>(v&31u)||(v>>10)>UP_SLOTS)return 0;
  for(uint32_t i=0;i<128;i++)if(!hp_ref_valid(hp_ref_unpack(hp_take(b+HP_HW_CELL+4*(t*128+i),4))))return 0;
  for(uint32_t c=0;c<4;c++){
   if(b[HP_HW_MAC+t*4+c]>127)return 0;
   for(uint32_t j=0;j<4;j++){const uint8_t*q=b+HP_HW_ROUTE+5*(t*16+c*4+j);int32_t a=(int8_t)q[4];if(!hp_ref_valid(hp_ref_unpack(hp_take(q,4)))||a< -64||a>63)return 0;}
  }
 }
 uint32_t ne=hp_take(b+HP_HW_NEVENT,2);if(ne>128)return 0;
 for(uint32_t i=0;i<ne;i++){uint32_t v=hp_take(b+HP_HW_EVENT+3*i,3);uint32_t c=(v>>8)&7;if(v>>19||c==4||c==5)return 0;}
 const hp_pair_store_t*p=(const void*)(b+HP_HW_PAIR);
 if(p->on>1||p->motion>1||p->base<0||(p->on&&(p->a>=4||p->b>=4||p->a==p->b)))return 0;
 for(uint32_t i=0;i<NSTEP;i++)if(p->lane[i]< -1)return 0;
 return version==2u?hp_snapshots_valid_v2((const void*)(b+HP_HW_META)):hp_snapshots_valid((hp_snaps_t)(const void*)(b+HP_HW_META));
}
static int hp_v2_decode(const hp_store_t*p,uint8_t*raw)
{
 return (p->version==2||p->version==3)&&p->size==HP_V2_SIZE&&p->reserved[1]>0&&p->reserved[1]<=HP_V2_META&&p->reserved[2]==(p->version==2?HP_RAW_META_V2:HP_RAW_META)&&hp_unzip(raw,p->reserved[2],(const uint8_t*)&p->model,p->reserved[1])&&hp_meta_valid(raw,p->version);
}
static uint32_t hp_v2_encode(uint8_t*out,const hp_model_t*m,const hp_pair_store_t*pair,hp_snaps_t snaps)
{
 uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));
 hp_pair_store_t captured;int live=pair==0;
 /* MIDI and sequencer ISRs may edit HOME automation and Pair controls.
  * Capture one coherent input, then restore IRQs before validation/compression. */
 uint32_t irq=perf_irq_save();
 if(live){hp_pair_capture(&captured);pair=&captured;}
 hp_meta_pack(raw,m,pair);if(snaps)memcpy(raw+HP_HW_META,snaps,sizeof hp_rm_snap);else memset(raw+HP_HW_META,0,sizeof hp_rm_snap);
 if(snaps==hp_rm_snap)hp_ps_pack((hp_rm_snap_t(*)[HP_RM_SNAP_SLOTS])(void*)(raw+HP_HW_META));   /* the live project: its performance settings go with the snapshots */
 if(live)for(uint32_t t=0;t<NTRK;t++){uint32_t v=hp_take(raw+2*t,2);hp_put(raw+2*t,(v&1023u)|(uint32_t)trk[t].user<<10,2);}
 perf_irq_restore(irq);
 if(!hp_meta_valid(raw,3u))return 0;
 return hp_zip(out,HP_V2_META,raw,sizeof raw);
}
#endif
