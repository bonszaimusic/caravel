/* SPDX-License-Identifier: GPL-3.0-only
 * HOME RANDOM MORPH v1 (experimental, volatile).
 * Included from home_ui.c, AFTER hp_get/hp_set have been defined.
 * Control-rate only. No audio-thread work, dynamic allocation, or flash writes.
 *
 * The HOME ACTION bank triggers start, freeze/resume, return, recapture.
 * The transport clock is BPM-based; snapshot A stays volatile.
 *
 * Snapshot A is taken on the FIRST RANDOM and is kept across further random
 * destinations until a new session is deliberately started after RETURN.
 * A freeze never applies a stale target value.
 */
static uint32_t hp_rm_rand(void)
{
 uint32_t x=hp_rm.seed;
 x^=x<<13;x^=x>>17;x^=x<<5;
 return hp_rm.seed=x?x:0x8C89D39Bu;
}
static uint32_t hp_rm_curve(uint32_t q,uint32_t index)
{
 if(q>=65536u)return 65536u;
 if(hp_rm.shape==HP_RM_LINEAR)return q;
 uint32_t s=(uint32_t)(((uint64_t)q*q*(196608u-2u*q))>>32);
 if(hp_rm.shape==HP_RM_SMOOTH)return s;
 /* DRIFT: three monotone trajectories with the SAME endpoints, no LFO jitter. */
 if(index%3u==0)return s;
 if(index%3u==1)return (uint32_t)(((uint64_t)s*s)>>16);
 return 65536u-(uint32_t)(((uint64_t)(65536u-s)*(65536u-s))>>16);
}
static int32_t hp_rm_interp(int32_t a,int32_t b,uint32_t q)
{
 int64_t n=(int64_t)(b-a)*q;
 return a+(int32_t)(n>=0?(n+32768)/65536:-((-n+32768)/65536));
}
static int hp_rm_same(hp_ref_t a,hp_ref_t b)
{
 return a.kind==b.kind&&a.track==b.track&&a.id==b.id&&a.aux==b.aux&&a.engine==b.engine;
}
static int hp_rm_safe(hp_ref_t r,const param_desc_t*d)
{
 if(!d||d->max<=d->min||d->fmt==F_ENUM||d->fmt==F_ONOFF)return 0;
 if(r.kind==HP_TRACK){
  uint32_t id=hp_tid(r);
  /* Don't change identity, gate/mute, sample, step, or transport controls.
   * Engine-specific parameters may be continuous but must pass hp_desc. */
  if(id==P_LEVEL||id==P_PAN||id==P_REV||id==P_FILT||
     (id>=P_E0&&id<P_COUNT))return 1;
  return motion_param(id)&&!param_insert(id);
 }
 if(r.kind==HP_GLOBAL)return 0; /* global tempo/routing are never randomised */
 if(r.kind==HP_LFO2||r.kind==HP_DX||r.kind==HP_FM6)return 1;
 if(r.kind==HP_PAIR)return r.id==2; /* morph amount only; never pair on/off */
 return 0; /* macro/switch/special controls require independent priority rules */
}
/* Random safety is stricter than manual/snapshot control. Snapshot recall may
 * intentionally restore a saved LEVEL; RANDOM must never randomise gain. */
static int hp_rm_random_allowed(hp_ref_t r,const param_desc_t*d)
{
 if(!d)return 0;
 if(r.kind==HP_TRACK && hp_tid(r)==P_LEVEL)return 0;
 if(d->fmt==F_DB)return 0;
 const char *label=d->label;
 if(label && (!strcmp(label,"LEVEL") || !strcmp(label,"LVL") ||
              !strcmp(label,"GAIN") || !strcmp(label,"VOLUME")))return 0;
 return 1;
}
/* Hz constraints apply to F_CUTOFF synth and filter parameters ONLY, not
 * bipolar P_FILT or sound-specific non-frequency parameters. Use the same
 * CUTOFF_HZ table as the real FM-1 user interface and DSP. */
static void hp_rm_cut_bounds(const param_desc_t*d,int32_t*lo,int32_t*hi)
{
 *lo=d->min;*hi=d->max;
 if(d->fmt!=F_CUTOFF)return;
 for(int32_t i=d->min;i<=d->max;i++){
  if(CUTOFF_HZ[i & 127]>=150u){*lo=i;break;}
 }
 for(int32_t i=d->max;i>=d->min;i--){
  if(CUTOFF_HZ[i & 127]<=6000u){*hi=i;break;}
 }
}
/* A physical HOME/source-page turn owns its control for the REST OF the
 * current morph (also through FREEZE/RESUME and RETURN). Never write it in
 * hp_rm_poll again. The next independently started morph may recapture it. */
static void hp_rm_disengage(hp_ref_t r)
{
 if(hp_rm.state==HP_RM_IDLE)return;
 if(r.track==255u)r.track=(uint8_t)song.sel;
 for(uint32_t i=0;i<hp_rm.count;i++)
  if(hp_rm_same(hp_rm.cell[i].ref,r))
   hp_rm.cell[i].released=1u;
}
static void hp_rm_reset(void)
{
 uint8_t bars=hp_rm.bars,depth=hp_rm.depth,scope=hp_rm.scope,shape=hp_rm.shape;
 uint32_t seed=hp_rm.seed;
 memset(&hp_rm,0,sizeof hp_rm);
 hp_rm.bars=bars?bars:8;hp_rm.depth=depth?depth:35;
 hp_rm.scope=scope<=HP_RM_ALL?scope:HP_RM_TRACK;
 hp_rm.shape=shape<=HP_RM_DRIFT?shape:HP_RM_SMOOTH;
 hp_rm.seed=seed?seed:0x8C89D39Bu;
}
static int hp_rm_add(hp_ref_t r,int random_only)
{
 if(!r.kind)return 1;
 if(r.track==255u)r.track=(uint8_t)song.sel; /* pin owner at capture, never follow later selection */
 const param_desc_t*d=hp_desc(r);
 if(!hp_rm_safe(r,d) || (random_only&&!hp_rm_random_allowed(r,d)))return 1;
 for(uint32_t i=0;i<hp_rm.count;i++)if(hp_rm_same(hp_rm.cell[i].ref,r))return 1;
 if(hp_rm.count==HP_RM_MAX)return 0; /* all-or-nothing, no hidden truncation */
 int32_t value=hp_get(r);
 if(value<d->min||value>d->max)return 1;
 hp_rm_cell_t*c=&hp_rm.cell[hp_rm.count++];
 c->ref=r;c->original=c->from=c->to=c->last=(int16_t)value;c->released=0u;
 return 1;
}
static int hp_rm_capture(int random_only)
{
 hp_rm.count=0;
 uint32_t t0=song.sel;
 if(hp_rm.scope==HP_RM_ROW){
  hp_track_t*h=&hp.track[t0];uint32_t row=h->active;
  for(uint32_t k=0;k<4;k++)if(!hp_rm_add(h->cell[row][k],random_only))goto full;
 }else{
  uint32_t lo=hp_rm.scope==HP_RM_TRACK?t0:0;
  uint32_t hi=hp_rm.scope==HP_RM_TRACK?t0+1:NTRK;
  for(uint32_t t=lo;t<hi;t++){
   hp_track_t*h=&hp.track[t];
   for(uint32_t row=0;row<h->count;row++)
    for(uint32_t k=0;k<4;k++){
     hp_ref_t r=h->cell[row][k];
     /* SEL-relative references resolve to the HOME owner track, not the
      * currently selected track, when capturing multiple tracks. */
     if(r.track==255u)r.track=(uint8_t)t;
     if(!hp_rm_add(r,random_only))goto full;
    }
  }
 }
 hp_rm.origin_track=(uint8_t)t0;
 hp_rm.origin_row=hp.track[t0].active;
 return hp_rm.count!=0u;
 full:hp_rm.count=0;ui_message("RANDOM: TOO MANY TARGETS");return 0;
}
static int hp_rm_valid(void)
{
 for(uint32_t i=0;i<hp_rm.count;i++){
  const hp_rm_cell_t*c=&hp_rm.cell[i];
  if(!c->released && !hp_rm_safe(c->ref,hp_desc(c->ref)))return 0;
 }
 return 1;
}
#if CARAVEL_ACTION_BANKS
/* Four explicit, independent snapshots. A slot captures exactly the current
 * morph-safe HOME refs within ROW/TRACK/ALL at capture time. Slots may have
 * different scopes. Their values are immutable until the next long A4 hold.
 * Any currently moving morph is cancelled without resetting audible controls.
 *
 * The shared hp_rm cells are only a scratch list while capturing. A capture
 * cannot overwrite an earlier snapshot unless the complete list succeeds.
 */
static void hp_rm_snapshot_clear(void)
{
 memset(hp_rm_snap,0,sizeof hp_rm_snap);
}
static uint32_t hp_rm_snapshot_count(uint32_t slot)
{
 return slot<HP_RM_SNAP_SLOTS?hp_rm_snap[slot].count:0u;
}
static int hp_rm_snapshot_store(uint32_t slot)
{
 if(slot>=HP_RM_SNAP_SLOTS)return 0;
 hp_rm_reset(); /* preserve current live sound; discard obsolete morph */
 if(!hp_rm_capture(0)){
  ui_message("SNAP: NO SAFE HOME TARGETS");ui.force=1;return 0;
 }
 hp_rm_snap_t *snap=&hp_rm_snap[slot];
 for(uint32_t i=0;i<hp_rm.count;i++){
  snap->cell[i].ref=hp_rm.cell[i].ref;
  snap->cell[i].value=hp_rm.cell[i].original;
 }
 snap->scope=hp_rm.scope;
 snap->origin_track=hp_rm.origin_track;
 snap->origin_row=hp_rm.origin_row;
 snap->count=(uint16_t)hp_rm.count; /* publish count only after all values */
 hp_rm_reset();
 char message[32];perf_format(message,sizeof message,"SNAP S%u SAVED",slot+1u);
 ui_message(message);ui.force=1;
 return 1;
}
/* Recall is all-or-nothing. Ref identities are pinned to captured tracks;
 * HOME layout or selection may change, but engine/type-invalid refs block the
 * entire morph without partially moving any control.
 * Morph starts from the ACTUAL live values, including manual/frozen edits.
 */
static int hp_rm_snapshot_recall(uint32_t slot,uint32_t now)
{
 if(slot>=HP_RM_SNAP_SLOTS || !hp_rm_snap[slot].count)return 0;
 const hp_rm_snap_t*snap=&hp_rm_snap[slot];
 if(snap->count>HP_RM_MAX){ui_message("SNAP: INVALID SIZE");return 0;}
 for(uint32_t i=0;i<snap->count;i++){
  const hp_rm_snap_cell_t*c=&snap->cell[i];
  const param_desc_t*d=hp_desc(c->ref);
  if(!hp_rm_safe(c->ref,d) || c->value<d->min || c->value>d->max){
   ui_message("SNAP: TARGET CHANGED");ui.force=1;return 0;
  }
 }
 hp_rm_reset();
 for(uint32_t i=0;i<snap->count;i++){
  const hp_rm_snap_cell_t*c=&snap->cell[i];
  hp_rm_cell_t*dst=&hp_rm.cell[i];
  int32_t live=hp_get(c->ref);
  dst->ref=c->ref;
  dst->original=dst->from=dst->last=(int16_t)live;
  dst->to=c->value;
 }
 hp_rm.count=snap->count;
 hp_rm.phase=0;
 hp_rm.stamp=now;
 hp_rm.state=HP_RM_RECALL;
 char message[32];perf_format(message,sizeof message,"MORPH TO SNAP S%u",slot+1u);
 ui_message(message);ui.force=1;
 return 1;
}
#endif

static void hp_rm_freeze(void)
{
 if(hp_rm.state==HP_RM_RANDOM||hp_rm.state==HP_RM_RETURN||hp_rm.state==HP_RM_RECALL){
  hp_rm.frozen_from=hp_rm.state;hp_rm.state=HP_RM_FROZEN;ui_message("RANDOM MORPH FROZEN");ui.force=1;
 }
}
static void hp_rm_resume(uint32_t now)
{
 if(hp_rm.state!=HP_RM_FROZEN)return;
 hp_rm.state=hp_rm.frozen_from==HP_RM_RETURN?HP_RM_RETURN:
  hp_rm.frozen_from==HP_RM_RECALL?HP_RM_RECALL:HP_RM_RANDOM;
 hp_rm.stamp=now;ui_message("RANDOM MORPH RESUMED");ui.force=1;
}
static void hp_rm_start(uint32_t now)
{
 if(hp_rm.state==HP_RM_IDLE||hp_rm.state==HP_RM_RECALL||
    (hp_rm.state==HP_RM_FROZEN&&hp_rm.frozen_from==HP_RM_RECALL)){
  hp_rm_reset();
  if(!hp_rm_capture(1)){ui_message("RANDOM: NO SAFE CONTROLS");return;}
 }
 if(!hp_rm_valid()){hp_rm_reset();ui_message("RANDOM: TARGET CHANGED");return;}
 for(uint32_t i=0;i<hp_rm.count;i++){
  hp_rm_cell_t*c=&hp_rm.cell[i];
  const param_desc_t*d=hp_desc(c->ref);
  int32_t cur=hp_get(c->ref);
  c->from=(int16_t)cur;c->last=(int16_t)cur;
  if(c->released)continue; /* manual override persists through re-randomise */
  if(!hp_rm_random_allowed(c->ref,d)){
   c->released=1u; /* preserve legacy captured LEVELs without writing */
   continue;
  }
  int32_t lower,upper;
  hp_rm_cut_bounds(d,&lower,&upper);
  /* Never force a user's manually chosen out-of-band cutoff to jump:
   * leave it untouched until the user explicitly moves it into 150..6000Hz. */
  if(d->fmt==F_CUTOFF && (cur<lower||cur>upper)){
   c->released=1u;
   continue;
  }
  int32_t span=upper-lower;
  int32_t radius=(span*hp_rm.depth+199)/200; /* depth is fraction of SAFE band */
  int32_t jitter=radius?(int32_t)(hp_rm_rand()%(2u*(uint32_t)radius+1u))-radius:0;
  c->to=(int16_t)clamp(cur+jitter,lower,upper);
 }
 hp_rm.phase=0;hp_rm.stamp=now;hp_rm.state=HP_RM_RANDOM;
 ui_message("RANDOM MORPH START");ui.force=1;
}
static void hp_rm_return(uint32_t now)
{
 if(hp_rm.state!=HP_RM_FROZEN&&hp_rm.state!=HP_RM_RANDOM&&hp_rm.state!=HP_RM_DONE)return;
 if(!hp_rm_valid()){hp_rm_reset();ui_message("RANDOM: TARGET CHANGED");return;}
 for(uint32_t i=0;i<hp_rm.count;i++){
  hp_rm_cell_t*c=&hp_rm.cell[i];
  if(c->released)continue; /* do not fight a manual adjustment on RETURN */
  c->from=(int16_t)hp_get(c->ref);
  c->to=c->original;c->last=c->from;
 }
 hp_rm.phase=0;hp_rm.stamp=now;hp_rm.state=HP_RM_RETURN;
 ui_message("RANDOM MORPH RETURN");ui.force=1;
}
static void hp_rm_press(uint32_t oct,uint32_t now)
{
 if(oct&2u){hp_rm_start(now);return;}
 if(oct&1u){
  if(hp_rm.state==HP_RM_RANDOM||hp_rm.state==HP_RM_RETURN)hp_rm_freeze();
  else if(hp_rm.state==HP_RM_FROZEN||hp_rm.state==HP_RM_DONE)hp_rm_return(now);
  else ui_message("OCT+: RANDOM FIRST");
 }
}
static void hp_rm_poll(uint32_t now)
{
 if(hp_rm.state!=HP_RM_RANDOM&&hp_rm.state!=HP_RM_RETURN&&hp_rm.state!=HP_RM_RECALL)return;
 if(!hp_rm_valid()){hp_rm_freeze();ui_message("RANDOM: PARAM UNAVAILABLE");return;}
 uint32_t delta=now-hp_rm.stamp;hp_rm.stamp=now;
 if(delta>1000u)delta=1000u; /* sleeping/USB stalls never leap many bars */
 uint32_t bpm=(uint32_t)clamp(song.g[G_BPM],20,300);
 uint32_t bars=hp_rm.bars?hp_rm.bars:8u;
 uint32_t bar_ms=240000u/bpm; /* 32-bit only: no __udivdi3 on pi32v2 */
 uint32_t advance=delta*65536u/(bar_ms*bars); /* delta <=1000, no overflow */
 hp_rm.phase=hp_rm.phase+advance>=65536u?65536u:hp_rm.phase+advance;
 for(uint32_t i=0;i<hp_rm.count;i++){
  hp_rm_cell_t*c=&hp_rm.cell[i];
  if(c->released)continue;
  uint32_t pos=hp_rm_curve(hp_rm.phase,i);
  int32_t next=hp_rm_interp(c->from,c->to,pos);
  if(next!=c->last){
   hp_set(c->ref,next,0);c->last=(int16_t)next;
  }
 }
 if(hp_rm.phase==65536u){
  int returned=hp_rm.state==HP_RM_RETURN, recalled=hp_rm.state==HP_RM_RECALL;
  hp_rm.state=returned||recalled?HP_RM_IDLE:HP_RM_DONE;
  ui_message(recalled?"SNAP REACHED":returned?"RANDOM RETURNED":"RANDOM DESTINATION");
 }
 ui.force=1;
}
