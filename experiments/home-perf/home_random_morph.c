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
 if(label && (str_eq(label,"LEVEL") || str_eq(label,"LVL") ||
              str_eq(label,"GAIN") || str_eq(label,"VOLUME")))return 0;
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
 * current morph (also through FREEZE/RESUME and RETURN). A new explicit
 * RANDOM/RECALL starts a separate trajectory. Never write it in
 * hp_rm_poll again. The next independently started morph may recapture it. */
static void hp_rm_disengage(hp_ref_t r)
{
 /* Also called from the MIDI path, in the audio block, while the UI may rebuild the cells (reset, capture, recall). Safe by
  * construction: the count is read once and bounded, every index stays inside the cell array, and the only write is a one-byte
  * flag, so a race can at worst release a cell that is being replaced anyway (the flag is cleared when a cell is filled) or miss
  * one for a block. The scan is at most 128 cells per control change (about 5 instructions per cell that does not match), not per audio sample. */
 uint32_t n=hp_rm.used;
 if(!n)return;
 if(n>HP_RM_MAX)n=HP_RM_MAX;
 if(r.track==255u)r.track=(uint8_t)song.sel;
 for(uint32_t i=0;i<n;i++){
  hp_rm_cell_t*c=&hp_rm.cell[i];
  /* fields compared in place (hp_rm_same takes its refs by value: two memcpy calls per id match on the target) */
  if(c->ref.id==r.id&&c->ref.kind==r.kind&&c->ref.track==r.track&&c->ref.aux==r.aux&&c->ref.engine==r.engine)c->released=1u;
 }
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
/* Origins share the existing 128 cells. Keep the current context at the
 * front so the interpolator and its CPU cost stay unchanged. Inactive
 * contexts retain originals; selecting a track alone never moves anything.
 * TRACK: 128..131, ROW: 0..127, ALL: 132, snapshot recall: 255. */
_Static_assert(NTRK==4u && HP_ROWS==32u && sizeof(hp_rm_cell_t)==18u,
               "RETURN context must fit existing cell padding");
static uint8_t hp_rm_current_context(void)
{
 return hp_rm.scope==HP_RM_ALL?132u:hp_rm.scope==HP_RM_TRACK?
  (uint8_t)(128u+song.sel):(uint8_t)(song.sel*HP_ROWS+hp.track[song.sel].active);
}
static int hp_rm_context_has(uint8_t context)
{
 for(uint32_t i=0;i<hp_rm.used;i++)if(hp_rm.cell[i].context==context)return 1;
 return 0;
}
static void hp_rm_group_context(uint8_t context)
{
 uint32_t n=0;
 for(uint32_t i=0;i<hp_rm.used;i++)if(hp_rm.cell[i].context==context){
  hp_rm_cell_t tmp=hp_rm.cell[n];hp_rm.cell[n]=hp_rm.cell[i];hp_rm.cell[i]=tmp;n++;
 }
 hp_rm.count=n;hp_rm.context=context;
 hp_rm.origin_track=(uint8_t)song.sel;hp_rm.origin_row=hp.track[song.sel].active;
}
static void hp_rm_select_context(uint8_t context)
{
 if(hp_rm.context==context)return;
 hp_rm_group_context(context);
 hp_rm.phase=0;hp_rm.state=hp_rm.count?HP_RM_DONE:HP_RM_IDLE;
}
static void hp_rm_drop_context(void)
{
 if(hp_rm.count){
  hp_rm.used=(uint16_t)(hp_rm.used-hp_rm.count);
  /* Destination precedes source; copy whole cells forward. The target
   * runtime provides memcpy but no memmove. */
  for(uint32_t i=0;i<hp_rm.used;i++)hp_rm.cell[i]=hp_rm.cell[i+hp_rm.count];
 }
 hp_rm.count=0;hp_rm.phase=0;hp_rm.state=HP_RM_IDLE;
}
static int hp_rm_add(hp_ref_t r,int random_only)
{
 if(!r.kind)return 1;
 if(r.track==255u)r.track=(uint8_t)song.sel; /* pin owner at capture, never follow later selection */
 const param_desc_t*d=hp_desc(r);
 if(!hp_rm_safe(r,d) || (random_only&&!hp_rm_random_allowed(r,d)))return 1;
 for(uint32_t i=0;i<hp_rm.used;i++)
  if(hp_rm.cell[i].context==hp_rm.context && hp_rm_same(hp_rm.cell[i].ref,r))return 1;
 if(hp_rm.used==HP_RM_MAX)return 0; /* preserve all other RETURN origins */
 int32_t value=hp_get(r);
 if(value<d->min||value>d->max)return 1;
 hp_rm_cell_t*c=&hp_rm.cell[hp_rm.used++];hp_rm.count++;
 c->ref=r;c->original=c->from=c->to=c->last=(int16_t)value;c->released=0u;
 c->context=hp_rm.context;
 return 1;
}
static int hp_rm_capture(int random_only)
{
 hp_rm.count=0;
 uint16_t old_used=hp_rm.used;
 hp_rm.context=hp_rm_current_context();
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
 hp_rm_group_context(hp_rm.context);
 if(!hp_rm.count)ui_message("RANDOM: NO SAFE CONTROLS");
 return hp_rm.count!=0u;
 full:hp_rm.used=old_used;hp_rm.count=0;
 ui_message(old_used?"RETURN MEMORY FULL":"RANDOM: TOO MANY TARGETS");return 0;
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
static void hp_rm_snapshot_say(uint32_t slot);
static void hp_rm_snapshot_owner(uint32_t slot,char*b)   /* who a slot belongs to: "T2" (a track), "ALL" or "-" (empty) */
{
 const hp_rm_snap_t*s=&hp_rm_snap[slot%HP_RM_SNAP_SLOTS];
 if(!s->count){b[0]='-';b[1]=0;}
 else if(s->scope==HP_RM_ALL){b[0]='A';b[1]=b[2]='L';b[3]=0;}
 else{b[0]='T';b[1]=(char)('1'+(s->origin_track&3u));b[2]=0;}
}
_Static_assert(P_COUNT<=512u && L2_FIRST+L2_N<=512u && DX_END<=512u && FP_SIZE<=512u && NENGINES<31u && NTRK<=4u,
               "Snapshot target packing must cover every parameter and engine");
static int hp_rm_snapshot_packable(hp_ref_t r)
{
 return r.id<512u && r.kind<8u && r.track<NTRK &&
        (r.engine==255u || r.engine<31u) && r.aux<16u;
}
static void hp_rm_snapshot_write_ref(hp_rm_snap_t *s,uint32_t i,hp_ref_t r)
{
 uint32_t engine=r.engine==255u?31u:r.engine;
 uint32_t v=r.id|((uint32_t)r.kind<<9u)|((uint32_t)r.track<<12u)|
            (engine<<14u)|((uint32_t)r.aux<<19u);
 for(uint32_t b=0;b<3u;b++)s->refs[i][b]=(uint8_t)(v>>(8u*b));
}
static hp_ref_t hp_rm_snapshot_ref(const hp_rm_snap_t *s,uint32_t i)
{
 uint32_t v=s->refs[i][0]|((uint32_t)s->refs[i][1]<<8u)|((uint32_t)s->refs[i][2]<<16u);
 hp_ref_t r={0};r.page=0xFFFFu;r.id=v&511u;r.kind=(v>>9u)&7u;
 r.track=(v>>12u)&3u;r.engine=(v>>14u)&31u;r.aux=(v>>19u)&15u;
 if(r.engine==31u)r.engine=255u;
 return r;
}
#if HP_JVN
static void hp_j_captured(void);
static uint8_t hp_j_arrived;     /* set by hp_rm_poll when a recall really reached its snapshot */
#endif
static int hp_rm_snapshot_store(uint32_t slot)
{
 if(slot>=HP_RM_SNAP_SLOTS)return 0;
 char was[4];hp_rm_snapshot_owner(slot,was);uint8_t had=hp_rm_snap[slot].count!=0u;      /* what the slot held before */
 hp_rm_reset(); /* preserve current live sound; discard obsolete morph */
 if(!hp_rm_capture(0)){
  ui_message("SNAP: NO SAFE HOME TARGETS");ui.force=1;return 0;
 }
 hp_rm_snap_t *snap=&hp_rm_snap[slot];
 for(uint32_t i=0;i<hp_rm.count;i++)if(!hp_rm_snapshot_packable(hp_rm.cell[i].ref)){
  hp_rm_reset();ui_message("SNAP: TARGET TOO LARGE");ui.force=1;return 0;
 }
 for(uint32_t i=0;i<hp_rm.count;i++){
  hp_rm_snapshot_write_ref(snap,i,hp_rm.cell[i].ref);
  snap->values[i]=hp_rm.cell[i].original;
 }
 memset(snap->refs+hp_rm.count,0,(HP_RM_MAX-hp_rm.count)*3u);
 memset(snap->values+hp_rm.count,0,(HP_RM_MAX-hp_rm.count)*sizeof snap->values[0]);
 snap->pad=0;hp_dirty=1;
 snap->scope=hp_rm.scope;
 snap->origin_track=hp_rm.origin_track;
 snap->origin_row=hp_rm.origin_row;
 snap->count=(uint16_t)hp_rm.count; /* publish count only after all values */
 hp_rm_reset();
#if HP_JVN
 hp_j_captured(); /* saving cancels the moving leg: a running journey ends here, PLAY continues from the selected slot */
#endif
 char message[32];
 if(had)perf_format(message,sizeof message,"S%u SAVED, WAS %s",slot+1u,was);      /* a filled slot is replaced: say whose it was */
 else perf_format(message,sizeof message,"SNAP S%u SAVED",slot+1u);
 ui_message(message);ui.force=1;
 return 1;
}
static void hp_rm_snapshot_delete(uint32_t slot)      /* RANDOM SETUP, A4 hold: empty the selected slot */
{
 if(slot>=HP_RM_SNAP_SLOTS)return;
 if(!hp_rm_snap[slot].count){hp_rm_snapshot_say(slot);return;}
 hp_rm_reset();
 memset(&hp_rm_snap[slot],0,sizeof hp_rm_snap[slot]);
 hp_dirty=1;
#if HP_JVN
 hp_j_captured();                                     /* the set of slots changed: a running journey ends here */
#endif
 char message[32];perf_format(message,sizeof message,"SNAP S%u DELETED",slot+1u);
 ui_message(message);ui.force=1;
}
/* Recall is all-or-nothing. Ref identities are pinned to captured tracks;
 * HOME layout or selection may change, but engine/type-invalid refs block the
 * entire morph without partially moving any control.
 * Morph starts from the ACTUAL live values, including manual/frozen edits.
 */
static void hp_rm_snapshot_say(uint32_t slot)       /* "SNAP S2 IS T1" or "SNAP S2 EMPTY" */
{
 char o[4],m[32];hp_rm_snapshot_owner(slot,o);
 if(hp_rm_snap[slot].count)perf_format(m,sizeof m,"SNAP S%u IS %s",slot+1u,o);else perf_format(m,sizeof m,"SNAP S%u EMPTY",slot+1u);
 ui_message(m);ui.force=1;
}
static uint8_t hp_rm_skip;   /* bit t: a journey leg leaves track t's controls alone (set only around the leg's recall) */
static int hp_rm_skipped(hp_ref_t r)
{
 return hp_rm_skip&&(r.kind==HP_TRACK||r.kind==HP_LFO2||r.kind==HP_DX||r.kind==HP_FM6)&&(hp_rm_skip>>(r.track&3u)&1u);
}
static int hp_rm_snapshot_recall(uint32_t slot,uint32_t now)
{
 if(slot>=HP_RM_SNAP_SLOTS || !hp_rm_snap[slot].count)return 0;
 const hp_rm_snap_t*snap=&hp_rm_snap[slot];
 if(snap->count>HP_RM_MAX){ui_message("SNAP: INVALID SIZE");return 0;}
 for(uint32_t i=0;i<snap->count;i++){
  hp_ref_t r=hp_rm_snapshot_ref(snap,i);
  if(hp_rm_skipped(r))continue;
  const param_desc_t*d=hp_desc(r);
  if(!hp_rm_safe(r,d) || snap->values[i]<d->min || snap->values[i]>d->max){
   ui_message("SNAP: TARGET CHANGED");ui.force=1;return 0;
  }
 }
 hp_rm_reset();
 uint32_t n=0;
 for(uint32_t i=0;i<snap->count;i++){
  hp_ref_t r=hp_rm_snapshot_ref(snap,i);
  if(hp_rm_skipped(r))continue;
  hp_rm_cell_t*dst=&hp_rm.cell[n++];
  int32_t live=hp_get(r);
  dst->ref=r;
  dst->context=255u;
  dst->original=dst->from=dst->last=(int16_t)live;
  dst->to=snap->values[i];
 }
 if(!n){ui_message("ALL TRACKS SKIPPED");ui.force=1;return 0;}
 hp_rm.count=n;
 hp_rm.used=n;hp_rm.context=255u;
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
 if(hp_rm.state==HP_RM_RECALL ||
    (hp_rm.state==HP_RM_FROZEN&&hp_rm.frozen_from==HP_RM_RECALL))
  hp_rm_reset();
 hp_rm_select_context(hp_rm_current_context());
 if(hp_rm.state==HP_RM_IDLE){
  hp_rm_drop_context();
  if(!hp_rm_capture(1))return;
 }
 if(!hp_rm_valid()){hp_rm_drop_context();ui_message("RANDOM: TARGET CHANGED");return;}
 for(uint32_t i=0;i<hp_rm.count;i++){
  hp_rm_cell_t*c=&hp_rm.cell[i];
  const param_desc_t*d=hp_desc(c->ref);
  int32_t cur=hp_get(c->ref);
  c->from=(int16_t)cur;c->last=(int16_t)cur;
  /* A new explicit RANDOM starts a new trajectory and may reclaim a
   * previously hand-edited control. RETURN/FREEZE never reclaim it. */
  c->released=0u;
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
 uint8_t context=hp_rm_current_context();
 if(!hp_rm_context_has(context)){
  ui_message("RETURN: NO ORIGIN HERE");ui.force=1;return;
 }
 hp_rm_select_context(context);
 if(hp_rm.state!=HP_RM_FROZEN&&hp_rm.state!=HP_RM_RANDOM&&hp_rm.state!=HP_RM_DONE)return;
 if(!hp_rm_valid()){hp_rm_drop_context();ui_message("RANDOM: TARGET CHANGED");return;}
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
  if(hp_rm.context!=hp_rm_current_context()){hp_rm_return(now);return;}
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
#if HP_JVN
  if(recalled)hp_j_arrived=1;
#endif
  if(returned || recalled)hp_rm_drop_context();
  ui_message(recalled?"SNAP REACHED":returned?"RANDOM RETURNED":"RANDOM DESTINATION");
 }
 ui.force=1;
}
#if HP_JVN
/* MORPH JOURNEY (pilot, opt-in, volatile): chains the snapshot recalls S1..S4 into one long movement. Nothing here is
 * stored in a project; it only calls hp_rm_snapshot_recall, so every leg starts from the ACTUAL live values and a
 * knob or CC that owns a parameter (hp_rm_disengage) keeps it until the leg ends; the next leg moves it again.
 *
 * Controls are MIDI Learn targets (HP_VFIRST+8 ..): PLAY (button: play / pause), DIR (button: forward / reverse),
 * NEXT (button), WAIT (absolute, 6 bands: 0/1/2/4/8/16 bars of rest after each arrival), and the RANDOM settings
 * BARS 7 / DEPTH 6 / SCOPE 3 / SHAPE 3 bands (absolute).
 * Buttons act on the rising edge through 64, so a release or a repeated high value does not trigger twice.
 * The MIDI side (an interrupt context) only leaves a message in the mailbox; hp_rm and the snapshots are touched by
 * the UI thread alone (hp_journey_poll). Pilot limits: 2..4 populated snapshots of one TRACK scope, one owner track and
 * one pinned target set (checked before start, nothing moves otherwise); it loops until stopped. A RANDOM, RETURN or
 * other morph started by hand ends it; a physical A3 recall during a journey is not followed. */
enum {HJ_PLAY,HJ_DIR,HJ_NEXT,HJ_WAIT,HJ_BARS,HJ_DEPTH,HJ_SCOPE,HJ_SHAPE};
static const uint8_t hp_j_waits[]={0,1,2,4,8,16};
static volatile uint16_t hp_j_pend;      /* ISR -> UI: bit i = target i has news */
static volatile uint8_t hp_j_val[HP_JVN];
static uint8_t hp_j_hi[HP_JVN];          /* ISR side only: the button's level, for the rising edge */
static struct {
 uint8_t on,paused,rev,wait,phase,cur,tgt;      /* phase 0: a leg is moving, 1: resting at cur */
 uint8_t held;                                  /* 1: another morph runs, the journey goes on by itself when it is done; 2: paused by hand while held */
 uint8_t skip;                                  /* bit t: track t is not taken along (saved with the project) */
 uint8_t set[4];                                /* BARS DEPTH SCOPE SHAPE: band+1 waiting for the morph to end, 0 none */
 uint32_t t0,left;
} hp_j;
static void hp_journey_cc(uint32_t id,uint32_t value)
{
 uint32_t i=id-HP_VFIRST-8u;
 if(i>=HP_JVN)return;
 if(i>=HJ_WAIT){hp_j_val[i]=(uint8_t)value;hp_j_pend=(uint16_t)(hp_j_pend|(1u<<i));return;}   /* absolute */
 {uint8_t hi=value>=64u;
  if(hi&&!hp_j_hi[i])hp_j_pend=(uint16_t)(hp_j_pend|(1u<<i));
  hp_j_hi[i]=hi;}
}
static uint32_t hp_j_band(uint32_t v,uint32_t n){uint32_t b=v*n/128u;return b<n?b:n-1u;}
static int hp_j_ready(const char**why)           /* any two snapshots will do: each slot is a scene, a leg moves exactly the controls stored in its target */
{
 uint32_t n=0;
 for(uint32_t s=0;s<HP_RM_SNAP_SLOTS;s++)if(hp_rm_snap[s].count)n++;
 if(n<2u){*why="NEED 2 SNAPS";return 0;}
 return 1;
}
static int hp_j_next(uint32_t from,uint32_t rev)   /* the next populated slot in the direction; -1 none */
{
 for(uint32_t i=1;i<=HP_RM_SNAP_SLOTS;i++){
  uint32_t s=(from+(rev?HP_RM_SNAP_SLOTS-i:i))%HP_RM_SNAP_SLOTS;
  if(hp_rm_snap[s].count)return (int)s;
 }
 return -1;
}
static int hp_j_moving(void){return hp_rm.state==HP_RM_RANDOM||hp_rm.state==HP_RM_RETURN||hp_rm.state==HP_RM_RECALL||hp_rm.state==HP_RM_FROZEN;}
static void hp_j_apply_settings(void)              /* latched at the start of a leg, never while one moves */
{
 if(hp_j_moving())return;
 if(hp_j.set[0]||hp_j.set[1]||hp_j.set[2]||hp_j.set[3])hp_dirty=1;
 if(hp_j.set[0]){hp_rm.bars=hp_rm_times[hp_j.set[0]-1u];hp_j.set[0]=0;}
 if(hp_j.set[1]){hp_rm.depth=hp_rm_depths[hp_j.set[1]-1u];hp_j.set[1]=0;}
 if(hp_j.set[2]){
  uint8_t next=(uint8_t)(hp_j.set[2]-1u);hp_j.set[2]=0;
  if(next!=hp_rm.scope){hp_rm_reset();hp_rm.scope=next;}
 }
 if(hp_j.set[3]){hp_rm.shape=(uint8_t)(hp_j.set[3]-1u);hp_j.set[3]=0;}
}
static void hp_j_stop(const char*msg){hp_j.on=0;hp_j.paused=0;hp_j.held=0;hp_j.phase=0;ui_message(msg);ui.force=1;}
static void hp_j_captured(void){hp_j.on=0;hp_j.paused=0;hp_j.held=0;hp_j.phase=0;}
static void hp_j_say(const char*what,uint32_t a,uint32_t b)
{
 char m[32];perf_format(m,sizeof m,what,a,b);ui_message(m);ui.force=1;
}
static int hp_j_leg(uint32_t to,uint32_t now)     /* a smooth leg from the live values to S(to) */
{
 if(hp_rm.state==HP_RM_RECALL||(hp_rm.state==HP_RM_FROZEN&&hp_rm.frozen_from==HP_RM_RECALL))hp_rm_reset();
 hp_j_apply_settings();
 hp_j_arrived=0;
 hp_rm_skip=hp_j.skip;
 int ok=hp_rm_snapshot_recall(to,now);
 hp_rm_skip=0;
 if(!ok){hp_j_captured();if(!ui.msg_t)ui_message("SNAP CHANGED");ui.force=1;return 0;}
 hp_j.tgt=(uint8_t)to;hp_j.phase=0;hp_j.held=0;
 action_snap_slot=(uint8_t)to;                      /* RANDOM's selected snapshot follows the journey: edit or recapture it there */
 return 1;
}
static void hp_j_advance(uint32_t now)
{
 int n=hp_j_next(hp_j.cur,hp_j.rev);
 if(n<0){hp_j_stop("NO SNAPS");return;}
 if(hp_j_leg((uint32_t)n,now))hp_j_say("JOURNEY S%u>S%u",hp_j.cur+1u,(uint32_t)n+1u);
}
static void hp_j_start(uint32_t now)
{
 const char*why=0;
 if(!hp_j_ready(&why)){ui_message(why);ui.force=1;return;}
 uint32_t first=action_snap_slot%HP_RM_SNAP_SLOTS;
 if(!hp_rm_snap[first].count){int n=hp_j_next(first,hp_j.rev);if(n<0)return;first=(uint32_t)n;}
 hp_j.on=1;hp_j.paused=0;hp_j.cur=(uint8_t)first;
 if(hp_j_leg(first,now))hp_j_say("JOURNEY PLAY >S%u",first+1u,0u);
}
static void hp_j_wait_begin(uint32_t now)
{
 uint32_t bars=hp_j_waits[hp_j.wait<6u?hp_j.wait:0u];
 if(!bars){hp_j_advance(now);return;}
 uint32_t bpm=(uint32_t)clamp(song.g[G_BPM],20,300);
 hp_j.phase=1;hp_j.t0=now;hp_j.left=bars*(240000u/bpm);
}
static void hp_j_set_wait(uint32_t w)              /* 0..5: rest 0/1/2/4/8/16 bars after each arrival */
{
 hp_j.wait=(uint8_t)(w<6u?w:5u);hp_dirty=1;
 hp_j_say("JOURNEY WAIT %u BAR",hp_j_waits[hp_j.wait],0u);
}
static void hp_j_button(uint32_t i,uint32_t now)
{
 if(i==HJ_PLAY){
  if(!hp_j.on){hp_j_start(now);return;}
  if(!hp_j.paused){
   hp_j.paused=1;
   if(hp_j.held)hp_j.held=2u;                           /* held by another morph: A1 turns the automatic go-on off */
   else if(hp_j.phase==0)hp_rm_freeze();
   else{uint32_t e=now-hp_j.t0;hp_j.left=e<hp_j.left?hp_j.left-e:0u;}
  }else{
   hp_j.paused=0;
   if(hp_j.held==2u){                                   /* paused while held: the leg starts again from the live values */
    if(hp_j.phase==0)hp_j_leg(hp_j.tgt,now);
    else{hp_j.held=0;hp_j.t0=now;}
   }else if(hp_j.phase==0)hp_rm_resume(now);else hp_j.t0=now;
  }
  if(hp_j.on)ui_message(hp_j.paused?"JOURNEY PAUSED":"JOURNEY PLAY");
  ui.force=1;
 }else if(i==HJ_DIR){
  hp_j.rev=!hp_j.rev;hp_dirty=1;                                   /* the running leg finishes; the NEXT one goes the new way (NEXT turns at once) */
  if(hp_j.on&&hp_j.phase==0)hp_j_say(hp_j.rev?"REVERSE AFTER S%u":"FORWARD AFTER S%u",hp_j.tgt+1u,0u);
  else ui_message(hp_j.rev?"JOURNEY REVERSE":"JOURNEY FORWARD");
  ui.force=1;
 }else{                                                  /* NEXT */
  if(!hp_j.on||hp_j.paused){ui_message(hp_j.on?"JOURNEY PAUSED":"JOURNEY OFF");ui.force=1;return;}
  hp_j.cur=hp_j.tgt;hp_j_advance(now);
 }
}
static void hp_j_hold(uint32_t now)               /* another morph took the engine: the journey waits and goes on by itself when it is done */
{
 if(hp_j.phase==1u){uint32_t e=now-hp_j.t0;hp_j.left=e<hp_j.left?hp_j.left-e:0u;}
 hp_j.held=1u;ui.force=1;
}
static void hp_journey_poll(uint32_t now)
{
 uint16_t pend;uint8_t val[HP_JVN];
 {uint32_t irq=perf_irq_save();pend=hp_j_pend;hp_j_pend=0;
  for(uint32_t i=0;i<HP_JVN;i++)val[i]=hp_j_val[i];
  perf_irq_restore(irq);}
 if(pend){
  for(uint32_t i=0;i<HP_JVN;i++)if(pend&(1u<<i)){
   if(i<=HJ_NEXT)hp_j_button(i,now);
   else if(i==HJ_WAIT)hp_j_set_wait(hp_j_band(val[i],6u));
   else{
    static const uint8_t counts[4]={7u,6u,3u,3u};
    uint32_t k=i-HJ_BARS;hp_j.set[k]=(uint8_t)(hp_j_band(val[i],counts[k])+1u);
   }
  }
 }
 hp_j_apply_settings();
 if(!hp_j.on)return;
 if(hp_j.held==1u){                                  /* held: wait until the other morph has ended (arrived, or reset) */
  if(hp_j_moving())return;
  hp_j.held=0;
  if(hp_j.phase==0)hp_j_leg(hp_j.tgt,now);
  else hp_j.t0=now;
  return;
 }
 if(hp_j.paused)return;
 if(hp_j.phase==0){
  if(hp_rm.state==HP_RM_RECALL||(hp_rm.state==HP_RM_FROZEN&&hp_rm.frozen_from==HP_RM_RECALL))return;   /* moving, or held by A2 */
  if(hp_rm.state==HP_RM_IDLE){
   if(hp_j_arrived){hp_j_arrived=0;hp_j.cur=hp_j.tgt;hp_j_wait_begin(now);return;}   /* a real arrival */
   hp_j_stop("JOURNEY STOPPED");return;                                            /* reset by something else: not an arrival */
  }
  hp_j_hold(now);                                  /* another morph (RANDOM, RETURN, its freeze or its end): hold and go on afterwards */
 }else{
  if(hp_j_moving()){hp_j_hold(now);return;}
  if(now-hp_j.t0>=hp_j.left)hp_j_advance(now);
 }
}
#endif
