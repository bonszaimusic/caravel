/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared, unchanged v1 metadata packing. */
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
