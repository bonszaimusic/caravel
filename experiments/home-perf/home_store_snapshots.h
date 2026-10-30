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
static int hp_snapshots_valid(const hp_rm_snap_t*s)
{
 for(uint32_t slot=0;slot<4;slot++){
  const hp_rm_snap_t*q=&s[slot];if(q->count>128||q->scope>HP_RM_ALL||q->origin_track>=NTRK||q->origin_row>=HP_ROWS||q->pad)return 0;
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
 if(snaps&&!hp_snapshots_valid(snaps))return 0;
 hp_meta_pack(raw,m,pair);if(snaps)memcpy(raw+HP_HW_META,snaps,sizeof hp_rm_snap);else memset(raw+HP_HW_META,0,sizeof hp_rm_snap);
 return hp_zip(out,HP_V2_META,raw,sizeof raw);
}
#endif
