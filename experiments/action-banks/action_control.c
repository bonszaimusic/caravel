/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental HOME action control. UI thread only; no allocations or audio loop work.
 * The five keys are consumed by seq.c's keyboard_block at note-on.
 */
#if CARAVEL_ACTION_BANKS
/* BANK on piano key 22: short press switches bank on RELEASE, a 500ms
 * hold toggles a latched encoder SETUP without switching bank.
 * Poll the physical key level so the gesture is identical in WASM and hardware.
 * All four ACTION keys stay usable in SETUP. No timer in the audio ISR. */
/* RANDOM A4 uses exactly the same physical hold policy as BANK:
 * tap (release < 500ms) selects the next snapshot slot, hold >=500ms
 * captures the current live values into the selected slot. The ordinary
 * ACTION A4 note-on is swallowed only in RANDOM; other banks are unchanged.
 */
static void action_snapshot_poll(uint32_t mask)
{
#if FELUCCA_RANDOM_MORPH
 const uint32_t bit=1u<<(ACT_KEY_FIRST+4u);
 if((action_bank!=0u&&action_bank!=ACT_BANK_JOURNEY) || !action_enabled || !(mask&bit) || (action_preheld&bit)){
  action_snap_down=0;action_snap_long=0;return;
 }
 if(fm1_in.notes&bit){
  if(!action_snap_down){
   action_snap_down=1;action_snap_long=0;action_snap_started=fm1_ms;
  }
  if(!action_snap_long && fm1_ms-action_snap_started>=ACTION_LONG_MS){
   action_snap_long=1;
#if HP_JVN
   if(ui.ml.on)ml_knob(&trk[song.sel],HP_VFIRST+8u+HJ_SLOT); /* MIDI LEARN: hold A4 picks JOURNEY SLOT, nothing is saved */
   else
#endif
   if(action_bank==0u&&action_setup)hp_rm_snapshot_delete(action_snap_slot);   /* RANDOM SETUP: A4 hold deletes the selected slot */
   else hp_rm_snapshot_store(action_snap_slot);
  }
 }else if(action_snap_down){
  action_snap_down=0;
#if HP_JVN
  if(ui.ml.on&&!action_snap_long){ml_knob(&trk[song.sel],HP_VFIRST+8u+HJ_WAIT);action_snap_long=0;return;} /* tap A4: JOURNEY WAIT */
#endif
  if(!action_snap_long){
#if HP_JVN
   if(action_bank==ACT_BANK_JOURNEY){hp_j_set_wait((hp_j.wait+1u)%6u);action_snap_long=0;return;} /* tap A4: WAIT 0/1/2/4/8/16 bars */
#endif
   action_snap_slot=(uint8_t)((action_snap_slot+1u)%HP_RM_SNAP_SLOTS);
   char message[32];
   char owner[4];hp_rm_snapshot_owner(action_snap_slot,owner);
   if(hp_rm_snapshot_count(action_snap_slot))perf_format(message,sizeof message,"SNAP S%u IS %s",action_snap_slot+1u,owner);
   else perf_format(message,sizeof message,"SNAP S%u EMPTY",action_snap_slot+1u);
   ui_message(message);
   ui.force=1;
  }
  action_snap_long=0;
 }
#else
 (void)mask;
#endif
}
static void action_refresh(void)
{
 uint32_t mask=action_enabled && ui.home && !ui.menu && !ui.confirm &&
     !ui.layer && !ui.ly && !ui.lock && !ui.uboot && !name_on() ?
     ACT_KEY_MASK : 0u;
 if(action_capture_mask!=mask){
  action_preheld |= fm1_in.notes & mask;
  action_capture_mask=mask;
 }
 if(!ui.home)action_home_tap_ms=0;
 action_snapshot_poll(mask);
 uint32_t bank_bit=1u<<ACT_KEY_FIRST;
 if(!(mask&bank_bit) || (action_preheld&bank_bit)){
  action_bank_down=0;action_bank_long=0;
  if(!ui.home || !action_enabled)action_setup=0;
  return;
 }
 if(fm1_in.notes & bank_bit){
  if(!action_bank_down){
   action_bank_down=1;action_bank_long=0;action_bank_started=fm1_ms;
  }
  if(!action_bank_long && fm1_ms-action_bank_started>=ACTION_LONG_MS){
   action_bank_long=1;
#if HP_JVN
   {
#endif
   action_setup=!action_setup;
#if CARAVEL_STASIS_EXPAND
   if(action_setup && action_bank==1u)action_stasis_grain_page=1u;
#endif
   ui.force=1;
   ui_message(action_setup?(
#if CARAVEL_STASIS_EXPAND
      action_bank==1u?"STASIS GRAIN: K1-K4":
#endif
#if HP_JVN
      action_bank==ACT_BANK_JOURNEY?"JOURNEY TRACKS: K1-K4":
#endif
      "ACTION SETUP: K1-K4"):"ACTION PERFORM: K1-K4 HOME");
#if HP_JVN
   }
#endif
  }
 }else if(action_bank_down){
  action_bank_down=0;
  if(!action_bank_long){
   action_bank=(uint8_t)((action_bank+1u)%ACT_BANKS);
   if(action_bank==ACT_BANK_JOURNEY)action_setup=0;
   ui.force=1;
   ui_message(action_bank_names[action_bank]);
  }
  action_bank_long=0;
 }
}
/* Called only on HOME TAP; never delays the existing first HOME tap. */
static int action_home_double_tap(void)
{
 if(!ui.home || ui.menu || ui.confirm || ui.layer || ui.ly){
  action_home_tap_ms=0;
  return 0;
 }
 if(action_home_tap_ms && fm1_ms-action_home_tap_ms<=330u){
  action_enabled=!action_enabled;
  action_setup=0;action_bank_down=0;action_bank_long=0;
  action_snap_down=0;action_snap_long=0;
  action_home_tap_ms=0;
  action_refresh();
  ui.force=1;
  ui_message(action_enabled?"HOME ACTION ON":"HOME ACTION OFF");
  return 1;
 }
 action_home_tap_ms=fm1_ms?fm1_ms:1u;
 return 0;
}

/* ACTION SETUP is a VIEW onto existing project/track parameters. The only
 * new values are the existing volatile Random Morph controls. Never mirror
 * STASIS, PAIR, or LIVE values into another settings struct.
 * For PAIR modulation we edit an existing MOD assignment if present;
 * we NEVER silently overwrite a user's MOD slots. */
static uint32_t action_pair_mod_slot(uint32_t source)
{
 if(!ep.on)return NMSLOT;
 const track_t*t=&trk[ep.a];
 for(uint32_t i=0;i<NMSLOT;i++)
  if(t->p[P_M1SRC+3u*i]==(int32_t)source &&
     t->p[P_M1DST+3u*i]==MD_PAIR)return i;
 return NMSLOT;
}
static int32_t action_setup_value(uint32_t slot)
{
 if(slot>=4u)return -1;
#if HP_JVN
 if(action_bank==ACT_BANK_JOURNEY)return (hp_j.skip>>slot&1u)?0:1;      /* 1: the track is taken along, 0: left alone */
#endif
 if(action_bank==0u){
#if FELUCCA_RANDOM_MORPH
#if HP_JVN
  if(slot==0u&&hp_j.set[0])return hp_rm_times[hp_j.set[0]-1u];   /* a change waiting for the running journey's leg to end */
  if(slot==3u&&hp_j.set[3])return hp_j.set[3]-1u;
  if(slot==2u&&hp_j.set[2])return hp_j.set[2]-1u;
  if(slot==1u&&hp_j.set[1])return hp_rm_depths[hp_j.set[1]-1u];
#endif
  return slot==0u?hp_rm.bars:slot==1u?hp_rm.depth:slot==2u?hp_rm.scope:hp_rm.shape;
#else
  return -1;
#endif
 }
 if(action_bank==1u){
#if CARAVEL_REVERB_MINIMAL
#if CARAVEL_STASIS_EXPAND
  if(action_stasis_grain_page)
   return slot==0u?st_expand.grain:slot==1u?st_expand.scan:
          slot==2u?st_expand.quality:st_expand.reverse;
#endif
  static const uint8_t ids[4]={G_RSIZE,G_DRREV,G_STAMT,G_STTIME};
  return song.g[ids[slot]];
#else
  return -1;
#endif
 }
 if(action_bank==2u){
  if(!ep.on)return -1;
  if(slot==0u)return (ep.base*127+16383)/32767;
  if(slot==3u)return -1; /* no invented fourth Pair option */
  uint32_t m=action_pair_mod_slot(slot==1u?MS_LFO:MS_LFO2);
  return m<NMSLOT?trk[ep.a].p[P_M1AMT+3u*m]:-1;
 }
 return trk[slot].p[P_LEVEL];
}
static void action_setup_caption(uint32_t slot,char *name,char *value)
{
 static const char*const labels[5][4]={
  {"BARS","DEPTH","SCOPE","SHAPE"},
  {"STR","MOTN","AMT","TIME"},
  {"MORPH","LFO1>PAIR","LFO2>PAIR","--"},
  {"LVL T1","LVL T2","LVL T3","LVL T4"},
  {"TRACK 1","TRACK 2","TRACK 3","TRACK 4"}   /* JOURNEY SETUP: which tracks it takes along */
 };
 str_cpy(name,labels[action_bank][slot&3u],16);
#if CARAVEL_STASIS_EXPAND
 if(action_bank==1u && action_stasis_grain_page){
  static const char *const grain_labels[4]={"GRAIN","SCAN","WINDOW","REVERSE"};
  str_cpy(name,grain_labels[slot&3u],16);
 }
#endif
 int32_t v=action_setup_value(slot);
#if CARAVEL_STASIS_EXPAND
 if(action_bank==1u && action_stasis_grain_page){
  if(slot==0u){
   str_cpy(value,v==0?"93ms":v==1?"186ms":"372ms",24);return;
  }
  if(slot==1u){
   perf_format(value,24,"%u%%",(uint32_t)v*100u/127u);return;
  }
  if(slot==2u){
   str_cpy(value,v==0?"HQ 0.74s":v==1?"LONG 1.49s":"DRONE 2.97s",24);return;
  }
  str_cpy(value,v?"ON":"OFF",24);return;
 }
#endif
 if(action_bank==ACT_BANK_JOURNEY){str_cpy(value,v?"ON":"SKIP",24);return;}
 if(action_bank==0u && slot==2u){
  str_cpy(value,v==0?"ROW":v==1?"TRACK":"ALL",24);return;
 }
 if(action_bank==0u && slot==3u){
  str_cpy(value,v==0?"LINEAR":v==1?"SMOOTH":"DRIFT",24);return;
 }
 if(action_bank==2u && slot==3u){
  str_cpy(value,"--",24);return;
 }
 if(action_bank==2u){
  if(!ep.on){str_cpy(value,"NO PAIR",24);return;}
  if((slot==1u||slot==2u)&&action_pair_mod_slot(slot==1u?MS_LFO:MS_LFO2)==NMSLOT){
   str_cpy(value,"MAP IN MOD",24);return;
  }
  /* -64..63 includes -1: never confuse a valid negative amount with NONE. */
 }else if(v<0){str_cpy(value,"UNAVAIL",24);return;}
 if(action_bank==0u && slot==0u)perf_format(value,24,"%u BAR",v);
 else if((action_bank==0u&&slot==1u)||(action_bank==2u&&slot==0u))
  perf_format(value,24,"%u%%",v);
 else if(action_bank==1u&&slot==3u)perf_format(value,24,"%u s",v);
 else if(action_bank==2u)perf_format(value,24,"%d",v);
 else fmt_int(value,v);
}
static void action_setup_turn(uint32_t slot,int32_t steps)
{
 if(slot>=4u||!steps)return;
#if HP_JVN
 if(action_bank==ACT_BANK_JOURNEY){                 /* K1..K4: turn right takes the track along, left leaves it alone (from the next leg) */
  if(ui.ml.on)return;
  uint8_t bit=(uint8_t)(1u<<slot),was=hp_j.skip;
  hp_j.skip=steps>0?(uint8_t)(hp_j.skip&~bit):(uint8_t)(hp_j.skip|bit);
  if(hp_j.skip!=was){hp_dirty=1;ui.force=1;}
  return;
 }
#endif
 if(action_bank==0u){
#if HP_JVN
  if(ui.ml.on){ml_knob(&trk[song.sel],HP_VFIRST+8u+HJ_BARS+slot);return;} /* MIDI LEARN: K1..K4 pick BARS DEPTH SCOPE SHAPE */
#endif
#if FELUCCA_RANDOM_MORPH
  hp_dirty=1;                         /* BARS DEPTH SCOPE SHAPE are saved with the project */
#if HP_JVN
  if(hp_j.on&&slot!=1u){   /* the journey's legs use them (SCOPE resets the morph): wait for the next leg */
   hp_dirty=1;
   if(slot==2u){
    int32_t sc=hp_j.set[2]?(int32_t)hp_j.set[2]-1:(int32_t)hp_rm.scope;
    hp_j.set[2]=(uint8_t)(clamp(sc+steps,0,2)+1);
   }else if(slot==0u){
    int32_t i=0;
    if(hp_j.set[0])i=(int32_t)hp_j.set[0]-1;else while(i<6&&hp_rm_times[i]!=hp_rm.bars)i++;
    hp_j.set[0]=(uint8_t)(clamp(i+steps,0,6)+1);
   }else{
    int32_t sh=hp_j.set[3]?(int32_t)hp_j.set[3]-1:(int32_t)hp_rm.shape;
    hp_j.set[3]=(uint8_t)(clamp(sh+steps,0,2)+1);
   }
   hp_j_apply_settings();ui.force=1;return;
  }
#endif
  if(slot<=1u){
   const uint8_t *values=slot==0u?hp_rm_times:hp_rm_depths;
   uint32_t n=slot==0u?7u:6u;
   uint32_t old=slot==0u?hp_rm.bars:hp_rm.depth;
   int32_t i=0;
   while(i<(int32_t)n-1 && values[i]!=old)i++;
   i=clamp(i+steps,0,(int32_t)n-1);
   if(slot==0u)hp_rm.bars=values[i];else hp_rm.depth=values[i];
  }else if(slot==2u){
   uint8_t next=(uint8_t)clamp((int32_t)hp_rm.scope+steps,0,2);
   if(next!=hp_rm.scope){
    hp_rm_reset(); /* new scope invalidates old captured target list */
    hp_rm.scope=next;
    ui_message("RANDOM: RECAPTURE AFTER SCOPE");
   }
  }else hp_rm.shape=(uint8_t)clamp((int32_t)hp_rm.shape+steps,0,2);
#else
  ui_message("RANDOM UNAVAILABLE");
#endif
 }else if(action_bank==1u){
#if CARAVEL_REVERB_MINIMAL
  if(song.g[G_RTYPE]!=3u){ui_message("FX: SET TYPE STASIS");return;}
#if CARAVEL_STASIS_EXPAND
  if(action_stasis_grain_page){const char *msg=stasis_exp_turn(slot,steps);if(msg)ui_message(msg);ui.force=1;return;}
#endif
  static const uint8_t ids[4]={G_RSIZE,G_DRREV,G_STAMT,G_STTIME};
  uint32_t id=ids[slot];
  song.g[id]=(int16_t)param_turn(&GP[id],song.g[id],steps);
#else
  ui_message("STASIS UNAVAILABLE");
#endif
 }else if(action_bank==2u){
  if(!ep.on){ui_message("PAIR NOT ACTIVE");return;}
  if(slot==0u){
   hp_ref_t r={0};r.kind=HP_PAIR;r.track=ep.a;r.engine=255;r.id=2;
   hp_set(r,param_turn(hp_desc(r),hp_get(r),steps),1);
  }else if(slot<3u){
   uint32_t m=action_pair_mod_slot(slot==1u?MS_LFO:MS_LFO2);
   if(m==NMSLOT){ui_message("SET PAIR LFO IN MOD FIRST");return;}
   trk[ep.a].p[P_M1AMT+3u*m]=(int16_t)clamp(
    trk[ep.a].p[P_M1AMT+3u*m]+steps,-64,63);
  }
 }else{
  trk[slot].p[P_LEVEL]=(int16_t)param_turn(&TP[P_LEVEL],
      trk[slot].p[P_LEVEL],steps);
 }
 ui.force=1;
}
static void action_pair(uint32_t action)
{
 if(!ep.on){ui_message("PAIR NOT ACTIVE");return;}
 hp_ref_t r={0};r.kind=HP_PAIR;r.engine=255;r.track=ep.a;
 r.id=action==3u?3u:2u;
 int32_t val=action==0u?0:action==1u?64:action==2u?127:!ep.motion;
 hp_set(r,val,0);
 ui.force=1;
}
static void action_stasis(uint32_t action)
{
#if CARAVEL_REVERB_MINIMAL
 if(song.g[G_RTYPE]!=3u){ui_message("SET REVERB TO STASIS");return;}
 if(action==0u){
  /* CAPT LIVE / HOLD, same persistent parameter as FX > REVERB. */
  song.g[G_DRLVL]=song.g[G_DRLVL]?0:1;
  ui_message(song.g[G_DRLVL]?"STASIS HOLD":"STASIS LIVE");
 }else if(action==3u){
  stasis_transport_stop();
  ui_message("STASIS RELEASE");
 }else{
  song.g[G_STMODE]=(action==2u)?1:0;
  uint32_t cancel=stasis_layer_can_cancel();
  fm1_irq_off();
  int ok=stasis_layer_command(cancel);
  fm1_irq_on();
  ui_message(!ok?"STASIS: CAPT HOLD FIRST":cancel?"STASIS CANCEL":action==1u?"STASIS ADD":"STASIS BLEND");
 }
 ui.force=1;
#else
 (void)action;ui_message("STASIS UNAVAILABLE");
#endif
}
static void action_random(uint32_t action)
{
#if FELUCCA_RANDOM_MORPH
 if(action==0u)hp_rm_start(fm1_ms);
 else if(action==1u){
  if(hp_rm.context!=255u && hp_rm.context!=hp_rm_current_context())
   ui_message("MORPH: OTHER TRACK/ROW");
  else if(hp_rm.state==HP_RM_FROZEN)hp_rm_resume(fm1_ms);
  else hp_rm_freeze();
 }
 else if(action==2u){
#if HP_JVN
  if(hp_j.on&&hp_rm_snapshot_count(action_snap_slot)){   /* GO during a journey steers it there smoothly and it goes on from that slot */
   hp_j.paused=0;
   if(hp_j_leg(action_snap_slot,fm1_ms)){char m[32];perf_format(m,sizeof m,"JOURNEY >S%u",action_snap_slot+1u);ui_message(m);}
  }else
#endif
  if(hp_rm_snapshot_count(action_snap_slot))
   hp_rm_snapshot_recall(action_snap_slot,fm1_ms);
  else if(hp_rm.used)
   hp_rm_return(fm1_ms);
  else{
   char message[32];perf_format(message,sizeof message,"SNAP S%u EMPTY",action_snap_slot+1u);
   ui_message(message);
  }
 }
 /* action 3 is handled by short/long physical A4 gesture above */
 ui.force=1;
#else
 (void)action;ui_message("RANDOM UNAVAILABLE");
#endif
}
static void action_key(uint32_t key)
{
 /* BANK press/release/long is handled by action_refresh(), not note-on. */
 if(key==ACT_KEY_FIRST)return;
 if(key<ACT_KEY_FIRST+1u || key>26u)return;
 uint32_t act=key-ACT_KEY_FIRST-1u;
 if(action_bank==0u||action_bank==ACT_BANK_JOURNEY){
#if HP_JVN
  if(ui.ml.on){if(act<3u)ml_knob(&trk[song.sel],HP_VFIRST+8u+act);return;} /* MIDI LEARN: A1 PLAY, A2 DIR, A3 NEXT */
#endif
  if(action_bank==0u){if(act!=3u)action_random(act);}
#if HP_JVN
  else if(act<3u)hp_j_button(act,fm1_ms);       /* A4 is the tap / hold gesture (WAIT / save) */
#else
  else ui_message("JOURNEY UNAVAILABLE");
#endif
 }

 else if(action_bank==1u)action_stasis(act);
 else if(action_bank==2u)action_pair(act);
 else{
  trk[act].p[P_MUTE]=!trk[act].p[P_MUTE];
  ui_message(trk[act].p[P_MUTE]?"TRACK MUTED":"TRACK UNMUTED");
  ui.force=1;
 }
}
static void action_dispatch(uint32_t pressed_notes)
{
 action_refresh();
 uint32_t keys=pressed_notes & action_capture_mask & ~action_preheld;
 for(uint32_t k=ACT_KEY_FIRST;k<27u;k++)
  if(keys&(1u<<k))action_key(k);
}
#endif
