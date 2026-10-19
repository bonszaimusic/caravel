/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sparse, bounded step automation. Main-loop writes publish under the same
 * interrupt guard as parameter batches. The ISR only scans MOTION_MAX fixed records (128 since 1.2, FUN10; 64 before).
 * p[] is the sounding value; motion_base_value() is the patch's saved value.
 * Two kinds of record share the MOTION_MAX (core.h MOTION_LOCK): an automation event sets its value at its step and the value
 * holds (to the next event, the loop's restart, a stop); a parameter lock (1.1, Discussions #75 / #53: hold a step,
 * turn a knob) sets its value at its step only when the step plays (its chance passed) and the next step puts back
 * what would sound without it: the latest automation event of the pass up to that step, else the sound's own value
 * (motion_unlock). A lock lasts the whole step, its RATCH parts too; swing only moves the step's start. */
static motion_store_t motion;
static int16_t motion_base[NTRK][P_COUNT];
static uint32_t motion_active[NTRK][(P_COUNT + 31u) / 32u];
static uint32_t motion_locked[NTRK][(P_COUNT + 31u) / 32u];   /* the ids a lock holds now (put back at the next step) */
static uint8_t motion_base_valid, motion_full;
static uint16_t motion_fx_recording;
static uint8_t motion_fx_owner[PF_NFX]; /* capture owner + 1; releases follow the original track */
static uint16_t motion_fx_next[NTRK];   /* next phase worth scanning; 256 = no event in this step */
static __attribute__((noinline)) void motion_fx_close(void);
static void motion_fx_snapshot(motion_store_t *m, uint32_t tracks);
static void motion_fx_refresh(track_t *t);
_Static_assert(MOTION_FX_FIRST + PF_NFX == 128u, "FX schema is fourteen stable effects");
static uint32_t motion_fx_reserved(void)
{
    uint32_t n = 0;
    for (uint32_t e = 0; e < PF_NFX; e++) n += motion_fx_owner[e] != 0u;
    return n;
}
static void motion_fx_meta(motion_store_t *m)
{
    m->rsv[0] &= 0x30u; /* bits 4..5: project global FX track, 0 = T1 */
    m->rsv[1] = 0;
    for (uint32_t i = 0; i < m->count; i++)
        if (MOTION_IS_FX(&m->event[i])) {
            m->rsv[0] |= (uint8_t)(1u << (m->event[i].place >> 6));
            m->rsv[1] = 0xF1u;
        }
}
static uint32_t motion_fx_track(void) { return (motion.rsv[0] >> 4) & 3u; }
static uint32_t motion_guard(void)
{
#if defined(FM1_IRQ_TARGET)
    uint32_t f = fm1_icfg();
    fm1_irq_off();
    return f;
#else
    return 0;
#endif
}
static void motion_unguard(uint32_t f)
{
#if defined(FM1_IRQ_TARGET)
    fm1_icfg_set(f);
#else
    (void)f;
#endif
}
/* OFF is idempotent; ON also updates master-buffer priority and must survive.
 * Keep the first release (it may end a hold from the preceding loop), then
 * remove further releases until the same track/effect has another ON.
 * Sort only an index: imported/undo stores need not be ordered across steps,
 * and simultaneous ONs must retain their original press order. */
static __attribute__((noinline)) uint32_t motion_fx_compact_off(motion_store_t *m, uint32_t tracks)
{
    uint8_t order[MOTION_MAX];
    uint32_t drop[(MOTION_MAX + 31u) / 32u] = {0};
    uint16_t off[NTRK] = {0};
    uint32_t f = motion_guard(), n = 0, removed = 0;
    for (uint32_t i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        if (!MOTION_IS_FX(e) || !(tracks & (1u << (e->place >> 6)))) continue;
        uint32_t at = (uint32_t)e->place << 8 | (uint8_t)e->value, p = n;
        while (p) {
            const motion_event_t *v = &m->event[order[p - 1u]];
            if (((uint32_t)v->place << 8 | (uint8_t)v->value) <= at) break;
            order[p] = order[p - 1u];
            p--;
        }
        order[p] = (uint8_t)i;
        n++;
    }
    for (uint32_t p = 0; p < n; p++) {
        uint32_t i = order[p];
        const motion_event_t *e = &m->event[i];
        uint32_t k = e->place >> 6, bit = PF_BIT(MOTION_ID(e) - MOTION_FX_FIRST);
        if (!(e->param & MOTION_LOCK)) off[k] &= (uint16_t)~bit;
        else if (off[k] & bit) { drop[i / 32u] |= 1u << (i % 32u); removed++; }
        else off[k] |= (uint16_t)bit;
    }
    if (removed) {
        n = 0;
        for (uint32_t i = 0; i < m->count; i++)
            if (!(drop[i / 32u] & (1u << (i % 32u)))) m->event[n++] = m->event[i];
        memset(m->event + n, 0, (m->count - n) * sizeof m->event[0]);
        m->count = (uint8_t)n;
        motion_fx_meta(m);
    }
    motion_unguard(f);
    return removed;
}
static int motion_param(uint32_t id)
{
    /* Sound only: transport, routing, voice allocation and discrete engine
     * changes never become automation. FX sends and continuous mix are safe. */
    return id < P_COUNT && (id <= P_REL || (id >= P_ED_FLT && id <= P_LD_AMP) ||
        (id >= P_DIST && id <= P_REV) || id == P_GLIDE || id == P_PAN ||
        id == P_DETUNE || (id >= P_FM1_ATK && id <= P_FM4_LEVEL) || (id >= P_LN0 && id <= P_LN7) ||
        id == P_MORPH || id == P_PD_FB || id == P_SPRD || (id >= P_ITYPE && id <= P_IMIX) || id == P_FILT || id >= P_E0);   /* (not the chord keys, nor 1.2's LFO SYNC TRIG
                                        * POL and QUANTIZE, nor 1.5's filter TYPE and ENV SYNC: settings; the DRUM lane
                                        * levels, SPREAD (#148: continuous, as PAN), 1.5's INSERT (TYPE too: a lock
                                        * switches it for a step, fx.c track_insert fades over), the track's FILTER (0.3.0: a sweep, glided in fx.c track_filter), E0..E7) */
}
static int motion_valid(const motion_store_t *m)
{
    uint32_t i, j;
    if (m->count > MOTION_MAX || (m->on & ~((1u << NTRK) - 1u))) return 0;
    for (i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        if (MOTION_IS_FX(e)) {
            if (MOTION_ID(e) - MOTION_FX_FIRST >= PF_NFX || m->rsv[1] != 0xF1u ||
                !(m->rsv[0] & (1u << (e->place >> 6)))) return 0;
        } else if (!motion_param(MOTION_ID(e)) || e->value < -64 || e->value > 127) return 0;
        for (j = 0; j < i; j++)
            if (MOTION_IS_FX(e) && MOTION_IS_FX(&m->event[j]) && m->event[j].place == e->place &&
                (uint8_t)m->event[j].value > (uint8_t)e->value) return 0;
        for (j = 0; j < i; j++)
            if (m->event[j].place == e->place && MOTION_ID(&m->event[j]) == MOTION_ID(e) &&
                (!MOTION_IS_FX(e) || (m->event[j].param == e->param && m->event[j].value == e->value))) return 0;
    }
    return 1;
}
static int motion_enabled(const track_t *t) { return (motion.on >> trk_index(t)) & 1u; }
static uint32_t motion_count(const track_t *t)
{
    uint32_t i, n = 0, k = trk_index(t);
    for (i = 0; i < motion.count; i++) n += (motion.event[i].place >> 6) == k;
    return n;
}
/* track t's parameter locks (of motion_count) */
static uint32_t motion_lock_count(const track_t *t)
{
    uint32_t i, n = 0, k = trk_index(t);
    for (i = 0; i < motion.count; i++) n += (motion.event[i].place >> 6) == k && (motion.event[i].param & MOTION_LOCK) && !MOTION_IS_FX(&motion.event[i]);
    return n;
}
/* the steps of track k that hold a lock, bit s = step s (the roll and the grid mark them) */
static uint64_t motion_lock_steps(uint32_t k)
{
    uint32_t i;
    uint64_t m = 0;
    for (i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) == k && (motion.event[i].param & MOTION_LOCK) && !MOTION_IS_FX(&motion.event[i]))
            m |= (uint64_t)1 << (motion.event[i].place & 63u);
    return m;
}
/* the lock of track t on step / id: 1 and its value in *v, 0 = none */
static int motion_lock_get(const track_t *t, uint32_t step, uint32_t id, int16_t *v)
{
    uint32_t i, place = trk_index(t) << 6 | step;
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == place && motion.event[i].param == (id | MOTION_LOCK)) {
            *v = motion.event[i].value;
            return 1;
        }
    return 0;
}
/* the parameters track k's MOTION will change (PLAY ON, at least one event): a bit per P_* id in m[].
 * Main loop only (the ISR never writes the events); at most MOTION_MAX records, once per card redraw. */
static uint32_t motion_mask(uint32_t k, uint32_t m[(P_COUNT + 31u) / 32u])
{
    uint32_t i, any = 0;
    memset(m, 0, ((P_COUNT + 31u) / 32u) * sizeof m[0]);
    if (k >= NTRK || !((motion.on >> k) & 1u)) return 0;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if ((e->place >> 6) != k || MOTION_ID(e) >= P_COUNT) continue;
        m[MOTION_ID(e) / 32u] |= 1u << (MOTION_ID(e) % 32u);
        any = 1;
    }
    return any;
}
static int16_t motion_base_value(const track_t *t, uint32_t id)
{
    uint32_t k = trk_index(t);
    return (motion_active[k][id / 32u] >> (id % 32u)) & 1u ? motion_base[k][id] : t->p[id];
}
static void motion_restore(track_t *t)
{
    uint32_t k = trk_index(t), id, f = motion_guard();
    for (id = 0; id < P_COUNT; id++)
        if ((motion_active[k][id / 32u] >> (id % 32u)) & 1u) t->p[id] = motion_base[k][id];
    memset(motion_active[k], 0, sizeof motion_active[k]);
    memset(motion_locked[k], 0, sizeof motion_locked[k]);
    motion_unguard(f);
}
static void motion_fx_reset(track_t *t)
{
    uint32_t k = trk_index(t);
    perf_motion[k] = 0;
    motion_fx_next[k] = 256u;
}
static void motion_rebase(track_t *t)
{
    uint32_t k = trk_index(t), f = motion_guard();
    motion_restore(t);
    motion_fx_reset(t);
    memcpy(motion_base[k], t->p, sizeof t->p);
    motion_base_valid = (uint8_t)((motion_base_valid & ~(1u << k)) | (song.playing ? 1u << k : 0u));
    motion_unguard(f);
}
static void motion_begin(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        motion_restore(&trk[k]);
        motion_fx_reset(&trk[k]);
        memcpy(motion_base[k], trk[k].p, sizeof trk[k].p);
    }
    motion_base_valid = (1u << NTRK) - 1u;
}
static void motion_end(void)
{
    for (uint32_t k = 0; k < NTRK; k++) { motion_restore(&trk[k]); motion_fx_reset(&trk[k]); }
    motion_base_valid = 0;
}
static void motion_set_enabled(track_t *t, uint32_t on)
{
    uint32_t f = motion_guard(), b = 1u << trk_index(t);
    if (!on)
        for (uint32_t e = 0; e < PF_NFX; e++)
            if (motion_fx_owner[e] == trk_index(t) + 1u) motion_fx_capture(e, 0);
    motion.on = (uint8_t)(on ? motion.on | b : motion.on & ~b);
    if (!on) { motion_restore(t); motion_fx_reset(t); }
    motion_unguard(f);
}
static void motion_clear(track_t *t)
{
    uint32_t f = motion_guard(), k = trk_index(t), i, n = 0;
    motion_restore(t);
    for (i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) != k) motion.event[n++] = motion.event[i];
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_fx_meta(&motion);
    motion.on &= (uint8_t)~(1u << k);
    motion_rebase(t);
    motion_full = 0;
    for (uint32_t e = 0; e < PF_NFX; e++)
        if (motion_fx_owner[e] == k + 1u) { motion_fx_owner[e] = 0; motion_fx_recording &= (uint16_t)~PF_BIT(e); }
    motion_unguard(f);
}
static void motion_reset(track_t *t) { motion_clear(t); }
/* a record of kind 0 (automation) or MOTION_LOCK on track t's step / id: a new one, or the one there (of either kind)
 * becomes this. 0 ok, 1 invalid, 2 the MOTION_MAX are used (motion_full: "AUTOMATION FULL") */
static int motion_put(track_t *t, uint32_t step, uint32_t id, int16_t value, uint32_t kind)
{
    uint32_t k = trk_index(t), i, f;
    const param_desc_t *d;
    if (k >= NTRK || step >= NSTEP || !motion_param(id)) return 1;
    d = param_desc_of(eng_idx(t->eng_req), id);
    if (value < d->min || value > d->max || value < -64 || value > 127) return 1;
    value = (int16_t)param_fit(d, value);                    /* (a retired KIT: the kit it plays) */
    f = motion_guard();
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == (k << 6 | step) && MOTION_ID(&motion.event[i]) == id) break;
    if (i == motion.count && i + motion_fx_reserved() >= MOTION_MAX) { motion_full = 1; motion_unguard(f); return 2; }
    motion.event[i].place = (uint8_t)(k << 6 | step);
    motion.event[i].param = (uint8_t)(id | kind);
    motion.event[i].value = (int8_t)value;
    RING_PUBLISH();
    if (i == motion.count) motion.count++;
    motion.on |= (uint8_t)(1u << k);
    motion_unguard(f);
    return 0;
}
static int motion_set_event(track_t *t, uint32_t step, uint32_t id, int16_t value) { return motion_put(t, step, id, value, 0); }
static int motion_set_lock(track_t *t, uint32_t step, uint32_t id, int16_t value)
{
    return motion_put(t, step, id, value, MOTION_LOCK);
}
static void motion_delete_event(track_t *t, uint32_t step, uint32_t id)   /* (either kind) */
{
    uint32_t f = motion_guard(), place = trk_index(t) << 6 | step, i, n = 0;
    motion_restore(t);
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place != place || MOTION_ID(&motion.event[i]) != id) motion.event[n++] = motion.event[i];
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_fx_meta(&motion);
    motion_unguard(f);
}
/* the locks of track t's step (NSTEP and up: of every step); their automation events stay. A lock sounding now goes
 * back at the next step as always (motion_locked). The locks it removed */
static uint32_t motion_clear_locks(track_t *t, uint32_t step)
{
    uint32_t f = motion_guard(), k = trk_index(t), i, n = 0, gone;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if ((e->place >> 6) == k && (e->param & MOTION_LOCK) && !MOTION_IS_FX(e) && (step >= NSTEP || (e->place & 63u) == step)) continue;
        motion.event[n++] = *e;
    }
    gone = motion.count - n;
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_fx_meta(&motion);
    motion_unguard(f);
    return gone;
}
static int motion_capture(track_t *t, uint32_t id, int16_t value)
{
    uint32_t k = trk_index(t), idx, len, period, f;
    if (!motion_param(id)) return 0;
    if (!rec_on(t) || t != TSEL) {
        /* A live edit becomes a new base, even when an earlier motion value is
         * currently sounding. Subsequent recorded events can still override it. */
        f = motion_guard();
        if ((motion_base_valid >> k) & 1u) motion_base[k][id] = value;
        motion_unguard(f);
        return 0;
    }
    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    period = div_samples((uint32_t)t->p[P_SDIV]);
    f = motion_guard();
    idx = t->seq_idx % len;
    if (t->seq_pos < 0x7FFFFFFFu && t->seq_pos > step_samples(t, period, idx) / 2u) idx = (idx + 1u) % len;
    /* Mark the immediate knob value transient too, so a stop before its next
     * quantized step still restores the original patch. */
    if ((motion_base_valid >> k) & 1u) motion_active[k][id / 32u] |= 1u << (id % 32u);
    motion_unguard(f);
    return motion_set_event(t, idx, id, value);
}
/* step `step` starts: the locks of the step before go back to what sounds without them: the latest automation event
 * of track t's id at a step up to this one in this pass (motion_restore at step 0 starts each pass), else the sound's
 * own value */
static __attribute__((noinline)) void motion_unlock(track_t *t, uint32_t step, const motion_store_t *m)
{
    uint32_t k = trk_index(t), id, i;
    for (id = 0; id < P_COUNT; id++) {
        int32_t best = -1;
        int16_t v;
        if (!((motion_locked[k][id / 32u] >> (id % 32u)) & 1u))
            continue;
        v = motion_base[k][id];
        for (i = 0; i < m->count; i++) {
            const motion_event_t *e = &m->event[i];
            int32_t s = (int32_t)(e->place & 63u);
            if ((e->place >> 6) != k || e->param != id || s > (int32_t)step || s <= best)
                continue;                               /* (e->param == id: automation only, no MOTION_LOCK) */
            best = s;
            v = (int16_t)param_fit(param_desc_of(eng_idx(t->eng_req), id), e->value);
        }
        t->p[id] = v;
    }
    memset(motion_locked[k], 0, sizeof motion_locked[k]);
}
/* step `step` of track t starts (seq.c seq_tick); play: it plays (its chance passed): its locks apply */
static __attribute__((noinline)) void motion_step(track_t *t, uint32_t step, const motion_store_t *m, uint32_t play)
{
    uint32_t k = trk_index(t), i, w, any = 0;
    if (!((m->on >> k) & 1u)) return;
    if (!((motion_base_valid >> k) & 1u)) {
        memcpy(motion_base[k], t->p, sizeof t->p);
        motion_base_valid |= (uint8_t)(1u << k);
    }
    for (w = 0; w < (P_COUNT + 31u) / 32u; w++)
        any |= motion_locked[k][w];
    if (!step) motion_restore(t);
    else if (any) motion_unlock(t, step, m);
    for (i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        uint32_t id = MOTION_ID(e);
        if (MOTION_IS_FX(e) || e->place != (k << 6 | step) || ((e->param & MOTION_LOCK) && !play)) continue;
        const param_desc_t *d = param_desc_of(eng_idx(t->eng_req), id);
        t->p[id] = (int16_t)param_fit(d, e->value);
        motion_active[k][id / 32u] |= 1u << (id % 32u);
        if (e->param & MOTION_LOCK)
            motion_locked[k][id / 32u] |= 1u << (id % 32u);
    }
}
static void motion_snapshot_track(track_t *t, motion_store_t *out)
{
    uint32_t f = motion_guard(), k = trk_index(t), i;
    memset(out, 0, sizeof *out);
    out->on = motion.on & (uint8_t)(1u << k);
    for (i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) == k) out->event[out->count++] = motion.event[i];
    motion_fx_meta(out);
    /* Closed, edited timelines are copied byte-for-byte for undo/redo. Only
     * an unfinished live take needs a synthetic release and OFF cleanup. */
    for (uint32_t e = 0; e < PF_NFX; e++)
        if (motion_fx_owner[e] == k + 1u) { motion_fx_snapshot(out, 1u << k); break; }
    motion_unguard(f);
}
static int motion_replace_track(track_t *t, const motion_store_t *in)
{
    uint32_t k = trk_index(t), n = motion.count - motion_count(t), f, reserved = 0;
    for (uint32_t e = 0; e < PF_NFX; e++)
        reserved += motion_fx_owner[e] && motion_fx_owner[e] != k + 1u;
    if (!motion_valid(in) || n + in->count + reserved > MOTION_MAX) return 2;
    for (uint32_t i = 0; i < in->count; i++) if ((in->event[i].place >> 6) != k) return 1;
    f = motion_guard();
    motion_clear(t);
    memcpy(motion.event + motion.count, in->event, in->count * sizeof in->event[0]);
    motion.count += in->count;
    motion.on |= in->on & (uint8_t)(1u << k);
    motion_fx_meta(&motion);
    motion_unguard(f);
    return 0;
}

/* ---- 1.1.5: SEQ > AUTOMATION (ui_events.c ev_*): the records of a track as a list, edited one by one ---- */
/* the record of track t on step / id (either kind): its index, -1 none */
static int32_t motion_find(const track_t *t, uint32_t step, uint32_t id)
{
    uint32_t i, place = trk_index(t) << 6 | step;
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == place && MOTION_ID(&motion.event[i]) == id)
            return (int32_t)i;
    return -1;
}
/* the indices of track k's records in list order (by step, then as they were added: their place in motion.event,
 * which a record keeps when its parameter or value changes): n of them into idx[MOTION_MAX]. 1.5.5 (#199): up to 1.5.1
 * a step's records were by parameter, so turning SEQ > AUTOMATION's KNOB 3 (the parameter) past another record of the
 * step moved the row being edited above or below it, and the selection with it (the "jump") */
static uint32_t motion_rows(uint32_t k, uint8_t *idx)
{
    uint32_t i, j, n = 0;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        uint32_t key = e->place & 63u;
        if ((e->place >> 6) != k)
            continue;
        for (j = n; j > 0u; j--) {                       /* (insertion, stable: at most MOTION_MAX) */
            const motion_event_t *o = &motion.event[idx[j - 1u]];
            if ((uint32_t)(o->place & 63u) <= key)
                break;
            idx[j] = idx[j - 1u];
        }
        idx[j] = (uint8_t)i;
        n++;
    }
    return n;
}
/* record i of track t moves to step / id with value v (its kind kept): 0 ok, 1 invalid or another record is there.
 * What it set sounds until the next step (motion_restore puts back the sound's values, as a delete does) */
static int motion_move(track_t *t, uint32_t i, uint32_t step, uint32_t id, int16_t v)
{
    const param_desc_t *d;
    int32_t at;
    uint32_t f;
    if (i >= motion.count || (motion.event[i].place >> 6) != trk_index(t) || step >= NSTEP || !motion_param(id))
        return 1;
    at = motion_find(t, step, id);
    if (at >= 0 && (uint32_t)at != i)
        return 1;
    d = param_desc_of(eng_idx(t->eng_req), id);
    if (v < d->min || v > d->max || v < -64 || v > 127)
        return 1;
    f = motion_guard();
    if (motion.event[i].place != (trk_index(t) << 6 | step) || MOTION_ID(&motion.event[i]) != id)
        motion_restore(t);                              /* (a value alone: what sounds stays) */
    motion.event[i].place = (uint8_t)(trk_index(t) << 6 | step);
    motion.event[i].param = (uint8_t)((motion.event[i].param & MOTION_LOCK) | id);
    motion.event[i].value = (int8_t)param_fit(d, v);
    motion_unguard(f);
    return 0;
}
/* Engine-specific ids: EDIT E1..E8, MORPH/PHASE feedback, DIGITAL operators, DRUM lane levels. */
static int motion_engine_id(uint32_t id)
{
    return id < P_COUNT && (id == P_MORPH || id == P_PD_FB || (id >= P_E0 && id <= P_E7) || (id >= P_FM1_ATK && id <= P_FM4_LEVEL) || (id >= P_LN0 && id <= P_LN7));
}
/* a sound load put engine `to` where `from` was: when they differ, track t's records on the engine's own ids go (their
 * meaning changed); the ones on the common parameters (LEVEL, ENV, LFO, FILTER ENV, the sends, GLIDE, PAN, DETUNE)
 * stay, both kinds. The records it removed */
static uint32_t motion_sound_loaded(track_t *t, uint32_t from, uint32_t to)
{
    uint32_t f, k = trk_index(t), i, n = 0, gone;
    if (eng_idx(from) == eng_idx(to))
        return 0;
    f = motion_guard();
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if ((e->place >> 6) == k && motion_engine_id(MOTION_ID(e)))
            continue;
        motion.event[n++] = *e;
    }
    gone = motion.count - n;
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_fx_meta(&motion);
    motion_unguard(f);
    return gone;
}


/* FX uses the sequencer's unnudged, swung step clock. Unlike knob motion its
 * two edges are never rounded to the next step. Playback stays at control-block
 * resolution; stored time scales with tempo and external MIDI clock. */
static __attribute__((noinline)) int motion_fx_put_store(motion_store_t *m, track_t *t, uint32_t step, uint32_t e, uint32_t phase, int down)
{
    uint32_t k = trk_index(t), place = k << 6 | step, id = MOTION_FX_FIRST + e;
    uint32_t param = id | (down ? 0u : MOTION_LOCK), i, at, f = motion_guard();
    if (k >= NTRK || step >= NSTEP || e >= PF_NFX || phase > 255u) { motion_unguard(f); return 1; }
    if (!down) {
        for (i = 0; i < m->count; i++) {
            const motion_event_t *v = &m->event[i];
            if (v->place == place && v->param == id && (uint8_t)v->value == phase) {
                if (phase < 255u) phase++;
                else { place = k << 6 | ((step + 1u) % (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP)); phase = 0; }
                break;
            }
        }
    }
    for (i = 0; i < m->count; i++) {
        motion_event_t *v = &m->event[i];
        if (v->place == place && v->param == param && (uint8_t)v->value == phase) {
            motion_unguard(f); return 0;
        }
    }
    if (m->count + (down && m == &motion ? motion_fx_reserved() + 1u : 0u) >= MOTION_MAX) {
        motion_full = 1; motion_unguard(f); return 2;
    }
    at = m->count;
    for (i = 0; i < m->count; i++) {
        const motion_event_t *v = &m->event[i];
        if (MOTION_IS_FX(v) && (v->place > place || (v->place == place && (uint8_t)v->value > phase))) {
            at = i; break;
        }
    }
    for (i = m->count; i > at; i--) m->event[i] = m->event[i - 1u];
    m->event[at] = (motion_event_t){(uint8_t)place, (uint8_t)param, (int8_t)phase};
    RING_PUBLISH();
    m->count++;
    m->on |= (uint8_t)(1u << k);
    motion_fx_meta(m);
    motion_unguard(f);
    return 0;
}
static int motion_fx_put(track_t *t, uint32_t step, uint32_t e, uint32_t phase, int down)
{
    return motion_fx_put_store(&motion, t, step, e, phase, down);
}
static void motion_fx_position(track_t *t, uint32_t *step, uint32_t *phase)
{
    uint32_t length;
    if (t->seq_pos >= 0x7FFFFFFFu) { *step = *phase = 0; return; }
    *step = t->seq_idx % (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    length = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), *step);
    *phase = length ? t->seq_pos * 256u / length : 0u;
    if (*phase > 255u) *phase = 255u;
}
/* Main-loop FX editing: keep each edge independent. Equal-time edges of the
 * same effect are rejected (including ON/OFF: no zero-duration pair). A live
 * capture on this track must end first, so its reserved OFF and undo snapshot
 * cannot change underneath the edit. No records or metadata are added. */
static int motion_fx_delete_ok(const track_t *t, uint32_t i)
{
    uint32_t k = trk_index(t);
    if (i >= motion.count || !MOTION_IS_FX(&motion.event[i]) ||
        (motion.event[i].place >> 6) != k) return 1;
    for (uint32_t e = 0; e < PF_NFX; e++)
        if (motion_fx_owner[e] == k + 1u) return 2;
    return 0;
}
static int motion_fx_edit_ok(const track_t *t, uint32_t i, uint32_t step, uint32_t phase)
{
    int err = motion_fx_delete_ok(t, i);
    if (err) return err;
    if (step >= NSTEP || phase > 255u) return 1;
    uint32_t k = trk_index(t);
    uint32_t place = k << 6 | step, id = MOTION_ID(&motion.event[i]);
    for (uint32_t j = 0; j < motion.count; j++)
        if (j != i && motion.event[j].place == place && MOTION_ID(&motion.event[j]) == id &&
            (uint8_t)motion.event[j].value == phase) return 3;
    return 0;
}
/* Move only the selected record. Insertion preserves the order of all other
 * records and the original press order at equal times. Imported/undo stores
 * need not be ordered across steps: only FX within the destination step must
 * remain ordered by phase. Return the moved record's new index, or MOTION_MAX. */
static uint32_t motion_fx_move(track_t *t, uint32_t i, uint32_t step, uint32_t phase)
{
    uint32_t f = motion_guard();
    if (motion_fx_edit_ok(t, i, step, phase)) { motion_unguard(f); return MOTION_MAX; }
    motion_event_t e = motion.event[i];
    uint32_t place = trk_index(t) << 6 | step, lo = 0, hi = motion.count - 1u;
    for (uint32_t j = 0; j < motion.count; j++) {
        const motion_event_t *v = &motion.event[j];
        if (j == i || !MOTION_IS_FX(v) || v->place != place) continue;
        uint32_t at = j - (j > i), ph = (uint8_t)v->value;
        if (ph < phase || (ph == phase && j < i)) lo = at + 1u;
        else if (at < hi) hi = at;
    }
    uint32_t to = (uint32_t)clamp((int32_t)i, (int32_t)lo, (int32_t)hi);
    if (to < i) for (uint32_t j = i; j > to; j--) motion.event[j] = motion.event[j - 1u];
    else for (uint32_t j = i; j < to; j++) motion.event[j] = motion.event[j + 1u];
    e.place = (uint8_t)place;
    e.value = (int8_t)phase;
    motion.event[to] = e;
    motion_fx_refresh(t);
    motion_unguard(f);
    return to;
}
/* Re-chase at the current playhead after a main-loop edit/undo. Choose the last
 * edge per effect on the cyclic timeline, then restore recorded ON priority in
 * time/press order and schedule the next edge in this step. This never changes
 * notes, the transport, live hold/latch bits or the audio ISR's scan path. */
static void motion_fx_refresh(track_t *t)
{
    uint32_t k = trk_index(t), f = motion_guard(), step, phase;
    uint32_t live = perf_held | perf_latched;
    uint32_t live_best = perf_pick(live | perf_motion[k]);
    perf_motion[k] = 0;
    motion_fx_next[k] = 256u;
    if (!song.playing || t->seq_pos >= 0x7FFFFFFFu || k != motion_fx_track() ||
        !((motion.on & motion.rsv[0]) & (1u << k))) { motion_unguard(f); return; }
    motion_fx_position(t, &step, &phase);
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), cycle = len << 8, now = step << 8 | phase;
    uint16_t age[PF_NFX];
    uint8_t last[PF_NFX], order[PF_NFX];
    memset(last, MOTION_MAX, sizeof last);
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (!MOTION_IS_FX(e) || (e->place >> 6) != k || (e->place & 63u) >= len) continue;
        uint32_t fx = MOTION_ID(e) - MOTION_FX_FIRST, at = (e->place & 63u) << 8 | (uint8_t)e->value;
        uint32_t ago = (now + cycle - at) % cycle;
        if (last[fx] == MOTION_MAX || ago <= age[fx]) { last[fx] = (uint8_t)i; age[fx] = (uint16_t)ago; }
        if ((e->place & 63u) == step && (uint8_t)e->value > phase &&
            (uint8_t)e->value < motion_fx_next[k]) motion_fx_next[k] = (uint8_t)e->value;
    }
    uint32_t n = 0;
    for (uint32_t fx = 0; fx < PF_NFX; fx++) {
        if (last[fx] == MOTION_MAX || (motion.event[last[fx]].param & MOTION_LOCK)) continue;
        uint32_t p = n;
        while (p && (age[order[p - 1u]] < age[fx] ||
            (age[order[p - 1u]] == age[fx] && last[order[p - 1u]] > last[fx]))) {
            order[p] = order[p - 1u]; p--;
        }
        order[p] = (uint8_t)fx; n++;
    }
    for (uint32_t p = 0; p < n; p++) {
        uint32_t fx = order[p];
        perf_motion[k] |= PF_BIT(fx);
        if (!(live & PF_BIT(fx))) perf_ord[fx] = ++perf_seq;
    }
    /* A manual buffer effect that was on top stays on top when past recorded
     * edges are re-chased; the edit itself is not a new live key press. */
    if (live_best < PF_N && (live & PF_BIT(live_best))) perf_ord[live_best] = ++perf_seq;
    motion_unguard(f);
}
/* A save while a key is still down takes its duration up to SAVE. The live
 * recording continues independently; snapshots never retain an orphan hold. */
static void motion_fx_snapshot(motion_store_t *m, uint32_t tracks)
{
    for (uint32_t e = 0; e < PF_NFX; e++) {
        uint32_t owner = motion_fx_owner[e], step, phase;
        if (!owner || !(tracks & (1u << (owner - 1u)))) continue;
        motion_fx_position(&trk[owner - 1u], &step, &phase);
        (void)motion_fx_put_store(m, &trk[owner - 1u], step, e, phase, 0);
    }
    motion_fx_compact_off(m, tracks);
}
static __attribute__((noinline)) void motion_fx_capture(uint32_t e, int down)
{
    track_t *t;
    uint32_t k, step, phase;
    if (e >= PF_NFX) return; /* black-key mutes and knob macros remain live */
    if (down) {
        t = &trk[motion_fx_track()];
        if (!rec_on(t)) return;
    } else {
        if (!motion_fx_owner[e]) return;
        t = &trk[motion_fx_owner[e] - 1u];
    }
    k = trk_index(t);
    motion_fx_position(t, &step, &phase);
    if (!motion_fx_put(t, step, e, phase, down)) {
        motion_fx_owner[e] = down ? (uint8_t)(k + 1u) : 0u;
        if (down) motion_fx_recording |= (uint16_t)PF_BIT(e);
        else {
            motion_fx_recording &= (uint16_t)~PF_BIT(e);
            motion_fx_compact_off(&motion, 1u << k);
        }
    }
}
static __attribute__((noinline)) void motion_fx_close(void)
{
    for (uint32_t e = 0; e < PF_NFX; e++)
        if (motion_fx_owner[e]) motion_fx_capture(e, 0);
}
/* Changing ownership closes old captures and drops replay; notes and parameter
 * automation stay on their tracks. No data is moved between tracks. */
static void motion_fx_set_track(uint32_t k)
{
    uint32_t f = motion_guard();
    if (k < NTRK && k != motion_fx_track()) {
        motion_fx_close();
        motion.rsv[0] = (uint8_t)((motion.rsv[0] & ~0x30u) | k << 4);
        memset(perf_motion, 0, sizeof perf_motion);
        for (uint32_t i = 0; i < NTRK; i++) motion_fx_next[i] = 256u;
    }
    motion_unguard(f);
}
static __attribute__((noinline)) void motion_fx_sync(void)
{
    if (!motion_fx_recording) return;
    uint32_t live = perf_kill ? 0u : perf_held | perf_latched;
    for (uint32_t e = 0; e < PF_NFX; e++)
        if (motion_fx_owner[e] && !(live & PF_BIT(e))) motion_fx_capture(e, 0);
}
/* Next deadline avoids scanning 128 records on every control block. All
 * captures/imports keep FX order; simultaneous events keep press order. */
static __attribute__((noinline)) void motion_fx_run(track_t *t, const motion_store_t *m,
                                                  uint32_t from, uint32_t to)
{
    uint32_t k = trk_index(t), place = k << 6 | t->seq_idx, next = 256u;
    for (uint32_t i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        uint32_t phase, fx;
        if (!MOTION_IS_FX(e) || e->place != place) continue;
        phase = (uint8_t)e->value;
        if (phase > to) { if (phase < next) next = phase; continue; }
        if (phase < from) continue;
        fx = MOTION_ID(e) - MOTION_FX_FIRST;
        if (e->param & MOTION_LOCK) perf_motion[k] &= ~PF_BIT(fx);
        else { perf_motion[k] |= PF_BIT(fx); perf_ord[fx] = ++perf_seq; }
    }
    motion_fx_next[k] = (uint16_t)next;
}
static __attribute__((noinline)) void motion_fx_step(track_t *t, const motion_store_t *m)
{
    uint32_t k = trk_index(t);
    motion_fx_next[k] = 256u;
    if (!m || k != motion_fx_track() || !((m->on & m->rsv[0]) & (1u << k))) { perf_motion[k] = 0; return; }
    if (!t->seq_idx) {
        /* Chase the last edge of each FX: a hold across the loop remains a
         * hold; a recorded release before the end starts the next pass off. */
        uint16_t last[PF_NFX] = {0};
        perf_motion[k] = 0;
        for (uint32_t i = 0; i < m->count; i++) {
            const motion_event_t *e = &m->event[i];
            uint32_t fx, at;
            if (!MOTION_IS_FX(e) || (e->place >> 6) != k || (e->place & 63u) >= (uint32_t)t->p[P_SLEN]) continue;
            fx = MOTION_ID(e) - MOTION_FX_FIRST;
            at = ((e->place & 63u) << 8 | (uint8_t)e->value) + 1u;
            if (at < last[fx]) continue;
            last[fx] = (uint16_t)at;
            if (e->param & MOTION_LOCK) perf_motion[k] &= ~PF_BIT(fx);
            else { perf_motion[k] |= PF_BIT(fx); perf_ord[fx] = ++perf_seq; }
        }
    }
    motion_fx_run(t, m, 0, 0);
}
static __attribute__((noinline)) void motion_fx_tick(track_t *t, const motion_store_t *m, uint32_t length)
{
    uint32_t k = trk_index(t), to;
    if (!m || k != motion_fx_track() || !((m->on & m->rsv[0]) & (1u << k))) return;
    to = length ? t->seq_pos * 256u / length : 255u;
    if (to > 255u) to = 255u;
    if (motion_fx_next[k] <= to) motion_fx_run(t, m, motion_fx_next[k], to);
}
/* Temporal pairing, not take identity: ON takes the next OFF, OFF the previous
 * ON, in the active loop and at 1/256-step precision. Dormant rows beyond LEN
 * have no playback neighbour and are deleted alone. Preserve all other rows. */
static void motion_fx_delete(track_t *t, uint32_t selected)
{
    uint32_t f = motion_guard(), n = 0;
    if (motion_fx_delete_ok(t, selected)) { motion_unguard(f); return; }
    const motion_event_t e = motion.event[selected];
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), period = len << 8;
    uint32_t at = (e.place & 63u) << 8 | (uint8_t)e.value;
    uint32_t partner = MOTION_MAX, best = period;
    if ((e.place & 63u) < len) {
        for (uint32_t i = 0; i < motion.count; i++) {
            const motion_event_t *v = &motion.event[i];
            if ((v->place >> 6) != (e.place >> 6) || (v->place & 63u) >= len ||
                MOTION_ID(v) != MOTION_ID(&e) || !((v->param ^ e.param) & MOTION_LOCK)) continue;
            uint32_t other = (v->place & 63u) << 8 | (uint8_t)v->value;
            uint32_t distance = e.param & MOTION_LOCK ?
                (at + period - other) % period : (other + period - at) % period;
            if (distance && distance < best) { best = distance; partner = i; }
        }
    }
    for (uint32_t i = 0; i < motion.count; i++) {
        if (i != selected && i != partner) motion.event[n++] = motion.event[i];
    }
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_fx_meta(&motion);
    motion_fx_refresh(t);
    motion_unguard(f);
}
