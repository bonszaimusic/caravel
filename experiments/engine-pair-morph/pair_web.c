/* SPDX-License-Identifier: GPL-3.0-only */
/* Emulator-only controls, diagnostics and versioned sidecar snapshots. */
EXPORT uint32_t web_pair_prepare(uint32_t a, uint32_t b, uint32_t ea, uint32_t eb)
{
    if (a >= NTRK || b >= NTRK || a == b || !pair_engine_ok(ea) || !pair_engine_ok(eb)) return 0;
    project_init_runtime();
    for (uint32_t k = 0; k < NTRK; k++) memset(trk[k].v, 0, sizeof trk[k].v);
    pair_cleanup_pending = 0; panic_req = 0;
    for (uint32_t m = 0; m < 2; m++) {
        track_t *t = &trk[m ? b : a];
        for (uint32_t id = 0; id < P_COUNT; id++) t->p[id] = TP[id].def;
        memset(t->step, 0, sizeof t->step);
        host_preset(t, m ? eb : ea, 0);
        t->p[P_VOICE] = V_POLY; t->p[P_SPRD] = 0;
        if (t->engine == ENGI_DRONE) t->p[P_E3] = 0;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    }
    voice_policy = 0; voice_policy_sync();
    song.sel = a; song.grid = 0; song.rec = 0;
    ui.force = 1;
    uint32_t ok=pair_enable(a,b);if(ok)go_home();return ok;
}
EXPORT int32_t web_pair_get(uint32_t what, uint32_t track)
{
    if (what == 0) return ep.on;
    if (what == 1) return ep.base;
    if (what == 2) return ep.effective;
    if (what == 3) return ep.budget;
    if (what == 4) return song.sel;
    if (what == 5) return ep.a;
    if (what == 6) return ep.b;
    if (what == 9) return ep.on && ep.motion;
    if (what == 10) return ep.on ? ep.motion_mix : 0;
    if (what == 14) return ep.on ? ep.cells : 0;
    if (track >= NTRK) return -1;
    if (what == 11) return trk[track].eng_req;
    if (what == 12) return pair_member(&trk[track]) ? ep.weight[track == ep.b][CTL-1] : -1;
    if (what == 13) return pair_member(&trk[track]) ? (track == ep.a ? ep.b : ep.a) : -1;
    if (what == 7) {
        uint32_t n = 0;
        for (uint32_t i = 0; i < NVOICE; i++) n += trk[track].v[i].active && trk[track].v[i].gate;
        return n;
    }
    if (what == 8) return trk[track].seq_idx;
    return -1;
}
EXPORT uint32_t web_pair_param(uint32_t track, uint32_t id, int32_t value)
{
    if (track >= NTRK || id >= P_COUNT) return 0;
    const param_desc_t *d = track_desc(&trk[track], id);
    trk[track].p[id] = clamp(value, d->min, d->max);
    ui.force = 1; return 1;
}
EXPORT uint32_t web_pair_param_get(uint32_t track, uint32_t id)
{ return track < NTRK && id < P_COUNT ? (uint32_t)(int32_t)trk[track].p[id] : 0; }
static const uint8_t PAIR_CONTROLS[] = {P_LEVEL,P_PAN,P_MUTE,P_LATCH,P_AMODE,P_LRATE,P_E0,P_E1,P_REV,P_DLY,P_CHOR};
EXPORT uint32_t web_pair_control(uint32_t track, uint32_t control, int32_t value)
{ return control < NELEM(PAIR_CONTROLS) ? web_pair_param(track,PAIR_CONTROLS[control],value) : 0; }
EXPORT int32_t web_pair_control_get(uint32_t track, uint32_t control)
{ return track < NTRK && control < NELEM(PAIR_CONTROLS) ? trk[track].p[PAIR_CONTROLS[control]] : 0; }
EXPORT void web_pair_select(uint32_t track) { if (track < NTRK) { song.sel = track; ui.force = 1; } }
EXPORT void web_pair_morph(uint32_t track, int32_t value)
{ if (track < NTRK && pair_member(&trk[track])) ep.base = clamp(value, 0, 32767); }
EXPORT void web_pair_off(void) { pair_disable(); ui.force=1; }
EXPORT uint32_t web_pair_enable(uint32_t a, uint32_t b) { ui.force=1; uint32_t ok=pair_enable(a,b);if(ok)go_home();return ok; }
EXPORT const char *web_pair_reason(void) { return pair_last_reason; }
EXPORT void web_pair_page(void)
{
    for (uint32_t i = 0; i < NPAGES; i++) if (PAGES[i].graph == GR_PAIR) {
        ui.home = 0; ui.page = i; ui.force = 1; page_entered(); return;
    }
}
EXPORT uint32_t web_pair_motion(uint32_t on)
{ if (!ep.on) return 0; ep.motion = !!on; return 1; }
EXPORT void web_pair_panic(void) { panic_req = (1u << NTRK) - 1u; }
EXPORT uint32_t web_pair_lfo(uint32_t track, uint32_t slot, uint32_t source, int32_t amount)
{
    if (track >= NTRK || slot >= NMSLOT || (source != MS_LFO && source != MS_LFO2)) return 0;
    trk[track].p[P_M1SRC + slot*3] = source;
    trk[track].p[P_M1DST + slot*3] = MD_PAIR;
    trk[track].p[P_M1AMT + slot*3] = clamp(amount, -64, 63);
    if (source == MS_LFO2) lfo2_state[track].fade = 32767;
    ui.force = 1; return 1;
}
EXPORT uint32_t web_pair_lfo2_rate(uint32_t track, int32_t rate)
{ if (track >= NTRK) return 0; lfo2_config[track].p[L2_RATE] = clamp(rate, 0, 127); return 1; }
EXPORT uint32_t web_pair_lfo2_rate_get(uint32_t track)
{ return track < NTRK ? lfo2_config[track].p[L2_RATE] : 0; }
EXPORT int32_t web_pair_depth_get(uint32_t track, uint32_t slot)
{ return track < NTRK && slot < NMSLOT && trk[track].p[P_M1DST+slot*3] == MD_PAIR ? trk[track].p[P_M1AMT+slot*3] : 0; }
EXPORT uint32_t web_pair_lane(uint32_t step, int32_t value)
{ if (step >= NSTEP) return 0; ep.lane[step] = clamp(value, -1, 32767); return 1; }
EXPORT void web_pair_sequence(void)
{
    if (!ep.on) return;
    track_t *t = &trk[ep.a];
    memset(t->step, 0, sizeof t->step);
    for (uint32_t i = 0; i < 16; i += 4) {
        uint8_t note = 48 + i;
        put_step(t, i, 1, &note, ST_NOTE, 0);
        ep.lane[i] = (int16_t)(i * 32767u / 12u);
    }
    t->p[P_SLEN] = 16; t->p[P_SGATE] = 96;
    ui.force = 1;
}

#define PAIR_HEADER 192u
static uint8_t pair_snapshot[PAIR_HEADER + PROJ_STORE_SIZE];
static void pair_put16(uint32_t off, int32_t v)
{ pair_snapshot[off] = v & 255; pair_snapshot[off+1] = (uint32_t)v >> 8; }
static int32_t pair_get16(uint32_t off)
{ return (int16_t)(pair_snapshot[off] | (uint32_t)pair_snapshot[off+1] << 8); }
EXPORT uint8_t *web_pair_snapshot_buf(void) { return pair_snapshot; }
EXPORT uint32_t web_pair_snapshot_size(void) { return sizeof pair_snapshot; }
EXPORT uint32_t web_pair_snapshot_save(void)
{
    if (!ep.on) return 0;
    memset(pair_snapshot, 0, sizeof pair_snapshot);
    memcpy(pair_snapshot, "EPM2", 4);
    pair_snapshot[12] = ep.a; pair_snapshot[13] = ep.b;
    pair_snapshot[14] = ep.motion;
    pair_put16(16, ep.base);
    for (uint32_t i = 0; i < NSTEP; i++) pair_put16(24 + i*2, ep.lane[i]);
    for (uint32_t t = 0; t < NTRK; t++)
        for (uint32_t k = 0; k < NMSLOT; k++)
            pair_snapshot[152 + t*4 + k] = trk[t].p[P_M1DST + k*3] == MD_PAIR;
    project_capture(&proj_scratch);
    if (!proj_pack(&proj_wire, &proj_scratch)) return 0;
    memcpy(pair_snapshot + PAIR_HEADER, &proj_wire, PROJ_STORE_SIZE);
    uint32_t hash = proj_hash(pair_snapshot + 12, sizeof pair_snapshot - 12);
    for (uint32_t i = 0; i < 4; i++) pair_snapshot[8+i] = hash >> (8*i);
    return sizeof pair_snapshot;
}
EXPORT uint32_t web_pair_snapshot_load(uint32_t size)
{
    uint32_t hash = 0, a = pair_snapshot[12], b = pair_snapshot[13];
    uint32_t legacy = !memcmp(pair_snapshot, "EPM1", 4);
    if (size != sizeof pair_snapshot || (!legacy && memcmp(pair_snapshot, "EPM2", 4)) ||
        a >= NTRK || b >= NTRK || a == b || pair_snapshot[14] > (legacy ? 0u : 1u)) return 0;
    for (uint32_t i = 0; i < 4; i++) hash |= (uint32_t)pair_snapshot[8+i] << (8*i);
    if (hash != proj_hash(pair_snapshot + 12, sizeof pair_snapshot - 12) || pair_get16(16) < 0) return 0;
    for (uint32_t i = 0; i < NSTEP; i++) if (pair_get16(24+i*2) < -1) return 0;
    if (!proj_import(&proj_scratch, pair_snapshot + PAIR_HEADER, PROJ_STORE_SIZE)) return 0;
    /* Validate supported engines and topology before touching the working session. */
    for (uint32_t m = 0; m < 2; m++) {
        const proj_trk_t *t = &proj_scratch.t[m ? b : a];
        if (!pair_engine_ok(t->engine) ||
            (uint32_t)t->p[P_VOICE] > V_UNISON || t->p[P_SPRD]) return 0;
    }
    if (project_restore_runtime(&proj_scratch)) return 0;
    /* A snapshot restores configuration, never sounding/latched notes. */
    for (uint32_t t = 0; t < NTRK; t++) {
        memset(trk[t].v, 0, sizeof trk[t].v); engine_block(&trk[t]);
    }
    voice_policy = 0; voice_policy_sync();
    if (!pair_enable(a, b)) return 0;
    ep.base = ep.effective = pair_get16(16);
    ep.motion = pair_snapshot[14];                  /* EPM1 reserved byte is zero: legacy morph OFF */
    for (uint32_t i = 0; i < NSTEP; i++) ep.lane[i] = pair_get16(24+i*2);
    for (uint32_t t = 0; t < NTRK; t++)
        for (uint32_t k = 0; k < NMSLOT; k++)
            if (pair_snapshot[152+t*4+k]) trk[t].p[P_M1DST + k*3] = MD_PAIR;
    ui.force = 1; return 1;
}
