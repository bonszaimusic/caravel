/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FORMAT 2 projects: complete HPM2 containers and native FUNW working copies.
 * Legacy import/conversion is intentionally absent. */
#include "project_format2.c"

#ifndef PROJ_HOST
/* the GM drum part of a project of before 1.0 (proj_drums_to_part): DRUM's first kit, as a preset load sets it
 * (until 1.0.2 the SAMPLE engine's PERC set, retired since) */


static project_t proj_scratch               /* decoded main-loop work, never audio ISR */
    __attribute__((section(".pool")));
static char proj_name[PROJ_NAME_LEN + 1u]    /* the name of the music as it is now (loaded, saved, the editor's */
    __attribute__((section(".pool")));       /* runtime restore); "" = none. A save takes it unless one is given */
#define PROJ_NO_SLOT 0xFFu
static uint8_t proj_cur = PROJ_NO_SLOT;      /* the slot the music was loaded from or last saved to (a rename of it
                                              * renames the music too); PROJ_NO_SLOT none (the editor's restore) */
static union {                               /* serialized main-loop work; no retained expansion */
    project_store_t s;
    uint8_t raw[
#if FELUCCA_PERF_HW
    10000
#else
    3840
#endif
    ];                         /* (the staging of a backup object, up to a storage object: editor_backup.c) */
} proj_wire_u __attribute__((section(".pool")));
#define proj_wire (proj_wire_u.s)
static uint8_t proj_wire_gen;                /* +1 whenever proj_wire is rewritten (a backup's runtime copy lives there) */
/* Reuse the four retained slots. A complemented mask survives a warm reset so
 * boot can discard uncommitted copies rather than mistake them for flash saves.
 * No additional project/pattern buffer, and no change to the on-flash format. */
static uint16_t project_ram_state __attribute__((section(".noinit")));
static uint32_t project_ram_mask(void)
{
    uint32_t mask = project_ram_state & 255u;
    return mask < 16u && (project_ram_state >> 8) == (mask ^ 255u) ? mask : 0u;
}
static void project_ram_set(uint32_t mask)
{
    mask &= 15u;
    project_ram_state = (uint16_t)(mask | ((mask ^ 255u) << 8));
    ui.force = 1;
}
#if FELUCCA_FLASH
static uint8_t project_save_retry;
static uint32_t project_save_retry_ms;
#endif

static void project_ram_notice(void)
{
    ui_message("RAM ONLY");
    str_cpy(ui.msg2, "STOP TO SAVE", sizeof ui.msg2);
}

#include "project_compat.c"

static void proj_steps(step_t *s)            /* a loaded sequence stays inside its fixed fields */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        if (s[i].n > 4u) s[i].n = 4;
        if (s[i].time > ST_REST) s[i].time = ST_REST;
        for (j = 0; j < 4u; j++) s[i].note[j] &= 127u;
        s[i].acc &= s[i].hit;
    }
}

/* an imported older format may hold values FUN7 cannot pack: keep them inside the fields and ranges */
static void proj_bound(project_t *q)
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        proj_steps(q->t[t].step);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i];
            s->flags &= 3u | SF_RATCH | SF_NUDGE;
            s->vel &= 127u;
            if (s->probability >= COND_FIRST + NCOND) s->probability = 0;
        }
        for (i = 0; i < P_COUNT; i++) {
            /* Keep disabled SLICE's source settings intact in saved-slot caches and exports. */
            if (q->t[t].engine == 13u && i >= P_E0) continue;
            const param_desc_t *d = param_desc_of(q->t[t].engine % NENGINES, i);
            q->t[t].p[i] = (int16_t)param_fit(d, q->t[t].p[i]);   /* (a retired KIT: the kit it plays) */
        }
    }
    q->sum = proj_sum(q);
}

/* A decode fills the staged proj_harmony only (never the live arrays), so a read-only probe or an in-place rename
 * cannot make the live Harmony/Link state follow the slot it decodes. harmony_save stages the live state for a
 * pack; harmony_restore installs a staged state in a committed load. (Round 7: the save/restore pairs around the
 * probes are gone: they wrote a stale state, and the ISR-owned bar clock, back over the live one.) */
static void harmony_save(harmony_saved_t *s)
{
    memcpy(s->root, harmony_root, sizeof s->root);
    memcpy(s->dur, harmony_dur, sizeof s->dur);
    memcpy(s->link_src, link_src, sizeof s->link_src);
    memcpy(s->link_retrig, link_retrig, sizeof s->link_retrig);
    memcpy(s->follow, harmony_follow, sizeof s->follow);
    s->key_mode = harmony_key_mode;
    s->key_root = harmony_key_root;
    s->key_scale = harmony_key_scale;
    memcpy(s->local_shape,harmony_local_shape,sizeof s->local_shape);
    s->on = harmony_on;
    s->bar = harmony_bar;
    s->base = harmony_base_bar;
}
static void harmony_restore(const harmony_saved_t *s)
{
    memcpy(harmony_root, s->root, sizeof s->root);
    memcpy(harmony_dur, s->dur, sizeof s->dur);
    memcpy(link_src, s->link_src, sizeof s->link_src);
    memcpy(link_retrig, s->link_retrig, sizeof s->link_retrig);
    memcpy(harmony_follow, s->follow, sizeof s->follow);
    harmony_key_mode = s->key_mode;
    harmony_key_root = s->key_root;
    harmony_key_scale = s->key_scale;
    memcpy(harmony_local_shape,s->local_shape,sizeof harmony_local_shape);
    harmony_on = s->on;
    harmony_bar = s->bar;
    harmony_base_bar = s->base;
}

#if FELUCCA_FLASH
/* slot from flash into RAM (format 7, or format 6 / 5 / 4 / 3 / 2 / 1 converted) */
static void proj_fetch(uint32_t slot)
{
    project_store_t *q = &proj_slot[slot & 3u];
    int n;
    proj_wire_gen++;
    n = st_load(OBJ_PROJECT0 + (slot & 3u), &proj_wire, sizeof proj_wire);
    if (!proj_import(&proj_scratch, &proj_wire, n))
        memset(q->raw, 0, 4);
    else {
        proj_bound(&proj_scratch);
        if (!proj_pack(q, &proj_scratch))
            memset(q->raw, 0, 4);
    }
}
#endif

static void project_capture(project_t *p)
{
    uint32_t i;
    uint32_t f;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC_V11;
    p->size = sizeof *p;
    /* Only the live data copy is guarded. Clearing, FM6 packing and hashing
     * run with audio interrupts enabled; the patch bytes are main-loop owned. */
    f = motion_guard();
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    /* Project-level sound design: the values belong to the music, not
     * device preferences. The fixed FUNW extension stores all five. */
#if CARAVEL_AFTERBURNER
    p->af_on = (uint8_t)clamp(afterburner.on, 0, 1);
    p->af_time = (uint8_t)clamp(afterburner.time, 10, 60);
    p->af_amt = (uint8_t)clamp(afterburner.amt, 0, 127);
    p->af_damp = (uint8_t)clamp(afterburner.damp, 0, 127);
#else
    p->af_on = 0; p->af_time = 30; p->af_amt = 32; p->af_damp = 70;
#endif
#if CARAVEL_STASIS_EXPAND
    p->st_tone = (uint8_t)clamp(stasis_tone, 0, 127);
#else
    p->st_tone = 64;
#endif
#if CARAVEL_STASIS2
    p->st_region = (stasis_region_saved_t){
        (uint8_t)clamp(st_expand.scan,0,127),
        (uint8_t)clamp(st_expand.length,0,127),
        (uint8_t)clamp(st_expand.grain,0,ST_GRAIN_MAX),
        (uint8_t)clamp(st_expand.quality,0,2),
        (uint8_t)clamp(st_expand.reverse,0,1)
    };
    p->st_region_present = 1u;
#endif
    p->sel = song.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    harmony_save(&proj_harmony);
    for (i = 0; i < NTRK; i++) {
        for (uint32_t j = 0; j < P_COUNT; j++) p->t[i].p[j] = motion_base_value(&trk[i], j);
        p->lfo2[i] = lfo2_config[i];
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
    }
    p->motion = motion;
    motion_fx_snapshot(&p->motion, (1u << NTRK) - 1u);
    memcpy(p->mch, song.mch, sizeof p->mch);
    motion_unguard(f);
    memcpy(p->name, proj_name, str_len(proj_name));
#if FELUCCA_PAIR_MORPH
    for (uint32_t t = 0; t < NTRK; t++)
        for (uint32_t k = 0; k < NMSLOT; k++)
            if (p->t[t].p[P_M1DST + 3*k] == MD_PAIR) p->t[t].p[P_M1DST + 3*k] = MD_OFF;
    for (uint32_t i = 0; i < p->motion.count; i++) {
        motion_event_t *e = &p->motion.event[i];
        for (uint32_t k = 0; k < NMSLOT; k++)
            if (!MOTION_IS_FX(e) && MOTION_ID(e) == P_M1DST + 3*k && e->value == MD_PAIR) e->value = MD_OFF;
    }
#endif
    p->sum = proj_sum(p);
}

/* the music as it is now -> slot, named `name` (0: the current name; "" none); 0 saved, nonzero refused or failed.
 * The current name becomes the saved one */
static int project_save_as(uint32_t slot, const char *name)
{
    if (project_compat_save_blocked(slot)) return 1;
#if FELUCCA_HOME_PERF
    return hp_project_save(slot,name);
#endif

    project_t *p = &proj_scratch;
    if (0) {                                /* SONG plays from its own frozen pattern cache */
        ui_message("STOP SONG FIRST");
        return 1;
    }
    project_capture(p);
    if (name) {
        memset(p->name, 0, sizeof p->name);
        memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    }
    proj_wire_gen++;
    if (!proj_pack(&proj_wire, p)) { ui_message("SAVE FORMAT ERROR"); return 2; }

    if (transport_busy()) {                            /* never erase/program flash during playback */
        project_ram_set(project_ram_mask() | (1u << (slot & 3u)));
        memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
        proj_name_get(proj_name, (const uint8_t *)p->name);
        proj_cur = (uint8_t)(slot & 3u);
        pat_from_set((slot & 3u) + 1u);
        project_ram_notice();
        project_compat_saved();
        return 0;
    }

#if FELUCCA_FLASH
    if (flash_ok) {
        if (st_save(OBJ_PROJECT0 + (slot & 3u), &proj_wire, sizeof proj_wire)) {
            ui_message("SAVE ERROR");
            return 2;
        }
        memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
        project_ram_set(project_ram_mask() & ~(1u << (slot & 3u)));
        proj_name_get(proj_name, (const uint8_t *)p->name);
        proj_cur = (uint8_t)(slot & 3u);
        pat_from_set((slot & 3u) + 1u);
        ui_message("SAVED");
        project_compat_saved();
        return 0;
    }
#endif
    memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
    project_ram_set(project_ram_mask() | (1u << (slot & 3u)));
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = (uint8_t)(slot & 3u);
    pat_from_set((slot & 3u) + 1u);
    ui_message("SAVED (RAM)");
    project_compat_saved();
    return 0;
}
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }

/* Main loop only: commit each saved snapshot, not the current working pattern.
 * One slot per pass, only after the transport is stopped. Failed A/B writes
 * preserve both the RAM snapshot and the previous durable copy, then retry. */
static void project_save_poll(void)
{
#if FELUCCA_PERF_HW
    return; /* Combined native + performance commit is owned by hp_autosave_poll. */
#endif
#if FELUCCA_FLASH
    uint32_t mask = project_ram_mask(), slot;
    if (!mask || !flash_ok || transport_busy() ||
        (project_save_retry && (uint32_t)(fm1_ms - project_save_retry_ms) < 1000u)) return;
    for (slot = 0; slot < 4u && !(mask & (1u << slot)); slot++) {}
    if (st_save(OBJ_PROJECT0 + slot, &proj_slot[slot], sizeof proj_slot[slot])) {
        project_save_retry = 1;
        project_save_retry_ms = fm1_ms;
        ui_message("RAM: SAVE ERROR");
        return;
    }
    project_save_retry = 0;
    project_ram_set(mask & ~(1u << slot));
    if (!project_ram_mask()) ui_message("PARTS SAVED");
#endif
}
static void project_cur_name(char *b) { str_cpy(b, proj_name, PROJ_NAME_LEN + 1u); }   /* b: 13 bytes */

/* slot's name -> b (PROJ_NAME_LEN + 1 bytes); 0 = an empty slot (b ""). Uses proj_scratch */
static int project_name(uint32_t slot, char *b) { return project_info(slot, b, 0, 0, 0); }
/* .. and, len48 not 0, the length of each track's pattern as SONG plays it (song_chain.c chain_tick: the clock track's
 * LEN steps of its DIV, clamped as chain_prepare does) in 1/48 quarter notes (a 4/4 bar: 192; 0 = an empty slot):
 * len48[0..NTRK-1] */
static uint32_t len48_of(int32_t len, int32_t div)
{
    uint32_t n = (uint32_t)clamp(len, TP[P_SLEN].min, TP[P_SLEN].max), d = (uint32_t)clamp(div, TP[P_SDIV].min, TP[P_SDIV].max);
    return n * (d < 6u ? 48u / DIV_DEN[d] : d < 10u ? 48u << (d - 5u) : 48u / DIV_DEN[d % 6u]);   /* (fx.c div_samples) */
}
/* SONG's picture of a pattern (ui_graph.c graph_song): its LEN steps in n = min(LEN, 16) cells (a cell: steps
 * c * LEN / n .. (c + 1) * LEN / n - 1), 2 bits a cell, cell c at bits 2c: a step weighs 0 (rest, empty, CHANCE 0),
 * 1 (a tie), 2 (a note or hit), 3 (accented); a cell's level is its steps' mean, rounded up (0..3) */
static uint32_t proj_density(const proj_trk_t *t, uint8_t *n)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), nc = len < 16u ? len : 16u, c, i, d = 0;
    for (c = 0; c < nc; c++) {
        uint32_t i0 = c * len / nc, i1 = (c + 1u) * len / nc, w = 0;
        for (i = i0; i < i1; i++) {
            const step_t *s = &t->step[i];
            if (s->time == ST_TIE)
                w += 1u;
            else if (s->time == ST_NOTE && (s->n || s->hit) && step_chance(s))
                w += (s->flags & SF_ACCENT) || s->acc ? 3u : 2u;
        }
        w = (w + (i1 - i0) - 1u) / (i1 - i0);
        d |= (w < 3u ? w : 3u) << (2u * c);
    }
    *n = (uint8_t)nc;
    return d;
}
/* .. and, den not 0, each track's picture (proj_density: den[k], its cells n[k]; 0 for an empty slot) */
static int project_info(uint32_t slot, char *b, uint16_t *len48, uint32_t *den, uint8_t *n)
{
    uint32_t k;
    b[0] = 0;
    if (len48)
        memset(len48, 0, NTRK * sizeof *len48);
    if (den) {
        memset(den, 0, NTRK * sizeof *den);
        memset(n, 0, NTRK);
    }
    if (!proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t))) {
        return 0;
    }
    proj_name_get(b, (const uint8_t *)proj_scratch.name);
    for (k = 0; len48 && k < NTRK; k++)
        len48[k] = (uint16_t)len48_of(proj_scratch.t[k].p[P_SLEN], proj_scratch.t[k].p[P_SDIV]);
    for (k = 0; den && k < NTRK; k++)
        den[k] = proj_density(&proj_scratch.t[k], &n[k]);
    return 1;
}

/* a stored project renamed in place (nothing else of it changes; the music playing is not touched, but the slot
 * it was loaded from / saved to: its name is the new one, the next SAVE's prefill): 0 done, 1 refused (playing,
 * an empty slot), 2 failed (the slot as it was) */
static int project_rename(uint32_t slot, const char *name)
{
#if FELUCCA_PERF_HW
    int hp_rc=hp_project_rename(slot&3u,name);
    if(hp_rc>=0)return hp_rc; /* Legacy native slots use the original rename below. */
#endif
    project_t *p = &proj_scratch;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    if (!proj_import(p, &proj_slot[slot & 3u], sizeof(project_store_t))) {
        ui_message("EMPTY SLOT");
        return 1;
    }
    memset(p->name, 0, sizeof p->name);
    memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    proj_wire_gen++;
    if (!proj_pack(&proj_wire, p)) { ui_message("SAVE FORMAT ERROR"); return 2; }
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_PROJECT0 + (slot & 3u), &proj_wire, sizeof proj_wire)) {
        ui_message("SAVE ERROR");
        return 2;
    }
#endif
    memcpy(&proj_slot[slot & 3u], &proj_wire, sizeof proj_wire);
    if (proj_cur == (slot & 3u))
        proj_name_get(proj_name, (const uint8_t *)p->name);
#if FELUCCA_FLASH
    if (flash_ok) { ui_message("RENAMED"); return 0; }
#endif
    ui_message("RENAMED (RAM)");
    return 0;
}

/* Initialize the working project; persistent slots, pending saves and device
 * preferences are untouched. Engines switch through their usual release fade. */
static void project_init_runtime(void)
{
    project_compat_state.trial = project_compat_state.hold_autosave = 0;
#if FELUCCA_HOME_PERF
    hp_init();
#endif

#if FELUCCA_PAIR_MORPH
    pair_disable();
#endif
    mom_back(0);
    fm1_irq_off();
    chordext_reset_runtime();
    seq_stop();
    transport_req = 0;
    song.rec = step_record_mask = 0;
    panic_req = (1u << NTRK) - 1u;
    mi_r = mi_w;
    /* Send panel note-offs using the old project routing before resetting it. */
    for (uint32_t k = 0; k < 27u; k++) key_off(k, &trk[kb_trk[k] % NTRK]);
    kb_prev = fm1_in.notes;
    memset(kb_note, KB_SILENT, sizeof kb_note);
    memset(kb_fx, 0, sizeof kb_fx);
    perf_held = perf_latched = perf_solo = 0;
    memset(perf_k, 0, sizeof perf_k);
    memset(perf_motion, 0, sizeof perf_motion);
    memset(drum_mute, 0, sizeof drum_mute);
    memset(&motion, 0, sizeof motion);
    memset(motion_fx_owner, 0, sizeof motion_fx_owner);
    motion_fx_recording = 0;
    motion_base_valid = 0;
    harmony_perf_clear();
    harmony_on = 0;
    harmony_key_mode = 1u;  /* fresh 1.0 project owns its global key */
    harmony_key_root = 0u;
    harmony_key_scale = 1u; /* MAJOR */
    memset(harmony_local_shape,CH_DIA3,sizeof harmony_local_shape);
    harmony_bar = 0;
    harmony_base_bar = HARM_BAR();
    memset(harmony_root, 0, sizeof harmony_root);
    memset(harmony_dur, 0, sizeof harmony_dur);
    memset(link_src, 0, sizeof link_src);
    memset(link_retrig, 0, sizeof link_retrig);
    memset(harmony_follow, 0, sizeof harmony_follow); /* 1.0: new tracks start in NOTES */
    for (uint32_t i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
#if CARAVEL_AFTERBURNER
    /* New projects have a deliberate, safe OFF default. */
    afterburner.on = 0; afterburner.time = 30;
    afterburner.amt = 32; afterburner.damp = 70;
#endif
#if CARAVEL_STASIS_EXPAND
    stasis_tone = 64; stasis_tone_sm = 64 * 256;
#endif
#if CARAVEL_STASIS2
    /* A fresh song must not inherit source-region settings from an old one. */
    st_expand.scan=0;st_expand.length=64;
    st_expand.grain=(int16_t)ST_GRAIN_DEFAULT;st_expand.quality=0;st_expand.reverse=0;
    st_hall_feed=st_hall_ret=64u;        /* FEED and HALL are runtime only: 64 = 100 % */
#endif
    song.sel = 0;
    memset(song.mch, 0, sizeof song.mch);
    fm1_irq_on();
    ml_off();
    undo_depth++;
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        fm1_irq_off();
        memset(t->step, 0, sizeof t->step);
        track_defaults(t);
        fm1_irq_on();
        fx_fresh++;                                     /* (a new project: FX HOLD keeps nothing of the old one) */
        set_engine_of(t, TRK_DEF[k][0]);
        apply_preset_to(t, TRK_DEF[k][1]);
        fx_fresh--;
        track_defaults_steps(t);
        t->nheld = t->arp_phys = 0;
        pat_sig[k] = steps_sig(t);
        pat_last[k] = TRK_DEF[k][2];
    }
    undo_depth--;
    memset(&undo, 0, sizeof undo);
    memset(ui.mpc, 0, sizeof ui.mpc);
    ui.entry_open = ui.cursor = ui.bank = ui.song_row = ui.hot_t = ui.ev_keep = 0;
    song.octave = 0;
    song.seq_mode = 0;
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    pat_from_set(0);
    sync_reload = ui.force = 1;
    ui_message("PROJECT INITIALIZED");
}

/* 1.5.5 (#199): a stored project erased (SAVE > PROJECT, KNOB 2 ERASE, the dialog): the slot empty. Written as a save
 * is (st_save: the other copy of the A/B pair erased and an empty record committed last, as a full backup's restore
 * writes an empty slot), so a power cut leaves the project there or the slot empty, never half of either. The music
 * playing is not touched: it keeps its name; were it loaded from or last saved to this slot it now has none (the
 * header's slot letter goes). 0 erased, 1 refused (playing, an empty slot), 2 failed (the slot as it was) */
static int project_erase(uint32_t slot)
{
    if (project_compat_save_blocked(slot)) return 1;
    uint32_t k;
    slot &= 3u;
    if (transport_busy()) {                            /* (a flash erase silences the audio: project_save_as) */
        ui_message("STOP TO ERASE");
        return 1;
    }
    if (!project_used(slot)) {
        ui_message("EMPTY SLOT");
        return 1;
    }
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_PROJECT0 + slot, &proj_wire, 0)) {
        ui_message("ERASE ERROR");
        return 2;
    }
#endif
#if FELUCCA_HOME_PERF
    if(hp_forget(slot)){ui_message("ERASE ERROR");return 2;}
#endif
    memset(&proj_slot[slot],0,sizeof proj_slot[slot]);
    if (proj_cur == slot)
        proj_cur = PROJ_NO_SLOT;
    for (k = 0; k < NTRK; k++)
        if (pat_from[k] == slot + 1u)
            pat_from[k] = 0;
#if FELUCCA_FLASH
    if (flash_ok) {
        char b[20] = "PROJECT A ERASED";
        b[8] = (char)('A' + slot);
        ui_message(b);
        return 0;
    }
#endif
    ui_message("ERASED (RAM)");
    return 0;
}

static int project_restore_runtime(const project_t *input)
{
    project_t *p = &proj_scratch;
    uint32_t i, k;
    if (!proj_ok(input) || !proj_engines_ok(input)) return 1;
    uint32_t compat = project_compat_flags(input);
    if (compat && project_compat_state.approved != compat) {
        ui_message("LOAD FROM SLOT FIRST");
        return 1; /* USB/current-session restores cannot bypass the dialog. */
    }
#if CARAVEL_ELASTIC_P1 && FELUCCA_HOME_PERF
    hp_elastic_p1_clear();
#endif
#if FELUCCA_PAIR_MORPH
    pair_disable();
#endif

    mom_back(0);                                  /* (ui_input.c MOMENTARY: the values as they were, then the load) */
    memset(drum_mute, 0, sizeof drum_mute);       /* (1.4.1: the DRUM lanes' mutes, performance state) */
    if (p != input) memcpy(p, input, sizeof *p);
    /* Only the approved working copy is converted. Stored originals stay intact. */
    if (compat & 1u) for (uint32_t t = 0; t < NTRK; t++)
        if (p->t[t].engine == 13u) p->t[t].engine = 0;
#if CARAVEL_REVERB_MINIMAL
    if (compat & 6u) p->g[G_RTYPE] = 2;
#endif
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    chordext_reset_runtime();
    seq_stop();
    transport_req = 0;
    harmony_restore(&proj_harmony);
    harmony_perf_clear();
    harmony_bar = 0;
    harmony_base_bar = HARM_BAR();
                                                       /* the FUN11 block already set harmony_on / roots / durations /
                                                       * link sources / retrigger in the import */
    motion = p->motion;
    motion_fx_compact_off(&motion, (1u << NTRK) - 1u);
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = 0;
    ui.song_row = 0;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
#if CARAVEL_AFTERBURNER
    afterburner.on = p->af_on; afterburner.time = p->af_time;
    afterburner.amt = p->af_amt; afterburner.damp = p->af_damp;
#endif
#if CARAVEL_STASIS_EXPAND
    stasis_tone = p->st_tone;
    stasis_tone_sm = (int32_t)stasis_tone * 256; /* no old-project glide */
#endif
#if CARAVEL_STASIS2
    /* Complete source REGION and capture-quality project roundtrip.
     * No legacy LAYER global ID is ever interpreted as a source setting. */
    st_expand.scan=p->st_region.pos;
    st_expand.length=p->st_region.length;
    st_expand.grain=p->st_region.grain;
    st_expand.quality=p->st_region.quality;
    st_expand.reverse=p->st_region.reverse;
    st_hall_feed=st_hall_ret=64u;        /* FEED and HALL are runtime only: a load starts at 100 % (64) */
#endif
    for (k = 0; k < NTRK; k++)                          /* MIDI IN TRACK's channels (1.5.5; 0, channels 1..4, before) */
        song.mch[k] = (uint8_t)(p->mch[k] < MCH_N ? p->mch[k] : 0u);
    midi_route = MIDI_ROUTE_AGAIN;                      /* (events_block: what the channels no longer hear let go) */
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine;                         /* (< NENGINES: proj_engines_ok) */
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        lfo2_load(k, &p->lfo2[k]);
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range (param_fit) */
            const param_desc_t *d = param_desc_of(e, i);
            t->p[i] = (int16_t)param_fit(d, s->p[i]);
        }
        t->preset = (uint8_t)(ENGINES[e]->npresets ? s->preset % ENGINES[e]->npresets : 0u);
        memcpy(t->step, s->step, sizeof t->step);
        proj_steps(t->step);
    }
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = PROJ_NO_SLOT;                            /* (project_load: its slot) */
    pat_from_set(0);                                    /* (project_load: its slot; the header's letter) */
    undo.trk = 0;                                       /* (ui.c) the undo copy belongs to the old project */
    undo_depth++;                                       /* and these loads take none */
    for(k=0;k<NTRK;k++)pat_sig[k]=~steps_sig(&trk[k]);
    undo_depth--;
    sync_reload = 1;
    ui.force = 1;
    project_compat_state.trial = project_compat_state.hold_autosave = (uint8_t)compat;
    if (compat && project_compat_state.source < 4u) project_compat_state.protected_slots |= 1u << project_compat_state.source;
    /* RESTORE LAST has no reliable source-slot identity: only empty slots are
     * safe copy destinations. Do not overwrite any occupied project. */
    if (compat && project_compat_state.source == 4u)
        for (uint32_t s = 0; s < 4u; s++)
            if (proj_slot[s].align) project_compat_state.protected_slots |= 1u << s;
    ui_message(compat ? "TEST LOADED / SAVE A COPY" : "LOADED");
    memset(snd_said, 0, sizeof snd_said);               /* a missing sample is said again, after LOADED */
    return 0;
}
static void project_load(uint32_t slot)
{
#if FELUCCA_HOME_PERF
    if(hp_project_load(slot)) return;
#endif

#if FELUCCA_FLASH
    if (flash_ok && !proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t))) proj_fetch(slot);
#endif
    if (!proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t))) {
        ui_message("EMPTY SLOT");
        return;
    }
    if (project_compat_request(&proj_scratch, slot & 3u))
        return;
    proj_bound(&proj_scratch);                          /* a retained RAM slot of an older format: as from flash */
    if (!project_restore_runtime(&proj_scratch)) {
#if FELUCCA_HOME_PERF
        hp_init();
        for(uint32_t t=0;t<NTRK;t++)hp_apply(t,hp_default);
#endif
        proj_cur = (uint8_t)(slot & 3u);
        pat_from_set((slot & 3u) + 1u);
    }
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
#include "settings_persist.c"
#if FELUCCA_FLASH
static persist_t persist_saved __attribute__((section(".pool")));
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

#if FELUCCA_FLASH
/* The JEDEC IDs of the 1 MiB parts this firmware saves to: the Puya P25Q80 of the first FM-1s and the UC25HQ80 of newer ones (Felucca #320). */
static int flash_id_known(uint32_t id) { return id == 0x856014u || id == 0xB36014u; }
#endif
static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = flash_id_known(FL_FAR(fl_jedec_ram)());   /* a known 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (settings_import(&p, n))
            persist_saved = p;
    }
    {   /* Uncommitted retained copies are discarded on reboot; flash stays authoritative. */
        uint32_t i;
        uint32_t dirty = project_ram_mask();
        project_ram_set(0);
        for (i = 0; i < 4u; i++)
            if ((dirty & (1u << i)) || !proj_import(&proj_scratch, &proj_slot[i], sizeof(project_store_t)))
                proj_fetch(i);
    }
    up_boot();                                     /* user presets */
#endif
}

static int project_used(uint32_t slot)
{
    return proj_import(&proj_scratch, &proj_slot[slot & 3u], sizeof(project_store_t));
}

/* Main loop only: no flash access or copies when the ISR changes rows. */
/* PATTERN retains instruments/globals and refuses mismatched engines. FULL
 * additionally stages sounds and FX; clock, tempo and MIDI routing stay live.
 * Latest valid request replaces the earlier one. Stop cancels pending requests. */
static void project_sound_prepare(const project_t *p)
{
    for (uint32_t i = 0; i < G_COUNT; i++)
        live_sound.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t e = p->t[k].engine;
        live_sound.lfo2[k] = p->lfo2[k];
        for (uint32_t j = 0; j < P_COUNT; j++)
            live_sound.p[k][j] = (int16_t)param_fit(param_desc_of(e, j), p->t[k].p[j]);
        live_sound.preset[k] = (uint8_t)(ENGINES[e]->npresets ?
            p->t[k].preset % ENGINES[e]->npresets : 0u);

    }
}
static uint32_t project_pattern_queue(uint32_t slot)
{
    uint32_t k, j, full = (ui_prefs2 & (PREF_FULL_LOAD >> 16)) != 0u;
    project_t *p = &proj_scratch;
    if (slot >= 4u || !proj_import(p, &proj_slot[slot], sizeof(project_store_t))) return 1;
    if (0u || 0u) return 2;
    if (!song.playing) return 4;
    if (full && project_compat_flags(p)) return 5; /* stop, then confirm a test load */
    proj_bound(p);
    for (k = 0; k < NTRK; k++)
        if (!full && p->t[k].engine != trk[k].eng_req) return 3;
    /* Withdraw before writing the shared buffer; the ISR never sees a partial copy. */
    fm1_irq_off();
    LAUNCH_STATE.live_pending = 0;
    fm1_irq_on();
    if (full) project_sound_prepare(p);
    LAUNCH_PATTERN.motion = p->motion;
    motion_fx_compact_off(&LAUNCH_PATTERN.motion, (1u << NTRK) - 1u);
    for (k = 0; k < NTRK; k++) {
        LAUNCH_STATE.live_engine[k] = p->t[k].engine;
        memcpy(LAUNCH_PATTERN.step[k], p->t[k].step, sizeof p->t[k].step);
        proj_steps(LAUNCH_PATTERN.step[k]);
        LAUNCH_PATTERN.qnt[k] = p->t[k].p[P_SQNT] != 0;
        for (j = 0; j < 4u; j++)
            LAUNCH_PATTERN.timing[k][j] = (int16_t)clamp(p->t[k].p[P_SLEN + j],
                TP[P_SLEN + j].min, TP[P_SLEN + j].max);
    }
    fm1_irq_off();
    if (!song.playing) { fm1_irq_on(); return 4; }
    LAUNCH_STATE.live_full = (uint8_t)full;
    RING_PUBLISH();
    LAUNCH_STATE.live_pending = (uint8_t)(slot + 1u);
    fm1_irq_on();
    return 0;
}


static void settings_poll(void)
{
    project_save_poll();                               /* queued A-D snapshots: commit only after STOP */
#if FELUCCA_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || transport_busy() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

/* The autosave (1.2, Discussion #130): the music as it is (what a project holds: every track's sound, steps, LEN DIV
 * SWING GATE, mix, the song, the automation and locks, the FM6 patches, the name) kept in its own storage object
 * (OBJ_AUTOSAVE, A/B at 0xE5000 / 0xE6000, never one of the four project slots), and put back at power-on
 * (autosave_boot) with MENU > SYSTEM > RESTORE LAST ON (the default). Flash wear: a 4 KiB erase per write, the two
 * sectors taking turns, so a write only when
 *   - something a project holds changed since the last one (autosave_sig: a hash of those values, no copy; not the
 *     selected track, nor the PROJECT page's own controls),
 *   - and it has stayed the same for AS_IDLE_MS (10 s) with the transport stopped, no key or button down, no voice
 *     sounding and no editor transfer (a backup or a sample: autosave_hold); an erase stops the audio for its
 *     ~40 ms, as every save does, so it never happens while anything plays,
 *   - and at least AS_GAP_MS (60 s) after the one before (AS_RETRY_MS after a failed write).
 * So at most one write a minute while editing, none while playing or idle: about 100,000 erase cycles per sector
 * give over 200,000 writes, a write a minute for 8 hours a day for over a year, far more with real pauses.
 * RESTORE LAST OFF writes none (and restores none). Never after a crash, a hang or a failed boot (main.c): the music
 * that was playing then could be what crashed. Checked every AS_POLL_MS; the main loop only (never the audio ISR) */
#define AS_POLL_MS 250u
#define AS_IDLE_MS 10000u
#define AS_GAP_MS 60000u
#define AS_RETRY_MS 30000u
static struct {
    uint32_t saved, seen;                        /* the signature written last (or loaded at power-on), seen last */
    uint32_t t, last, poll;                      /* fm1_ms: seen changed / busy, the last write, the last check */
    uint32_t writes;                             /* (written since power-on: the host tests count them) */
    uint8_t wrote, err;                          /* a write since power-on (last is valid); the last one failed */
} as __attribute__((section(".pool")));

static uint32_t as_mix(uint32_t h, const void *p, uint32_t n)   /* FNV-1a, continued */
{
    const uint8_t *b = p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}

/* what an autosave would hold, hashed (no copy, no RAM): project_capture's values */
static uint32_t autosave_sig(void)
{
    uint32_t h = 2166136261u, i, j;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        for (j = 0; j < P_COUNT; j++) {
            int16_t v = motion_base_value(t, j);
            h = as_mix(h, &v, sizeof v);
        }
        h = as_mix(h, &t->eng_req, 1);
        h = as_mix(h, &t->preset, 1);
        h = as_mix(h, t->step, sizeof t->step);
        h = as_mix(h, &lfo2_config[i], sizeof lfo2_config[i]);
    }
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_NAME && i != G_LOAD && i != G_SAVE)
            h = as_mix(h, &song.g[i], sizeof song.g[i]);
#if CARAVEL_AFTERBURNER
    h = as_mix(h, &afterburner, sizeof afterburner);
#endif
#if CARAVEL_STASIS_EXPAND
    h = as_mix(h, &stasis_tone, sizeof stasis_tone);
#endif
    h = as_mix(h, song.mch, sizeof song.mch);           /* (1.5.5: MIDI IN TRACK's channels, the project's) */
    h = as_mix(h, &motion, sizeof motion);
    h = as_mix(h, harmony_root, sizeof harmony_root);    /* (round 7: FUN11 puts HARMONY / LINK in the project; the transient bar and SHIFT stay out) */
    h = as_mix(h, harmony_dur, sizeof harmony_dur);
    h = as_mix(h, link_src, sizeof link_src);
    h = as_mix(h, link_retrig, sizeof link_retrig);
    h = as_mix(h, harmony_follow, sizeof harmony_follow);
    h = as_mix(h, &harmony_on, sizeof harmony_on);
    return as_mix(h, proj_name, sizeof proj_name);
}

/* nothing going on that an erase (the audio stopped ~40 ms) would be heard in, or would race */
static int autosave_quiet(void)
{
    uint32_t k, i;
    if (transport_busy() || fm1_in.notes || fm1_in.buttons)
        return 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 0;
    return 1;
}

static void autosave_hold(void) { as.t = fm1_ms; }   /* the editor's transfers: their staging RAM is proj_wire */

/* main loop, every pass (after settings_poll) */
static void autosave_poll(void)
{
#if FELUCCA_HOME_PERF
    hp_autosave_poll(); return;
#endif
    if (project_compat_state.hold_autosave) return;
    if (f2_old & F2_OLD_AUTO) return;                 /* (FORMAT 2: an old autosave stays until the notice is answered) */

#if FELUCCA_FLASH
    uint32_t sig;
    if (!flash_ok || fm1_ms - as.poll < AS_POLL_MS)
        return;
    as.poll = fm1_ms;
    sig = autosave_sig();
    if (sig != as.seen || !autosave_quiet()) {
        as.seen = sig;
        as.t = fm1_ms;
        return;
    }
    if (sig == as.saved || (ui_prefs & PREF_RESTORE_OFF) || fm1_ms - as.t < AS_IDLE_MS ||
        (as.wrote && fm1_ms - as.last < (as.err ? AS_RETRY_MS : AS_GAP_MS)))
        return;
    as.last = fm1_ms;
    as.wrote = 1;
    project_capture(&proj_scratch);
    proj_wire_gen++;
    as.err = !proj_pack(&proj_wire, &proj_scratch) || st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) != 0;
    if (!as.err) {
        as.saved = sig;
        as.writes++;
    }
#endif
}

/* power-on, after the defaults (main.c): the autosave becomes the music when `allowed` (no crash, hang or failed boot
 * before) and RESTORE LAST is ON; an autosave that does not load (none, torn, damaged, another format) is ignored.
 * The music now (restored or not) is what is saved: nothing is written until it changes. 1 = restored */
static void f2_boot(void);                      /* ui_f2.c */
static int autosave_boot(int allowed)
{
    f2_boot();                                  /* (once: what an older version left, before anything is read or written) */
#if FELUCCA_HOME_PERF
    if(hp_autosave_boot(allowed)) return 1;
    if (ui.confirm == CF_COMPAT_PROJECT) return 0;
#endif

    int rc = 0;
#if FELUCCA_FLASH
    if (flash_ok && allowed && !(ui_prefs & PREF_RESTORE_OFF)) {
        int n;
        proj_wire_gen++;
        n = st_load(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire);
        if (n > 0 && proj_import(&proj_scratch, &proj_wire, n)) {
            if (project_compat_request(&proj_scratch, 4)) return 0;
            proj_bound(&proj_scratch);
            if (!project_restore_runtime(&proj_scratch)) {
#if FELUCCA_HOME_PERF
                for(uint32_t t=0;t<NTRK;t++)hp_apply(t,hp_default);
#endif
                ui_message("RESTORED");
                rc = 1;
            }
        }
    }
#else
    (void)allowed;
#endif
    as.saved = as.seen = autosave_sig();
    as.t = fm1_ms;
    return rc;
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_store_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
#endif /* PROJ_HOST */


