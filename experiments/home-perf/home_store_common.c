/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared HPM1 validation, capture and restore. Backends provide hp_io. */
static uint32_t hp_hash(const void*p,uint32_t n){const uint8_t*b=p;uint32_t h=2166136261u;while(n--){h^=*b++;h*=16777619u;}return h;}
static void hp_pair_capture(hp_pair_store_t*p)
{
 memset(p,0,sizeof*p);p->on=ep.on;p->a=ep.a;p->b=ep.b;p->motion=ep.motion;p->base=ep.base;
 for(uint32_t i=0;i<hp.events;i++)if(hp.event[i].control>=6&&ep.on&&hp.pair_valid){p->base=hp.pair_base;p->motion=hp.pair_motion;break;}
 memcpy(p->lane,ep.lane,sizeof p->lane);
 for(uint32_t t=0;t<NTRK;t++)for(uint32_t k=0;k<NMSLOT;k++)p->mod[t][k]=trk[t].p[P_M1DST+3*k]==MD_PAIR;
}
static int hp_ref_valid(hp_ref_t r)
{
 if(!r.kind)return 1;if(r.track>=NTRK&&r.track!=255)return 0;
 if(r.kind==HP_TRACK)return r.aux<=1&&(r.aux?r.id<8&&r.engine<NENGINES:r.id<P_E0&&(r.engine==255||r.engine<NENGINES));
 if(r.kind==HP_GLOBAL)return r.id<=G_CDEPTH||r.id==G_RTYPE;
 if(r.kind==HP_PAIR||r.kind==HP_MACRO)return r.id<4;
 if(r.kind==HP_LFO2)return lfo2_target(r.id);
 if(r.kind==HP_DX)return dx_target(r.id);
 if(r.kind==HP_FM6)return r.id<FP_SIZE;
 return 0;
}
static int hp_model_valid(const hp_model_t*m)
{
 if(m->events>HP_EVENTS)return 0;
 for(uint32_t t=0;t<NTRK;t++){
  const hp_track_t*h=&m->track[t];if(h->pad[0]>UP_SLOTS)return 0;if(!h->count||h->count>HP_ROWS||h->active>=h->count)return 0;
  for(uint32_t row=0;row<HP_ROWS;row++)for(uint32_t k=0;k<4;k++)if(!hp_ref_valid(h->cell[row][k]))return 0;
  for(uint32_t c=0;c<4;c++){const hp_macro_t*a=&m->macro[t][c];if(a->base<0||a->base>127)return 0;for(uint32_t j=0;j<4;j++)if(!hp_ref_valid(a->route[j].ref)||a->route[j].amount< -64||a->route[j].amount>63)return 0;}
 }
 for(uint32_t i=0;i<m->events;i++){const hp_event_t*e=&m->event[i];if(e->track>=NTRK||e->step>=NSTEP||e->control>7||e->control==4||e->control==5||e->lock>1||e->value<0||e->value>127)return 0;}
 return 1;
}
static int hp_capsule_ok(const hp_store_t*p)
{
 if(p->magic!=HP_MAGIC||p->version!=1||p->size!=sizeof*p||p->reserved[0]!=hp_hash(p,20)||p->crc!=hp_hash((const uint8_t*)p+32,sizeof*p-32)||!hp_model_valid(&p->model))return 0;
 const hp_pair_store_t*q=&p->pair;if(q->on>1||q->motion>1||q->base<0)return 0;
 if(q->on&&(q->a>=NTRK||q->b>=NTRK||q->a==q->b))return 0;
 for(uint32_t i=0;i<NSTEP;i++)if(q->lane[i]<-1)return 0;
 return 1;
}
static void hp_capture(hp_store_t*p)
{
 memset(p,0,sizeof*p);p->magic=HP_MAGIC;p->version=1;p->size=sizeof*p;
 project_capture(&proj_scratch);proj_pack(&p->native,&proj_scratch);
 uint32_t irq=perf_irq_save();p->model=hp;for(uint32_t t=0;t<NTRK;t++)p->model.track[t].pad[0]=trk[t].user;hp_pair_capture(&p->pair);perf_irq_restore(irq);
}
static int hp_restore(const hp_store_t*p)
{
 if(!hp_capsule_ok(p))return 0;
 memcpy(&hp_io,p,sizeof hp_io);
 if(!proj_import(&proj_scratch,&hp_io.native,sizeof hp_io.native))return 0;
 hp_pair_store_t*pair=&hp_io.pair;
 if(pair->on)for(uint32_t m=0;m<2;m++){proj_trk_t*t=&proj_scratch.t[m?pair->b:pair->a];if(!pair_engine_ok(t->engine)||t->p[P_SPRD]||(uint32_t)t->p[P_VOICE]>V_UNISON)return 0;}
 if(project_restore_runtime(&proj_scratch))return 0;
 uint32_t irq=perf_irq_save();
 hp=hp_io.model;for(uint32_t t=0;t<NTRK;t++){uint32_t u=hp.track[t].pad[0];if(u&&(!up_used(u-1)||up_engine(u-1)==trk[t].eng_req))trk[t].user=u;}hp_last_valid=0;hp_backup_track=255;hp_motion_stop();
 if(pair->on){
  for(uint32_t t=0;t<NTRK;t++){memset(trk[t].v,0,sizeof trk[t].v);engine_block(&trk[t]);}
  voice_policy=0;voice_policy_sync();pair_cleanup_pending=0;panic_req=0;
  if(!pair_enable(pair->a,pair->b)){perf_irq_restore(irq);return 0;}
  ep.base=ep.effective=pair->base;ep.motion=pair->motion;hp.pair_base=pair->base;hp.pair_motion=pair->motion;hp.pair_valid=1;memcpy(ep.lane,pair->lane,sizeof ep.lane);
  for(uint32_t t=0;t<NTRK;t++)for(uint32_t k=0;k<NMSLOT;k++)if(pair->mod[t][k])trk[t].p[P_M1DST+3*k]=MD_PAIR;
 }
 perf_irq_restore(irq);hp_dirty=0;ui.force=1;return 1;
}
static uint32_t hp_signature(void)
{
 hp_pair_store_t p;hp_pair_capture(&p);return autosave_sig()^hp_hash(&hp,sizeof hp)^hp_hash(&p,sizeof p)^((uint32_t)trk[0].user|(uint32_t)trk[1].user<<8|(uint32_t)trk[2].user<<16|(uint32_t)trk[3].user<<24);
}

/* Canonical sound comparison; no flags, snapshots, flash writes or DSP changes. */
static int hp_sound_changed(const track_t *t)
{
 uint32_t k=trk_index(t),u=user_of(t),e=t->eng_req%NENGINES;
 int16_t p[P_COUNT];lfo2_config_t l2={{0}};uint8_t patch[128],actual[128];
 const engine_t*engine=ENGINES[e];
 if(u<UP_SLOTS){
  const up_rec_t*r=up_rec(u);if(r->engine!=e)return 0;
  up_values(r,p);if(r->ver>=8u)memcpy(l2.p,r->packed+UP_L2_AT,L2_N);
 }else{
  if(!engine->npresets)return 0;
  preset_sound_values(e,t->preset,p,&l2);
 }
 for(uint32_t i=0;i<P_COUNT;i++)if(!param_kept(i)&&!(e==ENGI_FM6&&i==P_E7)&&motion_base_value(t,i)!=p[i])return 1;
 if(memcmp(&l2,&lfo2_config[k],sizeof l2))return 1;
 if(e==ENGI_FM6){
  if(u>=UP_SLOTS||upf_get(u,patch))memcpy(patch,p[P_E7]>=0&&p[P_E7]<FM6_NFACTORY?FM6_FACTORY[p[P_E7]]:FM6_INIT,128);
  fm6_pack(fm6_patch[k],actual);return memcmp(patch,actual,128);
 }
 if(dx_track(t)&&dx_ready[k]){
  if(u>=UP_SLOTS||upf_get(u,patch)){dx_config_t c={0};memcpy(c.lane,DX_KIT_DEF,sizeof DX_KIT_DEF);memcpy(c.layer,DX_LAY_DEF,sizeof DX_LAY_DEF);dx_pack_config(&c,patch);}
  dx_pack_config(&dx_config[k],actual);return memcmp(patch,actual,128);
 }
 return 0;
}
