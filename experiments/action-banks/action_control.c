/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental HOME action control. UI thread only; no allocations or audio loop work.
 * The five keys are consumed by seq.c's keyboard_block at note-on.
 */
#if CARAVEL_ACTION_BANKS
static void action_refresh(void)
{
 uint32_t mask=action_enabled && ui.home && !ui.menu && !ui.confirm &&
     !ui.layer && !ui.ly && !ui.lock && !ui.uboot && !name_on() ?
     ACT_KEY_MASK : 0u;
 if(action_capture_mask!=mask){
  /* Existing keys retain their prior note identity across mode changes. */
  action_preheld |= fm1_in.notes & mask;
  action_capture_mask=mask;
 }
 if(!ui.home)action_home_tap_ms=0;
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
  action_home_tap_ms=0;
  action_refresh();
  ui.force=1;
  ui_message(action_enabled?"HOME ACTION ON":"HOME ACTION OFF");
  return 1;
 }
 action_home_tap_ms=fm1_ms?fm1_ms:1u;
 return 0;
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
  int ok=stasis_layer_command(cancel);
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
  if(hp_rm.state==HP_RM_FROZEN)hp_rm_resume(fm1_ms);
  else hp_rm_freeze();
 }
 else if(action==2u)hp_rm_return(fm1_ms);
 else {
  /* A new snapshot is intentionally requested; no sound parameter changes. */
  hp_rm_reset();
  ui_message("RANDOM A RESET: NEXT CAPTURE");
 }
 ui.force=1;
#else
 (void)action;ui_message("RANDOM UNAVAILABLE");
#endif
}
static void action_key(uint32_t key)
{
 if(key==ACT_KEY_FIRST){
  action_bank=(uint8_t)((action_bank+1u)%ACT_BANKS);
  ui.force=1;
  ui_message(action_bank_names[action_bank]);
  return;
 }
 if(key<ACT_KEY_FIRST+1u || key>26u)return;
 uint32_t act=key-ACT_KEY_FIRST-1u;
 if(action_bank==0u)action_random(act);
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
