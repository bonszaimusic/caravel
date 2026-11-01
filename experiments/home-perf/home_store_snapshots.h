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
/* The RANDOM / JOURNEY performance settings of a project travel in the one-byte pad fields of snapshot slots 0 and 1, which every earlier
 * firmware insists are zero (so an older firmware refuses such a project instead of misreading it; nothing is lost):
 *   slot 0 pad: bit 7 present, bits 0..2 BARS index (hp_rm_times), bits 3..5 DEPTH index (hp_rm_depths), bit 6 zero
 *   slot 1 pad: bits 0..1 SCOPE, bits 2..3 SHAPE, bit 4 DIRECTION (1 = reverse), bits 5..7 WAIT index (0/1/2/4/8/16 bars)
 * Both zero = a project made before this: the defaults apply. Slots 2 and 3 stay zero. The size and layout of the block do not
 * change. A journey is never stored as running: a loaded project starts stopped, with the first slot selected. A change that is
 * waiting for the next leg is saved as the intended value. */
#define HP_PS_PRESENT 0x80u
static int hp_ps_valid(const hp_rm_snap_t*s)
{
 uint32_t a=s[0].pad,b=s[1].pad;
 if(s[2].pad||s[3].pad)return 0;
 if(!a)return !b;
 return (a&HP_PS_PRESENT)&&!(a&0x40u)&&(a&7u)<7u&&((a>>3)&7u)<6u&&(b&3u)<3u&&((b>>2)&3u)<3u&&((b>>5)&7u)<6u;
}
#define HP_PS_DEFAULT_WORD (HP_PS_PRESENT|3u|(2u<<3)|((1u|(1u<<2))<<8))   /* BARS 8, DEPTH 35, TRACK, SMOOTH; WAIT 0, forward */
static uint32_t hp_ps_idx(const uint8_t*tab,uint32_t n,uint32_t v,uint32_t fallback){for(uint32_t i=0;i<n;i++)if(tab[i]==v)return i;return fallback;}
static uint32_t hp_ps_word(void)                      /* low byte: slot 0's pad, next byte: slot 1's pad */
{
 uint32_t bars=hp_j.set[0]?hp_j.set[0]-1u:hp_ps_idx(hp_rm_times,7u,hp_rm.bars,3u);
 uint32_t depth=hp_j.set[1]?hp_j.set[1]-1u:hp_ps_idx(hp_rm_depths,6u,hp_rm.depth,2u);
 uint32_t scope=hp_j.set[2]?hp_j.set[2]-1u:hp_rm.scope;
 uint32_t shape=hp_j.set[3]?hp_j.set[3]-1u:hp_rm.shape;
 uint32_t a=HP_PS_PRESENT|bars|depth<<3;
 uint32_t b=(scope&3u)|(shape&3u)<<2|(uint32_t)(hp_j.rev&1u)<<4|(uint32_t)(hp_j.wait<6u?hp_j.wait:0u)<<5;
 return a|b<<8;
}
static void hp_ps_pack(hp_rm_snap_t*s){uint32_t w=hp_ps_word();s[0].pad=(uint8_t)w;s[1].pad=(uint8_t)(w>>8);}
static void hp_ps_apply(hp_rm_snap_t*s)               /* s: the loaded snapshots (the pads are cleared again), 0 = a project without any */
{
 uint32_t a=s?s[0].pad:0,b=s?s[1].pad:0;
 if(!a){a=HP_PS_DEFAULT_WORD&255u;b=HP_PS_DEFAULT_WORD>>8;}
 hp_rm.bars=hp_rm_times[a&7u];hp_rm.depth=hp_rm_depths[(a>>3)&7u];hp_rm.scope=(uint8_t)(b&3u);hp_rm.shape=(uint8_t)((b>>2)&3u);
 hp_j.rev=(uint8_t)((b>>4)&1u);hp_j.wait=(uint8_t)((b>>5)&7u);
 hp_j.on=hp_j.paused=0;hp_j.phase=0;hp_j.cur=hp_j.tgt=0;hp_j.set[0]=hp_j.set[1]=hp_j.set[2]=hp_j.set[3]=0;hp_j_arrived=0;
 if(s){s[0].pad=s[1].pad=0;}
}
static int hp_snapshots_valid(const hp_rm_snap_t*s)
{
 if(!hp_ps_valid(s))return 0;
 for(uint32_t slot=0;slot<4;slot++){
  const hp_rm_snap_t*q=&s[slot];if(q->count>128||q->scope>HP_RM_ALL||q->origin_track>=NTRK||q->origin_row>=HP_ROWS)return 0;
  for(uint32_t i=0;i<q->count;i++){
   hp_ref_t r=hp_rm_snapshot_ref(q,i);const param_desc_t*d=0;
   if(q->refs[i][2]&128u||!hp_ref_valid(r)||!r.kind)return 0;
   if(r.kind==HP_TRACK){if(r.aux>1)return 0;d=param_desc_of(r.engine==255u?0:r.engine,hp_tid(r));}
   else if(r.kind==HP_LFO2)d=lfo2_desc(r.id);
   else if(r.kind==HP_DX)d=dx_desc(r.id);
   else if(r.kind==HP_PAIR&&r.id==2)d=&HP_PDESC[2];
   else if(r.kind==HP_FM6){if(q->values[i]<0||q->values[i]>fm6_max(r.id))return 0;continue;}
   if(!d||q->values[i]<d->min||q->values[i]>d->max)return 0;
   for(uint32_t j=0;j<i;j++)if(hp_rm_same(r,hp_rm_snapshot_ref(q,j)))return 0;
  }
 }
 return 1;
}
static int hp_meta_valid(const uint8_t*b)
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
 return hp_snapshots_valid((const void*)(b+HP_HW_META));
}
static int hp_v2_decode(const hp_store_t*p,uint8_t*raw)
{
 return p->version==2&&p->size==HP_V2_SIZE&&p->reserved[1]>0&&p->reserved[1]<=HP_V2_META&&p->reserved[2]==HP_RAW_META&&hp_unzip(raw,HP_RAW_META,(const uint8_t*)&p->model,p->reserved[1])&&hp_meta_valid(raw);
}
static uint32_t hp_v2_encode(uint8_t*out,const hp_model_t*m,const hp_pair_store_t*pair,const hp_rm_snap_t*snaps)
{
 uint8_t raw[HP_RAW_META] __attribute__((aligned(4)));
 hp_pair_store_t captured;int live=pair==0;
 /* MIDI and sequencer ISRs may edit HOME automation and Pair controls.
  * Capture one coherent input, then restore IRQs before validation/compression. */
 uint32_t irq=perf_irq_save();
 if(live){hp_pair_capture(&captured);pair=&captured;}
 hp_meta_pack(raw,m,pair);if(snaps)memcpy(raw+HP_HW_META,snaps,sizeof hp_rm_snap);else memset(raw+HP_HW_META,0,sizeof hp_rm_snap);
 if(snaps==hp_rm_snap)hp_ps_pack((hp_rm_snap_t*)(void*)(raw+HP_HW_META));   /* the live project: its performance settings go with the snapshots */
 if(live)for(uint32_t t=0;t<NTRK;t++){uint32_t v=hp_take(raw+2*t,2);hp_put(raw+2*t,(v&1023u)|(uint32_t)trk[t].user<<10,2);}
 perf_irq_restore(irq);
 if(!hp_meta_valid(raw))return 0;
 return hp_zip(out,HP_V2_META,raw,sizeof raw);
}
#endif
