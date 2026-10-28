/* SPDX-License-Identifier: GPL-3.0-only */
/* Typed HOME references. UI-only configuration; bounded, allocation-free audio application. */
static hp_ref_t hp_track_ref(uint32_t t,uint32_t id,uint32_t page)
{
 hp_ref_t r={0};r.kind=HP_TRACK;r.track=t;r.page=page;r.engine=255;
 if(id>=P_E0){r.engine=trk[t].eng_req;r.id=id-P_E0;r.aux=1;}else {r.id=id;if(motion_engine_id(id)||(id==P_FTYPE&&trk[t].eng_req==ENGI_SEM))r.engine=trk[t].eng_req;}
 return r;
}
static uint32_t hp_tid(hp_ref_t r) {return r.aux ? P_E0+r.id:r.id;}
static uint32_t hp_owner(hp_ref_t r) {return r.track==255?song.sel:r.track;}
static const param_desc_t HP_MDESC[4]={
 {"MAC 1",F_PCT,0,127,0,0,0},{"MAC 2",F_PCT,0,127,0,0,0},
 {"MAC 3",F_PCT,0,127,0,0,0},{"MAC 4",F_PCT,0,127,0,0,0}};
static const param_desc_t HP_PDESC[4]={
 {"PARTNER",F_INT,1,4,1,0,0},{"PAIR",F_ONOFF,0,1,0,0,0},
 {"MORPH",F_PCT,0,127,64,0,0},{"MOTION",F_ONOFF,0,1,0,0,0}};
static const param_desc_t *hp_vdesc(uint32_t id){return id<HP_VFIRST+4?&HP_MDESC[(id-HP_VFIRST)&3]:&HP_PDESC[(id-HP_VFIRST-4)&3];}
static param_desc_t HP_FMDESC={"OP VAL",F_INT,0,127,0,0,0};
static const param_desc_t *hp_desc(hp_ref_t r)
{
 uint32_t t=hp_owner(r);if(t>=NTRK)return 0;
 if(r.kind==HP_TRACK){uint32_t id=hp_tid(r);return id<P_COUNT&&(r.engine==255||r.engine==trk[t].eng_req)?track_desc(&trk[t],id):0;}
 if(r.kind==HP_GLOBAL)return r.id<G_COUNT?&GP[r.id]:0;
 if(r.kind==HP_LFO2)return lfo2_desc(r.id);
 if(r.kind==HP_DX)return dx_track(&trk[t])?dx_desc(r.id):0;
 if(r.kind==HP_MACRO)return r.id<4?&HP_MDESC[r.id]:0;
 if(r.kind==HP_PAIR)return r.id<4?&HP_PDESC[r.id]:0;
 if(r.kind==HP_FM6){static const char*const names[]={"OP","STAGE","COARSE","FINE","LEVEL","MODE","DETUNE","VEL","RATE","ENV LVL"};HP_FMDESC.label=names[r.aux<10?r.aux:0];HP_FMDESC.max=fm6_max(r.id);return trk[t].eng_req==ENGI_FM6&&r.id<FP_SIZE?&HP_FMDESC:0;}
 return 0;
}
static int32_t hp_get(hp_ref_t r)
{
 uint32_t t=hp_owner(r);
 if(!hp_desc(r))return 0;
 if(r.kind==HP_TRACK)return trk[t].p[hp_tid(r)];
 if(r.kind==HP_GLOBAL)return song.g[r.id];
 if(r.kind==HP_LFO2)return lfo2_get(t,r.id);
 if(r.kind==HP_DX)return dx_get(t,r.id);
 if(r.kind==HP_MACRO)return song.playing?hp_macro_live[t][r.id]:hp.macro[t][r.id].base;
 if(r.kind==HP_FM6)return fm6_patch[t][r.id];
 if(r.kind==HP_PAIR){if(r.id==0)return pair_member(&trk[t])?(t==ep.a?ep.b:ep.a)+1:pair_ui_partner[t]+1;
  if(r.id==1)return pair_member(&trk[t]);if(r.id==2)return pair_member(&trk[t])?(ep.base*127+16383)/32767:0;return pair_member(&trk[t])&&ep.motion;}
 return 0;
}
static void hp_event_put(uint32_t t,uint32_t step,uint32_t control,int32_t value,int lock)
{
 uint32_t irq=perf_irq_save();
 uint32_t i;for(i=0;i<hp.events;i++)if(hp.event[i].track==t&&hp.event[i].step==step&&hp.event[i].control==control)break;
 if(i==HP_EVENTS){perf_irq_restore(irq);ui_message("PERF AUTOMATION FULL");return;}
 hp.event[i]=(hp_event_t){t,step,control,!!lock,value,0};if(i==hp.events)hp.events++;hp_dirty=1;perf_irq_restore(irq);
}
static void hp_special_set(uint32_t t,uint32_t c,int32_t value,int capture)
{
 uint32_t irq=perf_irq_save();
 if(c<4){hp_macro_live[t][c]=value;if(!capture||!song.playing||!rec_on(&trk[t])||t!=song.sel)hp.macro[t][c].base=value;}
 else if(pair_member(&trk[t])){if(c==6)ep.base=value*32767/127;if(c==7)ep.motion=!!value;
  if(!capture||!song.playing||!rec_on(&trk[t])||t!=song.sel){hp.pair_base=ep.base;hp.pair_motion=ep.motion;hp.pair_valid=1;}
 }
 if(capture&&song.playing&&rec_on(&trk[t])&&t==song.sel)
  {uint32_t owner=c>=6&&pair_member(&trk[t])?ep.a:t;hp_event_put(owner,trk[owner].seq_idx%clamp(trk[owner].p[P_SLEN],1,NSTEP),c,value,0);}
 hp_dirty=1;perf_irq_restore(irq);
}
static void hp_set(hp_ref_t r,int32_t value,int capture)
{
 uint32_t t=hp_owner(r);const param_desc_t*d=hp_desc(r);if(!d)return;value=param_fit(d,value);
 if(r.kind==HP_TRACK){trk[t].p[hp_tid(r)]=value;if(capture)motion_capture(&trk[t],hp_tid(r),value);}
 else if(r.kind==HP_GLOBAL)song.g[r.id]=value;
 else if(r.kind==HP_LFO2)lfo2_set(t,r.id,value);
 else if(r.kind==HP_DX)dx_set(t,r.id,value);
 else if(r.kind==HP_MACRO)hp_special_set(t,r.id,value,capture);
 else if(r.kind==HP_FM6){uint8_t p[FP_SIZE+1u];memcpy(p,fm6_patch[t],FP_SIZE);p[r.id]=value;fm6_set_patch(t,p);fm6_own_ok&=~(1u<<t);fm6_slot[t]=FM6_OWN;trk[t].p[P_E7]=FM6_OWN;}
 else if(r.kind==HP_PAIR){
  if(r.id==0){if(ep.on)ui_message("UNPAIR TO CHANGE PARTNER");else {int32_t other=value-1;if(other==(int32_t)t)other+=(other>(int32_t)pair_ui_partner[t]?1:-1);if(other>=0&&other<NTRK)pair_ui_partner[t]=other;}}
  else if(r.id==1){if(value&&!ep.on){if(!pair_enable(t,pair_ui_partner[t]))ui_message(pair_last_reason);}else if(!value&&pair_member(&trk[t]))pair_disable();else if(value&&!pair_member(&trk[t]))ui_message("ONE PAIR: UNPAIR FIRST");}
  else if(pair_member(&trk[t]))hp_special_set(t,4+r.id,value,capture);
  else ui_message("OPEN PAIR / TURN PAIR ON");
 }
 hp_dirty=1;ui.force=1;
}
#if FELUCCA_RANDOM_MORPH
#include "home_random_morph.c"
#endif
static void hp_midi(track_t*t,uint32_t id,uint32_t value)
{
 hp_ref_t r={0};r.kind=id<HP_VFIRST+4?HP_MACRO:HP_PAIR;r.id=(id-HP_VFIRST)&3;r.track=trk_index(t);r.engine=255;
 const param_desc_t*d=hp_desc(r);if(d)hp_set(r,d->min+(value*(d->max-d->min)+63)/127,1);
}
static void hp_touch(uint32_t slot)
{
 const page_t*pg=cur_page();hp_last_valid=0;hp_ref_t r={0};r.track=song.sel;r.engine=255;r.page=ui.page;
 if(pg->graph==GR_PAIR){r.kind=HP_PAIR;r.id=slot;}
 else if(pg->graph==GR_MACRO){r.kind=HP_MACRO;r.id=slot;}
 else if(lfo2_page(pg->graph)){r.kind=HP_LFO2;r.id=lfo2_page_target(pg->graph,slot);}
 else if(dx_page(pg->graph)){r.kind=HP_DX;r.id=dx_page_target(pg->graph,slot);}
 else if(fop_page(pg->graph)){uint32_t f=fop_field(slot);if(f==FO_OP||f==FO_STG)return;r.kind=HP_FM6;r.id=fop_byte(f);r.aux=f;}
 else if(pg->scope==SC_TRK){static const uint8_t ids[]={P_LEVEL,P_PAN,P_REV,P_MUTE};r=hp_track_ref(song.sel,ids[slot],ui.page);}
 else {int16_t *p;const param_desc_t*d=page_desc(pg,slot,&p);
  if(!d||!p||d->max==d->min||pg->scope==SC_STEP||pg->graph==GR_HOMEEDIT||pg->graph==GR_MACROMAP)return;
  if(pg->scope==SC_GLOBAL){uint32_t id=p-song.g;if(id>G_CDEPTH&&id!=G_RTYPE)return;r.kind=HP_GLOBAL;r.id=id;}
  else r=hp_track_ref(song.sel,p-TSEL->p,ui.page);
 }
 if(!hp_desc(r))return;hp_last=r;hp_last_valid=1;hp_last_track=song.sel;
}
static void hp_apply(uint32_t t,uint32_t set)
{
 if(t>=NTRK)return;hp_track_t *h=&hp.track[t];memset(h,0,sizeof *h);h->count=1;
 if(set){for(uint32_t k=0;k<4;k++){h->cell[0][k]=hp_track_ref(t,ENGINES[trk[t].eng_req]->knob[k],0xFFFF);}}
 if(set==2){static const uint8_t p[]={P_LEVEL,P_FILT,P_DLY,P_REV};h->count=2;for(uint32_t k=0;k<4;k++)h->cell[1][k]=hp_track_ref(t,p[k],0xFFFF);}
 hp_dirty=1;ui.force=1;
}
static void hp_init(void)
{
 uint32_t irq=perf_irq_save();
 memset(&hp,0,sizeof hp);memset(hp_macro_live,0,sizeof hp_macro_live);memset(hp_macro_count,0,sizeof hp_macro_count);
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,hp_default);hp_last_valid=0;hp_apply_armed=0;hp_edit_track=hp_backup_track=255;perf_irq_restore(irq);
}
static void hp_backup_take(uint32_t t){hp_edit_backup=hp.track[t];hp_backup_track=t;}
static void hp_apply_template(uint32_t t,uint32_t set){if(t>=NTRK||set>2)return;hp_backup_take(t);hp_apply(t,set);hp_edit_track=255;}
static void hp_add_last(void)
{
 if(!hp_last_valid||hp_last_track!=song.sel||(!ui.home&&hp_last.page!=ui.page)||!hp_desc(hp_last)) {ui_message("TURN SOURCE KNOB FIRST");return;}
 hp_track_t*h=&hp.track[song.sel];
 for(uint32_t row=0;row<h->count;row++)for(uint32_t k=0;k<4;k++){
  hp_ref_t a=h->cell[row][k],b=hp_last;a.page=b.page=0;
  if(!memcmp(&a,&b,sizeof a)){h->active=row;ui_message("ALREADY ON HOME");return;}}
 uint32_t row,k;for(row=0;row<h->count;row++){for(k=0;k<4;k++)if(!h->cell[row][k].kind)goto found;}
 if(h->count==HP_ROWS){ui_message("HOME FULL: 32 ROWS");return;}k=0;
 found:hp_backup_take(song.sel);if(row==h->count)h->count++;h->cell[row][k]=hp_last;h->active=row;hp_dirty=1;
 char b[32];perf_format(b,sizeof b,"ADDED TO HOME %u.%u",row+1,k+1);ui_message(b);ui.force=1;
}
static void hp_row_turn(int32_t steps){hp_track_t*h=&hp.track[song.sel];h->active=clamp((int32_t)h->active+steps,0,h->count-1);hp_dirty=1;ui.force=1;}
static hp_ref_t hp_current(uint32_t k){return hp.track[song.sel].cell[hp.track[song.sel].active][k&3];}
static void hp_turn(uint32_t slot,int32_t steps,int momentary)
{
 hp_ref_t r=hp_current(slot);const param_desc_t*d=hp_desc(r);if(!d){ui_message(r.kind?"TARGET NOT AVAILABLE":"EMPTY: GLO+HOME TO ADD");return;}
 hp_last=r;hp_last.page=0xFFFF;hp_last_valid=1;hp_last_track=song.sel;
 if(momentary){if(r.kind!=HP_TRACK&&r.kind!=HP_GLOBAL){ui_message("USE NORMAL TURN");return;}
  int16_t*vp=r.kind==HP_TRACK?&trk[hp_owner(r)].p[hp_tid(r)]:&song.g[r.id];uint32_t i;
  for(i=0;i<mom.n&&mom.vp[i]!=vp;i++);if(i==mom.n){if(i==MOM_N){ui_message("MOMENTARY FULL");return;}mom.vp[i]=vp;mom.v[i]=*vp;mom.n++;}
  *vp=param_turn(d,*vp,steps);ui.force=1;return;}
 hp_set(r,param_turn(d,hp_get(r),accel(EN_K1+slot,steps,d->fmt==F_ENUM?0:d->max-d->min)),1);
 uint32_t t=hp_owner(r);
 if(r.kind==HP_TRACK)ml_knob(&trk[t],hp_tid(r));
 else if(r.kind==HP_LFO2||r.kind==HP_DX)ml_knob(&trk[t],r.id);
 else if(r.kind==HP_MACRO||r.kind==HP_PAIR)ml_knob(&trk[t],HP_VFIRST+(r.kind==HP_PAIR?4:0)+r.id);
}
static uint32_t hp_lock_id(uint32_t k){hp_ref_t r=hp_current(k);uint32_t id=hp_tid(r);return r.kind==HP_TRACK&&hp_owner(r)==song.sel&&hp_desc(r)&&motion_param(id)?id:0xFF;}
static void hp_lock_turn(uint32_t slot,int32_t steps,uint64_t held)
{
 hp_ref_t r=hp_current(slot);const param_desc_t*d=hp_desc(r);uint32_t id=hp_lock_id(slot);
 if(chain_busy()){ui_message("STOP SONG TO EDIT LOCKS");return;}if(!d){ui_message("NO LOCKS FOR THIS CONTROL");return;}
 if(id!=0xFF){motion_undo_take(TSEL,0x100|slot);for(uint32_t i=0;i<NSTEP;i++)if((held>>i)&1){int16_t v;if(!motion_lock_get(TSEL,i,id,&v))v=motion_base_value(TSEL,id);motion_set_lock(TSEL,i,id,param_turn(d,v,steps));}motion_undo_done(TSEL);}
 else if((r.kind==HP_MACRO||(r.kind==HP_PAIR&&r.id>=2))&&hp_owner(r)==song.sel){for(uint32_t i=0;i<NSTEP;i++)if((held>>i)&1)hp_event_put(song.sel,i,(r.kind==HP_PAIR?4:0)+r.id,param_turn(d,hp_get(r),steps),1);}
 else ui_message(hp_owner(r)!=song.sel&&r.kind!=HP_GLOBAL?"SELECT TARGET TRACK FOR LOCK":"NO LOCKS FOR THIS CONTROL");
 ui.plk_rm&=~held;
}
static void hp_motion_step(track_t*t,uint32_t step)
{
 uint32_t tr=trk_index(t);for(uint32_t c=0;c<4;c++){
 int16_t value=hp.macro[tr][c].base;int best=-1;
 for(uint32_t i=0;i<hp.events;i++){hp_event_t*e=&hp.event[i];if(e->track==tr&&e->control==c&&!e->lock&&e->step<=step&&(int)e->step>best){best=e->step;value=e->value;}}
 for(uint32_t i=0;i<hp.events;i++){hp_event_t*e=&hp.event[i];if(e->track==tr&&e->control==c&&e->lock&&e->step==step)value=e->value;}
 hp_macro_live[tr][c]=value;}
 if(ep.on&&tr==ep.a){int32_t value[2]={hp.pair_base*127/32767,hp.pair_motion};int best[2]={-1,-1};uint32_t used=0;
 for(uint32_t i=0;i<hp.events;i++){hp_event_t*e=&hp.event[i];if(e->track!=tr||e->control<6)continue;used=1;uint32_t c=e->control-6;if(!e->lock&&e->step<=step&&(int)e->step>best[c]){best[c]=e->step;value[c]=e->value;}}
 for(uint32_t i=0;i<hp.events;i++){hp_event_t*e=&hp.event[i];if(e->track==tr&&e->control>=6&&e->lock&&e->step==step)value[e->control-6]=e->value;}
 if(used){ep.base=value[0]*32767/127;ep.motion=!!value[1];}}

}
static void hp_motion_stop(void){for(uint32_t t=0;t<NTRK;t++)for(uint32_t c=0;c<4;c++)hp_macro_live[t][c]=hp.macro[t][c].base;if(ep.on&&hp.pair_valid){uint32_t used=0;for(uint32_t i=0;i<hp.events;i++)used|=hp.event[i].control>=6;if(used){ep.base=hp.pair_base;ep.motion=hp.pair_motion;}else{hp.pair_base=ep.base;hp.pair_motion=ep.motion;}}}
static void hp_begin(track_t*t)
{
 uint32_t tr=trk_index(t),count=0,changed=0;int32_t off[HP_MACRO_TARGETS];
 hp_macro_count[tr]=0;
 for(uint32_t m=0;m<4;m++){int32_t value=song.playing?hp_macro_live[tr][m]:hp.macro[tr][m].base;if(!value)continue;
 for(uint32_t j=0;j<4;j++){hp_route_t *route=&hp.macro[tr][m].route[j];hp_ref_t r=route->ref;uint32_t id=hp_tid(r);
 if(r.kind!=HP_TRACK||hp_owner(r)!=tr||!route->amount||id>=P_COUNT)continue;
 const param_desc_t*d=hp_desc(r);if(!d||d->fmt==F_ENUM||d->fmt==F_ONOFF)continue;
 uint32_t at;for(at=0;at<count;at++)if(hp_macro_ids[tr][at]==id)break;
 if(at==count){hp_macro_ids[tr][count++]=id;off[at]=0;}
 off[at]+=value*route->amount*(d->max-d->min)/(127*64);}}
 for(uint32_t at=0;at<count;at++)if(off[at]){uint32_t id=hp_macro_ids[tr][at];const param_desc_t*d=track_desc(t,id);
 hp_macro_keep[tr][changed]=t->p[id];hp_macro_ids[tr][changed++]=id;t->p[id]=clamp(t->p[id]+off[at],d->min,d->max);}
 hp_macro_count[tr]=changed;
}
static void hp_end(track_t*t){uint32_t tr=trk_index(t);for(uint32_t at=0;at<hp_macro_count[tr];at++)t->p[hp_macro_ids[tr][at]]=hp_macro_keep[tr][at];hp_macro_count[tr]=0;}
static void hp_open_graph(uint32_t graph){for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].graph==graph){ui.home=0;ui.page=i;page_entered();ui.force=1;return;}}
static int hp_oct_button(uint32_t b){if(ui.home)return 0;uint32_t g=cur_page()->graph;return g==GR_HOMEEDIT||(g==GR_MACRO&&b==B_OCTUP)||(g==GR_MACROMAP&&b==B_OCTDN);}
static void hp_editor_sync(void){if(hp_edit_track!=song.sel){hp_edit_track=song.sel;hp_edit_row=hp.track[song.sel].active;hp_edit_col=hp_edit_action=0;}hp_edit_row=clamp(hp_edit_row,0,hp.track[song.sel].count-1);}
static void hp_open_editor(void){hp_edit_track=255;hp_editor_sync();hp_open_graph(GR_HOMEEDIT);}
static void hp_source(void)
{
 hp_ref_t r=hp.track[song.sel].cell[hp_edit_row][hp_edit_col];if(!hp_desc(r)){ui_message("SOURCE NOT AVAILABLE");return;}
 track_select(hp_owner(r));
 if(r.kind==HP_TRACK&&hp_tid(r)>=P_M1SRC&&hp_tid(r)<=P_M4AMT)mod_ui_slot=(hp_tid(r)-P_M1SRC)/3;
 if(r.kind==HP_PAIR){hp_open_graph(GR_PAIR);return;}if(r.kind==HP_MACRO){hp_open_graph(GR_MACRO);return;}
 if(r.kind==HP_FM6){fop_op=5-r.id/FP_OP;if(r.aux==FO_RATE||r.aux==FO_ELVL)fop_stg=(r.id%FP_OP)-FO_OFF[r.aux];}
 if(r.kind==HP_DX&&r.id>=DX_SOUND_FIRST){dx_ui_lane=(r.id-DX_SOUND_FIRST)/16;uint32_t field=(r.id-DX_SOUND_FIRST)%16;dx_ui_side=field>=5&&field<10;}
 if(r.page<NPAGES&&page_visible(r.page)){ui.home=0;ui.page=r.page;page_entered();ui.force=1;return;}
 uint32_t id=hp_tid(r);for(uint32_t i=0;i<NPAGES;i++)for(uint32_t k=0;k<4;k++)if(PAGES[i].id[k]==id&&page_visible(i)&&((r.kind==HP_GLOBAL)==(PAGES[i].scope==SC_GLOBAL))){ui.home=0;ui.page=i;page_entered();ui.force=1;return;}
 ui_message("OPEN SOURCE VIA ITS BUTTON");
}
static const char*const HP_ACTION[]={"SOURCE","REMOVE","CELL LEFT","CELL RIGHT","ROW UP","ROW DOWN","ADD ROW","DELETE ROW","UNDO"};
static void hp_editor_apply(void)
{
 hp_editor_sync();
 hp_track_t*h=&hp.track[song.sel];uint32_t r=hp_edit_row,k=hp_edit_col;if(hp_edit_action==0){hp_source();return;}
 if(hp_edit_action==8){if(hp_backup_track==song.sel){hp_track_t v=*h;*h=hp_edit_backup;hp_edit_backup=v;hp_edit_row=h->active;hp_dirty=1;ui.force=1;ui_message("HOME UNDO / REDO");}else ui_message("NO HOME EDIT TO UNDO");return;}
 if(hp_edit_action==6&&h->count==HP_ROWS){ui_message("HOME FULL: 32 ROWS");return;}
 if(hp_edit_action==1&&!h->cell[r][k].kind){ui_message("CELL ALREADY EMPTY");return;}
 if((hp_edit_action==2&&r*4+k==0)||(hp_edit_action==3&&r*4+k+1==h->count*4)){ui_message("NO CELL IN THIS DIRECTION");return;}
 if((hp_edit_action==4&&r==0)||(hp_edit_action==5&&r+1==h->count)){ui_message("NO ROW IN THIS DIRECTION");return;}
 hp_backup_take(song.sel);
 if(hp_edit_action==1)memset(&h->cell[r][k],0,sizeof(hp_ref_t));
 if(hp_edit_action==2||hp_edit_action==3){uint32_t flat=r*4+k,to=clamp((int32_t)flat+(hp_edit_action==2?-1:1),0,h->count*4-1);hp_ref_t v=h->cell[to/4][to%4];h->cell[to/4][to%4]=h->cell[r][k];h->cell[r][k]=v;hp_edit_row=to/4;hp_edit_col=to%4;}
 if(hp_edit_action==4||hp_edit_action==5){uint32_t to=clamp((int32_t)r+(hp_edit_action==4?-1:1),0,h->count-1);hp_ref_t v[4];memcpy(v,h->cell[to],sizeof v);memcpy(h->cell[to],h->cell[r],sizeof v);memcpy(h->cell[r],v,sizeof v);hp_edit_row=to;}
 if(hp_edit_action==6){if(h->count<HP_ROWS){hp_edit_row=h->count++;memset(h->cell[hp_edit_row],0,sizeof h->cell[0]);}else ui_message("HOME FULL");}
 if(hp_edit_action==7){for(uint32_t j=r;j+1<h->count;j++)memcpy(h->cell[j],h->cell[j+1],sizeof h->cell[0]);memset(h->cell[h->count-1],0,sizeof h->cell[0]);if(h->count>1)h->count--;hp_edit_row=clamp(r,0,h->count-1);}
 h->active=hp_edit_row;hp_dirty=1;ui.force=1;
 ui_message("HOME EDITED / UNDO AVAILABLE");
}
static void hp_page_knob(uint32_t slot,int32_t steps)
{
 uint32_t graph=cur_page()->graph;
 if(graph==GR_MACRO){hp_macro_sel=slot;hp_ref_t r={.kind=HP_MACRO,.track=song.sel,.engine=255,.id=slot,.page=ui.page};hp_last=r;hp_last_track=song.sel;hp_last_valid=1;hp_set(r,clamp(hp_get(r)+steps,0,127),1);ml_knob(TSEL,HP_VFIRST+slot);}
 else if(graph==GR_MACROMAP){uint32_t irq=perf_irq_save();hp_macro_t*m=&hp.macro[song.sel][hp_macro_sel];hp_route_t*rt=&m->route[hp_route_sel];
  if(slot==0)hp_macro_sel=clamp((int32_t)hp_macro_sel+steps,0,3);
  if(slot==1)hp_route_sel=clamp((int32_t)hp_route_sel+steps,0,3);
  if(slot==2){int32_t id=rt->ref.kind==HP_TRACK?(int32_t)hp_tid(rt->ref):-1,dir=steps>0?1:-1;uint32_t n=steps>0?steps:-steps;while(n--){int32_t candidate=id;while(1){candidate+=dir;if(candidate<0){id=-1;break;}if(candidate>=P_COUNT)break;const param_desc_t*d=track_desc(TSEL,candidate);if(d->fmt!=F_ENUM&&d->fmt!=F_ONOFF&&motion_param(candidate)){id=candidate;break;}}}if(id<0)memset(rt,0,sizeof*rt);else rt->ref=hp_track_ref(song.sel,id,0xFFFF);}
  if(slot==3)rt->amount=clamp(rt->amount+steps,-64,63);hp_dirty=1;perf_irq_restore(irq);
 }else if(graph==GR_HOMEEDIT){hp_editor_sync();if(slot==0)hp_edit_row=clamp((int32_t)hp_edit_row+steps,0,hp.track[song.sel].count-1);if(slot==1)hp_edit_col=clamp((int32_t)hp_edit_col+steps,0,3);if(slot==2)hp_edit_action=clamp((int32_t)hp_edit_action+steps,0,8);if(slot==3){hp_ref_t*r=&hp.track[song.sel].cell[hp_edit_row][hp_edit_col];if(r->kind&&r->kind!=HP_GLOBAL){int32_t owner=r->track==255?4:r->track;owner=clamp(owner+steps,0,4);uint8_t next=owner==4?255:owner;if(r->track!=next){hp_backup_take(song.sel);r->track=next;hp_dirty=1;}}else ui_message(r->kind?"GLOBAL: ALL TRACKS":"EMPTY CELL: ADD VIA GLO+HOME");}}
 ui.force=1;
}
static void hp_page_cards(void)
{
 char b[16];const char*u;uint32_t g=cur_page()->graph;
 if(g==GR_MACRO){for(uint32_t k=0;k<4;k++){hp_ref_t r={.kind=HP_MACRO,.track=song.sel,.id=k};param_format(&HP_MDESC[k],hp_get(r),b,&u);draw_column(k,HP_MDESC[k].label,b,u,VAL(k),hp_get(r)*1000/127,ICON_NONE);}return;}
 if(g==GR_MACROMAP){hp_route_t*r=&hp.macro[song.sel][hp_macro_sel].route[hp_route_sel];fmt_int(b,hp_macro_sel+1);draw_column(0,"MACRO",b,"",VAL(0),-1,ICON_NONE);fmt_int(b,hp_route_sel+1);draw_column(1,"SLOT",b,"",VAL(1),-1,ICON_NONE);const param_desc_t*d=hp_desc(r->ref);draw_column(2,"PARAM",d?d->label:r->ref.kind?"UNAVAIL":"NONE","",VAL(2),-1,ICON_NONE);fmt_int(b,r->amount);draw_column(3,"AMOUNT",b,"",VAL(3),-1,ICON_NONE);return;}
 hp_editor_sync();fmt_int(b,hp_edit_row+1);draw_column(0,"ROW",b,"",VAL(0),-1,ICON_NONE);fmt_int(b,hp_edit_col+1);draw_column(1,"CELL",b,"",VAL(1),-1,ICON_NONE);draw_column(2,"ACTION",HP_ACTION[hp_edit_action],"",VAL(2),-1,ICON_NONE);hp_ref_t r=hp.track[song.sel].cell[hp_edit_row][hp_edit_col];if(!r.kind)str_cpy(b,"--",sizeof b);else if(r.kind==HP_GLOBAL)str_cpy(b,"GLOBAL",sizeof b);else if(r.track==255)str_cpy(b,"SEL",sizeof b);else perf_format(b,sizeof b,"T%u",r.track+1);draw_column(3,"TRACK",b,"",VAL(3),-1,ICON_NONE);
}
static uint32_t hp_routes_active(uint32_t t,uint32_t m)
{uint32_t n=0;for(uint32_t j=0;j<4;j++){hp_route_t*r=&hp.macro[t][m].route[j];n+=r->ref.kind==HP_TRACK&&hp_owner(r->ref)==t&&hp_desc(r->ref)&&r->amount;}return n;}
static void hp_page_graph(void)
{
 char b[64];uint32_t g=cur_page()->graph;cv_begin(240,H_GRAPH,T_BG);cv_rrect(3,0,234,H_GRAPH,5,T_SURF,T_BG);cv_bg=T_SURF;cv_oy=0;
 if(g==GR_MACRO){
  perf_format(b,sizeof b,"T%u MACROS / OCT+: MAP MAC %u",song.sel+1,hp_macro_sel+1);cv_text(8,3,&AF_S,b,T_ACCENT);
  for(uint32_t m=0;m<4;m++){uint32_t n=hp_routes_active(song.sel,m);perf_format(b,sizeof b,"MAC %u: %u/4 SLOTS%s",m+1,n,n?"":" / NO EFFECT");cv_text(8,23+m*17,&AF_S,b,n?T_TEXT:T_MID);}
  cv_text(8,94,&AF_S,"MAP: PARAM + AMOUNT / 0 NEUTRAL",T_MID);cv_text(8,108,&AF_S,"TURN MAC / GLO+HOME: ADD",T_MID);
 }else if(g==GR_MACROMAP){
  perf_format(b,sizeof b,"MAC %u / ALL SLOTS ON TRACK %u",hp_macro_sel+1,song.sel+1);cv_text(8,3,&AF_S,b,T_ACCENT);
  for(uint32_t j=0;j<4;j++){hp_route_t*r=&hp.macro[song.sel][hp_macro_sel].route[j];const param_desc_t*d=hp_desc(r->ref);perf_format(b,sizeof b,"%s%u  %s  %+d",j==hp_route_sel?">":" ",j+1,d?d->label:r->ref.kind?"UNAVAIL":"EMPTY",r->amount);cv_text(8,23+j*17,&AF_S,b,j==hp_route_sel?T_ACCENT:T_MID);}
  cv_text(8,94,&AF_S,"K3: NONE REMOVES / K4: AMOUNT",T_MID);cv_text(8,108,&AF_S,"OCT-: PLAY MACROS / 0 NEUTRAL",T_MID);
 }else {
  hp_editor_sync();hp_ref_t r=hp.track[song.sel].cell[hp_edit_row][hp_edit_col];const param_desc_t*d=hp_desc(r);
  perf_format(b,sizeof b,"HOME T%u / ROW %u / CELL %u",song.sel+1,hp_edit_row+1,hp_edit_col+1);cv_text(8,3,&AF_S,b,T_ACCENT);
  perf_format(b,sizeof b,"PARAM: %s",d?d->label:r.kind?"UNAVAIL (ENGINE CHANGED)":"EMPTY");cv_text(8,25,&AF_S,b,T_TEXT);
  if(d){const char*u;char v[16];param_format(d,hp_get(r),v,&u);perf_format(b,sizeof b,"VALUE: %s %s",v,u?u:"");cv_text(8,43,&AF_S,b,T_TEXT);}
  perf_format(b,sizeof b,"OCT+: %s",HP_ACTION[hp_edit_action]);cv_text(8,65,&AF_S,b,T_ACCENT);
  cv_text(8,84,&AF_S,!r.kind?"EMPTY: ADD FROM A SOURCE PAGE":r.kind==HP_GLOBAL?"GLOBAL: SHARED BY ALL TRACKS":r.track==255?"SEL FOLLOWS SELECTED TRACK":"K4: TRACK CHANGES IMMEDIATELY",T_MID);
  cv_text(8,108,&AF_S,"UNDO: LAST HOME EDIT / OCT-: HOME",T_MID);
 }
 cv_blit(0,Y_GRAPH);
}
/* HOME: one expanded live row and a compact preview; physical order is retained. */
static const char*hp_row_name(const hp_track_t*h,uint32_t row)
{
 uint32_t bits=0;
 for(uint32_t i=0;i<4;i++){
  hp_ref_t v=h->cell[row][i];if(!v.kind)continue;
  uint32_t id=hp_tid(v);
  bits|=v.kind==HP_PAIR?4u:v.kind==HP_MACRO?8u:
        v.kind==HP_GLOBAL||(v.kind==HP_TRACK&&(param_send(id)||param_insert(id)))?2u:1u;
 }
 return bits==4?"PAIR":bits==8?"MACRO":bits==2?"FX":bits==1?"SOUND":bits?"MIX":"EMPTY";
}
static void hp_perf_cell(hp_ref_t r,uint32_t k,int y,int active)
{
 int x=3+(int)k*59,h=active?72:28,hot=active&&ui.hot_t&&ui.hot_col==k;
#if CARAVEL_ACTION_BANKS
 if(active&&action_enabled)h=52;
#endif
 uint16_t bg=active?T_RAISE:T_SURF,edge=hot?T_TEXT:active?T_ACCENT:T_LINE;
 cv_rrect(x,y,56,h,4,edge,T_BG);cv_rrect(x+1,y+1,54,h-2,3,bg,edge);cv_bg=bg;
 const param_desc_t*d=r.kind?hp_desc(r):0;
 if(!d){cv_text_in(x,y+(active?23:6),56,active?&AF_M:&AF_S,r.kind?"UNAVAIL":"--",T_DIM,bg);return;}
 int foreign=r.kind==HP_GLOBAL||(r.track!=255&&hp_owner(r)!=song.sel);
 char val[24],line[32];const char*unit;int32_t value=hp_get(r);param_format(d,value,val,&unit);
 if(active){
  int label_y=2;
  if(foreign){perf_format(line,sizeof line,r.kind==HP_GLOBAL?"GLOBAL":"T%u",hp_owner(r)+1);cv_text(x+4,y+1,&AF_S,line,T_ACCENT);label_y=15;}
  const aafont_t*label=!foreign&&text_w(&AF_M,d->label)<=48?&AF_M:&AF_S;
  cv_free_text(x+4,y+label_y,label,d->label,T_TEXT,bg,48);
  const aafont_t*f=!foreign&&large_face_has(val)&&text_w(&AF_L,val)<=48?&AF_L:text_w(&AF_M,val)<=48?&AF_M:&AF_S;
  cv_free_text(x+4,y+(foreign?31:23),f,val,T_TEXT,bg,48);
  #if CARAVEL_ACTION_BANKS
  if(action_enabled){
   if(unit)cv_text_r(x+52,y+35,&AF_S,unit,T_MID,bg);
  }else
#endif
  if(unit)cv_text_r(x+52,y+53,&AF_S,unit,T_MID,bg);
  if(d->max>d->min&&d->fmt!=F_ENUM){
   int32_t pos=clamp((value-d->min)*48/(d->max-d->min),0,48);
   int32_t zero=d->min<0&&d->max>0?(-d->min)*48/(d->max-d->min):0;
   #if CARAVEL_ACTION_BANKS
   int gy=action_enabled?48:68;
#else
   int gy=68;
#endif
   cv_rect(x+4,y+gy,48,2,T_LINE);
   if(pos!=zero)cv_rect(x+4+(pos<zero?pos:zero),y+gy,pos<zero?zero-pos:pos-zero,2,T_ACCENT);
   if(zero)cv_rect(x+4+zero,y+gy-1,1,4,T_MID);
  }
 }else{
  if(foreign){if(r.kind==HP_GLOBAL)perf_format(line,sizeof line,"G:%s",d->label);else perf_format(line,sizeof line,"T%u:%s",hp_owner(r)+1,d->label);}
  else str_cpy(line,d->label,sizeof line);
  cv_free_text(x+4,y,&AF_S,line,T_MID,bg,48);
  perf_format(line,sizeof line,"%s%s",val,unit?unit:"");cv_free_text(x+4,y+13,&AF_S,line,T_TEXT,bg,48);
 }
}
static void hp_draw_home(void)
{
 hp_track_t*h=&hp.track[song.sel];uint32_t first=h->active&~1u;
 char title[48],preset[16];int changed=hp_sound_changed(TSEL),member=pair_member(TSEL);
 draw_head();for(uint32_t band=0;band<2;band++){cv_begin(240,108,T_BG);cv_oy=-(int32_t)(108*band);cv_bg=T_BG;
 cv_rrect(3,0,234,48,5,T_THEME,T_BG);cv_bg=T_THEME;
 sound_name(TSEL,preset);perf_format(title,sizeof title,"%s%s",preset[0]?preset:"CUSTOM",changed?"*":"");
 cv_free_text(10,0,&AF_M,title,T_INK,T_THEME,184);
 if(member){
  uint32_t partner=song.sel==ep.a?ep.b:ep.a;sound_name(&trk[partner],preset);
  perf_format(title,sizeof title,"T%u %s",partner+1,preset);cv_free_text(10,20,&AF_S,title,T_INK,T_THEME,142);
  perf_format(title,sizeof title,"M%u%%",(ep.effective*100+16383)/32767);cv_text_r(196,20,&AF_S,title,T_INK,T_THEME);
  cv_rect(10,39,184,3,T_MID);uint32_t w=clamp(ep.effective,0,32767)*184/32767;
  if(w)cv_rect(10,39,w,3,T_INK);
 }else cv_free_text(10,24,&AF_S,ENGINES[TSEL->eng_req%NENGINES]->name,T_INK,T_THEME,184);
 cv_rrect(202,3,32,22,3,T_BG,T_THEME);perf_format(title,sizeof title,"T%u",song.sel+1);cv_text_in(202,5,32,&AF_M,title,T_TEXT,T_BG);
#if FELUCCA_CPU2
 cv_text_r(229,29,&AF_S,C1_ON()?"2C":"1C",T_INK,T_THEME);
#endif
 int y=52;
 for(uint32_t half=0;half<2;half++){
  uint32_t row=first+half;int active=row==h->active;cv_bg=T_BG;
  if(row>=h->count)continue;
  uint16_t fill=active?T_ACCENT:T_SURF,ink=active?T_INK:T_MID;
  cv_rrect(3,y,234,active?14:12,3,fill,T_BG);cv_bg=fill;
  perf_format(title,sizeof title,"%s %u/%u",hp_row_name(h,row),row+1,h->count);cv_text(7,y,&AF_S,title,ink);
  cv_text_r(232,y,&AF_S,active?"K1-K4":"SELECT",ink,fill);
  for(uint32_t k=0;k<4;k++)hp_perf_cell(h->cell[row][k],k,y+(active?16:13),active);
  #if CARAVEL_ACTION_BANKS
  y+=active?(action_enabled?72:92):45;
#else
  y+=active?92:45;
#endif
 }
#if CARAVEL_ACTION_BANKS
 if(action_enabled){
  cv_rrect(3,171,234,31,4,T_SURF,T_BG);
  cv_bg=T_SURF;
  perf_format(title,sizeof title,"ACTION %u/%u  %s",action_bank+1,ACT_BANKS,action_bank_names[action_bank]);
  cv_text(7,172,&AF_S,title,T_ACCENT);
  for(uint32_t k=0;k<4;k++)
   cv_free_text(7+(int)k*59,187,&AF_S,action_labels[action_bank][k],T_TEXT,T_SURF,52);
  cv_free_text(5,204,&AF_S,ui.msg_t?ui.msg:"KEY 22: BANK / 23-26: ACT",ui.msg_t?T_ACCENT:T_MID,T_BG,230);
 }else
#endif
  cv_free_text(5,198,&AF_S,ui.msg_t?ui.msg:"SELECT: ROW / ALGO: TRACK",ui.msg_t?T_ACCENT:T_MID,T_BG,230);
 cv_blit(0,24+108*band);}
 cv_oy=0;
}

static void hp_motion_clear(track_t*t){uint32_t irq=perf_irq_save();uint32_t tr=trk_index(t),n=0;for(uint32_t i=0;i<hp.events;i++)if(hp.event[i].track!=tr)hp.event[n++]=hp.event[i];memset(hp.event+n,0,(HP_EVENTS-n)*sizeof hp.event[0]);hp.events=n;for(uint32_t c=0;c<4;c++)hp_macro_live[tr][c]=hp.macro[tr][c].base;hp_dirty=1;perf_irq_restore(irq);}
