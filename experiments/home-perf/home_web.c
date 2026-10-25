/* SPDX-License-Identifier: GPL-3.0-only */
EXPORT int32_t web_home_get(uint32_t what,uint32_t t,uint32_t row,uint32_t k)
{
 if(t>=NTRK)return -1;if(what==0)return hp.track[t].count;if(what==1)return hp.track[t].active;if(what==2)return hp_default;if(what==8)return hp.events;if(what==9){uint32_t mask=0;for(uint32_t s=0;s<4;s++)if(hp_slot(s))mask|=1u<<s;return mask;}if(what==10)return hp_pending_mask;
 if(row>=HP_ROWS||k>=4)return -1;hp_ref_t r=hp.track[t].cell[row][k];if(what==3)return r.kind;if(what==4)return r.id;if(what==5)return hp_get(r);if(what==6)return r.track;if(what==7)return !!hp_desc(r);return -1;
}
EXPORT void web_home_apply(uint32_t t,uint32_t set){if(set<3)hp_apply_template(t,set);}
EXPORT void web_home_default(uint32_t set){if(set<3){hp_default=set;hp_dirty=1;}}
EXPORT void web_home_row(uint32_t row){hp.track[song.sel].active=clamp(row,0,hp.track[song.sel].count-1);go_home();}
EXPORT void web_home_open(uint32_t page){if(ui.menu){settings_save();ui.menu=0;hp_apply_armed=0;}ui.ly=ui.layer=ui.lock=0;if(page==0)home_tap();if(page==1)hp_open_editor();if(page==2)hp_open_graph(GR_MACRO);if(page==3)hp_open_graph(GR_MACROMAP);if(page==4)hp_open_graph(GR_PAIR);if(page==5){ui.fam_last[FAM_GLO]=hp_global_page(0,0);open_family(FAM_GLO);}if(page==6){ui.fam_last[FAM_GLO]=hp_global_page(0,1);open_family(FAM_GLO);}if(page==7){ui.fam_last[FAM_GLO]=hp_global_page(0,2);open_family(FAM_GLO);}}
EXPORT const char*web_home_notice(void){return ui.msg;}
EXPORT uint32_t web_home_assign(uint32_t t,uint32_t row,uint32_t col,uint32_t kind,uint32_t owner,uint32_t id)
{
 if(t>=NTRK||row>=HP_ROWS||col>=4||owner>=NTRK)return 0;hp_ref_t r={.kind=kind,.track=owner,.engine=255,.id=id,.page=0xFFFF};if(kind==HP_TRACK)r=hp_track_ref(owner,id,0xFFFF);if(!hp_ref_valid(r))return 0;
 hp.track[t].cell[row][col]=r;if(hp.track[t].count<=row)hp.track[t].count=row+1;hp_dirty=1;ui.force=1;return 1;
}
EXPORT uint32_t web_home_save(uint32_t slot){return slot<4&&!hp_project_save(slot,0);}
EXPORT uint32_t web_home_load(uint32_t slot){return slot<4&&hp_project_load(slot);}
EXPORT uint32_t web_home_project_size(void){return sizeof(hp_store_t);}
EXPORT uint8_t*web_home_project_buf(void){return (uint8_t*)&hp_io;}
EXPORT uint32_t web_home_export(uint32_t slot){const hp_store_t*p=hp_slot(slot);if(!p)return 0;memcpy(&hp_io,p,sizeof hp_io);return sizeof hp_io;}
EXPORT uint32_t web_home_import(uint32_t n){return n==sizeof hp_io&&hp_restore(&hp_io);}
EXPORT uint32_t web_home_macro_route(uint32_t t,uint32_t m,uint32_t route,uint32_t id,int32_t amount)
{if(t>=NTRK||m>=4||route>=4||id>=P_COUNT)return 0;const param_desc_t*d=track_desc(&trk[t],id);if(!motion_param(id)||d->fmt==F_ENUM||d->fmt==F_ONOFF)return 0;hp.macro[t][m].route[route].ref=hp_track_ref(t,id,0xFFFF);hp.macro[t][m].route[route].amount=clamp(amount,-64,63);return 1;}
EXPORT uint32_t web_home_macro_set(uint32_t t,uint32_t m,int32_t value){if(t>=NTRK||m>=4)return 0;hp_special_set(t,m,clamp(value,0,127),1);return 1;}
EXPORT int32_t web_home_macro_get(uint32_t t,uint32_t m){return t<NTRK&&m<4?hp.macro[t][m].base:-1;}

EXPORT void web_home_add(void){hp_add_last();}
