/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 *
 * CARAVEL_UX_LAYERS: the two live layers of the 1.0.0 concept (docs/VORSCHLAEGE_1_0_0_GESAMTMATRIX_2026-11-15.md):
 *   BUILD  the pages as they are, plus a tab "BUILD" in the header and an overview on HOME (the four tracks);
 *   PLAY   one view of its own: seven AREAS (one per button), each with one or two GROUPS of four parameters; KNOB 1..4
 *          are the four arcs of the orbit. Only what is played live: everything else is BUILD (E40).
 * HOME held 1 s toggles BUILD and PLAY (nothing else may be held); GLO tapped twice opens the menu, from BUILD only.
 * Included by ui_draw.c after home_ui.c (it reads and sets parameters through the HOME performance model, hp_get/hp_set);
 * the input hooks are called from ui_input.c and ui_layer.c. Prototype for the browser emulator (FELUCCA_WEB). */
#if CARAVEL_UX_LAYERS
#ifndef CARAVEL_UX_EVOLVE
#define CARAVEL_UX_EVOLVE 0
#endif
#if CARAVEL_UX_EVOLVE && !(FELUCCA_RANDOM_MORPH && CARAVEL_ACTION_BANKS && FELUCCA_HOME_PERF)
#error "CARAVEL_UX_EVOLVE needs the HOME performance model, ACTION banks and RANDOM MORPH"
#endif
#define UXL_KFILT 0xF0u                 /* per-track FILTER */
#define UXL_KTONE 0xF1u                 /* separate global STASIS TONE */
static const param_desc_t UXL_FILT_DESC = {"FILTER", F_INT, -100, 100, 0, 0, ""};
#if CARAVEL_STASIS_EXPAND
static const param_desc_t UXL_STTONE_DESC = {"TONE", F_INT, 0, 127, 64, 0, ""};
#endif
static const param_desc_t UXL_FX_DESC = {"FX", F_INT, 0, 100, 0, 0, ""};
/* SPACE/FILTER: K1 edits this track's persisted P_FILT; K2..K4 are the live CRUSH/THROW/DEPTH macros (perform.c perf_k). */
static int32_t uxl_fx_get(uint32_t id) {
    if (!id) { int32_t v = tf_eff(TSEL); return v < 0 ? v * 100 / 64 : v * 100 / 63; }
    return id == 3u && !perf_harm_on() ? 100 - perf_k[3] : perf_k[id & 3u];
}
static void uxl_fx_turn(uint32_t id, uint32_t k, int32_t s)
{
    id &= 3u;
    if (id == 3u && !perf_harm_on())
        s = -s;
    int32_t delta = accel(EN_K1 + k, s, id ? 100 : 127);
    if (!id) tf_macro_turn(delta);
    else perf_k[id] = (int8_t)clamp((int32_t)perf_k[id] + delta, 0, 100);
}
static const char *uxl_fxm_label(uint32_t id) { return id == 0u ? "FILTER" : id == 1u ? "CRUSH" : id == 2u ? "THROW" : perf_harm_on() ? "SHIMR" : "DEPTH"; }
static void uxl_fx_text(uint32_t id, char *b, uint32_t n)
{
    int32_t v = uxl_fx_get(id);
    if (id == 0u) {
        if (v) { str_cpy(b, v < 0 ? "LP " : "HP ", n); fmt_int(b + 3, v < 0 ? -v : v); }
        else str_cpy(b, "OFF", n);
    } else if (id == 3u || v) {
        fmt_int(b, v);
        str_cpy(b + str_len(b), " %", n - str_len(b));
    } else {
        str_cpy(b, "OFF", n);
    }
}
enum { UXL_SPACE, UXL_HARM, UXL_SHAPE, UXL_MOVE, UXL_SOUND, UXL_MIX, UXL_ARP, UXL_N };
typedef struct { uint8_t kind; uint16_t id; uint8_t trk; const char *lab; } uxl_tg_t;   /* trk 255: the selected track */
typedef struct { const char *name; uint8_t n; uxl_tg_t t[4]; } uxl_gr_t;
typedef struct { const char *name; uint8_t btn, ng; uxl_gr_t g[5]; } uxl_area_t;   /* (MOVE has five: LFO and the EVOLVE verbs) */

#if CARAVEL_ELASTIC_P1
#define UXL_EVN 4
#else
#define UXL_EVN 3
#endif
#define UXL_T(k, i) {HP_TRACK, (i), 255, 0}
#define UXL_G(i) {HP_GLOBAL, (i), 255, 0}
static const uxl_area_t UXL_AREA[UXL_N] = {
    /* (E61: the delay and room settings are the FX pages in BUILD; two groups at most, E40) */
    /* (E74, E77: SPACE has SENDS (all four), FILTER (the FX layer's four macros FILTER CRUSH THROW DEPTH as arcs: they stay when FX is let
     * go) and STASIS (its three and the FILTER on K4, which would be empty); the delay's own settings are BUILD's) */
    {"SPACE", B_FX, 3, {{"SENDS", 4, {UXL_T(0, P_DIST), UXL_T(0, P_CHOR), UXL_T(0, P_DLY), UXL_T(0, P_REV)}},
                        {"FILTER", 4, {{UXL_KFILT, 0, 255, 0}, {UXL_KFILT, 1, 255, 0}, {UXL_KFILT, 2, 255, 0}, {UXL_KFILT, 3, 255, 0}}},
                        {"STASIS", 4, {UXL_G(G_DRREV), UXL_G(G_STAMT), UXL_G(G_STTIME),
#if CARAVEL_STASIS_EXPAND
                            {UXL_KTONE, 0, 255, 0}
#else
                            {UXL_KFILT, 0, 255, 0}
#endif
                        }}}},
    {"HARM", B_SCL, 1, {{"SCALE", 4, {UXL_T(0, P_ROOT), UXL_T(0, P_SCALE), UXL_T(0, P_TRANS), UXL_T(0, P_LATCH)}}}},
    {"SHAPE", B_ENV, 2, {{"ENV", 4, {UXL_T(0, P_ATK), UXL_T(0, P_DEC), UXL_T(0, P_SUS), UXL_T(0, P_REL)}},
                         {"MACRO", 4, {{HP_MACRO, 0, 255, 0}, {HP_MACRO, 1, 255, 0}, {HP_MACRO, 2, 255, 0}, {HP_MACRO, 3, 255, 0}}}}},
#if CARAVEL_UX_EVOLVE
    /* MOVE: A the LFO; B.. EVOLVE (E55, E62): one group per verb, RANDOM, JOURNEY, ELASTIC, PAIR; the arcs are what is changed while playing */
    {"MOVE", B_LFO, 1 + UXL_EVN, {{"LFO", 4, {UXL_T(0, P_LRATE), UXL_T(0, P_LD_PIT), UXL_T(0, P_LD_FLT), UXL_T(0, P_LD_AMP)}},
                        {"RANDOM", 0, {{0}}}, {"JOURNEY", 0, {{0}}},
#if CARAVEL_ELASTIC_P1
                        {"ELASTIC", 0, {{0}}},
#endif
                        {"PAIR", 0, {{0}}}}},
#else
    {"MOVE", B_LFO, 1, {{"LFO", 4, {UXL_T(0, P_LRATE), UXL_T(0, P_LD_PIT), UXL_T(0, P_LD_FLT), UXL_T(0, P_LD_AMP)}}}},
#endif
    {"SOUND", B_EDIT, 3, {{"KNOBS", 4, {{0}, {0}, {0}, {0}}}, {"EDIT", 4, {{0}, {0}, {0}, {0}}}, {"EDIT 2", 4, {{0}, {0}, {0}, {0}}}}},   /* A: the engine's four default knobs; B, C: its other EDIT slots (E50): uxl_group */
    {"MIX", B_GLO, 2, {{"LEVEL", 4, {{HP_TRACK, P_LEVEL, 0, "T1"}, {HP_TRACK, P_LEVEL, 1, "T2"}, {HP_TRACK, P_LEVEL, 2, "T3"}, {HP_TRACK, P_LEVEL, 3, "T4"}}},
                       {"TEMPO", 2, {UXL_G(G_BPM), UXL_G(G_SWING), {0}, {0}}}}},
    {"ARP", B_ARP, 2, {{"ARP", 4, {UXL_T(0, P_AMODE), UXL_T(0, P_ARATE), UXL_T(0, P_AOCT), UXL_T(0, P_AGATE)}},
                       {"MORE", 3, {UXL_T(0, P_ASWING), UXL_T(0, P_APROB), UXL_T(0, P_AHOLD), {0}}}}},
};
/* the display order of the areas' chips: the panel's (FX SEL ENV LFO EDIT GLO, then ARP) */

static struct {
    uint8_t play;                       /* 0 BUILD, 1 PLAY */
    uint8_t area, grp[UXL_N];           /* the area shown, the group last used in each */
    uint8_t hot_k, hot_t;               /* the arc just turned (its value in the middle), frames left */
    uint8_t glo_n;                      /* GLO taps in a row (BUILD): two open the menu */
    uint32_t glo_ms;                    /* .. when the last was let go */
    uint32_t sig;                       /* what the PLAY view drew last */
    uint32_t hint_until;                /* entry help A: five seconds, first input dismisses */
    uint32_t lvl_t;                     /* the LEVELS screen (PLAY, HOME tapped): when it opened */
    uint8_t lvl;                        /* .. it shows how to switch between PLAY and BUILD; any input closes it */
    uint8_t cap_view, cap_auto;         /* CAPTURE: its view while REC is held; frames of the view after an action */
    uint8_t rec_viewed, seq_fired;      /* the view was shown in this REC press (no capture on release); SEQ held 700 ms cleared the loop */
    uint8_t rec_knob_used;             /* K1/K2 REC gesture always consumes the release/tap */
    uint32_t rec_t, seq_t;              /* REC / SEQ pressed since (fm1_ms | 1) */
    uint8_t mom;                        /* MOMENTARY in PLAY: the arcs (bit k) turned with LFO held, to go back when it is let go */
    int16_t mom_v[4];                   /* .. their values before */
    hp_ref_t mom_r[4];                  /* .. and what they were (the area may change while LFO is held) */
    uint8_t pv;                         /* SAFE PREVIEW: PRESETS only chooses, OCT+ loads, OCT- cancels (E10) */
    uint16_t pv_idx;                    /* the sound chosen: an index into the PRESETS list */
    uint8_t ev;                         /* EVOLVE: the verb the keys under LFO act on (0 RANDOM, 1 JOURNEY, 2 ELASTIC, 3 PAIR): the last one shown */
    uint8_t pl_area, pl_grp;            /* the area and group PLAY was entered with (HOME held: BUILD follows when they changed) */
    uint8_t el_bg;                      /* ELASTIC: the tracks (bit t) whose springs run in the background: turns are plain again */
    uint8_t inter, uset;                /* INTERACT (BUILD): the mode is on; the tracks (bit t) that have a set of their own */
    uint8_t ev_a, ev_g, ev_init;        /* EVOLVE: the group last played, an arc turned (a ROW is this group's four targets) */
} uxl;

#define UXL_ON 1
#define UXL_PLAY (uxl.play)

static void layer_tap(uint32_t l);      /* ui_layer.c */
static void uxl_area_tap(uint32_t area);
static int uxl_free(void);
#if CARAVEL_UX_CAPTURE
static void uxl_cap_rec(void);
static void uxl_cap_loop(void);
static void uxl_cap_poll(void);
static uint32_t uxl_cap_get(uint32_t i, uint32_t what)    /* the snapshot of a track: 0 state, 1 full, 2 loop events, 3 live events */
{
    cap_view_t v;
    cap_view(i, &v);
    return what == 0u ? v.st : what == 1u ? v.full : what == 2u ? v.ln : v.rn;
}
#endif
static void action_stasis(uint32_t action);   /* action_control.c: HOLD ADD BLEND RELEASE */
static int page_tap(uint32_t b);        /* ui_input.c */

static const uint8_t UXL_SIN[91] = {0,4,9,13,18,22,27,31,36,40,44,49,53,58,62,66,71,75,79,83,88,92,96,100,104,108,112,116,120,124,128,132,136,139,143,147,150,154,158,161,165,168,171,175,178,181,184,187,190,193,196,199,202,204,207,210,212,215,217,219,222,224,226,228,230,232,234,236,237,239,241,242,243,245,246,247,248,249,250,251,252,253,254,254,255,255,255,255,255,255,255};
static int32_t uxl_sin(int32_t a)       /* Q8, a in degrees, any */
{
    a %= 360;
    if (a < 0) a += 360;
    if (a <= 90) return UXL_SIN[a] > 255 ? 256 : UXL_SIN[a];
    if (a <= 180) return UXL_SIN[180 - a] > 255 ? 256 : UXL_SIN[180 - a];
    if (a <= 270) return -(UXL_SIN[a - 180] > 255 ? 256 : UXL_SIN[a - 180]);
    return -(UXL_SIN[360 - a] > 255 ? 256 : UXL_SIN[360 - a]);
}

/* SOUND: the engine's EDIT slots that are not among its four default knobs (those are group A), in order, if there are any */
static uint32_t uxl_sound_rest(uint8_t out[8])
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t k, j, n = 0;
    for (k = 0; k < 8u; k++) {
        uint32_t id = (uint32_t)P_E0 + k;
        const param_desc_t *d;
        int skip = 0;
        for (j = 0; j < 4u; j++)
            if (e->knob[j] == id)
                skip = 1;
        if (skip)
            continue;
        d = track_desc(TSEL, id);
        if (!d || !d->label || !d->label[0] || (d->label[0] == '-' && !d->label[1]))
            continue;
        out[n++] = (uint8_t)id;
    }
    return n;
}
static uint32_t uxl_area_ng(uint32_t area)             /* how many groups the area has (SOUND: by the engine) */
{
    if (area == UXL_SOUND) {
        uint8_t r[8];
        uint32_t n = uxl_sound_rest(r);
        return n == 0u ? 1u : n <= 4u ? 2u : 3u;
    }
    return UXL_AREA[area].ng;
}
/* the group's targets (SOUND: the engine's four default knobs) */
static uint32_t uxl_group(uint32_t area, uint32_t g, uxl_tg_t out[4])
{
    const uxl_gr_t *gr = &UXL_AREA[area].g[g];
    uint32_t k;
    if (area == UXL_SOUND) {
        const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
        uint8_t r[8];
        uint32_t n = uxl_sound_rest(r), from = g ? (g - 1u) * 4u : 0u, m = 0;
        if (g == 0u) {
            for (k = 0; k < 4u; k++)
                out[k] = (uxl_tg_t){HP_TRACK, e->knob[k], 255, 0};
            return 4;
        }
        for (k = 0; k < 4u; k++)
            out[k] = from + k < n ? (uxl_tg_t){HP_TRACK, r[from + k], 255, 0} : (uxl_tg_t){0, 0, 0, 0};
        for (k = 0; k < 4u; k++)
            m += out[k].kind != 0u;
        return m;
    }
    for (k = 0; k < 4u; k++)
        out[k] = k < gr->n ? gr->t[k] : (uxl_tg_t){0, 0, 0, 0};
    return gr->n;
}
static hp_ref_t uxl_ref(const uxl_tg_t *t)
{
    hp_ref_t r = {0};
    r.kind = t->kind;
    r.id = t->id;
    r.track = t->trk == 255 ? song.sel : t->trk;
    r.engine = 255;
    return r;
}

/* F6 A: two short hints on entry, no help gesture on HOME.
 * Millisecond subtraction also handles wrap of fm1_ms. */
static uint32_t uxl_start_help(void)
{
    int32_t left=(int32_t)(uxl.hint_until-fm1_ms);
    if(!uxl.play || !uxl.hint_until || left<=0)return 0;
    return left>2500?1u:2u;
}

static uint32_t uxl_area_btn(void) { return UXL_AREA[uxl.area].btn; }
static uint32_t uxl_group_now(void) { return uxl.grp[uxl.area] < uxl_area_ng(uxl.area) ? uxl.grp[uxl.area] : 0u; }

#if CARAVEL_UX_EVOLVE
/* ------------------------------------------------------------------ EVOLVE ---
 * RANDOM, JOURNEY, ELASTIC and PAIR as groups B.. of MOVE (E55). The machinery is the existing ACTION banks' (action_control.c):
 * MOVE B..E show the verb's SETUP on the four arcs (action_setup_caption / action_setup_turn), and the verbs are the four keys
 * D5 E5 F5 G5 held with LFO (A1..A4, as under SEL for HARM). The targets of a morph are what PLAY shows: the HOME rows are
 * written from the PLAY groups (uxl_hp_sync), a ROW is the group last played, a TRACK all of its groups. */
static void action_key(uint32_t key);   /* action_control.c */
static const uint8_t UXL_EVB[UXL_EVN] = {0u, ACT_BANK_JOURNEY,
#if CARAVEL_ELASTIC_P1
    ACT_BANK_ELASTIC,
#endif
    2u};
#define UXL_EV_PAIR (UXL_EVN - 1u)
static const char *const UXL_EVNAME[UXL_EVN] = {"RANDOM", "JOURNEY",
#if CARAVEL_ELASTIC_P1
    "ELASTIC",
#endif
    "PAIR"};
static int uxl_ev_group(void) { return uxl.area == UXL_MOVE && uxl_group_now() >= 1u ? (int)uxl_group_now() - 1 : -1; }   /* the verb on show */
static void uxl_ev_loc(void)            /* the group last played: called when an arc of it is turned (not an EVOLVE group) */
{
    if (uxl_ev_group() < 0) {
        uxl.ev_a = uxl.area;
        uxl.ev_g = (uint8_t)uxl_group_now();
    }
}
/* the HOME rows of all tracks written from the PLAY groups: one row per group (MOVE: the LFO only), the active row of the
 * selected track is the group last played */
static void uxl_hp_sync(void)
{
    uint32_t t, a, g, k, r, keep = song.sel;
    for (t = 0; t < NTRK; t++) {
        hp_track_t *h = &hp.track[t];
        uint32_t act = 0;
        if (uxl.uset & (1u << t))                        /* a set made in INTERACT stays as it is */
            continue;
        song.sel = (uint8_t)t;
        r = 0;
        for (a = 0; a < UXL_N; a++) {
            uint32_t ng = a == UXL_MOVE ? 1u : a == UXL_MIX ? 0u : uxl_area_ng(a);   /* (MIX: the levels and the tempo are not morphed) */
            for (g = 0; g < ng; g++) {
                uxl_tg_t tg[4];
                uxl_group(a, g, tg);
                for (k = 0; k < 4u; k++) {
                    hp_ref_t x = {0};
                    if (tg[k].kind && tg[k].kind != UXL_KFILT && tg[k].kind != UXL_KTONE) {
                        x.kind = tg[k].kind;
                        x.id = tg[k].id;
                        x.track = tg[k].trk;
                        x.engine = 255;
                    }
                    h->cell[r][k] = x;
                }
                if (t == keep && a == uxl.ev_a && g == uxl.ev_g)
                    act = r;
                r++;
            }
        }
        h->count = (uint8_t)r;
        h->active = (uint8_t)act;
    }
    song.sel = (uint8_t)keep;
}
#if CARAVEL_ELASTIC_P1
/* ELASTIC in PLAY (E57): armed with A1, then every arc of every area whose parameter the springs allow is a spring, up to four
 * at a time (the one touched longest ago is let go for a fifth). No CELL or ROW: the springs are bound to the parameters, in a
 * HOME row of their own (UXL_EL_ROW) that nothing else writes. A1 again lets all go. */
#define UXL_EL_ROW 31u
static uint32_t uxl_el_age[NTRK][HP_ER_MAX];
static int uxl_el_slot_of(uint32_t t, uint32_t id)
{
    uint32_t k;
    for (k = 0; k < HP_ER_MAX; k++)
        if ((hp_er_track[t].valid & (1u << k)) && hp_er_track[t].id[k] == id)
            return (int)k;
    return -1;
}
static void uxl_el_arm_toggle(void)
{
    uint32_t t = song.sel;
    if (hp_el1.on) {
        hp_elastic_p1_clear_current();
        ui_message("ELASTIC OFF");
        ui.force = 1;
        return;
    }
    if (pair_member(&trk[t])) {
        ui_message("ELASTIC: NOT ON A PAIR TRACK");
        return;
    }
    hp_el_scope = 1;
    uxl.el_bg &= (uint8_t)~(1u << t);
    hp_er_reset(&hp_er, (uint8_t)t, (uint8_t)UXL_EL_ROW, trk[t].engine);
    memset(hp.track[t].cell[UXL_EL_ROW], 0, sizeof hp.track[t].cell[UXL_EL_ROW]);
    hp_el1.on = 1;
    hp_el1.track = (uint8_t)t;
    hp_el1.row = (uint8_t)UXL_EL_ROW;
    hp_el1.slot = 255u;
    hp_el1.id = 0u;
    hp_el1.engine = trk[t].engine;
    hp_el_pick = 0u;
    hp_el_row_held = 0u;
    hp_el_free_track[t] = 1u;
    hp_elastic_p1_publish();
    ui_message("ELASTIC ARMED: TURN ANY KNOB");
    ui.force = 1;
}
static void uxl_el_clamp(uint32_t t, uint32_t slot)    /* a spring inside its (new) range */
{
    hp_elastic_t *e = &hp_er_track[t].spring[slot];
    hp_et_t *m = &hp_el_row_time_track[t][slot];
    hp_elastic_cfg_t cf = hp_el_scfg[t][slot];
    if (!hp_elastic_limit(e, cf)) {
        hp_et_cancel(e, m);
    } else {
        e->x = hp_et_clamp_x(e, e->x, cf);
        m->start_q8 = hp_et_clamp_x(e, m->start_q8, cf);
        m->goal_q8 = hp_et_clamp_x(e, m->goal_q8, cf);
    }
}
static void uxl_el_stamp(uint32_t t, uint32_t slot)    /* a touch: the spring takes the settings as they are now (the brush) */
{
    uint8_t old_range = hp_el_scfg[t][slot].range;
    hp_el_stimes[t][slot].pull = hp_el_times_track[t].pull;
    hp_el_stimes[t][slot].back = hp_el_times_track[t].back;
    hp_el_stimes[t][slot].bounce = hp_el_times_track[t].bounce;
    hp_el_scfg[t][slot] = hp_el_cfg_track[t];
    if (hp_el_scfg[t][slot].range < old_range)
        uxl_el_clamp(t, slot);
}
static void uxl_el_bg_toggle(void)                /* A2: the springs into the background (they run on), turns are plain; again: turns pull again */
{
    uint32_t t = song.sel;
    if (!hp_el1.on || !hp_el_free_track[t]) {
        ui_message("ELASTIC: A1 FIRST");
        return;
    }
    uxl.el_bg ^= (uint8_t)(1u << t);
    ui_message(uxl.el_bg & (1u << t) ? "SPRINGS RUN ON: TURNS ARE PLAIN" : "SPRINGS ON: TURNS PULL");
    ui.force = 1;
}
static int uxl_el_turn(hp_ref_t r0, int32_t s)   /* an arc turned while armed: 1 if it was a spring */
{
    uint32_t t = song.sel, id, k;
    int slot;
    const param_desc_t *d;
    hp_ref_t v;
    if (!hp_el1.on || !hp_el_free_track[t] || r0.kind != HP_TRACK || (uxl.el_bg & (1u << t)))
        return 0;                                       /* (in the background: the arc sets its value as usual) */
    id = hp_tid(r0);
    v = hp_track_ref(t, id, 0);
    d = track_desc(&trk[t], id);
    if (id >= P_COUNT || !d || !hp_elastic_p1_ref_target(v, t))
        return 0;                                       /* not a spring parameter: the arc sets it as usual */
    slot = uxl_el_slot_of(t, id);
    if (slot < 0) {
        for (k = 0; k < HP_ER_MAX; k++)
            if (!(hp_er.valid & (1u << k))) {
                slot = (int)k;
                break;
            }
        if (slot < 0) {                                 /* all four busy: the one touched longest ago goes */
            slot = 0;
            for (k = 1; k < HP_ER_MAX; k++)
                if ((int32_t)(uxl_el_age[t][slot] - uxl_el_age[t][k]) > 0)
                    slot = (int)k;
            hp_er_unbind(&hp_er, (uint32_t)slot);
            hp_er_audio[t][slot] = 0u;
            hp_et_reset(&hp_el_row_time[slot]);
        }
        hp_rm_disengage(v);                             /* a spring owns this target now */
        hp.track[t].cell[UXL_EL_ROW][slot] = v;
        if (!hp_er_bind(&hp_er, (uint32_t)slot, (uint16_t)id, trk[t].p[id], d->min, d->max, fm1_ms))
            return 0;
        hp_et_reset(&hp_el_row_time[slot]);
        hp_el_stimes[t][slot] = hp_el_times_track[t];   /* a new spring starts with the track's settings, then has its own */
        hp_el_scfg[t][slot] = hp_el_cfg_track[t];
    }
    uxl_el_age[t][slot] = fm1_ms;
    uxl_el_stamp(t, (uint32_t)slot);                    /* (the spring takes the settings of the moment: PULL, RETURN, BOUNCE, RANGE) */
    if (!hp_el_row_held) {
        hp_et_touch(&hp_er.spring[slot], &hp_el_row_time[slot], s, fm1_ms, hp_el_stimes[t][slot], hp_el_scfg[t][slot]);
        hp_er.last = (uint8_t)slot;
    }
    hp_elastic_p1_publish();
    ui.force = 1;
    return 1;
}
/* the setup (E59, E63): page 1 is PULL RETURN RANGE BOUNCE as they are now: they stay when you change page, a touch of a parameter hands
 * them to its spring (each spring keeps its own until it is touched again), and a turn also acts live on the spring touched last.
 * Page 2 (A4 held) is for the track: COUPLE REFLECT SEED. */
static int uxl_el_cur(void)                     /* the spring touched last, -1: none */
{
    uint32_t t = song.sel;
    if (hp_el1.on && hp_el_free_track[t] && hp_er.last < HP_ER_MAX && (hp_er.valid & (1u << hp_er.last)))
        return (int)hp_er.last;
    return -1;
}
static void uxl_el_cap_ex(uint32_t k, char *name, char *value, int page2, int tmpl)
{
    uint32_t t = song.sel, keep_pg = hp_el_setup_page;
    hp_et_cfg_t keep_t = hp_el_times_track[t];
    hp_elastic_cfg_t keep_c = hp_el_cfg_track[t];
    int cur = tmpl ? -1 : uxl_el_cur();             /* BUILD template vs PLAY last-touched spring */
    /* BUILD edits the track-wide template; PLAY shows the last touched spring. */
    if (page2) {                                    /* the track's: COUPLE REFLECT SEED */
        static const uint8_t M[4] = {2, 1, 3, 255};
        if (M[k & 3u] == 255u) {
            str_cpy(name, "--", 16);
            str_cpy(value, "--", 24);
            return;
        }
        hp_el_setup_page = 1;
        hp_elastic_p1_setup_caption(M[k & 3u], name, value);
        hp_el_setup_page = (uint8_t)keep_pg;
        return;
    }
    if (cur >= 0) {                                 /* the spring's own values on show */
        hp_el_times_track[t] = hp_el_stimes[t][cur];
        hp_el_cfg_track[t] = hp_el_scfg[t][cur];
    }
    if (k == 3u) {
        hp_el_setup_page = 1;
        hp_elastic_p1_setup_caption(0, name, value);
    } else {
        hp_el_setup_page = 0;
        hp_elastic_p1_setup_caption(k + 1u, name, value);
    }
    hp_el_setup_page = (uint8_t)keep_pg;
    hp_el_times_track[t] = keep_t;
    hp_el_cfg_track[t] = keep_c;
}
static void uxl_el_cap(uint32_t k, char *name, char *value) { uxl_el_cap_ex(k, name, value, hp_el_setup_page & 1u, 0); }
/* Every ELASTIC value arc follows the *same* settings that its label displays.
 * BUILD uses the track's new-spring template; PLAY shows the last touched
 * spring when one is selected. Angles are limited to the 76-degree quadrant. */
static int32_t uxl_el_arc_frac(uint32_t k, int page2, int tmpl)
{
    const uint32_t t = song.sel;
    hp_et_cfg_t tm = hp_el_times_track[t];
    hp_elastic_cfg_t cf = hp_el_cfg_track[t];
    int cur = page2 || tmpl ? -1 : uxl_el_cur();
    if (cur >= 0) {
        tm = hp_el_stimes[t][cur];
        cf = hp_el_scfg[t][cur];
    }
    if (page2) {
        if (k == 0u) return clamp((int32_t)tm.couple, 0, 100) * 76 / 100;
        if (k == 1u) return clamp((int32_t)tm.reflect, 0, 100) * 76 / 100;
        if (k == 2u) return (int32_t)tm.seed * 76 / 255;
        return -1; /* K4 is not a parameter on SPRINGS. */
    }
    if (k == 0u) return clamp((int32_t)tm.pull, 0, 100) * 76 / 100;
    if (k == 1u) return clamp((int32_t)tm.back, 0, (int32_t)HP_ES_RETURN_INF) * 76 / (int32_t)HP_ES_RETURN_INF;
    if (k == 2u) return clamp((int32_t)cf.range, 0, 100) * 76 / 100;
    if (k == 3u) {
        /* The new clock-synced periods are discrete steps; legacy v4
         * numeric bounce values still have a meaningful visual range. */
        if (hp_es_is_sync(tm.bounce))
            return ((int32_t)hp_es_index(tm.bounce) - (int32_t)HP_ES_MIN_INDEX) * 76 /
                   (int32_t)(HP_ES_COUNT - HP_ES_MIN_INDEX - 1u);
        return clamp((int32_t)tm.bounce, 0, 1000) * 76 / 1000;
    }
    return -1;
}
static void uxl_el_setup_turn_ex(uint32_t k, int32_t s, int page2, int tmpl)
{
    uint32_t t = song.sel, keep_pg = hp_el_setup_page;
    int cur = tmpl ? -1 : uxl_el_cur();
    hp_et_cfg_t *tm;
    hp_elastic_cfg_t *cf;
    if (!s)
        return;
    if (page2) {
        static const uint8_t M[4] = {2, 1, 3, 255};
        if (M[k & 3u] != 255u) {
            hp_el_setup_page = 1;
            hp_elastic_p1_setup_turn(M[k & 3u], s);
            hp_el_setup_page = (uint8_t)keep_pg;
        }
        return;
    }
    tm = &hp_el_times_track[t];                     /* the settings of the moment: the next touch gives them to its spring */
    cf = &hp_el_cfg_track[t];
    if (k == 0u) {
        tm->pull = (uint8_t)clamp((int32_t)tm->pull + s, 0, 100);
    } else if (k == 1u) {
        tm->back = (uint8_t)clamp((int32_t)tm->back + s, 0, (int32_t)HP_ES_RETURN_INF);
        if (tm->back == HP_ES_RETURN_INF && !hp_es_is_sync(tm->bounce))
            tm->bounce = HP_ES_DEFAULT;             /* INF is a tempo gesture */
    } else if (k == 2u) {
        cf->range = (uint8_t)clamp((int32_t)cf->range + s, 0, 100);
    } else {
        tm->bounce = hp_es_step(tm->bounce, s);
    }
    if (cur >= 0) {                                 /* and live on the spring touched last */
        hp_el_stimes[t][cur].pull = tm->pull;
        hp_el_stimes[t][cur].back = tm->back;
        hp_el_stimes[t][cur].bounce = tm->bounce;
        if (k == 2u) {
            hp_el_scfg[t][cur] = *cf;
            uxl_el_clamp(t, (uint32_t)cur);
        }
    }
    hp_dirty = 1;
    if (cur >= 0)
        hp_elastic_p1_publish();
    ui.force = 1;
}
static void uxl_el_setup_turn(uint32_t k, int32_t s) { uxl_el_setup_turn_ex(k, s, hp_el_setup_page & 1u, 0); }
#endif
/* what stays on the arcs in PLAY (E60): only what is changed while playing; the rest is the setup in BUILD (the LFO family) */
static int uxl_ev_show(uint32_t b, uint32_t k)
{
    if (b == 0u)
        return k < 2u;                              /* RANDOM: BARS DEPTH */
    if (b == ACT_BANK_JOURNEY)
        return 0;                                   /* JOURNEY: the verbs only */
#if CARAVEL_ELASTIC_P1
    if (b == ACT_BANK_ELASTIC)
        return !(hp_el_setup_page & 1u) || k == 0u; /* a spring's four; page 2: COUPLE */
#endif
    return k == 0u;                                 /* PAIR: MORPH */
}
static void uxl_ev_cap(uint32_t b, uint32_t k, char *name, char *value)
{
    if (!uxl_ev_show(b, k)) {
        str_cpy(name, "--", 16);
        str_cpy(value, "--", 24);
        return;
    }
#if CARAVEL_ELASTIC_P1
    if (b == ACT_BANK_ELASTIC) {
        uxl_el_cap(k, name, value);
        return;
    }
#endif
    action_bank = (uint8_t)b;
    action_setup_caption(k, name, value);
}
static void uxl_ev_setup_turn(uint32_t b, uint32_t k, int32_t s)
{
    if (!uxl_ev_show(b, k))
        return;
#if CARAVEL_ELASTIC_P1
    if (b == ACT_BANK_ELASTIC) {
        uxl_el_setup_turn(k, s);
        return;
    }
#endif
    action_bank = (uint8_t)b;
    action_setup_turn(k, s);
}
static void uxl_ev_verb(uint32_t i)     /* A1..A4 of the verb shown last */
{
    uxl_hp_sync();
    action_bank = UXL_EVB[uxl.ev];
#if CARAVEL_ELASTIC_P1
    if (UXL_EVB[uxl.ev] == ACT_BANK_ELASTIC && i == 0u) {
        uxl_el_arm_toggle();
        return;
    }
    if (UXL_EVB[uxl.ev] == ACT_BANK_ELASTIC && i == 1u) {   /* (EXCITE is gone: E64) */
        uxl_el_bg_toggle();
        return;
    }
#endif
    action_key(action_keys[i]);
    ui.force = 1;
}
static void uxl_ev_a4(int is_long)      /* A4: a tap and a hold (RANDOM, JOURNEY, ELASTIC) */
{
    uint32_t b = UXL_EVB[uxl.ev];
    uxl_hp_sync();
    action_bank = (uint8_t)b;
    if (b == 0u || b == ACT_BANK_JOURNEY) {
        if (is_long) {
            hp_rm_snapshot_store(action_snap_slot);
        } else if (b == 0u) {
            action_snap_slot = (uint8_t)((action_snap_slot + 1u) % HP_RM_SNAP_SLOTS);
            hp_rm_snapshot_say(song.sel, action_snap_slot);
        } else {
            hp_j_set_wait((hp_j.wait + 1u) % 6u);
        }
    }
#if CARAVEL_ELASTIC_P1
    else if (b == ACT_BANK_ELASTIC) {
        if (is_long) {
            hp_elastic_p1_setup_page_toggle();
            ui_message("ELASTIC: SETUP PAGE");
        } else {
            hp_elastic_p1_action(3u);
        }
    }
#endif
    ui.force = 1;
}
/* LFO held in PLAY: D5 E5 F5 G5 (the four white keys in a row, they light) are the verbs 1..4 (E67; E66's buttons are dropped) */
static uint32_t uxl_vk_mask(void)               /* the four keys as bits of the key state */
{
    uint32_t i, m = 0;
    for (i = 0; i < 4u; i++)
        m |= 1u << action_keys[i];
    return m;
}
static int uxl_swallow(uint32_t k)              /* PLAY, LFO held: these four keys are verbs, never notes (also called from the audio side) */
{
    uint32_t i;
    if (!uxl.play || !(fm1_in.buttons & (1u << panel.btn[B_LFO])))
        return 0;
    for (i = 0; i < 4u; i++)
        if (action_keys[i] == k)
            return 1;
    return 0;
}
static void uxl_ev_poll(void)
{
    static uint32_t prev, t_a4;
    static uint8_t fired;
    uint32_t edge = fm1_in.notes & ~prev, i, bit = 1u << action_keys[3];
    int lfo = uxl.play && (fm1_in.buttons & (1u << panel.btn[B_LFO]));
    prev = fm1_in.notes;
    if (lfo) {
        for (i = 0; i < 3u; i++)
            if (edge & (1u << action_keys[i])) {
                ui.pg_down &= (uint16_t)~(1u << panel.btn[B_LFO]);   /* (LFO's release is no tap) */
                uxl_ev_verb(i);
            }
        if (edge & bit) {
            ui.pg_down &= (uint16_t)~(1u << panel.btn[B_LFO]);
            if (uxl.ev == UXL_EV_PAIR) {
                uxl_ev_verb(3u);
            } else {
                t_a4 = fm1_ms | 1u;
                fired = 0;
            }
        }
        if (t_a4 && !fired && fm1_ms - (t_a4 & ~1u) >= 500u) {
            fired = 1;
            uxl_ev_a4(1);
        }
    }
    if (t_a4 && (!lfo || !(fm1_in.notes & bit))) {
        if (!fired)
            uxl_ev_a4(0);
        t_a4 = 0;
        fired = 0;
    }
}

/* ---------------------------------------------------------------- INTERACT ---
 * BUILD, ARP held 700 ms: the targets of RANDOM, JOURNEY, the snapshots and ELASTIC's row are chosen by hand (E56). On any page a
 * KNOB turned right puts its parameter into the selected track's set, left takes it out; the value is not changed. Up to 32 targets
 * per track. A track without a set uses what PLAY shows (uxl_hp_sync); taking the last one out gives that back. The set is kept in
 * the track's HOME rows (four to a row, the row of the one added last is the ROW of RANDOM and ELASTIC). */
#define UXL_SET_MAX 32u
static int uxl_inter_ref(uint32_t slot, hp_ref_t *out)    /* the parameter a knob has on this page (as hp_touch, without its effects) */
{
    const page_t *pg = cur_page();
    if (fx_delay_suspended(pg, slot)) return 0; /* unavailable FX cannot enter INTERACT */
#if CARAVEL_AFTERBURNER
    /* AFTER's controls are virtual, not pointers into song.g[]. */
    if (str_eq(pg->title, "AFTER")) return 0;
#endif
    hp_ref_t r = {0};
    r.track = song.sel;
    r.engine = 255;
    r.page = ui.page;
    if (pg->graph == GR_PAIR) { r.kind = HP_PAIR; r.id = slot; }
    else if (pg->graph == GR_MACRO) { r.kind = HP_MACRO; r.id = slot; }
    else if (lfo2_page(pg->graph)) { r.kind = HP_LFO2; r.id = lfo2_page_target(pg->graph, slot); }
 else if (pg->scope == SC_TRK) {
        static const uint8_t ids[] = {P_LEVEL, P_PAN, P_REV, P_MUTE};
        r = hp_track_ref(song.sel, ids[slot], ui.page);
    } else {
        int16_t *p;
        const param_desc_t *d = page_desc(pg, slot, &p);
        if (!d || !p || d->max == d->min || pg->scope == SC_STEP || pg->graph == GR_HOMEEDIT || pg->graph == GR_MACROMAP)
            return 0;
        if (pg->scope == SC_GLOBAL) {
            uint32_t id = (uint32_t)(p - song.g);
            if (id > G_CDEPTH && id != G_RTYPE) return 0;
            r.kind = HP_GLOBAL; r.id = (uint16_t)id;
        } else {
            r = hp_track_ref(song.sel, (uint32_t)(p - TSEL->p), ui.page);
        }
    }
    if (!hp_desc(r)) return 0;
    *out = r;
    return 1;
}
static uint32_t uxl_set_count(uint32_t t)
{
    uint32_t n = 0, i, k;
    for (i = 0; i < hp.track[t].count; i++)
        for (k = 0; k < 4u; k++)
            n += hp.track[t].cell[i][k].kind != 0u;
    return n;
}
static int uxl_ref_same(hp_ref_t a, hp_ref_t b)   /* the same parameter, whichever way it was written (a page's, PLAY's) */
{
    if (a.kind != b.kind)
        return 0;
    if (a.kind == HP_GLOBAL)
        return a.id == b.id;
    if (a.kind == HP_TRACK)
        return hp_owner(a) == hp_owner(b) && hp_tid(a) == hp_tid(b);
    return hp_owner(a) == hp_owner(b) && a.id == b.id && a.aux == b.aux;
}
static int uxl_set_find(uint32_t t, hp_ref_t r, uint32_t *row, uint32_t *col)   /* in the set of track t? */
{
    uint32_t i, k;
    for (i = 0; i < hp.track[t].count; i++)
        for (k = 0; k < 4u; k++) {
            hp_ref_t a = hp.track[t].cell[i][k];
            if (a.kind && uxl_ref_same(a, r)) {
                if (row) *row = i;
                if (col) *col = k;
                return 1;
            }
        }
    return 0;
}
/* is this parameter among the targets a RANDOM or JOURNEY morph (and the snapshots) would move on the selected track now?
 * With a set of its own: in it (with SCOPE ROW: in its last four); else: among what PLAY shows (with ROW: the group last played). */
static int uxl_morph_has(hp_ref_t r)
{
    uint32_t t = song.sel, row = 0, a, g, k;
    const param_desc_t *d = hp_desc(r);
    uxl_tg_t tg[4];
    if (!d || !hp_rm_safe(r, d) || hp_rm_fm6_patch(r))
        return 0;
    if (uxl.uset & (1u << t))
        return uxl_set_find(t, r, &row, 0) && (hp_rm.scope != HP_RM_ROW || row == hp.track[t].active);
    for (a = 0; a < UXL_N; a++) {
        uint32_t ng = a == UXL_MOVE ? 1u : a == UXL_MIX ? 0u : uxl_area_ng(a);
        for (g = 0; g < ng; g++) {
            if (hp_rm.scope == HP_RM_ROW && !(a == uxl.ev_a && g == uxl.ev_g))
                continue;
            uxl_group(a, g, tg);
            for (k = 0; k < 4u; k++)
                if (tg[k].kind && tg[k].kind != UXL_KFILT && tg[k].kind != UXL_KTONE &&
                    uxl_ref_same(uxl_ref(&tg[k]), r))
                    return 1;
        }
    }
    return 0;
}
static int uxl_inter_in(uint32_t slot)          /* the column's parameter is in the set (for the marks) */
{
    hp_ref_t r;
    return (uxl.uset & (1u << song.sel)) && uxl_inter_ref(slot, &r) && uxl_set_find(song.sel, r, 0, 0);
}
static void uxl_inter_msg(const char *w, hp_ref_t r, uint32_t n)
{
    char m[40];
    const param_desc_t *d = hp_desc(r);
    str_cpy(m, w, sizeof m);
    str_cpy(m + str_len(m), d && d->label ? d->label : "?", sizeof m - str_len(m));
    str_cpy(m + str_len(m), "  ", sizeof m - str_len(m));
    fmt_int(m + str_len(m), (int32_t)n);
    str_cpy(m + str_len(m), "/32", sizeof m - str_len(m));
    ui_message(m);
}
static void uxl_inter_knob(uint32_t k, int32_t s)
{
    hp_ref_t r;
    hp_track_t *h;
    uint32_t t = song.sel, row = 0, col = 0, n, i, j;
    if (ui.home) {
        ui_message("INTERACT: OPEN A PAGE");
        return;
    }
    if (!uxl_inter_ref(k, &r)) {
        ui_message("NOT A TARGET");
        return;
    }
    h = &hp.track[t];
    if (!(uxl.uset & (1u << t))) {                  /* the first one: the set starts empty (the PLAY default is dropped) */
        memset(h->cell, 0, sizeof h->cell);
        h->count = 1;
        h->active = 0;
        uxl.uset |= (uint8_t)(1u << t);
    }
    n = uxl_set_count(t);
    if (s > 0) {
        if (uxl_set_find(t, r, &row, &col)) {
            ui_message("ALREADY IN THE SET");
            return;
        }
        if (n >= UXL_SET_MAX) {
            ui_message("SET FULL: 32 TARGETS");
            return;
        }
        for (i = 0; i < h->count; i++)
            for (j = 0; j < 4u; j++)
                if (!h->cell[i][j].kind)
                    goto place;
        if (h->count >= 31u) {                       /* (row 31 is the springs') */
            ui_message("SET FULL: ROWS");
            return;
        }
        i = h->count;                                /* no gap: a new row */
        j = 0;
        h->count++;
    place:
        h->cell[i][j] = r;
        h->active = (uint8_t)i;
        uxl_inter_msg("+ ", r, n + 1u);
    } else {
        if (!uxl_set_find(t, r, &row, &col)) {
            ui_message("NOT IN THE SET");
            return;
        }
        memset(&h->cell[row][col], 0, sizeof h->cell[row][col]);
        while (h->count > 1u && !h->cell[h->count - 1u][0].kind && !h->cell[h->count - 1u][1].kind &&
               !h->cell[h->count - 1u][2].kind && !h->cell[h->count - 1u][3].kind)
            h->count--;
        if (h->active >= h->count)
            h->active = (uint8_t)(h->count - 1u);
        if (n == 1u) {
            uxl.uset &= (uint8_t)~(1u << t);          /* the last one out: PLAY's default again */
            ui_message("SET EMPTY: PLAY'S DEFAULT");
        } else {
            uxl_inter_msg("- ", r, n - 1u);
        }
    }
    hp_dirty = 1;
    ui.force = 1;
}
static void uxl_inter_poll(void)                  /* BUILD, ARP held 700 ms: INTERACT on and off */
{
    static uint32_t t0;
    static uint8_t fired;
    uint32_t arp = 1u << panel.btn[B_ARP];
    if (!uxl.play && (fm1_in.buttons & arp) && !(fm1_in.buttons & ~arp) && !fm1_in.notes && uxl_free()) {
        if (!t0)
            t0 = fm1_ms | 1u;
        else if (!fired && fm1_ms - (t0 & ~1u) >= 700u) {
            fired = 1;
            uxl.inter ^= 1u;
            ui.pg_down &= (uint16_t)~(1u << panel.btn[B_ARP]);   /* (no page tap when it is let go) */
            ui_message(uxl.inter ? "INTERACT: KNOB > ADD, < REMOVE" : "INTERACT OFF");
            ui.force = 1;
        }
    } else {
        t0 = 0;
        fired = 0;
    }
}
/* the arcs: how far (0..76 degrees) a setup value is, -1: no arc */
static int32_t uxl_ev_frac(uint32_t b, uint32_t k)
{
    int32_t v, i, n = 0;
    if (b == ACT_BANK_JOURNEY) {
        v = action_setup_value(k);
        return k == 0u ? (v ? 76 : 0) : k == 1u ? v * 76 / 2 : -1;
    }
    if (b == 2u) {
        if (!ep.on)
            return -1;
        v = action_setup_value(k);
        if (k == 0u)
            return v * 76 / 127;
        return k < 3u && v >= -64 && v <= 63 ? (v + 64) * 76 / 127 : -1;
    }
#if CARAVEL_ELASTIC_P1
    if (b == ACT_BANK_ELASTIC)
        return uxl_el_arc_frac(k, hp_el_setup_page & 1u, 0);
#endif
    if (b != 0u)
        return -1;
    v = action_setup_value(k);
    if (k == 0u) {
        while (n < 6 && hp_rm_times[n] != v) n++;
        return n * 76 / 6;
    }
    if (k == 1u) {
        while (n < 5 && hp_rm_depths[n] != v) n++;
        return n * 76 / 5;
    }
    (void)i;
    return v * 76 / 2;
}
#endif

/* ------------------------------------------------------------------ input --- */
static int uxl_pv_on(void) { return uxl.play && uxl.pv; }
static void uxl_presets(int32_t s)      /* PRESETS in PLAY: a choice, not a load */
{
    uxl.hint_until=0;
    uint32_t total, cur = preset_pos(&total);
    if (!total) {
        ui_message("NO SOUNDS IN LIST");
        return;
    }
    if (!uxl.pv) {
        uxl.pv = 1;
        uxl.pv_idx = (uint16_t)(cur >= total ? 0u : cur);
    }
    uxl.pv_idx = (uint16_t)((uxl.pv_idx + (s > 0 ? 1u : total - 1u)) % total);
    uxl.hot_t = 0;
    ui.force = 1;
}
static void uxl_pv_resolve(int load)
{
    if (load) {
        preset_go(uxl.pv_idx);
        ui_message("LOADED");
    } else {
        ui_message("CANCELLED");
    }
    uxl.pv = 0;
    ui.force = 1;
}
/* SEL held in PLAY: D5 E5 F5 G5 are the HARM verbs PREV NEXT HOLD RESET (E2), the other keys stay the layer's ROOT */
/* OCT- and OCT+ held together for 700 ms: PANIC, in either layer (every note off, FX held off; the mutes stay) */
static void uxl_panic_only(void)
{
    static uint32_t t0;
    static uint8_t fired;
    uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    if ((fm1_in.buttons & both) != both) {
        t0 = 0;
        fired = 0;
        return;
    }
    if (!t0) {
        t0 = fm1_ms | 1u;
        return;
    }
    if (!fired && fm1_ms - (t0 & ~1u) >= 700u) {
        fired = 1;
        panic_req |= (uint8_t)((1u << NTRK) - 1u);
        perf_kill = 1;
        perf_latched = 0;
#if CARAVEL_UX_CAPTURE
        (void)cap_post(CAP_OP_ALL_PAUSE, 0u, 0u);        /* (the audio context does it at its next block) */
#endif
        ui_say("PANIC", "");
        ui.msg_t = 90;
        ui.force = 1;
    }
}
static void uxl_mom_poll(void)
{
    uint32_t lfo = (fm1_in.buttons >> panel.btn[B_LFO]) & 1u, k;
    if (uxl.mom && (!lfo || !uxl.play)) {               /* LFO let go: the values back */
        for (k = 0; k < 4u; k++)
            if ((uxl.mom & (1u << k)) && hp_desc(uxl.mom_r[k]))
                hp_set(uxl.mom_r[k], uxl.mom_v[k], 0);
        uxl.mom = 0;
        ui.force = 1;
        ui_message("BACK");
    }
}
static const uint8_t UXL_SV_KEY[4] = {18, 19, 24, 26};     /* B4 C5 F5 G5 */
static const uint8_t UXL_SV_POS[4] = {10, 11, 14, 15};     /* .. as white keys F3 .. G5 */
static void uxl_stasis_poll(void)                   /* FX held in PLAY: B4 C5 F5 G5 pressed are HOLD ADD BLEND RELEASE */
{
    static uint32_t prev;
    uint32_t edge = fm1_in.notes & ~prev, i;
    prev = fm1_in.notes;
    if (uxl.play && ui.ly == LAYER_FX && (fm1_in.buttons & (1u << panel.btn[B_FX])))
        for (i = 0; i < 4u; i++)
            if (edge & (1u << UXL_SV_KEY[i]))
                action_stasis(i);
}
static void uxl_hold_no_tap_poll(void)               /* BUILD: ENV or LFO held (0.4 s) or used with a key is no page tap when it is let go (the page used to
                                                      * go round to the next one, with its own picture, each time the owner held LFO to try the verbs) */
{
    static uint32_t t0[2], prev;
    static const uint8_t BTN[2] = {B_ENV, B_LFO};
    uint32_t i, edge = fm1_in.notes & ~prev;
    prev = fm1_in.notes;
    for (i = 0; i < 2u; i++) {
        uint32_t bit = 1u << panel.btn[BTN[i]];
        if (!uxl.play && (fm1_in.buttons & bit)) {
            if (!t0[i])
                t0[i] = fm1_ms | 1u;
            if (fm1_ms - (t0[i] & ~1u) >= 400u || edge)
                ui.pg_down &= (uint16_t)~bit;
        } else {
            t0[i] = 0;
        }
    }
}
static void uxl_panic_poll(void)
{
    uxl_hold_no_tap_poll();
#if CARAVEL_UX_CAPTURE
    uxl_cap_poll();
#endif
    uxl_panic_only();
    uxl_mom_poll();
    uxl_stasis_poll();
#if CARAVEL_UX_EVOLVE
    uxl_ev_poll();
    uxl_inter_poll();
#endif
#if CARAVEL_UX_CAPTURE
    {
        uint32_t rec = 1u << panel.btn[B_REC], seq = 1u << panel.btn[B_SEQ];
        if (uxl.play && (fm1_in.buttons & rec) ) {       /* REC held: K1 chooses the length, the length shows over the circle */
            if (!uxl.rec_t) {
                uxl.rec_t = fm1_ms | 1u;
                uxl.rec_viewed = 0;
                uxl.rec_knob_used = 0;
            } else if (!uxl.cap_view && fm1_ms - (uxl.rec_t & ~1u) >= 350u) {
                uxl.cap_view = 1;
                uxl.rec_viewed = 1;
                ui.force = 1;
            }
        } else {
            uint32_t was = uxl.rec_t;
            if (uxl.cap_view)
                ui.force = 1;
            uxl.cap_view = 0;
            uxl.rec_t = 0;
            if (was && uxl.play && !uxl.rec_viewed && uxl_free())   /* let go without the view: a tap, the capture */
                uxl_cap_rec();
            uxl.rec_viewed = 0;
        }
        if (uxl.play && (fm1_in.buttons & seq) ) {       /* SEQ held 700 ms: the loop and the live notes are wiped */
            if (!uxl.seq_t)
                uxl.seq_t = fm1_ms | 1u;
            else if (!uxl.seq_fired && fm1_ms - (uxl.seq_t & ~1u) >= 700u) {
                uxl.seq_fired = 1;
                if (!cap_post(CAP_OP_WIPE, cap_selected(), 0u))
                    ui_message("CAPTURE BUSY");
                ui.force = 1;
            }
        } else {
            if (uxl.seq_t && uxl.play && !uxl.seq_fired && uxl_free())   /* a tap: the loop pauses or goes on */
                uxl_cap_loop();
            uxl.seq_fired = 0;
            uxl.seq_t = 0;
        }
    }
#endif
    {   /* EDIT in PLAY is a plain button (its hold layer is shut): a tap, the SOUND area */
        static uint32_t t_edit;
        if (uxl.play && (fm1_in.buttons & (1u << panel.btn[B_EDIT]))) {
            if (!t_edit)
                t_edit = fm1_ms | 1u;
        } else {
            if (t_edit && uxl.play && uxl_free())
                uxl_area_tap(UXL_SOUND);
            t_edit = 0;
        }
    }
}
/* PLAY, FX held (SPACE): B4 C5 F5 G5 are the STASIS verbs HOLD ADD BLEND RELEASE (E26, E30: the hold-FLANGER and -PHASER are gone);
 * the effect map gives those four white keys no effect while PLAY is on */
static void uxl_fx_map_filter(void)
{
    uint32_t i;
    if (!uxl.play)
        return;
    for (i = 0; i < 4u; i++)
        if (perf_map[UXL_SV_POS[i]] != PF_N) {
            perf_map[UXL_SV_POS[i]] = PF_N;
            perf_remap = 1;
        }
}
static const char *uxl_fx_label(uint32_t p)
{
    static const char *const L[4] = {"HOLD", "ADD", "BLND", "REL"};
    uint32_t i;
    if (uxl.play)
        for (i = 0; i < 4u; i++)
            if (UXL_SV_POS[i] == p)
                return L[i];
    return 0;
}
static uint32_t uxl_harm_keys(uint32_t keys)
{
    static const uint8_t KEY[4] = {21, 23, 24, 26}, CMD[4] = {2, 3, 1, 4};   /* H_PREV H_NEXT H_HOLD H_RESET (seq.c) */
    static const char *const NAME[4] = {"HARM PREV", "HARM NEXT", "HARM HOLD", "HARM RESET"};
    uint32_t i;
    if (uxl.play && ui.ly == LAYER_GLO && (keys & (1u << 9))) {      /* GLO SET's D4, MIDI LEARN: BUILD only */
        keys &= ~(1u << 9);
        ui_message("LEARN: BUILD ONLY");
    }
    if (!uxl.play || ui.ly != LAYER_SCL)
        return keys;
    for (i = 0; i < 4u; i++)
        if (keys & (1u << KEY[i])) {
            keys &= ~(1u << KEY[i]);
            if (!harmony_on) {
                ui_message("LINK: ENABLE HARMONY");
            } else {
                harmony_command = CMD[i];
                ui_message(NAME[i]);
                ui.force = 1;
            }
        }
    return keys;
}
static int uxl_free(void) { return !ui.menu && !ui.confirm && !name_on() && !ui.uboot; }

static void uxl_show_area(void)
{
    char b[24];
    str_cpy(b, UXL_AREA[uxl.area].name, sizeof b);
    if (uxl_area_ng(uxl.area) > 1u) {
        uint32_t n = str_len(b);
        b[n] = ' ';
        b[n + 1u] = (char)('A' + uxl_group_now());
        b[n + 2u] = 0;
    }
    ui_message(b);
}

/* an area's button tapped in PLAY: the area; the same one again: its next group */
static void uxl_area_tap(uint32_t area)
{
    if (area == uxl.area) {
        if (uxl_area_ng(area) > 1u)
            uxl.grp[area] = (uint8_t)((uxl_group_now() + 1u) % uxl_area_ng(area));
    } else {
        uxl.area = (uint8_t)area;
    }
    uxl.hot_t = 0;
    ui.force = 1;
    ui.msg_t = 0;
}
static void uxl_select(int32_t s)       /* SELECT in PLAY: the group within the area */
{
    uxl.hint_until=0;
    uint32_t ng = uxl_area_ng(uxl.area);
    if (ng > 1u) {
        uxl.grp[uxl.area] = (uint8_t)clamp((int32_t)uxl_group_now() + (s > 0 ? 1 : -1), 0, (int32_t)ng - 1);
        uxl.hot_t = 0;
        ui.force = 1;
    }
}
static void uxl_cap_knob(uint32_t k, int32_t s);
static void uxl_knob(uint32_t k, int32_t s)
{
    uxl.hint_until=0;
    uxl_tg_t tg[4];
#if CARAVEL_UX_CAPTURE
    if (fm1_in.buttons & (1u << panel.btn[B_REC])) {    /* REC held: the arcs are not turned, K1 is the CAPTURE length */
        uxl_cap_knob(k, s);
        return;
    }
#endif
    hp_ref_t r;
    const param_desc_t *d;
#if CARAVEL_UX_EVOLVE
    if (uxl_ev_group() >= 0) {                          /* EVOLVE: the arcs are the verb's setup */
        uxl.ev = (uint8_t)uxl_ev_group();
        uxl_ev_setup_turn(UXL_EVB[uxl_ev_group()], k, s);
        uxl.hot_k = (uint8_t)k;
        uxl.hot_t = 40;
        ui.force = 1;
        return;
    }
#endif
    uxl_group(uxl.area, uxl_group_now(), tg);
    if (!tg[k].kind)
        return;
#if CARAVEL_STASIS_EXPAND
    if (tg[k].kind == UXL_KTONE) {
        if (song.g[G_RTYPE] != 3u) { ui_message("SET REVERB STASIS"); return; }
        stasis_tone = (int16_t)clamp((int32_t)stasis_tone +
                                   accel(EN_K1 + k, s, 127), 0, 127);
        uxl.hot_k = (uint8_t)k;
        uxl.hot_t = 40;
        ui.force = 1;
        return;
    }
#endif
    if (tg[k].kind == UXL_KFILT) {                      /* FILTER edits P_FILT; other FX macros keep their current live behavior */
        uxl_fx_turn(tg[k].id, k, s);
        uxl.hot_k = (uint8_t)k;
        uxl.hot_t = 40;
        ui.force = 1;
        return;
    }
#if CARAVEL_UX_EVOLVE
    uxl_ev_loc();
#endif
    r = uxl_ref(&tg[k]);
    d = hp_desc(r);
    if (!d)
        return;
    if (fm1_in.buttons & (1u << panel.btn[B_LFO])) {    /* LFO held: MOMENTARY, the value goes back when LFO is let go */
        if (!(uxl.mom & (1u << k))) {
            uxl.mom |= (uint8_t)(1u << k);
            uxl.mom_v[k] = (int16_t)hp_get(r);
            uxl.mom_r[k] = r;
        }
        hp_set(r, param_turn(d, hp_get(r), s), 0);
    } else {
#if CARAVEL_ELASTIC_P1
        if (hp_el1.on && uxl_el_turn(r, s)) {           /* ELASTIC armed: a spring parameter is pulled, not set */
            uxl.hot_k = (uint8_t)k;
            uxl.hot_t = 40;
            ui.force = 1;
            return;
        }
#endif
        hp_set(r, param_turn(d, hp_get(r), s), 1);
    }
    uxl.hot_k = (uint8_t)k;
    uxl.hot_t = 40;
    ui.force = 1;
}
static void uxl_overview_knob(uint32_t k, int32_t s)   /* BUILD, HOME: K1..K4 are the levels of the four tracks */
{
    hp_ref_t r = {0};
    const param_desc_t *d;
    r.kind = HP_TRACK;
    r.id = P_LEVEL;
    r.track = (uint8_t)k;
    r.engine = 255;
    d = hp_desc(r);
    if (d) {
        hp_set(r, param_turn(d, hp_get(r), s), 1);
        ui.force = 1;
    }
}
static void uxl_page_tap(uint32_t b)    /* ENV LFO ARP (the layer buttons: uxl_layer_tap) */
{
    uint32_t a;
    for (a = 0; a < UXL_N; a++)
        if (UXL_AREA[a].btn == b) {
            uxl_area_tap(a);
            return;
        }
}
/* BUILD <-> PLAY stay in the same block of functions (E65): the family of the page you are on is the area you land in, and back */
static const uint8_t UXL_FAM[UXL_N] = {FAM_FX, FAM_SCL, FAM_ENV, FAM_LFO, FAM_EDIT, FAM_GLO, FAM_ARP};
#if CARAVEL_UX_EVOLVE
static uint32_t uxl_evp_idx(void);
static uint32_t uxl_page_of(uint32_t graph, uint32_t nth)     /* the nth page with this graph; NPAGES: none */
{
    uint32_t i, n = 0;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == graph) {
            if (n == nth)
                return i;
            n++;
        }
    return NPAGES;
}
#endif
static void uxl_area_from_page(void)             /* going to PLAY: the area of the BUILD page shown (the overview: the last one) */
{
    const page_t *pg = cur_page();
    uint32_t a;
    if (ui.home)
        return;
#if CARAVEL_UX_EVOLVE
    if (pg->graph == GR_EVOLVE) {                   /* RANDOM, JOURNEY, SPRING, SPRINGS: MOVE's verbs */
        uint32_t idx = uxl_evp_idx();
        uxl.area = UXL_MOVE;
        uxl.grp[UXL_MOVE] = (uint8_t)(idx == 0u ? 1u : idx == 1u ? 2u : 3u);
        return;
    }
    if (pg->graph == GR_PAIR) {
        uxl.area = UXL_MOVE;
        uxl.grp[UXL_MOVE] = (uint8_t)UXL_EVN;
        return;
    }
#endif
    for (a = 0; a < UXL_N; a++)
        if (UXL_FAM[a] == pg->fam) {
            uxl.area = (uint8_t)a;
            return;
        }
}
static void uxl_page_from_area(void)             /* going to BUILD, the area changed in PLAY: the family of the area (or the verb's page) */
{
    uint32_t fam = UXL_FAM[uxl.area];
#if CARAVEL_UX_EVOLVE
    if (uxl.area == UXL_MOVE && uxl_group_now() >= 1u) {
        uint32_t v = uxl_group_now() - 1u, i;
        i = v == UXL_EV_PAIR ? uxl_page_of(GR_PAIR, 0) : uxl_page_of(GR_EVOLVE, v);
        if (i < NPAGES) {
            ui.page = (uint8_t)i;
            ui.fam_last[PAGES[i].fam] = ui.page;
            ui.home = 0;
            page_entered();
            return;
        }
    }
#endif
    if (ui.home || cur_page()->fam != fam)
        open_family(fam);
}
static void uxl_toggle(void)            /* HOME held: BUILD <-> PLAY, only with nothing else held */
{
    uint32_t others = fm1_in.buttons & ~(1u << panel.btn[B_HOME]);
    if (!uxl_free() || others || fm1_in.notes)
        return;
    if (!uxl.play) {                                    /* to PLAY: the area of this block of functions */
        uxl_area_from_page();
        uxl.pl_area = uxl.area;
        uxl.pl_grp = (uint8_t)uxl_group_now();
    } else if (uxl.area != uxl.pl_area || uxl_group_now() != uxl.pl_grp) {   /* to BUILD, the area changed: its block of functions */
        uxl.play = 0;
        uxl_page_from_area();
        uxl.play = 1;
    }
    uxl.play ^= 1u;
    uxl.lvl = 0;
#if CARAVEL_UX_EVOLVE
    uxl.inter = 0;
    if (uxl.play && !uxl.ev_init) {                     /* the first row a ROW means: SOUND A */
        uxl.ev_init = 1;
        uxl.ev_a = UXL_SOUND;
        uxl.ev_g = 0;
    }
#endif
    uxl.hint_until=uxl.play?fm1_ms+5000u:0u;
    uxl.pv = 0;
    uxl.hot_t = 0;
    ui.layer = 0;
    ui.force = 1;
    uxl.cap_view = 0;
    uxl.cap_auto = 0;
    if(uxl.play)ui.msg_t=0;             /* the entry help, not a redundant PLAY message */
    else ui_message("BUILD");
}
/* Power-on (main.c, and the emulator's web_power_on): PLAY, the level the instrument is played in (CARAVEL_START_PLAY, default on; the tests' emulators
 * go on to BUILD by web_test_build). The LEVELS screen is open at the first power-on, until it has been shown once: its bit is kept with the settings
 * (bit 7 of the byte layer_seen uses, favorites.factory[15][31]; the layers use bits 1..6). Held buttons at power-on (the panel setup): BUILD as before. */
#ifndef CARAVEL_START_PLAY
#define CARAVEL_START_PLAY 1
#endif
#define UXL_SEEN (favorites.factory[15][31])
static void uxl_boot(void)
{
    if (!CARAVEL_START_PLAY)
        return;
    uxl.play = 0;
    uxl_toggle();
    if (uxl.play && !((UXL_SEEN >> 7) & 1u)) {
        uxl.lvl = 1;
        uxl.lvl_t = fm1_ms;
        UXL_SEEN |= 0x80u;
        settings_save();
    }
}
#if CARAVEL_UX_CAPTURE
static void uxl_cap_rec(void)                       /* REC tapped: the live notes become the loop, or are added to it (overdub, E45) */
{
    if (!cap_post(CAP_OP_REC, cap_selected(), CAP_BARS[cap_li[cap_selected()]]))     /* (the audio context answers, uxl_cap_poll says it) */
        ui_message("CAPTURE BUSY");
}
static void uxl_cap_loop(void)                      /* SEQ tapped: the loop pauses or goes on */
{
    if (!cap_post(CAP_OP_TOGGLE, cap_selected(), 0u))
        ui_message("CAPTURE BUSY");
}
static void uxl_cap_poll(void)                      /* what the audio context did with the commands: the messages */
{
    static const char *const M[] = {0, "CAPTURED", "OVERDUB", "NOTHING TO CAPTURE", "NOTHING TO ADD", "LOOP FULL", "LOOP PAUSED",
                                    "LOOP RUNS", "NO LOOP", "NO CLOCK: PRESS PLAY", "CLEARED",
                                    "QUANT: NEXT LOOP", "QUANT OFF: NEXT", "QUANT COLLISION: OFF"};
    uint32_t c;
    while ((c = cap_result()) != CAPR_NONE) {
        if (c < sizeof M / sizeof M[0])
            ui_message(M[c]);
        if (c == CAPR_PAUSED || c == CAPR_RUNS)
            uxl.cap_auto = 150;
        ui.force = 1;
    }
}
static void uxl_cap_knob(uint32_t k, int32_t s)   /* REC held: K1 LENGTH unchanged, K2 post-CAPTURE QUANT */
{
    uint32_t owner = cap_selected();
    cap_view_t v;
    if (!s || k > 1u) return;
    /* Even a turn BEFORE the 350ms view threshold must never arm a REC tap
     * or a layer-tap overdub when the button is released. */
    uxl.rec_knob_used = 1u;
    uxl.rec_viewed = 1u;
    uxl.cap_view = 1u;
    ui.rec_t0 |= 2u;  /* btn_hold() consumes the corresponding release */
    ui.pg_down &= (uint16_t)~(1u << panel.btn[B_REC]);
    if (k == 0u) {
        if (uxl_cap_get(owner, 2u)) {
            ui_message("LENGTH: CLEAR LOOP FIRST");
        } else {
            cap_li[owner] = (uint8_t)clamp((int32_t)cap_li[owner] +
                                             (s > 0 ? 1 : -1), 0, 5);
        }
    } else {
        uint32_t next;
        cap_view(owner, &v);
        if (!v.ln) {
            ui_message("CAPTURE LOOP FIRST"); /* no hidden pre-quant for future takes */
        } else {
            next = (uint32_t)clamp((int32_t)v.requested + (s > 0 ? 1 : -1), 0, 5);
            if (next != v.requested && !cap_post(CAP_OP_QUANT, owner, next))
                ui_message("CAPTURE BUSY");
        }
    }
    ui.force = 1u;
}
#endif
/* the layer buttons' taps (FX SEL EDIT GLO SEQ REC) */
static void uxl_layer_tap(uint32_t l)
{
    if (uxl.play) {
        switch (l) {
        case LAYER_FX: uxl_area_tap(UXL_SPACE); break;
        case LAYER_SCL: uxl_area_tap(UXL_HARM); break;
        case LAYER_EDIT: uxl_area_tap(UXL_SOUND); break;
        case LAYER_GLO: uxl_area_tap(UXL_MIX); break;
#if CARAVEL_UX_CAPTURE
        case LAYER_SEQ:
            if (uxl.seq_fired) { uxl.seq_fired = 0; break; }
            uxl_cap_loop();
            break;
        default:
            if (uxl.rec_knob_used) {
                uxl.rec_knob_used = 0;
                uxl.rec_viewed = 0;
                break;
            }
            if (uxl.rec_viewed) { uxl.rec_viewed = 0; break; }
            uxl_cap_rec();
            break;
#else
        case LAYER_SEQ: ui_message("LOOP: LATER"); break;
        default: ui_message("CAPTURE: LATER"); break;
#endif
        }
        return;
    }
    if (l == LAYER_GLO) {               /* BUILD: tapped twice (300 ms): the menu */
        if (uxl.glo_n && fm1_ms - uxl.glo_ms <= 300u && uxl_free()) {
            uxl.glo_n = 0;
            ui.menu = 1;
            ui.force = 1;
            song.seq_mode = 0;
            return;
        }
        uxl.glo_n = 1;
        uxl.glo_ms = fm1_ms;
    } else {
        uxl.glo_n = 0;
    }
    layer_tap(l);
}
/* a layer's button held in PLAY: EDIT SET, SEQ TOOLS and REC SET change sounds and patterns: BUILD only */
static int uxl_layer_blocked(uint32_t l)
{
    return uxl.play && (l == LAYER_EDIT || l == LAYER_SEQ || l == LAYER_REC);
}
static void uxl_home_tap(void)
{
    if (uxl.play) {                                     /* PLAY: HOME tapped shows how the levels are switched (the SOUND area is EDIT's) */
        uxl.lvl = !uxl.lvl;
        uxl.lvl_t = fm1_ms;
        uxl.hot_t = 0;
        ui.force = 1;
    } else {
        go_home();
    }
}

/* ------------------------------------------------------------------- draw --- */
static void uxl_text_c(int32_t cx, int32_t y, const aafont_t *f, const char *s, uint16_t fg)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, fg);
}
static void uxl_text_r(int32_t xr, int32_t y, const aafont_t *f, const char *s, uint16_t fg)
{
    cv_text(xr - text_w(f, s), y, f, s, fg);
}
static void uxl_pt(int32_t cx, int32_t cy, int32_t r, int32_t a, int32_t *x, int32_t *y)   /* a clockwise from 12 o'clock */
{
    *x = cx + (r * uxl_sin(a) + 128) / 256;
    *y = cy - (r * uxl_sin(a + 90) + 128) / 256;
}
static const uint8_t UXL_ATAN[65] = {0,1,2,3,4,4,5,6,7,8,9,10,11,11,12,13,14,15,16,17,17,18,19,20,21,21,22,23,24,24,25,26,27,27,28,29,29,30,31,31,32,33,33,34,35,35,36,36,37,37,38,39,39,40,40,41,41,42,42,43,43,44,44,45,45};
static int32_t uxl_ang(int32_t dx, int32_t dy)   /* degrees clockwise from 12 o'clock, 0..359 (dx right, dy up) */
{
    int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, a;
    if (ax == 0 && ay == 0)
        return 0;
    if (ax <= ay)                                /* steeper than 45 degrees: from the vertical */
        a = UXL_ATAN[ay ? ax * 64 / ay : 0];
    else
        a = 90 - UXL_ATAN[ax ? ay * 64 / ax : 0];
    if (dx >= 0 && dy >= 0) return a;            /* top right */
    if (dx >= 0) return 180 - a;                 /* bottom right */
    if (dy < 0) return 180 + a;                  /* bottom left */
    return 360 - a;                              /* top left */
}
static int32_t uxl_y0;                           /* the strip being drawn: its first row (the arcs skip the rest) */
/* an arc of a ring (radii r0..r1, centre cx cy) over a0..a1 degrees: the track in c0, a0f..a1f in c1 (a pixel scan, no gaps) */
static void uxl_ring(int32_t cx, int32_t cy, int32_t r0, int32_t r1, int32_t a0, int32_t a1, int32_t a0f, int32_t a1f, uint16_t c0, uint16_t c1)
{
    int32_t x, y, xa = cx - r1, xb = cx + r1, ya = cy - r1, yb = cy + r1;
    int32_t lo = r0 * r0, hi = r1 * r1;
    if (ya < uxl_y0) ya = uxl_y0;
    if (yb > uxl_y0 + 119) yb = uxl_y0 + 119;
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            int32_t dx = x - cx, dy = cy - y, d2 = dx * dx + dy * dy, a;
            if (d2 < lo || d2 > hi)
                continue;
            a = uxl_ang(dx, dy);
            if (a < a0 || a > a1)
                continue;
            cv_pset(x, y, a >= a0f && a <= a1f ? c1 : c0);
        }
}
/* the value as an arc over its quadrant: bipolar parameters fill from their zero */
static void uxl_orbit_arc_f(uint32_t k, int32_t f, int32_t z, int hot)   /* f, z: degrees of the arc (0..76): the value, the zero */
{
    static const int16_t START[4] = {277, 7, 187, 97};         /* K1/K2 top, K3/K4 bottom, reading order; 76 degrees each */
    int32_t a0, a1;
    const int32_t CX = 120, CY = 116;
    uint16_t c = hot ? T_ACCENT : T_THEME;
    a0 = START[k] + (f < z ? f : z);
    a1 = START[k] + (f < z ? z : f);
    uxl_ring(CX, CY, 58, 71, START[k], START[k] + 76, a0, a1 > a0 ? a1 : a0 - 1, T_LINE, c);
    {   /* the end marker */
        int32_t x, y;
        x = CX + (66 * uxl_sin(START[k] + f) + 128) / 256;
        y = CY - (66 * uxl_sin(START[k] + f + 90) + 128) / 256;
        cv_rect(x - 4, y - 4, 9, 9, hot ? T_ACCENT : T_TEXT);
    }
}
/* a spring on an arc (E63): a coil from the value it returns to (the anchor) to the weight, which is where the parameter is now.
 * Stretched, the coil is longer; at rest it is a short coil beside the anchor, so an armed arc still reads as a spring. */
static void uxl_orbit_spring(uint32_t k, int32_t fa, int32_t fl)
{
    static const int16_t START[4] = {277, 7, 187, 97};
    int32_t d = fl - fa, ad = d < 0 ? -d : d, n, i, a0 = fa, a1 = fl, x, y, px = 0, py = 0;
    if (ad >= 2)                                        /* stretched: the stretch itself lit on the ring */
        uxl_ring(120, 116, 58, 71, START[k] + (fa < fl ? fa : fl), START[k] + (fa < fl ? fl : fa), 0, 0, T_ACCENT, T_ACCENT);
    if (ad < 3) {                                       /* at rest: a coil beside the anchor, towards the middle of the arc */
        a0 = fa;
        a1 = fa + (fa < 38 ? 20 : -20);
        fl = fa;
    }
    n = (a1 > a0 ? a1 - a0 : a0 - a1) / 4;
    if (n < 5) n = 5;
    if (n > 18) n = 18;
    for (i = 0; i <= n; i++) {
        int32_t ang = START[k] + a0 + (a1 - a0) * i / n, r = 66 + (i == 0 || i == n ? 0 : (i & 1) ? 11 : -11);
        x = 120 + (r * uxl_sin(ang) + 128) / 256;
        y = 116 - (r * uxl_sin(ang + 90) + 128) / 256;
        if (i) {
            cv_line(px, py, x, y, T_ACCENT);
            cv_line(px + 1, py, x + 1, y, T_ACCENT);
            cv_line(px, py + 1, x, y + 1, T_ACCENT);
        }
        px = x;
        py = y;
    }
    x = 120 + (66 * uxl_sin(START[k] + fl) + 128) / 256;   /* the weight */
    y = 116 - (66 * uxl_sin(START[k] + fl + 90) + 128) / 256;
    cv_rrect(x - 8, y - 8, 17, 17, 8, T_ACCENT, T_BG);
    cv_rrect(x - 3, y - 3, 7, 7, 3, T_BG, T_ACCENT);
}
static void uxl_orbit_arc(uint32_t k, const param_desc_t *d, int32_t v, int hot)
{
    int32_t span = d->max - d->min, f = span > 0 ? (v - d->min) * 76 / span : 0, z = 0;
    if (d->min < 0 && d->max > 0)
        z = (0 - d->min) * 76 / span;
    uxl_orbit_arc_f(k, f, z, hot);
}
static uint16_t uxl_hot(uint32_t k) { return uxl.hot_t && uxl.hot_k == k ? T_ACCENT : T_TEXT; }

static void uxl_paint_head(void)
{
    char b[16];
    const char *u;
    int32_t x;
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    cv_rrect(4, 3, 50, 19, 5, T_ACCENT, T_BG);
    cv_text_on(11, 4, &AF_M, "PLAY", T_INK, T_ACCENT);
    b[0] = 'T';
    b[1] = (char)('1' + song.sel);
    b[2] = 0;
    cv_text(60, 5, &AF_M, b, T_TEXT);
    (void)e;
    (void)u;
    /* State badges have fixed slots. Messages use their own strip below. */
    x = 80;
    if (TSEL->p[P_MUTE]) {
        cv_rect(x, 4, 38, 17, T_TEXT);
        cv_text_on(x + 4, 5, &AF_S, "MUTE", T_INK, T_TEXT);
    }
#if CARAVEL_UX_CAPTURE
    {
        cap_view_t cv;
        cap_view(cap_selected(), &cv);
        if (cv.st || cv.full) {
            const char *label = cv.full ? "FULL" : cv.st == 1u ? "LOOP" : "WAIT";
            cv_frame(122, 4, 38, 17, T_TEXT);
            uxl_text_c(141, 5, &AF_S, label, T_TEXT);
        }
    }
#endif
    if (song.rec)
        cv_icon_mid(162, H_HEAD / 2, 16, (song.rec >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, T_BG);
    fmt_int(b, song.g[G_BPM]);
    uxl_text_r(206, 5, &AF_M, b, ui.bpm_t ? T_ACCENT : T_THEME);
    cv_icon_mid(224, H_HEAD / 2, 16, song.playing ? ICON_X_PLAY : ICON_X_STOP, song.playing ? T_THEME : T_MID, T_BG);
}
/* Peek maps keep their layer and PLAY identity, messages have their own footer row. */
static int uxl_layer_head_draw(void)
{
    static const char *const LABEL[] = {"", "FX HOLD", "GLO SET", "SEL SET"};
    if (!uxl.play || !ui.layer || ui.layer > LAYER_SCL) return 0;
    cv_begin(240,24,T_BG); cv_bg=T_BG;
    cv_rrect(4,3,50,19,5,T_ACCENT,T_BG);
    cv_text_on(11,4,&AF_M,"PLAY",T_INK,T_ACCENT);
    cv_text(64,5,&AF_S,LABEL[ui.layer],T_TEXT);
    cv_text(184,5,&AF_S,layer_locked()?"LOCK":"HOLD",T_TEXT);
    cv_blit(0,0);
    return 1;
}
#if CARAVEL_UX_CAPTURE
/* CAPTURE in the middle of PLAY, always (E41, owner's wish): a small dim ring with the loop's notes as dots and its pointer
 * while there is a loop (none while LIVE); inside it the band of the notes played, up to 32 bars: a new note
 * enters at the right and moves left. The band is the LIVE window (what REC would take), also while a loop runs. */
static void uxl_paint_widget(void)
{
    const int32_t CX = 120, CY = 116, BX = 88, BY = 96, BW = 64, BH = 40, RM = 44;
    uint32_t i = cap_selected(), now = perf_bars_q16, k, g0 = cap_gen[i];
    cap_view_t v;
    uint32_t len, nb, span, st;                           /* len, span: 1/4096 bar (the events' unit) */
    int32_t lo = 127, hi = 0, a;
    char b[16];
    cap_view(i, &v);
    st = v.st;
    len = (uint32_t)v.bars << 12;
    nb = v.ln && len ? (uint32_t)v.bars : CAP_BARS[cap_li[i]];
    span = nb << 12;
    uxl_ring(CX, CY, 40, 48, 0, 359, 0, 359, T_LINE, T_LINE);
    cv_rect(BX, BY, BW, BH, T_SURF);
    for (k = 0; k < v.rn; k++) {                         /* the pitch range of what is in view */
        const cap_ev_t *e = cap_view_ring(&v, i, k);
        if (e->vel && cap_age(now, e->t) <= span) {
            if ((int32_t)e->note < lo) lo = e->note;
            if ((int32_t)e->note > hi) hi = e->note;
        }
    }
    if (st == 1u && len)                                 /* (a running loop's own notes are in the band too) */
        for (k = 0; k < v.ln; k++) {
            const cap_ev_t *e = &cap_loop[i][k];
            if (e->vel) {
                if ((int32_t)e->note < lo) lo = e->note;
                if ((int32_t)e->note > hi) hi = e->note;
            }
        }
    if (hi < lo) { lo = 48; hi = 72; }
    lo -= 2; hi += 2;
    if (hi - lo < 12) { hi = lo + 12; }
    {   /* the bar lines, as the bars move */
        uint32_t cur = now & ~0xFFFFu;
        for (k = 0; k <= nb; k += nb > 16u ? 4u : nb > 8u ? 2u : 1u) {
            uint32_t tb = cur - (k << 16);
            int32_t x = BX + BW - (int32_t)(((uint32_t)CAP_U(now - tb) * (uint32_t)BW) / span);
            if ((v.wrapped || cur >= (k << 16)) && x >= BX && x < BX + BW)
                cv_rect(x, BY, 1, BH, T_LINE);
        }
    }
    for (k = 0; k < v.rn; k++) {                         /* the notes: the newest at the right edge */
        const cap_ev_t *e = cap_view_ring(&v, i, k);
        uint32_t j, on = cap_age(now, e->t), off = 0, start;
        int32_t x0, x1, y;
        if (!e->vel)
            continue;
        for (j = k + 1u; j < v.rn; j++) {
            const cap_ev_t *o = cap_view_ring(&v, i, j);
            if (!o->vel && o->note == e->note) { off = cap_age(now, o->t); break; }
        }
        if (off > span)
            continue;
        start = on > span ? span : on;
        x0 = BX + BW - (int32_t)(((uint32_t)start * (uint32_t)BW) / span);
        x1 = BX + BW - (int32_t)(((uint32_t)off * (uint32_t)BW) / span);
        y = BY + BH - 3 - ((int32_t)e->note - lo) * (BH - 6) / (hi - lo);
        cv_rect(x0, y, x1 - x0 < 2 ? 2 : x1 - x0, 3, T_THEME);
    }
    if (st == 1u && len) {                               /* the running loop's notes in the band, the other colour: this pass and the one before */
        uint32_t ph = CAP_U((now - v.o) % (len << 4));
        for (k = 0; k < v.ln; k++) {
            const cap_ev_t *e = &cap_loop[i][k];
            uint32_t j, d = len, r, age0;
            if (!e->vel)
                continue;
            for (j = 1; j < v.ln; j++) {                 /* its end: the next end of that note, round the loop */
                const cap_ev_t *o = &cap_loop[i][(k + j) % v.ln];
                if (!o->vel && o->note == e->note) { d = (o->t + len - e->t) % len; break; }
            }
            age0 = (ph + len - e->t) % len;              /* how long ago its last pass began */
            for (r = 0; r < 2u; r++) {
                uint32_t age = age0 + r * len, end = age > d ? age - d : 0u, start = age > span ? span : age;
                int32_t x0, x1, y;
                if (end > span)
                    continue;
                x0 = BX + BW - (int32_t)(((uint32_t)start * (uint32_t)BW) / span);
                x1 = BX + BW - (int32_t)(((uint32_t)end * (uint32_t)BW) / span);
                y = BY + BH - 3 - ((int32_t)e->note - lo) * (BH - 6) / (hi - lo);
                cv_frame(x0, y - 1, x1 - x0 < 3 ? 3 : x1 - x0, 5, T_ACCENT);
                cv_rect(x0, y - 2, 1, 7, T_ACCENT);
            }
        }
    }
    if (st && len) {                                     /* the loop: its notes on the ring, the pointer at its phase */
        uint32_t ph = CAP_U(st == 1u ? (now - v.o) % (len << 4) : v.o);
        for (k = 0; k < v.ln; k++) {
            const cap_ev_t *e = &cap_loop[i][k];
            int32_t x, y, ang;
            if (!e->vel)
                continue;
            ang = (int32_t)(((uint32_t)e->t * 360u) / len);
            x = CX + (RM * uxl_sin(ang) + 128) / 256;
            y = CY - (RM * uxl_sin(ang + 90) + 128) / 256;
            cv_rect(x - 2, y - 2, 5, 5, st == 1u ? T_ACCENT : T_MID);
        }
        a = (int32_t)(((uint32_t)ph * 360u) / len);
        {
            int32_t x = CX + (RM * uxl_sin(a) + 128) / 256, y = CY - (RM * uxl_sin(a + 90) + 128) / 256;
            cv_rect(x - 4, y - 4, 9, 9, st == 1u ? T_TEXT : T_MID);
        }
    }
    if (nb >= 10u) {
        b[0] = (char)('0' + nb / 10u);
        b[1] = (char)('0' + nb % 10u);
        str_cpy(b + 2, " BARS", sizeof b - 2u);
    } else {
        b[0] = (char)('0' + nb);
        str_cpy(b + 1, nb == 1u ? " BAR" : " BARS", sizeof b - 1u);
    }
    uxl_text_c(120, 77, &AF_S, b, uxl.cap_view ? T_ACCENT : T_MID);
    if (v.full) {
        cv_frame(102, 146, 36, 14, T_TEXT);
        cv_text_on(106, 146, &AF_S, "FULL", T_TEXT, T_BG);
    } else if (st == 2u)                                 /* (LIVE and LOOP need no word: the pointer and the header chip say it) */
        uxl_text_c(120, 139, &AF_S, "PAUSE", T_MID);
    if (cap_gen[i] != g0)                                /* a write fell inside the frame: draw it again */
        ui.force = 1;
}
#endif
/* a title as a graph, 46 x 28 px: the envelope drawn, the part this arc sets bright (kind 0..3: ATK DEC SUS REL), or a sine
 * (kind 4: LFO RATE) */
#define UXL_GW 46
static void uxl_glyph(uint32_t kind, int32_t x, int32_t y, int hot)
{
    static const int8_t P[5][2] = {{0, 26}, {12, 0}, {24, 10}, {34, 10}, {45, 26}};
    uint16_t dim = T_LINE, on = hot ? T_ACCENT : T_TEXT;
    uint32_t i;
    int32_t t;
    if (kind == 4u) {
        int32_t px = 0, py = 13;
        for (i = 1; i < (uint32_t)UXL_GW; i++) {
            int32_t cy = 13 - (11 * uxl_sin((int32_t)(i * 720u / (uint32_t)UXL_GW)) + 128) / 256;
            for (t = 0; t < 3; t++)
                cv_line(x + px, y + py + t, x + (int32_t)i, y + cy + t, on);
            px = (int32_t)i;
            py = cy;
        }
        return;
    }
    for (i = 0; i < 4u; i++) {
        uint16_t c = i == kind ? on : dim;
        int32_t n = i == kind ? 3 : 1;
        for (t = 0; t < n; t++)
            cv_line(x + P[i][0], y + P[i][1] + t, x + P[i + 1][0], y + P[i + 1][1] + t, c);
        if (i == kind)
            for (t = 1; t < 3; t++)
                cv_line(x + P[i][0] + t, y + P[i][1], x + P[i + 1][0] + t, y + P[i + 1][1], c);
    }
}
#if CARAVEL_UX_EVOLVE
/* the EVOLVE groups: the verb's setup on the four arcs, the four verbs in the circle, the state in the row below */
static void uxl_paint_evolve(uint32_t ev)
{
    uint32_t b = UXL_EVB[ev], k;
    for (k = 0; k < 4u; k++) {
        char nm[16], val[24];
        int32_t cx = (k & 1u) == 0u ? 4 : 236, cy = k < 2u ? 46 : 150, f;
        int hot = uxl.hot_t && uxl.hot_k == k;
        uxl_ev_cap(b, k, nm, val);
        if (nm[0] == '-')
            continue;
        f = uxl_ev_frac(b, k);
        if (f >= 0)
            uxl_orbit_arc_f(k, f, b == 2u && k > 0u ? 38 : 0, hot);
        else
            uxl_orbit_arc_f(k, 0, 0, 0);
        if (cx < 120) {
            cv_text(cx, cy, &AF_M, nm, hot ? T_ACCENT : T_TEXT);
            cv_text(cx, cy + 21, &AF_S, val, uxl_hot(k));
        } else {
            uxl_text_r(cx, cy, &AF_M, nm, hot ? T_ACCENT : T_TEXT);
            uxl_text_r(cx, cy + 21, &AF_S, val, uxl_hot(k));
        }
    }
}
static void uxl_paint_ev_legend(uint32_t ev)    /* in the circle: the four verbs, numbered like the four lit keys (D5 E5 F5 G5) while LFO is held */
{
    uint32_t b = UXL_EVB[ev], i, ev_k = ev;
    int held = (fm1_in.buttons & (1u << panel.btn[B_LFO])) != 0;
    for (i = 0; i < 4u; i++) {
        char ln[20];
        static const char *const BN[4] = {"1 ", "2 ", "3 ", "4 "};
        uint32_t o = str_len(BN[i]);
        str_cpy(ln, BN[i], sizeof ln);
        str_cpy(ln + o, action_labels[b][i], sizeof ln - o);
#if CARAVEL_ELASTIC_P1
        if (b == ACT_BANK_ELASTIC && i == 1u)
            str_cpy(ln + o, uxl.el_bg & (1u << song.sel) ? "SPRING: BG" : "SPRING: ON", sizeof ln - o);
#endif
        uxl_text_c(120, 84 + (int32_t)i * 15, &AF_S, ln, held && (fm1_in.notes & (1u << action_keys[i])) ? T_ACCENT : T_TEXT);
    }
    {   /* the foot: LFO held; else which group a ROW means (RANDOM scope ROW, ELASTIC), else the hint */
        char ft[24];
        int row = (b == 0u && hp_rm.scope == HP_RM_ROW);
        if (held) {
            str_cpy(ft, "LFO: ", sizeof ft);
            str_cpy(ft + 5, UXL_EVNAME[ev_k], sizeof ft - 5u);
#if CARAVEL_ELASTIC_P1
        } else if (b == ACT_BANK_ELASTIC && hp_el1.on) {
            int cs = uxl_el_cur();
            const param_desc_t *cd = cs >= 0 ? track_desc(&trk[song.sel], hp_er.id[cs]) : 0;
            if (cd && (hp_el_setup_page & 1u) == 0u) {
                str_cpy(ft, "LIVE ON ", sizeof ft);
                str_cpy(ft + 8, cd->label, sizeof ft - 8u);
            } else {
                str_cpy(ft, (hp_el_setup_page & 1u) ? "ALL SPRINGS" : (uxl.el_bg & (1u << song.sel)) ? "TURNS ARE PLAIN" : "TURN ANY KNOB", sizeof ft);
            }
#endif
        } else if (row && (uxl.uset & (1u << song.sel))) {   /* a set made by hand: ROW is the four added last */
            str_cpy(ft, "SET ", sizeof ft);
            fmt_int(ft + 4, (int32_t)uxl_set_count(song.sel));
            str_cpy(ft + str_len(ft), " ROW:LAST 4", sizeof ft - str_len(ft));
        } else if (row) {
            str_cpy(ft, "ROW ", sizeof ft);
            str_cpy(ft + 4, UXL_AREA[uxl.ev_a].name, sizeof ft - 4u);
            if (uxl_area_ng(uxl.ev_a) > 1u) {
                uint32_t n = str_len(ft);
                ft[n] = ' ';
                ft[n + 1u] = (char)('A' + uxl.ev_g);
                ft[n + 2u] = 0;
            }
        } else {
            str_cpy(ft, b == 0u || b == ACT_BANK_JOURNEY ? "DOT = IN THE SET" : "HOLD LFO", sizeof ft);
        }
        uxl_text_c(120, 146, &AF_S, ft, held ? T_ACCENT : T_DIM);
    }
}
static void uxl_ev_status(uint32_t ev, char *o, uint32_t n)
{
    uint32_t b = UXL_EVB[ev];
    if (b == 0u) {
        static const char *const W[] = {"READY", "MORPHING", "FROZEN", "RETURN", "READY", "GO SNAP"};
        str_cpy(o, "RANDOM ", n);
        str_cpy(o + 7, hp_rm.state < 6u ? W[hp_rm.state] : "", n - 7u);
    } else if (b == ACT_BANK_JOURNEY) {
        str_cpy(o, !hp_j.on ? "JOURNEY OFF" : hp_j.paused ? "JOURNEY PAUSED" : hp_j.phase ? "JOURNEY REST" : "JOURNEY LEG", n);
        if (hp_j.on && n > 20u)
            str_cpy(o + str_len(o), hp_j.rev ? " <" : " >", n - str_len(o));
#if CARAVEL_ELASTIC_P1
    } else if (b == ACT_BANK_ELASTIC) {
        if (!hp_el1.on)
            str_cpy(o, "ELASTIC OFF: A1 ARMS", n);
        else
            perf_format(o, n, "ELASTIC %u/4 SPRINGS%s%s", hp_er_count(&hp_er), (uxl.el_bg & (1u << song.sel)) ? " BG" : "", hp_el_row_held ? " HOLD" : "");
#endif
    } else {
        if (ep.on)
            perf_format(o, n, "PAIR T%u+T%u", ep.a + 1u, ep.b + 1u);
        else
            str_cpy(o, "NO PAIR", n);
    }
}
static void uxl_paint_ev_row(uint32_t ev)       /* the row under the circle: the state, and the four snapshots of RANDOM and JOURNEY */
{
    char st[32];
    uint32_t b = UXL_EVB[ev], i;
    uxl_ev_status(ev, st, sizeof st);
    cv_free_text(4, 198, &AF_S, st, T_MID, T_BG, b == 0u || b == ACT_BANK_JOURNEY ? 168 : 232);
    if (b == 0u || b == ACT_BANK_JOURNEY)
        for (i = 0; i < 4u; i++) {
            int32_t x = 178 + (int32_t)i * 15;
            uint32_t sel = b == 0u ? action_snap_slot : hp_j.cur;
            if (hp_rm_snapshot_count(song.sel, i))
                cv_rect(x, 199, 10, 10, T_TEXT);
            else
                cv_frame(x, 199, 10, 10, T_MID);
            if (i == sel)
                cv_frame(x - 2, 197, 14, 14, T_ACCENT);
        }
}
#endif
#if CARAVEL_UX_EVOLVE
/* ------------------------------------------------- the BUILD pages (LFO family) */
static uint32_t uxl_evp_idx(void)                 /* RANDOM 0, JOURNEY 1, SPRING 2, SPRINGS 3 */
{
    uint32_t i, n = 0;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_EVOLVE) {
            if (&PAGES[i] == cur_page())
                return n;
            n++;
        }
    return 0;
}
static void uxl_evp_cap(uint32_t pg, uint32_t k, char *nm, char *val)
{
    str_cpy(nm, "--", 16);
    str_cpy(val, "--", 24);
    if (pg == 0u) {                                 /* BARS DEPTH SCOPE SHAPE */
        action_bank = 0;
        action_setup_caption(k, nm, val);
    } else if (pg == 1u) {                          /* SCOPE TRANSP WAIT */
        if (k < 2u) {
            action_bank = ACT_BANK_JOURNEY;
            action_setup_caption(k, nm, val);
        } else if (k == 2u) {
            str_cpy(nm, "WAIT", 16);
            perf_format(val, 24, "%u BAR", hp_j_waits[hp_j.wait]);
        }
    }
#if CARAVEL_ELASTIC_P1
    else if (pg == 2u) {                            /* what a new spring starts with: PULL RETURN RANGE BOUNCE */
        uxl_el_cap_ex(k, nm, val, 0, 1);
    } else {                                        /* the track's: COUPLE REFLECT SEED */
        uxl_el_cap_ex(k, nm, val, 1, 1);
    }
#endif
}
static void uxl_evp_knob(uint32_t slot, int32_t steps)
{
    uint32_t pg = uxl_evp_idx();
    if (!steps)
        return;
    if (pg == 0u) {
        action_bank = 0;
        action_setup_turn(slot, steps);
    } else if (pg == 1u) {
        if (slot < 2u) {
            action_bank = ACT_BANK_JOURNEY;
            action_setup_turn(slot, steps);
        } else if (slot == 2u) {
            hp_j_set_wait((uint32_t)clamp((int32_t)hp_j.wait + (steps > 0 ? 1 : -1), 0, 5));
        }
    }
#if CARAVEL_ELASTIC_P1
    else {
        uxl_el_setup_turn_ex(slot, steps, pg == 3u, 1);
    }
#endif
    ui.force = 1;
}
/* RANDOM, JOURNEY, SPRING START, ALL SPRINGS in PLAY's manner: the page's four values at the corners of the ring, the state in the middle (and,
 * for RANDOM and JOURNEY, the track's four snapshot slots as dots: amber = a snapshot is there, a grey ring = empty, a ring round it = the
 * chosen one), the hints under it. The page's own data (uxl_evp_cap, uxl_ev_status): only the drawing is new; it draws when something changed. */
static void uxl_ev_ring_paint(uint32_t pg, const char nm[4][16], const char val[4][24], const char *st)
{
    static const char *const H1[4] = {"LIVE IN PLAY: BARS, DEPTH", "LIVE IN PLAY: A1-A4, WAIT", "A NEW SPRING STARTS WITH THESE", "FOR EVERY SPRING OF THE TRACK"};
    static const char *const H2[4] = {"DOT ON A CARD = IN THE SET", "MOVE C: HOLD LFO + D5 E5 F5 G5", "EACH ONE HAS ITS OWN IN PLAY (MOVE D)", "COUPLE ALSO ON MOVE D, A4 HELD"};
    static const int16_t START[4] = {277, 7, 187, 97};
    uint32_t k, i;
    char f[32];
    cv_rect(0, 0, 240, 240, T_BG);
    for (k = 0; k < 4u; k++) {
        int32_t f = -1;
#if CARAVEL_ELASTIC_P1
        if (pg >= 2u)
            f = uxl_el_arc_frac(k, pg == 3u, 1);
#endif
        if (f >= 0)
            uxl_orbit_arc_f(k, f, 0, 0);
        else
            uxl_ring(120, 116, 58, 71, START[k], START[k] + 76, 0, 0, T_LINE, T_LINE);
    }
    for (k = 0; k < 4u; k++) {
        int32_t cx = (k & 1u) == 0u ? 4 : 236, cy = k < 2u ? 46 : 150;
        if (nm[k][0] == '-')
            continue;
        if (cx < 120) {
            cv_text(cx, cy, &AF_M, nm[k], T_TEXT);
            cv_text(cx, cy + 21, &AF_S, val[k], T_MID);
        } else {
            uxl_text_r(cx, cy, &AF_M, nm[k], T_TEXT);
            uxl_text_r(cx, cy + 21, &AF_S, val[k], T_MID);
        }
    }
    {   /* the state in the middle: one line, or two when it has a colon ("ELASTIC OFF: A1 ARMS") */
        char l1[40];
        const char *c = st;
        uint32_t n = 0;
        while (c[n] && c[n] != ':')
            n++;
        if (c[n] == ':' && n < sizeof l1) {
            memcpy(l1, st, n);
            l1[n] = 0;
            text_fit(f, sizeof f, l1, &AF_M, 104);
            uxl_text_c(120, 92, &AF_M, f, T_TEXT);
            text_fit(f, sizeof f, st + n + 2u, &AF_S, 104);
            uxl_text_c(120, 114, &AF_S, f, T_MID);
        } else {
            text_fit(f, sizeof f, st, text_w(&AF_M, st) <= 104 ? &AF_M : &AF_S, 104);
            uxl_text_c(120, pg < 2u ? 98 : 108, text_w(&AF_M, st) <= 104 ? &AF_M : &AF_S, f, T_TEXT);
        }
    }
    if (pg < 2u) {
        uint32_t sel = pg == 0u ? action_snap_slot : hp_j.cur;
        for (i = 0; i < 4u; i++) {
            int32_t x = 120 + ((int32_t)i * 2 - 3) * 9, y = 130;
            if (i == sel)
                cv_rrect(x - 7, y - 7, 14, 14, 7, T_ACCENT, T_BG);
            if (hp_rm_snapshot_count(song.sel, i)) {
                cv_rrect(x - 5, y - 5, 10, 10, 5, T_THEME, T_BG);
            } else {
                cv_rrect(x - 5, y - 5, 10, 10, 5, T_MID, T_BG);
                cv_rrect(x - 3, y - 3, 6, 6, 3, T_BG, T_MID);
            }
        }
    }
    text_fit(f, sizeof f, H1[pg], &AF_S, 226);
    cv_text(8, 192, &AF_S, f, T_MID);
    if (pg < 2u) {
        cv_text(8, 206, &AF_S, uxl.inter ? "HOLD ARP: DONE" : "SELECT TARGETS: HOLD ARP", uxl.inter ? T_ACCENT : T_TEXT);
        text_fit(f, sizeof f, H2[pg], &AF_S, 226);
        cv_text(8, 220, &AF_S, f, T_MID);
    } else {
        text_fit(f, sizeof f, H2[pg], &AF_S, 226);
        cv_text(8, 206, &AF_S, f, T_MID);
    }
}
static void uxl_ev_cards(void)
{
    static char nm[4][16], val[4][24], st[40];
    static uint32_t last;
    uint32_t pg = uxl_evp_idx(), k, h = 2166136261u, i;
    for (k = 0; k < 4u; k++)
        uxl_evp_cap(pg, k, nm[k], val[k]);
    uxl_ev_status(pg > 2u ? 2u : pg, st, sizeof st);
    for (k = 0; k < 4u; k++)
        for (i = 0; nm[k][i] || val[k][i]; i++)
            h = (h ^ (uint8_t)(nm[k][i] ^ (val[k][i] << 3))) * 16777619u;
    for (i = 0; st[i]; i++)
        h = (h ^ (uint8_t)st[i]) * 16777619u;
    for (i = 0; i < 4u; i++)
        h = (h ^ (hp_rm_snapshot_count(song.sel, i) ? 1u : 0u)) * 16777619u;
    h = (h ^ (pg == 0u ? action_snap_slot : hp_j.cur) ^ ((uint32_t)uxl.inter << 4) ^ (pg << 8)) * 16777619u;
    if (!ui.force && h == last)
        return;
    last = h;
    cv_begin(240, 120, T_BG);
    cv_bg = T_BG;
    cv_oy = -28;
    uxl_y0 = 28;
    uxl_ev_ring_paint(pg, (const char (*)[16])nm, (const char (*)[24])val, st);
    cv_oy = 0;
    cv_blit(0, 28);
    cv_begin(240, 92, T_BG);
    cv_bg = T_BG;
    cv_oy = -148;
    uxl_y0 = 148;
    uxl_ev_ring_paint(pg, (const char (*)[16])nm, (const char (*)[24])val, st);
    cv_oy = 0;
    cv_blit(0, 148);
}
static void uxl_ev_graph(void)                       /* (the whole page is drawn by uxl_ev_cards) */
{
}
#endif
/* HOME's pill: the HOME keycap and what holding it does, on the theme colour (BUILD's overview and PLAY's LEVELS screen) */
static void uxl_hold_pill(int32_t y, int32_t h, const char *word)
{
    int32_t w = kc_w(KC_HOME) + 8 + text_w(&AF_M, word), x = 120 - w / 2;
    cv_rrect(4, y, 232, h, 10, T_THEME, T_BG);
    x = cv_keycap(x, y + (h - KC_H) / 2, KC_HOME, T_INK, T_THEME, T_THEME);
    cv_text_on(x + 8, y + (h - 16) / 2, &AF_M, word, T_INK, T_THEME);
}
/* PLAY, HOME tapped: the two levels and how to switch (any input closes it) */
static void uxl_paint_levels(void)
{
    int32_t r;
    cv_rrect(4, 3, 50, 19, 5, T_ACCENT, T_BG);
    cv_text_on(11, 4, &AF_M, "PLAY", T_INK, T_ACCENT);
    cv_text(64, 5, &AF_M, "TWO LEVELS", T_TEXT);
    cv_rrect(4, 34, 232, 52, 8, T_SURF, T_BG);                       /* PLAY: here */
    cv_rrect(12, 42, 50, 19, 5, T_ACCENT, T_SURF);
    cv_text_on(19, 43, &AF_M, "PLAY", T_INK, T_ACCENT);
    cv_text(72, 43, &AF_M, "YOU ARE HERE", T_ACCENT);
    cv_text(12, 66, &AF_S, "PERFORM: ARCS, LOOPS, EFFECTS", T_MID);
    cv_rrect(4, 92, 232, 52, 8, T_SURF, T_BG);                       /* BUILD */
    cv_rrect(12, 100, 70, 19, 5, T_TEXT, T_SURF);
    for (r = 0; r < 12; r++) {
        int32_t hw = (r * 6) / 11;
        cv_rect(21 - hw, 102 + r, 2 * hw + 1, 1, T_THEME);
    }
    cv_rect(21, 106, 1, 4, T_INK);
    cv_rect(21, 111, 1, 1, T_INK);
    cv_text_on(30, 101, &AF_M, "BUILD", T_INK, T_TEXT);
    cv_text(92, 101, &AF_M, "MAKE AND SAVE", T_TEXT);
    cv_text(12, 124, &AF_S, "SOUNDS, PAGES, PROJECTS", T_MID);
    uxl_hold_pill(154, 36, "HOLD 1 SECOND");
    uxl_text_c(120, 197, &AF_S, "SWITCHES BUILD AND PLAY", T_TEXT);
    uxl_text_c(120, 218, &AF_S, "ANY KEY CLOSES THIS", T_DIM);
}
/* the LEVELS screen: any button (but HOME, whose tap closes it again, and whose hold switches), key or knob closes it; the turn is not passed on */
static void uxl_lvl_poll(uint32_t pressed, uint32_t notes)
{
    uint32_t i;
    if (!uxl.lvl)
        return;
    if (!uxl.play || fm1_ms - uxl.lvl_t > 30000u) {
        uxl.lvl = 0;
        ui.force = 1;
        return;
    }
    if ((pressed & ~(1u << panel.btn[B_HOME])) || notes) {
        uxl.lvl = 0;
        ui.force = 1;
    }
    for (i = 0; i < NE; i++)
        if (panel_enc(i)) {
            uxl.lvl = 0;
            ui.force = 1;
        }
}
static void uxl_paint(void)
{
    uxl_tg_t tg[4];
    uint32_t k, n = uxl_group(uxl.area, uxl_group_now(), tg), g = uxl_group_now();
    const uxl_area_t *ar = &UXL_AREA[uxl.area];
    char b[16];
    const char *u;
#if CARAVEL_UX_EVOLVE
    int ev;
    ev = uxl_ev_group();
    if (ev >= 0)
        uxl.ev = (uint8_t)ev;
#endif
    if (uxl.lvl) {
        uxl_paint_levels();
        return;
    }
    uxl_paint_head();
#if CARAVEL_UX_EVOLVE
    if (ev >= 0
#if CARAVEL_UX_CAPTURE
        && !uxl.cap_view
#endif
        )
        uxl_paint_evolve((uint32_t)ev);
#endif
    for (k = 0; k < 4u; k++) {                          /* the four arcs and their corners */
        hp_ref_t r;
        const param_desc_t *d;
#if CARAVEL_UX_CAPTURE
        if (uxl.cap_view)                               /* REC held: only K1 is shown, as the CAPTURE length (below) */
            break;
#endif
        int32_t v, cx = (k & 1u) == 0u ? 4 : 236, cy = k < 2u ? 46 : 150;
        const char *lab;
        int hot = uxl.hot_t && uxl.hot_k == k;
        if (k >= n || !tg[k].kind)                      /* (EVOLVE groups have no targets: n is 0) */
            continue;
#if CARAVEL_STASIS_EXPAND
        if (tg[k].kind == UXL_KTONE) {
            v = song.g[G_RTYPE] == 3u ? stasis_tone : 64;
            d = &UXL_STTONE_DESC;
            uxl_orbit_arc(k, d, v, hot);
            if (song.g[G_RTYPE] == 3u) fmt_int(b, v);
            else str_cpy(b, "N/A", sizeof b);
            u = "";
            r = (hp_ref_t){0};
        } else
#endif
        if (tg[k].kind == UXL_KFILT) {                  /* per-track FILTER or the other live FX macros */
            v = uxl_fx_get(tg[k].id);
            d = tg[k].id == 0u ? &UXL_FILT_DESC : &UXL_FX_DESC;
            uxl_orbit_arc(k, d, v, hot);
            uxl_fx_text(tg[k].id, b, sizeof b);
            u = "";
            r = (hp_ref_t){0};
        } else {
        r = uxl_ref(&tg[k]);
        d = hp_desc(r);
        if (!d)
            continue;
        v = hp_get(r);
        uxl_orbit_arc(k, d, v, hot);
        param_format(d, v, b, &u);
        }
#if CARAVEL_UX_EVOLVE && CARAVEL_ELASTIC_P1
        if (hp_el1.on && hp_el_free_track[song.sel] && r.kind == HP_TRACK) {   /* a spring on this arc: where it is now */
            int sl = uxl_el_slot_of(song.sel, hp_tid(r));
            if (sl >= 0 && d->max > d->min) {
                int32_t live = hp_elastic_p13_row_value(song.sel, (uint32_t)sl);
                uxl_orbit_spring(k, (hp_get(r) - d->min) * 76 / (d->max - d->min), (live - d->min) * 76 / (d->max - d->min));
            }
        }
#endif
        lab = tg[k].kind == UXL_KFILT ? uxl_fxm_label(tg[k].id) : tg[k].lab ? tg[k].lab : d->label;
        {
            /* PLAY intentionally mixes TRACK and GLOBAL targets in one area. */
            int global = tg[k].kind == HP_GLOBAL || tg[k].kind == UXL_KTONE;
            int gl = uxl.area == UXL_SHAPE && g == 0u ? (int)k : uxl.area == UXL_MOVE && g == 0u && k == 0u ? 4 : -1;
            int32_t vy = gl >= 0 ? cy + 32 : cy + 21;
            if (cx < 120) {
                if (gl >= 0) uxl_glyph((uint32_t)gl, cx, cy, hot); else cv_text(cx, cy, &AF_M, lab, hot ? T_ACCENT : global ? T_THEME : T_TEXT);
                if (global) cv_text(cx + 80, cy - 1, &AF_S, "G", T_THEME);
                cv_text(cx, vy, &AF_S, b, tg[k].kind == UXL_KFILT && !tg[k].id && !tf_eff(TSEL) ? T_DIM : uxl_hot(k));           /* the value smaller than the title: the arc shows it too */
#if CARAVEL_UX_EVOLVE
                if (uxl_morph_has(r)) cv_rrect(cx + text_w(&AF_S, b) + 5, vy + 3, 6, 6, 2, T_THEME, T_BG);
#endif
            } else {
                if (gl >= 0) uxl_glyph((uint32_t)gl, cx - UXL_GW, cy, hot); else uxl_text_r(cx, cy, &AF_M, lab, hot ? T_ACCENT : global ? T_THEME : T_TEXT);
                if (global) cv_text(cx - 85, cy - 1, &AF_S, "G", T_THEME);
                uxl_text_r(cx, vy, &AF_S, b, tg[k].kind == UXL_KFILT && !tg[k].id && !tf_eff(TSEL) ? T_DIM : uxl_hot(k));
#if CARAVEL_UX_EVOLVE
                if (uxl_morph_has(r)) cv_rrect(cx - text_w(&AF_S, b) - 11, vy + 3, 6, 6, 2, T_THEME, T_BG);
#endif
            }
        }
    }
#if CARAVEL_UX_EVOLVE
    if (ev >= 0) uxl_paint_ev_legend((uint32_t)ev);
    else if (fm1_in.buttons & (1u << panel.btn[B_LFO])) uxl_paint_ev_legend(uxl.ev);   /* LFO held in any area: the verbs of the verb shown last */
    else
#endif
#if CARAVEL_UX_CAPTURE
    uxl_paint_widget();
#endif
    if (uxl.pv) {                                       /* SAFE PREVIEW: the sound chosen: its name above the circle */
        char en[10], nm[16];
        uint32_t pk, pe = preset_at(uxl.pv_idx, &pk);
        int32_t w, x;
        if (pe == NENGINES) {
            str_cpy(en, "USER", sizeof en);
            up_name(pk, nm);
        } else {
            str_cpy(en, ENGINES[pe]->name, sizeof en);
            str_cpy(nm, ENGINES[pe]->presets[pk].name, sizeof nm);
        }
        w = text_w(&AF_S, en) + 8 + text_w(&AF_M, nm);
        x = 120 - w / 2;
        cv_text(x, 31, &AF_S, en, T_MID);
        cv_text(x + text_w(&AF_S, en) + 8, 27, &AF_M, nm, T_ACCENT);
        cv_rect(76, 80, 88, 76, T_BG);                  /* the circle's inside: the way to answer */
        uxl_text_c(120, 94, &AF_S, "PREVIEW", T_ACCENT);
        uxl_text_c(120, 112, &AF_S, "OCT+ LOAD", T_THEME);
        uxl_text_c(120, 128, &AF_S, "OCT- CANCEL", T_MID);
    }
#if CARAVEL_UX_CAPTURE
    else if (uxl.cap_view) {                          /* REC held: owner LENGTH + post-CAPTURE QUANT */
        static const char *const Q[6] = {"OFF", "1/16", "1/8", "1/4", "1/2", "1/1"};
        uint32_t i, track = cap_selected(), on = cap_li[track], mode;
        char label[] = "T1 LENGTH", qlab[] = "T1 QUANT";
        cap_view_t cv;
        label[1] = qlab[1] = (char)('1' + track);
        cap_view(track, &cv);
        mode = cv.ln ? (uint32_t)cv.requested : 0u;
        if (mode > 5u) mode = 0u;
        uxl_orbit_arc_f(0, (int32_t)on * 76 / 5, 0, 1);
        uxl_orbit_arc_f(1, (int32_t)mode * 76 / 5, 0, 1);
        cv_text(4, 46, &AF_M, label, T_ACCENT);
        cv_text(127, 46, &AF_M, qlab, T_THEME);
        for (i = 0; i < 6u; i++) {
            char sb[3];
            int32_t x = 4 + (int32_t)i * 19;
            uint32_t bars = CAP_BARS[i];
            if (bars >= 10u) {
                sb[0] = (char)('0' + bars / 10u);
                sb[1] = (char)('0' + bars % 10u);
                sb[2] = 0;
            } else {
                sb[0] = (char)('0' + bars);
                sb[1] = 0;
            }
            if (i == on) {
                cv_rrect(x - 1, 66, 19, 16, 3, T_ACCENT, T_BG);
                cv_text_on(x + 2, 67, &AF_S, sb, T_INK, T_ACCENT);
            } else {
                cv_text(x + 2, 67, &AF_S, sb, cv.ln ? T_DIM : T_MID);
            }
        }
        cv_rrect(143, 64, 64, 22, 6, T_SURF, T_BG);
        uxl_text_c(175, 67, &AF_M, Q[mode], cv.ln ? T_THEME : T_DIM);
        if (!cv.ln)
            cv_text(139, 93, &AF_S, "NO LOOP", T_DIM);
        else if (cv.quant_pending)
            cv_text(130, 93, &AF_S, "NEXT LOOP", T_ACCENT);
        else if (cv.st == 2u)
            cv_text(130, 93, &AF_S, "PAUSED", T_MID);
    }
#endif
#if CARAVEL_UX_EVOLVE
    else if (uxl.hot_t && ev >= 0 && uxl.hot_k < 4u) {   /* EVOLVE: the setup just turned: its name and value */
        char nm[16], val[24], ln[44];
        uxl_ev_cap(UXL_EVB[ev], uxl.hot_k, nm, val);
        str_cpy(ln, nm, sizeof ln);
        str_cpy(ln + str_len(ln), " ", sizeof ln - str_len(ln));
        str_cpy(ln + str_len(ln), val, sizeof ln - str_len(ln));
        uxl_text_c(120, 27, &AF_M, ln, T_ACCENT);
    }
#endif
#if CARAVEL_STASIS_EXPAND
    else if (uxl.hot_t && uxl.hot_k < n && tg[uxl.hot_k].kind == UXL_KTONE) {
        char ln[24] = "STASIS TONE ";
        fmt_int(ln + str_len(ln), stasis_tone);
        uxl_text_c(120, 27, &AF_M, ln, T_ACCENT);
    }
#endif
    else if (uxl.hot_t && uxl.hot_k < n && tg[uxl.hot_k].kind == UXL_KFILT) {
        char ln[28];
        str_cpy(ln, uxl_fxm_label(tg[uxl.hot_k].id), sizeof ln);
        str_cpy(ln + str_len(ln), " ", sizeof ln - str_len(ln));
        uxl_fx_text(tg[uxl.hot_k].id, ln + str_len(ln), sizeof ln - str_len(ln));
        uxl_text_c(120, 27, &AF_M, ln, T_ACCENT);
    }
    else if (uxl.hot_t && uxl.hot_k < n && tg[uxl.hot_k].kind) {   /* above the circle: the arc just turned, as a line */
        hp_ref_t r = uxl_ref(&tg[uxl.hot_k]);
        const param_desc_t *d = hp_desc(r);
        if (d) {
            char ln[28];
            param_format(d, hp_get(r), b, &u);
            str_cpy(ln, tg[uxl.hot_k].lab ? tg[uxl.hot_k].lab : d->label, sizeof ln);
            str_cpy(ln + str_len(ln), " ", sizeof ln - str_len(ln));
            str_cpy(ln + str_len(ln), b, sizeof ln - str_len(ln));
            if (u && u[0]) {
                str_cpy(ln + str_len(ln), " ", sizeof ln - str_len(ln));
                str_cpy(ln + str_len(ln), u, sizeof ln - str_len(ln));
            }
            uxl_text_c(120, 27, &AF_M, ln, T_ACCENT);
        }
    } else {                                            /* otherwise: the area, its group and the groups as dots */
        const char *gname = ar->g[g].name;
#if CARAVEL_UX_EVOLVE
        if (ev >= 0) gname = UXL_EVNAME[ev];
#endif
        int32_t wn = text_w(&AF_M, ar->name), wg = text_w(&AF_S, gname), wd = uxl_area_ng(uxl.area) > 1u ? (int32_t)uxl_area_ng(uxl.area) * 10 + 6 : 0;
        int32_t x = 120 - (wn + 8 + wg + wd) / 2;
        cv_text(x, 27, &AF_M, ar->name, T_TEXT);
        cv_text(x + wn + 8, 31, &AF_S, gname, T_MID);
        if (uxl_area_ng(uxl.area) > 1u)
            for (k = 0; k < uxl_area_ng(uxl.area); k++)
                cv_rrect(x + wn + 8 + wg + 8 + (int32_t)k * 10, 33, 6, 6, 3, k == g ? T_ACCENT : T_LINE, T_BG);
    }
#if !CARAVEL_UX_CAPTURE
    uxl_text_c(120, 108, &AF_S, "NO CAPTURE", T_DIM);
#endif
    /* F9: messages never paint over MUTE/LOOP/REC. Free text is bounded. */
    if (ui.msg_t)
        cv_free_text(4, 198, &AF_S, ui.msg, T_TEXT, T_BG, 232);
    else if (uxl_start_help())
        cv_free_text(4,198,&AF_S,uxl_start_help()==1u ? "HOME 1s: BUILD / REC+K1: LEN" :
            "SEQ700: CLEAR / FX HOLD: STASIS",T_TEXT,T_BG,232);
#if CARAVEL_UX_EVOLVE
    else if (ev >= 0)
        uxl_paint_ev_row((uint32_t)ev);
#endif
    else {
        char hs[40], rs[16];
        const char *pending = "";
#if CARAVEL_ACTION_BANKS
        static const char *const ACT[] = {"", " HOLD", " PREV", " NEXT", " RESET", " RESUME"};
        if (harmony_pending < NELEM(ACT)) pending = ACT[harmony_pending];
#endif
#if CARAVEL_ACTION_BANKS
        if (!pending[0] && harmony_hold) pending=" HOLD";
        if (!pending[0] && harmony_shift_pending) pending=" SHIFT";
#endif
        str_cpy(hs, harmony_on ? "HARM " : "HARM OFF", sizeof hs);
        if (harmony_on) {
            char chord[12];harmony_card_name(harmony_chord_at(harmony_bar),chord,1);
            str_cpy(hs + str_len(hs), chord, sizeof hs - str_len(hs));
            str_cpy(hs + str_len(hs), pending, sizeof hs - str_len(hs));
        }
        param_format(&GP[G_RTYPE], song.g[G_RTYPE], rs, &u);
        if(song.g[G_RTYPE]==3)str_cpy(rs,"STASIS",sizeof rs);
        str_cpy(hs + str_len(hs), "  REVERB ", sizeof hs - str_len(hs));
        str_cpy(hs + str_len(hs), rs, sizeof hs - str_len(hs));
#if CARAVEL_UX_EVOLVE
        if (hp_rm.state == HP_RM_RANDOM || hp_rm.state == HP_RM_RETURN || hp_rm.state == HP_RM_RECALL)
            str_cpy(hs + str_len(hs), "  RANDOM", sizeof hs - str_len(hs));
        else if (hp_j.on)
            str_cpy(hs + str_len(hs), "  JOURNEY", sizeof hs - str_len(hs));
#if CARAVEL_ELASTIC_P1
        else if (hp_el1.on)
            str_cpy(hs + str_len(hs), "  ELASTIC", sizeof hs - str_len(hs));
#endif
#endif
        cv_free_text(4, 198, &AF_S, hs, T_MID, T_BG, 232);
    }
    /* Four tracks: number, real engine abbreviation, M (stored mute), X
     * (temporary kill), and O / || (loop running / waiting). Empty means off. */
    for (k = 0; k < NTRK; k++) {
        char ts[12]; int32_t tx = 4 + (int32_t)k * 59;
        ts[0] = (char)('1' + k); ts[1] = ' '; ts[2] = 0;
        str_cpy(ts + 2, eng_abbr(ENGINES[trk[k].eng_req % NENGINES]->name), sizeof ts - 2u);
        cv_text(tx, 216, &AF_S, ts, k == song.sel ? T_ACCENT : T_TEXT);          /* (the selected track in the accent) */
        if (trk[k].p[P_MUTE]) cv_rect(tx, 228, 8, 1, T_TEXT);
        if (!perf_kill && ((perf_held | perf_latched) & PF_BIT(PF_M1 + k))) cv_line(tx+9, 224, tx+13, 228, T_TEXT);
#if CARAVEL_UX_CAPTURE
        cap_view_t cvk;
        cap_view(cap_owner(k), &cvk);
        if (cvk.st == 1u) cv_frame(tx+49, 219, 5, 5, T_TEXT);
        else if (cvk.st == 2u) { cv_rect(tx+49,219,1,5,T_TEXT); cv_rect(tx+52,219,1,5,T_TEXT); }
#endif
    }
    /* (variant B, E53: no area chips - the lit key and the head already name the area; the two rows above stay) */
}
static uint32_t uxl_cap_sig(void)
{
#if CARAVEL_UX_CAPTURE
    cap_view_t v;
    cap_view(cap_selected(), &v);
    return 7919u + (song.playing ? (perf_bars_q16 >> 11) * 31u : 0u) +
           v.st * 17u + v.rn * 3u + v.ln * 13u + v.quant * 43u +
           v.requested * 79u + v.quant_pending * 97u +
           cap_li[cap_selected()] * 5u + (uint32_t)song.playing +
           (uint32_t)uxl.cap_view * 977u;
#else
    return 0;
#endif
}
static uint32_t uxl_play_sig(void)
{
    uxl_tg_t tg[4];
    uint32_t k, s = uxl.area * 31u + uxl_group_now() * 7u + song.sel * 131u + (uint32_t)uxl.hot_t * 17u + uxl.hot_k + (uint32_t)song.playing * 3u +
                    (uint32_t)song.g[G_BPM] * 101u + (uint32_t)song.rec * 5u + (uint32_t)TSEL->p[P_MUTE] * 999u + (uint32_t)uxl.pv * 77u + (uint32_t)uxl.pv_idx * 3u + uxl_cap_sig() + (ui.msg_t ? str_hash(7u, ui.msg) : 0u);
    s = s * 33u + uxl_start_help() + uxl.lvl * 977u;
    for(k=0;k<4u;k++) s=s*33u+hroot_get(k)+hdur_get(k)*17u;
    s = s * 33u + harmony_on + harmony_bar * 7u + (uint32_t)song.g[G_RTYPE] * 131u;
#if CARAVEL_ACTION_BANKS
    s = s * 33u + harmony_pending + harmony_hold * 17u + (uint32_t)(int32_t)harmony_shift * 977u;
#endif
    for (k = 0; k < NTRK; k++) {
        s = s * 33u + trk[k].eng_req + trk[k].p[P_MUTE] * 17u;
#if CARAVEL_UX_CAPTURE
        s = s * 33u + uxl_cap_get(cap_owner(k), 0u) + uxl_cap_get(cap_owner(k), 1u) * 7u;
#endif
    }
#if CARAVEL_UX_EVOLVE
    {   /* EVOLVE: the buttons under LFO (the legend), the verbs' state, the snapshots */
        s = s * 33u + (fm1_in.buttons & (1u << panel.btn[B_LFO])) + (fm1_in.notes & uxl_vk_mask());
        s = s * 33u + hp_rm.state * 7u + hp_j.on * 3u + hp_j.paused * 5u + hp_j.phase * 9u + hp_j.cur * 11u + hp_j.wait * 13u + action_snap_slot * 17u + uxl.el_bg * 19u + hp_rm.scope * 23u + hp_rm.shape * 29u + hp_rm.bars * 31u + hp_rm.depth * 37u;
        for (k = 0; k < HP_RM_SNAP_SLOTS; k++)
            s = s * 33u + hp_rm_snapshot_count(song.sel, k);
        s = s * 33u + (uint32_t)ep.on * 3u + (uint32_t)ep.base;
#if CARAVEL_ELASTIC_P1
        s = s * 33u + hp_el1.on * 3u + hp_er_count(&hp_er) * 5u + hp_el_row_held * 7u + hp_el_setup_page * 11u;
#endif
    }
#endif
    uxl_group(uxl.area, uxl_group_now(), tg);
    for (k = 0; k < 4u; k++)
        if (tg[k].kind == UXL_KTONE) {
            s = s * 16777619u + (uint32_t)stasis_tone + 7u;
        } else if (tg[k].kind == UXL_KFILT) {
            s = s * 16777619u + (uint32_t)(tg[k].id ? (int32_t)perf_k[tg[k].id & 3u] : tf_eff(TSEL));
        } else if (tg[k].kind) {
            hp_ref_t r = uxl_ref(&tg[k]);
            s = s * 16777619u + (uint32_t)hp_get(r) + 7u * (uint32_t)(tg[k].id);
        }
    return s;
}
static void uxl_paint_any(void)
{
    uxl_paint();
}
static void uxl_draw_play(void)
{
    uint32_t s = uxl_play_sig();
    if (ui.force || s != uxl.sig) {
        uxl.sig = s;
        cv_begin(240, 120, T_BG);
        cv_bg = T_BG;
        cv_oy = 0;
        uxl_y0 = 0;
        uxl_paint_any();
        cv_blit(0, 0);
        cv_begin(240, 120, T_BG);
        cv_bg = T_BG;
        cv_oy = -120;
        uxl_y0 = 120;
        uxl_paint_any();
        cv_oy = 0;
        cv_blit(0, 120);
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (uxl.cap_auto && !uxl.cap_view && --uxl.cap_auto == 0)
        ui.force = 1;
    if (uxl.hot_t) {
        uxl.hot_t--;
        if (!uxl.hot_t) ui.force = 1;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = ui.force && uxl.hot_t ? 1 : 0;
}

/* BUILD's HOME: the four tracks, the selected one framed */
static void uxl_draw_overview(void)
{
    uint32_t t, sig = song.sel * 17u + (uint32_t)song.playing;
    char pn[16], nb[12];
    for (t = 0; t < NTRK; t++)
        sig = sig * 31u + (uint32_t)trk[t].p[P_MUTE] + (uint32_t)trk[t].eng_req * 7u + (uint32_t)trk[t].preset * 13u + (uint32_t)trk[t].p[P_LEVEL] * 3u;
#if CARAVEL_UX_EVOLVE
    for (t = 0; t < NTRK; t++) {
        sig = sig * 31u + hp_eng[t].state * 3u + hp_jn[t].on * 5u + (uint32_t)pair_member(&trk[t]) * 7u;
#if CARAVEL_ELASTIC_P1
        sig = sig * 31u + hp_el1_track[t].on * 11u;
#endif
#if CARAVEL_UX_CAPTURE
        sig = sig * 31u + uxl_cap_get(cap_owner(t), 0u) * 13u;
#endif
    }
#endif
    sig += ui.msg_t ? str_hash(7u, ui.msg) : 0u;
    draw_head();
    if (ui.force || sig != uxl.sig) {
        uxl.sig = sig;
        uint32_t half;
        for (half = 0; half < 2u; half++) {             /* two strips of two rows (the canvas is 124 rows high) */
            cv_begin(240, 84, T_BG);
            cv_bg = T_BG;
            for (t = half * 2u; t < half * 2u + 2u; t++) {
                const track_t *tr = &trk[t];
                const engine_t *e = ENGINES[tr->eng_req % NENGINES];
                int32_t y = (int32_t)(t - half * 2u) * 42;                 /* (the band starts at 28: the strip's rows 24..27 stay out of it; rows 42 apart: room for the HOME pill below) */
                int sel = t == song.sel;
                cv_rrect(3, y, 234, 38, 6, sel ? T_SEL : T_SURF, T_BG);
                nb[0] = 'T';
                nb[1] = (char)('1' + t);
                nb[2] = 0;
                cv_text(10, y + 10, &AF_M, nb, sel ? T_INK : T_THEME);
                sound_name(tr, pn);
                cv_text(40, y + 2, &AF_M, e->name, sel ? T_INK : T_TEXT);
                cv_text(40, y + 20, &AF_S, pn, sel ? T_INK : T_MID);
                cv_rect(150, y + 28, 82, 5, sel ? T_MID : T_LINE);   /* (the selected card is orange: its bar needs a track of its own) */
                cv_rect(150, y + 28, 82 * tr->p[P_LEVEL] / 127, 5, tr->p[P_MUTE] ? T_DIM : sel ? T_INK : T_THEME);
                if (tr->p[P_MUTE]) {
                    cv_rrect(194, y + 3, 38, 17, 4, sel ? T_INK : T_TEXT, sel ? T_SEL : T_SURF);
                    uxl_text_c(213, y + 4, &AF_S, "MUTE", sel ? T_SEL : T_INK);
                }
#if CARAVEL_UX_EVOLVE
                {   /* what runs on the track: its loop, a morph, a journey, springs, a pair (E68) */
                    const char *tag[4];
                    uint32_t nt = 0, q;
#if CARAVEL_UX_CAPTURE
                    if (uxl_cap_get(cap_owner(t), 0u) == 1u) tag[nt++] = "LOOP";
#endif
                    if (hp_eng[t].state == HP_RM_RANDOM || hp_eng[t].state == HP_RM_RETURN || hp_eng[t].state == HP_RM_RECALL) tag[nt++] = "RND";
                    if (hp_jn[t].on) tag[nt++] = "JRN";
#if CARAVEL_ELASTIC_P1
                    if (hp_el1_track[t].on) tag[nt++] = "ELA";
#endif
                    if (pair_member(&trk[t])) tag[nt++] = "PAIR";
                    for (q = 0; q < nt && q < 3u; q++) {
                        int32_t tx = 150 + (int32_t)q * 29;
                        cv_rrect(tx, y + 5, 27, 13, 3, sel ? T_INK : T_THEME, sel ? T_SEL : T_SURF);
                        cv_text_on(tx + 13 - text_w(&AF_S, tag[q]) / 2, y + 4, &AF_S, tag[q], sel ? T_SEL : T_INK, sel ? T_INK : T_THEME);
                    }
                }
#endif
            }
            cv_blit(0, 28 + (int32_t)half * 84);
        }
        cv_begin(240, 44, T_BG);                          /* (from 196: the rows end there, no gap left to what a layer drew) */
        cv_bg = T_BG;
        uxl_hold_pill(4, 38, "HOLD 1 SECOND: PLAY");
        cv_blit(0, 196);
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.hot_t)
        ui.hot_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}

#if CARAVEL_UX_EVOLVE
/* ---------------------------------------------------------------------------------------------------- BUILD, layout C (E69) ---
 * The two-bank pages with plain cards (DK_PLAIN, DK_EDIT): the bank the knobs act on is four rounded cards in 2 x 2 (K1 K2 / K3 K4,
 * as the arcs of PLAY), the other bank one dim row below it with its four values (E51: both banks stay in view). The page name is
 * in the orange bubble in the header, the track, engine and sound in the header's second line (ui_uxl.c); no footer. */
static int uxl_c_page(void)
{
    uint32_t bottom = 0, r;
    const dual_t *dd;
    if (ui.home || ui.menu || ui.confirm || name_on() || ui.uboot || ui.layer || uxl.play)
        return 0;
    if (!dual_page(cur_page())) {                       /* one bank: a page of plain cards (no graph, no engine picture) */
        const page_t *pg = cur_page();
        if (pg->graph != GR_NONE || (pg->scope != SC_TRACK && pg->scope != SC_GLOBAL && pg->scope != SC_ENGINE))
            return 0;
        if (pg->scope == SC_ENGINE && edit_has_picture())
            return 0;
        return 2;
    }
    dd = dual_of(cur_page(), &bottom);
    if (!dd || (dd->kind != DK_PLAIN && dd->kind != DK_EDIT))
        return 0;
    for (r = 0; r < 2u; r++) {
        const page_t *rp = dual_row_page(dd, r);
        if (rp && (rp->graph == GR_MOD || rp->graph == GR_INS
#if CARAVEL_REVERB_MINIMAL
                   || rp->graph == GR_STLAYER
#endif
#if CARAVEL_STASIS_EXPAND
                   || rp->graph == GR_STGRAIN
#endif
                   ))
            return 0;                                   /* (their cards are pictures or actions: the old layout) */
    }
    return 1;
}
/* a small ring, 270 degrees open at the bottom, 4 px wide, smooth edges (coverage per pixel) and round ends: the value from its start,
 * or from the middle for a bipolar one (f, z in 0..1000); on the card colour bgc */
static uint32_t uxl_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
static int32_t uxl_cap_cov(int32_t px, int32_t py, int32_t ccx, int32_t ccy)   /* a round end of radius 2: 0..2 (half pixels) */
{
    int32_t dx = px - ccx, dy = py - ccy, c;
    if (dx > 6 || dx < -6 || dy > 6 || dy < -6)
        return 0;
    c = 4 + 1 - (int32_t)uxl_isqrt((uint32_t)(dx * dx + dy * dy));
    return c < 0 ? 0 : c > 2 ? 2 : c;
}
static void uxl_ring_c(int32_t cx, int32_t cy, int32_t f, int32_t z, uint16_t cf, uint16_t bgc)
{
    int32_t x, y, sv = f * 270 / 1000, sz = z * 270 / 1000, lo = sv < sz ? sv : sz, hi = sv < sz ? sz : sv, k;
    int32_t ccx[3], ccy[3], ch[3] = {0, 270, 0};
    int fill = hi > lo;
    uint16_t tcol = ux_mix(bgc, T_MID, 32);                        /* the track: a lighter gray than the card */
    ch[0] = lo;
    ch[2] = hi;
    for (k = 0; k < 3; k++) {                                       /* the centres of the round ends (half pixels): track start, track end, fill start; the fill end below */
        int32_t a = 225 + (k == 1 ? 270 : k == 0 ? 0 : lo);
        ccx[k] = 2 * cx + 1 + (30 * uxl_sin(a) + 128) / 256;
        ccy[k] = 2 * cy + 1 - (30 * uxl_sin(a + 90) + 128) / 256;
    }
    for (y = cy - 19; y <= cy + 19; y++) {
        if (y < uxl_y0 || y > uxl_y0 + 123)
            continue;
        for (x = cx - 19; x <= cx + 19; x++) {
            int32_t px = 2 * x + 1, py = 2 * y + 1, dxh = px - (2 * cx + 1), dyh = (2 * cy + 1) - py, dh, rad, s, tc = 0, fc = 0, c, ea;
            uint16_t col;
            dh = (int32_t)uxl_isqrt((uint32_t)(dxh * dxh + dyh * dyh));
            rad = dh - 26 < 34 - dh ? dh - 26 : 34 - dh;
            rad = rad + 1 < 0 ? 0 : rad + 1 > 2 ? 2 : rad + 1;
            s = uxl_ang(dxh, dyh) - 225;
            if (s < 0)
                s += 360;
            if (rad && s <= 270) {
                tc = rad;
                if (fill && s >= lo && s <= hi)
                    fc = rad;
            }
            c = uxl_cap_cov(px, py, ccx[0], ccy[0]);
            tc = c > tc ? c : tc;
            c = uxl_cap_cov(px, py, ccx[1], ccy[1]);
            tc = c > tc ? c : tc;
            if (fill) {
                c = uxl_cap_cov(px, py, ccx[2], ccy[2]);
                fc = c > fc ? c : fc;
                ea = 225 + hi;
                c = uxl_cap_cov(px, py, 2 * cx + 1 + (30 * uxl_sin(ea) + 128) / 256, 2 * cy + 1 - (30 * uxl_sin(ea + 90) + 128) / 256);
                fc = c > fc ? c : fc;
            }
            if (!tc && !fc)
                continue;
            col = tc ? ux_mix(bgc, tcol, tc * 50) : bgc;
            if (fc)
                col = ux_mix(col, cf, fc * 50);
            cv_pset(x, y, col);
        }
    }
}
static const char *uxl_c_title(void)               /* the name of the bank the knobs act on (the header's bubble) */
{
    uint32_t bottom = 0;
    const dual_t *dd = dual_page(cur_page()) ? dual_of(cur_page(), &bottom) : 0;
    const char *nm = dd ? (bottom ? dd->hb : dd->ht) : cur_page()->title;
    return text_w(&AF_S, nm) > 84 ? cur_page()->title : nm;    /* (a long description: the page's own name; line 2 needs the room) */
}
static const param_desc_t *uxl_c_card(const dual_t *dd, const page_t *rp, uint32_t row, uint32_t col, int32_t *v)
{
    int16_t *vp = 0;
    const param_desc_t *d;
    if (dd)
        return dual_card(dd, rp, row, col, v);
    d = rp ? page_desc(rp, col, &vp) : 0;
    *v = d && vp ? *vp : 0;
    return d;
}
static void uxl_cards_c_paint(const dual_t *dd, uint32_t act, const page_t *const rps[2])
{
    uint32_t col, other = 1u - act;
    char val[12], line[24];
    const char *unit;
    uint16_t cs = ux_mix(T_BG, T_MID, 15), cd = ux_mix(T_BG, T_MID, 9);   /* neutral grays of the palette (T_SURF is its tinted one) */
    cv_rect(0, 0, 240, 240, T_BG);
    for (col = 0; col < 4u; col++) {
        int32_t v, x = 6 + (int32_t)(col & 1u) * 118, y = 36 + (int32_t)(col >> 1) * 62;
        const param_desc_t *d = uxl_c_card(dd, rps[act], act, col, &v);
        int hot = ui.hot_t && ui.hot_col == col, ok = d && d->label && d->label[0] && d->label[0] != '-';
        cv_rrect(x, y, 110, 56, 9, cs, T_BG);
        cv_rect(x + 4, y + 14, 2, 28, ok ? (rps[act] && rps[act]->scope == SC_GLOBAL ? T_THEME : T_ACCENT) : T_LINE);
        if (!ok) {
            cv_text(x + 12, y + 18, &AF_M, "--", T_DIM);
            continue;
        }
        if (rps[act] && fx_delay_suspended(rps[act], col)) {
            cv_text(x + 12, y + 10, &AF_M, d->label, T_DIM);
            cv_text(x + 12, y + 32, &AF_S, "OFF: STASIS", T_DIM);
            continue;
        }
        cv_text(x + 12, y + 10, &AF_M, d->label,
                hot ? T_ACCENT : rps[act] && rps[act]->scope == SC_GLOBAL ? T_THEME : T_TEXT);
        /* Corner is owned by INTERACT/morph; the small G stays below it. */
        if (rps[act] && rps[act]->scope == SC_GLOBAL)
            cv_text(x + 98, y + 43, &AF_S, "G", T_THEME);
        {
            uint8_t nm = fmt_named;
            hp_ref_t r;
            param_format(d, v, val, &unit);
            fmt_named = nm;
            str_cpy(line, val, sizeof line);
            if (unit && unit[0]) {
                str_cpy(line + str_len(line), " ", sizeof line - str_len(line));
                str_cpy(line + str_len(line), unit, sizeof line - str_len(line));
            }
            cv_text(x + 12, y + 32, &AF_S, line, hot ? T_ACCENT : T_MID);
            if (uxl_inter_ref(col, &r)) {                         /* the mark in the corner: INTERACT's set, or what a morph moves */
                if (uxl.inter) {
                    if (uxl_inter_in(col)) cv_rect(x + 96, y + 4, 9, 9, T_ACCENT);
                    else cv_frame(x + 96, y + 4, 9, 9, T_TEXT);
                } else if (uxl_morph_has(r)) {
                    cv_rrect(x + 100, y + 5, 5, 5, 2, T_THEME, cs);
                }
            } else if (uxl.inter) {
                cv_rect(x + 98, y + 8, 6, 1, T_MID);
            }
        }
        {
            int32_t span = d->max - d->min, f = span > 0 ? (v - d->min) * 1000 / span : 0, z = 0;
            if (d->min < 0 && d->max > 0)
                z = (0 - d->min) * 1000 / span;
            uxl_ring_c(x + 84, y + 28, f, z, hot ? T_ACCENT : T_THEME, cs);
        }
    }
    if (!dd)
        return;                                                    /* (one bank: nothing below the cards) */
    cv_rrect(6, 164, 228, 68, 9, cd, T_BG);                    /* the other bank: its name (blue, the next page), its four values */
    cv_text(16, 169, &AF_S, other ? dd->hb : dd->ht, T_ACCENT);
    cv_text(222, 169, &AF_S, ">", T_ACCENT);
    for (col = 0; col < 4u; col++) {
        int32_t v;
        const param_desc_t *d = uxl_c_card(dd, rps[other], other, col, &v);
        int32_t x = 16 + (int32_t)col * 54;
        if (!d || !d->label || !d->label[0] || d->label[0] == '-') {
            cv_text(x, 190, &AF_S, "--", T_DIM);
            continue;
        }
        if (rps[other] && fx_delay_suspended(rps[other], col)) {
            cv_text(x, 188, &AF_S, d->label, T_DIM);
            cv_text(x, 205, &AF_S, "OFF", T_DIM);
            continue;
        }
        {
            uint8_t nm = fmt_named;
            param_format(d, v, val, &unit);
            fmt_named = nm;
        }
        cv_text(x, 188, &AF_S, d->label,
                rps[other] && rps[other]->scope == SC_GLOBAL ? T_THEME : T_MID);
        if (rps[other] && rps[other]->scope == SC_GLOBAL && text_w(&AF_S, d->label) < 37)
            cv_text(x + 40, 188, &AF_S, "G", T_THEME);
        cv_text(x, 205, text_w(&AF_M, val) <= 50 ? &AF_M : &AF_S, val, T_TEXT);
    }
}
static void uxl_cards_c(void)
{
    uint32_t bottom = 0, row, col, sig;
    const dual_t *dd = dual_page(cur_page()) ? dual_of(cur_page(), &bottom) : 0;
    const page_t *rps[2] = {dd ? dual_row_page(dd, 0) : cur_page(), dd ? dual_row_page(dd, 1) : 0};
    sig = 0xC0DE00u + bottom + (uint32_t)ui.page * 257u + song.sel * 31u + (uint32_t)uxl.inter * 7u + uxl.uset * 13u + (uint32_t)ui.hot_t * 17u + ui.hot_col +
          hp_rm.scope * 5u + (uint32_t)(uxl.uset & (1u << song.sel)) * 3u;
    for (row = 0; row < (dd ? 2u : 1u); row++)
        for (col = 0; col < 4u; col++) {
            int32_t v;
            const param_desc_t *d = uxl_c_card(dd, rps[row], row, col, &v);
            sig = sig * 33u + (d ? (uint32_t)v : 0x7FFFu);
        }
    if (!ui.force && ui.graph_sig == sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, 120, T_BG);                                       /* two strips under the header (the canvas is 124 rows high) */
    cv_bg = T_BG;
    cv_oy = -28;
    uxl_y0 = 28;
    uxl_cards_c_paint(dd, bottom, rps);
    cv_oy = 0;
    cv_blit(0, 28);
    cv_begin(240, 92, T_BG);
    cv_bg = T_BG;
    cv_oy = -148;
    uxl_y0 = 148;
    uxl_cards_c_paint(dd, bottom, rps);
    cv_oy = 0;
    cv_blit(0, 148);
}
#endif

/* ---- BUILD in PLAY's manner: the ENV and LFO pages (the owner: BUILD is to be rebuilt to match PLAY, the boxes gone) ----
 * The page's four parameters as PLAY shows an area: the four arcs on the ring (K1 K2 above, K3 K4 below), the name and the value at
 * each arc's corner, and in the middle the picture of the page: the envelope for ENV, the wave for LFO. The data and the knobs are the
 * page's own (uxl_c_card, the dual page's row that is current): only the drawing is new. The other row is the next page of the family
 * (ENV, ENV DEST; LFO, LFO SYNC), named at the foot. */
static int uxl_ring_page(void)
{
    const dual_t *dd;
    if (uxl.play || ui.home || ui.menu || ui.confirm || name_on() || ui.uboot || ui.layer)
        return 0;
    if (uxl_c_page())                                   /* every page of plain cards (EDIT, FX, SCL, ARP, VOICE, CLOCK ...): the ring too */
        return 1;
    if (!dual_page(cur_page()))
        return 0;
    dd = dual_of(cur_page(), 0);
    if (!dd)
        return 0;
    if (dd->kind == DK_ENV || dd->kind == DK_LFO || dd->kind == DK_LFO2 || dd->kind == DK_INS || dd->kind == DK_LINK || dd->kind == DK_HARMONY || dd->kind == DK_CHEXT ||
        dd->kind == DK_GRAPH)                           /* (the SLICER: its pattern in the middle) */
        return 1;
    if (dd->kind == DK_PLAIN || dd->kind == DK_EDIT)           /* the pages the cards left out: MOD (a matrix slot's four values), STASIS' GRAIN and LAYER and the like */
        return 1;
    return 0;
}
/* "3/8": where this page is among its family's (the header's dots, as a number) */
static void uxl_page_pos(char *out, uint32_t cap)
{
    uint32_t i, n = 0, at = 0;
#if CARAVEL_STASIS_EXPAND
    /* The FX page counter must follow the real navigation sequence rather
     * than PAGES[] indices (AFTER and GRAV have append-only physical IDs). */
    if (cur_page()->fam == FAM_FX) {
        uint8_t pages[NPAGES];
        n = stasis_fx_pages(pages);
        for (i = 0; i < n; i++)
            if (pages[i] == ui.page) at = i + 1u;
        fmt_int(out, (int32_t)(at ? at : 1u));
        str_cpy(out + str_len(out), "/", cap - str_len(out));
        fmt_int(out + str_len(out), (int32_t)(n ? n : 1u));
        return;
    }
#endif
    const page_t *cp = dual_top_page(cur_page());
    if (!cp)
        cp = cur_page();
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == cur_page()->fam && page_visible(i) && !dual_bottom(&PAGES[i])) {
            n++;
            if (&PAGES[i] == cp)
                at = n;
        }
    fmt_int(out, (int32_t)(at ? at : 1));
    str_cpy(out + str_len(out), "/", cap - str_len(out));
    fmt_int(out + str_len(out), (int32_t)(n ? n : 1));
}
/* Harmony map in BUILD's existing orbit center.  These are NOTE/KEY relationships,
 * not audio routing.  A line style describes the role; its brightness describes
 * whether that configured relationship is presently usable.  Never store UI state. */
#if !FELUCCA_FM4
enum { UXL_HM_NONE, UXL_HM_FOLLOW, UXL_HM_CHORD, UXL_HM_EXT, UXL_HM_KEY };
static const int16_t uxl_hm_x[5] = {91, 149, 91, 149, 120}; /* 4 tracks, then HARM bus */
static const int16_t uxl_hm_y[5] = {87, 87, 145, 145, 116};

static uint32_t uxl_hm_view(const dual_t *dd, uint32_t row)
{
    if (dd && dd->kind == DK_PLAIN && str_eq(dd->top, "KEY"))
        return row ? UXL_HM_CHORD : UXL_HM_KEY;
    if (dd && (dd->kind == DK_LINK || dd->kind == DK_HARMONY || dd->kind == DK_CHEXT))
        return UXL_HM_CHORD;
    return UXL_HM_NONE;
}
static int uxl_hm_key_ready(uint32_t track, uint32_t source)
{
    const track_t *t = &trk[track];
    if (source >= NTRK || source == track || link_src[source])
        return 0;                                      /* no self-link or key chain */
    if (harmony_key_mode && (t->p[P_CSRC] ||
        (harmony_follow[track] && (t->p[P_CHRD] == CH_OFF || t->p[P_CHRD] == CH_BUS))))
        return 0;                                      /* stored but suspended */
    return 1;
}
static uint32_t uxl_hm_sig(uint32_t view)
{
    uint32_t i, sig = harmony_on + harmony_key_mode * 7u + song.sel * 31u + view * 131u;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        int32_t src = t->p[P_CSRC];
        sig = sig * 33u + harm_role(t) * 17u + harmony_follow[i] * 3u +
              (uint32_t)t->p[P_CHRD] * 7u + (uint32_t)(src + 1) * 13u + link_src[i];
        if (src >= 1 && src <= 5)
            sig = sig * 33u + ext_state(t, src, 0);     /* WAIT/READY changes are visible */
    }
    return sig;
}
static int32_t uxl_hm_sign(int32_t v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
/* Dashes are reserved for KEY inheritance, never used merely for an inactive route. */
static void uxl_hm_stroke(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                           uint32_t style, uint16_t c)
{
    int32_t dx = x1 - x0, dy = y1 - y0, n = dx < 0 ? -dx : dx, t = dy < 0 ? -dy : dy;
    if (t > n) n = t;
    if (style == UXL_HM_KEY && n) {
        int32_t j;
        for (j = 0; j < n; j += 7) {
            int32_t z = j + 4 < n ? j + 4 : n;
            cv_line(x0 + dx * j / n, y0 + dy * j / n,
                    x0 + dx * z / n, y0 + dy * z / n, c);
        }
    } else {
        cv_line(x0, y0, x1, y1, c);
        if (style == UXL_HM_CHORD) {                    /* two lines = chord voices */
            int32_t ox = (dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy) ? 0 : 3;
            int32_t oy = ox ? 0 : 3;
            cv_line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, c);
        }
        if (style == UXL_HM_FOLLOW) {                   /* single line + arrow */
            int32_t sx = uxl_hm_sign(dx), sy = uxl_hm_sign(dy);
            if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) {
                cv_line(x1 - 4 * sx, y1 - 3, x1, y1, c);
                cv_line(x1 - 4 * sx, y1 + 3, x1, y1, c);
            } else {
                cv_line(x1 - 3, y1 - 4 * sy, x1, y1, c);
                cv_line(x1 + 3, y1 - 4 * sy, x1, y1, c);
            }
        }
        if (style == UXL_HM_EXT) {                      /* plus = extension tones */
            int32_t mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
            cv_rect(mx - 1, my - 4, 3, 9, c);
            cv_rect(mx - 4, my - 1, 9, 3, c);
        }
    }
}
static void uxl_hm_edge(uint32_t source, uint32_t dest, uint32_t style, uint16_t c)
{
    int32_t x0 = uxl_hm_x[source], y0 = uxl_hm_y[source];
    int32_t x1 = uxl_hm_x[dest], y1 = uxl_hm_y[dest];
    int32_t sx = uxl_hm_sign(x1 - x0), sy = uxl_hm_sign(y1 - y0);
    if (source == dest || source > NTRK || dest >= NTRK)
        return;
    /* Shorten both ends to leave room for the 20x18 track badges and bus. */
    x0 += sx * (source == NTRK ? 11 : 10);
    y0 += sy * (source == NTRK ? 9 : 9);
    x1 -= sx * 10;
    y1 -= sy * 9;
    uxl_hm_stroke(x0, y0, x1, y1, style, c);
}
static void uxl_hm_paint(const dual_t *dd, uint32_t row, uint32_t view)
{
    uint32_t pass, i;
    if (!view) return;
    /* Other tracks first, selected track last. Nodes are always at fixed locations. */
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < NTRK; i++) {
            const track_t *t = &trk[i];
            uint32_t role = harm_role(t), source = NTRK, style = UXL_HM_NONE;
            int ready = 0;
            int32_t src = t->p[P_CSRC];
            if ((i == song.sel) != (pass == 1u)) continue;
            if (view == UXL_HM_KEY) {
                if (link_src[i] >= 1 && link_src[i] <= NTRK) {
                    source = link_src[i] - 1u;
                    style = UXL_HM_KEY;
                    ready = uxl_hm_key_ready(i, source);
                }
            } else if (role == HROLE_FOLLOW) {
                style = UXL_HM_FOLLOW; ready = harmony_on;
            } else if (role == HROLE_CHORD && t->p[P_CHRD] == CH_BUS) {
                style = UXL_HM_CHORD; ready = harmony_on;
            } else if (role == HROLE_EXT && src >= 1 && src <= 5) {
                style = UXL_HM_EXT;
                source = src == 1 ? NTRK : (uint32_t)src - 2u;
                ready = ext_state(t, src, 0) == EXT_ST_OK;
            }
            if (style != UXL_HM_NONE)
                uxl_hm_edge(source, i, style,
                    !ready ? T_DIM : i == song.sel ? T_ACCENT : T_MID);
        }
    if (view != UXL_HM_KEY) {
        cv_rrect(111, 108, 18, 16, 4, harmony_on ? T_THEME : T_DIM, T_BG);
        uxl_text_c(120, 109, &AF_S, "H", harmony_on ? T_THEME : T_DIM);
    } else
        uxl_text_c(120, 110, &AF_S, "KEY", T_MID);
    for (i = 0; i < NTRK; i++) {
        int32_t x = uxl_hm_x[i], y = uxl_hm_y[i], selected = i == song.sel;
        char name[3] = {'T', (char)('1' + i), 0};
        cv_rrect(x - 11, y - 9, 22, 18, 8,
                 selected ? T_ACCENT : T_MID, selected ? T_ACCENT : T_BG);
        uxl_text_c(x, y - 6, &AF_S, name, selected ? T_INK : T_TEXT);
    }
    /* Retain the EXT detail's source-chord readout, only move it below the map. */
    if (dd && dd->kind == DK_CHEXT && row) {
        ext_info_t ei;
        char b[20] = "SRC --";
        ext_info(song.sel, &ei);
        if (ei.have_chord && ei.state != EXT_ST_OFF) {
            b[0] = 'S'; b[1] = 'R'; b[2] = 'C'; b[3] = ' '; b[4] = 0;
            chord_name(b + 4, ei.root, ei.mask);
        }
        uxl_text_c(120, 158, &AF_S, b, ei.have_chord ? T_TEXT : T_DIM);
    } else if (dd && dd->kind == DK_LINK && row && harm_role(TSEL) == HROLE_EXT)
        uxl_text_c(120, 158, &AF_S, "K4: RETRIG", T_MID);
    else
        uxl_text_c(120, 158, &AF_S, view == UXL_HM_KEY ? "KEY FROM" : "HARM ROUTE", T_MID);
}
#endif /* !FELUCCA_FM4 */

static void uxl_ring_build_paint(const dual_t *dd, uint32_t act, const page_t *const rps[2])
{
    int pic = dd && (dd->kind == DK_ENV || dd->kind == DK_LFO || (dd->kind == DK_INS && !act) || dd->kind == DK_GRAPH);   /* (INSERT: the type's curve; SLICER: the pattern) */
    uint32_t k;
    char val[12], line[24], pn[16];
    const char *unit;
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    cv_rect(0, 0, 240, 240, T_BG);
    if (pic) {
        if (dd->kind != DK_INS && dd->kind != DK_GRAPH)
            cv_rect(88, 96, 64, 40, T_SURF);                          /* the middle: PLAY's capture band here, the page's picture in BUILD */
        uxl_ring(120, 116, 40, 48, 0, 359, 0, 359, T_LINE, T_LINE);
        if (dd->kind == DK_GRAPH) {                                    /* SLICER: the 16 steps (an open step a lit bar, a closed one what DEPTH leaves, STUT dashes) */
            uint32_t i, pat = sl_pattern(TSEL), mode = (uint32_t)TSEL->p[P_SLCR], cur = sl[TSEL - trk].idx;
            int32_t hb = 16, open = hb - TSEL->p[P_SLDEPTH] * hb / 127;
            for (i = 0; i < 16u; i++) {
                int32_t x = 88 + (int32_t)i * 4, y = 108;
                uint16_t col = mode != SL_OFF && i == cur ? T_ACCENT : mode == SL_OFF ? T_DIM : T_THEME;
                if ((pat >> i) & 1u)
                    cv_rect(x, y, 3, hb, col);
                else if (mode == SL_STUT) {
                    int32_t yy;
                    for (yy = y; yy < y + hb; yy += 3)
                        cv_rect(x, yy, 3, 1, col);
                } else {
                    cv_rect(x, y + hb - 2, 3, 2, i == cur && mode != SL_OFF ? T_ACCENT : T_RAISE);
                    if (open > 2)
                        cv_rect(x, y + hb - open, 3, open, i == cur && mode != SL_OFF ? T_ACCENT : T_RAISE);
                }
            }
            {
                static const char *const MD[3] = {"OFF", "GATE", "STUT"};
                uxl_text_c(120, 128, &AF_S, MD[mode % 3u], mode == SL_OFF ? T_DIM : T_MID);
            }
        } else if (dd->kind == DK_INS)
            insert_curve(TSEL, T_THEME, 95, 50, 116, 14);
        else if (dd->kind == DK_ENV)
            env_icon(92, 101, 56, 30, TSEL->p[rps[0]->id[0]], TSEL->p[rps[0]->id[1]], TSEL->p[rps[0]->id[2]], TSEL->p[rps[0]->id[3]], T_THEME);
        else
            lfo2_wave_icon(94, 101, 52, 30, TSEL->p[P_LWAVE], T_THEME);
    } else {
#if !FELUCCA_FM4
        /* A compact relationship map replaces only the previously free center
         * of KEY/CHORD/HARMONY/HARM/EXT. Orbit arcs, labels and tabs stay put. */
        uxl_hm_paint(dd, act, uxl_hm_view(dd, act));
#endif
    }
    for (k = 0; k < 4u; k++) {
        int32_t v, cx = (k & 1u) == 0u ? 4 : 236, cy = k < 2u ? 46 : 150;
        const param_desc_t *d = uxl_c_card(dd, rps[act], act, k, &v);
        int hot = ui.hot_t && ui.hot_col == k, ok = d && d->label && d->label[0] && d->label[0] != '-';
        hp_ref_t r;
        int off = 0, chx;
        chext_view_t cv;
#if CARAVEL_REVERB_MINIMAL
        if (rps[act] && rps[act]->graph == GR_STLAYER) {
            if (k == 3u) {                                             /* LAYER's fourth card: the action (ADD, BLEND, CANCEL), the state, what OCT+ does */
                const char *an = stasis_layer_can_cancel() ? "CANCEL" : song.g[G_STMODE] == 1 ? "BLEND" : "ADD";
                if (cx < 120) {
                    cv_text(cx, cy, &AF_M, an, T_TEXT);
                    cv_text(cx, cy + 21, &AF_S, stasis_layer_status(), T_ACCENT);
                } else {
                    uxl_text_r(cx, cy, &AF_M, an, T_TEXT);
                    uxl_text_r(cx, cy + 21, &AF_S, stasis_layer_status(), T_ACCENT);
                }
                continue;
            }
            off = (k == 1u && song.g[G_STMODE] == 1) || (k == 2u && song.g[G_STMODE] != 1);
        }
#endif
        if (!ok && dd && dd->kind == DK_LINK && !act && k == 2u) {      /* LINK's KEY with no source track: named, dim */
            cv_text(cx, cy, &AF_M, "KEY", T_DIM);
            cv_text(cx, cy + 21, &AF_S, "--", T_DIM);
            continue;
        }
        chx = dd && dd->kind == DK_CHEXT && act;
        if (chx) {
            chext_view(k, &cv);
            off = cv.disabled;
        }
        if (!ok)
            continue;
        if (rps[act] && fx_delay_suspended(rps[act], k)) {
            if (cx < 120) {
                cv_text(cx, cy, &AF_M, d->label, T_DIM);
                cv_text(cx, cy + 21, &AF_S, "OFF: STASIS", T_DIM);
            } else {
                uxl_text_r(cx, cy, &AF_M, d->label, T_DIM);
                uxl_text_r(cx, cy + 21, &AF_S, "OFF: STASIS", T_DIM);
            }
            continue;
        }
        if (!off && !(chx && k == 3u))                                  /* (NOTES is a display: no arc) */
            uxl_orbit_arc(k, d, v, hot);
        {
            uint8_t nm = fmt_named;
            param_format(d, v, val, &unit);
            if (dd && dd->kind == DK_HARMONY && !act && harmony_edit_dur != 0u)
                unit = harmony_card_text(k, val);
            fmt_named = nm;
        }
        if (dd && dd->kind == DK_CHEXT && act)                          /* CHORD EXT: its own text (K4: the notes the follower takes) */
            str_cpy(val, cv.value, sizeof val), unit = "";
        str_cpy(line, val, sizeof line);
        if (unit && unit[0]) {
            str_cpy(line + str_len(line), " ", sizeof line - str_len(line));
            str_cpy(line + str_len(line), unit, sizeof line - str_len(line));
        }
        {   /* Scope is per visible page row: HARMONY, REVERB and tempo
             * are global, whereas HARM/LINK, sends and engines are not. */
            int global = rps[act] && rps[act]->scope == SC_GLOBAL;
            uint16_t lc = off ? T_DIM : hot ? T_ACCENT : global ? T_THEME : T_TEXT;
            if (cx < 120) {
                cv_text(cx, cy, &AF_M, d->label, lc);
                cv_text(cx, cy + 21, &AF_S, line, off ? T_DIM : hot ? T_ACCENT : T_MID);
            } else {
                uxl_text_r(cx, cy, &AF_M, d->label, lc);
                uxl_text_r(cx, cy + 21, &AF_S, line, off ? T_DIM : hot ? T_ACCENT : T_MID);
            }
            /* G is independent of the knob label and avoids renaming it. */
            if (global) cv_text(cx < 120 ? cx + 80 : cx - 85, cy - 1, &AF_S, "G", T_THEME);
        }
        if (chx && cv.detail[0]) {                                      /* the second small line: the source's chord, why a status, n of m */
            if (cx < 120)
                cv_text(cx, cy + 33, &AF_S, cv.detail, off ? T_DIM : T_MID);
            else
                uxl_text_r(cx, cy + 33, &AF_S, cv.detail, off ? T_DIM : T_MID);
        }
        if (uxl_inter_ref(k, &r)) {                                    /* the mark by the name: INTERACT's set, or what a morph moves */
            int32_t mx = cx < 120 ? cx + text_w(&AF_M, d->label) + 6 : cx - text_w(&AF_M, d->label) - 14;
            if (uxl.inter) {
                if (uxl_inter_in(k)) cv_rect(mx, cy + 4, 8, 8, T_ACCENT);
                else cv_frame(mx, cy + 4, 8, 8, T_TEXT);
            } else if (uxl_morph_has(r)) {
                cv_rrect(mx + 1, cy + 5, 5, 5, 2, T_THEME, T_BG);
            }
        }
    }
    {                                                                  /* the foot line on every ring page: the sound at the left, the page's place in its family at the right */
        char pos[12];
        sound_name(TSEL, pn);
        str_cpy(line, "T", sizeof line);
        line[1] = (char)('1' + song.sel);
        line[2] = ' ';
        line[3] = 0;
        str_cpy(line + 3, eng_abbr(e->name), sizeof line - 3u);
        cv_text(8, 198, &AF_S, line, T_MID);
        cv_text(8 + text_w(&AF_S, line) + 8, 198, &AF_S, pn, T_TEXT);
        uxl_page_pos(pos, sizeof pos);
        uxl_text_r(232, 198, &AF_S, pos, T_MID);
    }
#if CARAVEL_REVERB_MINIMAL
    if (dd && rps[act] && rps[act]->graph == GR_STLAYER) {              /* LAYER: what OCT+ and OCT- do */
        const char *aw = song.g[G_STMODE] == 1 ? "BLEND" : "ADD";                /* (in the free middle of the ring) */
        cv_key_hint(120 - kh_w(KC_OCTUP, aw) / 2, 96, KC_OCTUP, aw, 1, T_BG);
        if (stasis_layer_can_cancel())
            cv_key_hint(120 - kh_w(KC_OCTDN, "CANCEL") / 2, 118, KC_OCTDN, "CANCEL", 1, T_BG);
    }
#endif
    {
        int32_t x = 8;
        uint32_t row;
        for (row = 0; row < (dd ? 2u : 1u); row++) {
            const char *nm = dd ? (row ? dd->bot : dd->top) : cur_page()->title;
            if (dd && row && dd->kind == DK_LINK)
                nm = harm_role(TSEL) == HROLE_EXT ? "EXT RETRIG" : "HARMONY";
            else if (dd && row && dd->kind == DK_HARMONY)
                nm = "DURATION";
            else if (dd && row && dd->kind == DK_CHEXT)
                nm = "EXT";
            if (!nm[0])
                continue;
            cv_text(x, 219, &AF_M, nm, row == act ? T_ACCENT : T_MID);
            if (row == act)
                cv_rect(x, 236, text_w(&AF_M, nm), 2, T_ACCENT);
            x += text_w(&AF_M, nm) + 18;
        }
    }
}
static void uxl_ring_build(void)
{
    uint32_t bottom = 0, row, col, sig;
    const dual_t *dd = dual_page(cur_page()) ? dual_of(cur_page(), &bottom) : 0;
    const page_t *rps[2] = {dd ? dual_row_page(dd, 0) : cur_page(), dd ? dual_row_page(dd, 1) : 0};
    if (dd && dd->kind == DK_LINK)
        bottom = link_setup_row ? 1u : 0u;
    if (dd && dd->kind == DK_HARMONY)
        bottom = harmony_edit_dur == 2u ? 1u : 0u; /* setup and chord roots use the top row; only duration uses the bottom */
    sig = 0xB11D00u + bottom + (uint32_t)ui.page * 257u + song.sel * 31u + (uint32_t)uxl.inter * 7u + uxl.uset * 13u + (uint32_t)ui.hot_t * 17u + ui.hot_col +
          hp_rm.scope * 5u + (uint32_t)(uxl.uset & (1u << song.sel)) * 3u + (uint32_t)TSEL->eng_req * 101u;
    for (row = 0; row < (dd ? 2u : 1u); row++)
        for (col = 0; col < 4u; col++) {
            int32_t v;
            const param_desc_t *d = uxl_c_card(dd, rps[row], row, col, &v);
            sig = sig * 33u + (d ? (uint32_t)v : 0x7FFFu);
        }
#if !FELUCCA_FM4
    {   /* Redraw on mode/source/key changes or when an EXT chord becomes ready. */
        uint32_t hm = uxl_hm_view(dd, bottom);
        if (hm) sig = sig * 33u + uxl_hm_sig(hm);
    }
#endif
    sig = sig * 33u + (uint32_t)(uintptr_t)cur_page();
#if CARAVEL_REVERB_MINIMAL
    if (rps[bottom] && rps[bottom]->graph == GR_STLAYER) {                       /* the action and its state move by themselves */
        const char *ss = stasis_layer_status();
        sig = sig * 33u + (uint32_t)stasis_layer_can_cancel() + (uint32_t)song.g[G_STMODE] * 5u;
        while (*ss)
            sig = sig * 31u + (uint8_t)*ss++;
    }
#endif
    if (dd && dd->kind == DK_LFO)
        sig = sig * 33u + (uint32_t)TSEL->p[P_LWAVE];
    if (dd && dd->kind == DK_CHEXT) {                                                 /* CHORD EXT: the source chord and the notes move by themselves */
        ext_info_t ei;
        ext_info(song.sel, &ei);
        sig = sig * 33u + ei.state * 3u + ei.why * 5u + ei.have_chord * 7u + ei.root * 11u + ei.mask * 13u + ei.npc * 17u + ei.pc[0] * 19u + ei.pc[1] * 23u + ei.pc[2] * 29u + ei.mono * 31u + ei.want * 37u;
    }
    if (dd && dd->kind == DK_GRAPH)                                                   /* the SLICER's playing step and pattern */
        sig = sig * 33u + sl[TSEL - trk].idx * 7u + sl_pattern(TSEL) * 131u;
    if (!ui.force && ui.graph_sig == sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, 120, T_BG);
    cv_bg = T_BG;
    cv_oy = -28;
    uxl_y0 = 28;
    uxl_ring_build_paint(dd, bottom, rps);
    cv_oy = 0;
    cv_blit(0, 28);
    cv_begin(240, 92, T_BG);
    cv_bg = T_BG;
    cv_oy = -148;
    uxl_y0 = 148;
    uxl_ring_build_paint(dd, bottom, rps);
    cv_oy = 0;
    cv_blit(0, 148);
}
/* ---- BUILD in PLAY's manner: the SAVE family (USER, PRESETS, PHRASES, PROJECT, TOOLS) ----
 * The ring is the chooser: for a list (PRESETS, USER, PHRASES) its pointer is the place in the list, for PROJECT the four arcs are the
 * slots A..D, for TOOLS the four arcs are the four actions. The four corners are the four knobs (name and value, as PLAY's arcs), the
 * middle names what is chosen, a strip under the ring has the chosen entry between its neighbours, then the keys' hints and the family's
 * five pages. Data and knobs are the pages' own (the old pages' helpers): only the drawing is new. */
/* The SAVE family and the MIXER, in the manner of the HOME screen: rows as cards, the chosen one in the theme colour (E111).
 *   top      the family's pages as tabs (where you are among them)
 *   middle   four rows: USER and PHRASES and PRESETS a window of four of their list, the chosen one the second; PROJECT the four slots A..D;
 *            TOOLS the four actions; MIXER the four tracks (level, pan, send, mute)
 *   chosen row  the actions (K2..K4) as pills at its right end, the picked one filled; TOOLS: the action's word
 *   below    what K1 turns and the place in the list; the keys (OCT+ what it does, EDIT NAME, OCT- back) as keycaps
 * Data and keys are the old pages' own (foot_hint, act_col, act_ready ...): only the drawing is new. */
typedef struct { char tag[8], name[16], st[14]; uint8_t used, sel; } uxl_sv_row_t;
typedef struct {
    uint8_t kind;                                       /* 0 a list or the slots, 1 TOOLS, 2 the MIXER */
    uint8_t picked, ready;                              /* the picked action (1..4 = K1..K4), would OCT+ do something */
    uxl_sv_row_t r[4];
    char cl[4][2][12];                                  /* the bar under the rows: K1..K4 as label and value */
    uint8_t ck[4];                                      /* .. 0 empty, 1 a value (label over value), 2 an action (its word) */
    char pos[12];                                       /* the place in the list, "5/67" */
    uint8_t smalltag;                                   /* PRESETS: the tags are engine names */
    char what[28];                                      /* what the page saves or loads, in words */
    char ha[16], hb[16];
    uint8_t edit;
    char mx[4][3][14];                                  /* the MIXER: level, pan and send, per track */
    uint8_t mlv[4], mmute[4];
} uxl_save_t;
static int uxl_save_page(void)
{
    if (uxl.play || ui.home || ui.menu || ui.confirm || name_on() || ui.uboot || ui.layer)
        return 0;
    return cur_page()->fam == FAM_SAVE || cur_page()->graph == GR_TRK;
}
static void uxl_sv_win(uint32_t cur, uint32_t n, uint32_t *first)
{
    uint32_t f = cur > 0u ? cur - 1u : 0u;
    if (n > 4u && f + 4u > n)
        f = n - 4u;
    if (n <= 4u)
        f = 0u;
    *first = f;
}
static void uxl_sv_cell(uxl_save_t *m, uint32_t c, uint32_t kind, const char *label, const char *val)
{
    m->ck[c] = (uint8_t)kind;
    str_cpy(m->cl[c][0], label, sizeof m->cl[c][0]);
    str_cpy(m->cl[c][1], val ? val : "", sizeof m->cl[c][1]);
}
static void uxl_save_model(uxl_save_t *m)
{
    uint32_t g = cur_page()->graph, i, first, n;
    char tag[8], nm[16];
    memset(m, 0, sizeof *m);
    if (g == GR_TRK) {                                  /* the MIXER */
        char sn[16];
        const char *un;
        m->kind = 2;
        for (i = 0; i < NTRK; i++) {
            const track_t *tk = &trk[i];
            m->r[i].tag[0] = 'T';
            m->r[i].tag[1] = (char)('1' + i);
            str_cpy(m->r[i].name, ENGINES[tk->eng_req % NENGINES]->name, sizeof m->r[i].name);
            sound_name(tk, sn);
            str_cpy(m->r[i].st, sn, sizeof m->r[i].st);
            m->r[i].sel = i == song.sel;
            {
                static const uint8_t ID[3] = {P_LEVEL, P_PAN, P_REV};
                uint32_t j;
                for (j = 0; j < 3u; j++) {
                    param_format(&TP[ID[j]], tk->p[ID[j]], m->mx[i][j], &un);
                    if (un && un[0]) {
                        str_cpy(m->mx[i][j] + str_len(m->mx[i][j]), " ", sizeof m->mx[i][j] - str_len(m->mx[i][j]));
                        str_cpy(m->mx[i][j] + str_len(m->mx[i][j]), un, sizeof m->mx[i][j] - str_len(m->mx[i][j]));
                    }
                }
            }
            m->mlv[i] = (uint8_t)clamp(tk->p[P_LEVEL], 0, 127);
            m->mmute[i] = tk->p[P_MUTE] != 0;
        }
        uxl_sv_cell(m, 0, 1, "LEVEL", m->mx[song.sel % NTRK][0]);
        uxl_sv_cell(m, 1, 1, "PAN", m->mx[song.sel % NTRK][1]);
        uxl_sv_cell(m, 2, 1, "REV", m->mx[song.sel % NTRK][2]);
        uxl_sv_cell(m, 3, 1, "MUTE", m->mmute[song.sel % NTRK] ? "ON" : "OFF");
        return;
    }
    if (g == GR_BROWSE) {
        uint32_t total, cur = preset_pos(&total), e, k;
        m->kind = 0;
        m->smalltag = 1;
        fmt_int(m->pos, (int32_t)cur + 1);
        str_cpy(m->pos + str_len(m->pos), "/", sizeof m->pos - str_len(m->pos));
        fmt_int(m->pos + str_len(m->pos), (int32_t)total);
        if (cur >= total)
            str_cpy(m->pos, "--", sizeof m->pos);
        uxl_sv_cell(m, 0, 1, "No.", m->pos);
        uxl_sv_cell(m, 1, 1, "ENG", ENGINES[TSEL->eng_req % NENGINES]->name);
        uxl_sv_cell(m, 2, 1, "FAV", preset_favorite() ? "ON" : "OFF");
        uxl_sv_cell(m, 3, 1, "LIST", list_name(list_mode()));
        if (!total) {
            str_cpy(m->r[1].name, list_mode() == 1u ? "NO FAVORITES" : "NO SOUNDS", sizeof m->r[1].name);
            m->r[1].sel = 1;
        } else {
            uxl_sv_win(cur, total, &first);
            for (i = 0; i < 4u && first + i < total; i++) {
                e = preset_at(first + i, &k);
                if (e == NENGINES) {
                    up_slot_label(tag, k);
                    up_name(k, nm);
                } else {
                    str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
                    str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
                }
                str_cpy(m->r[i].tag, tag, sizeof m->r[i].tag);
                str_cpy(m->r[i].name, nm, sizeof m->r[i].name);
                m->r[i].used = 1;
                m->r[i].sel = first + i == cur;
            }
        }
    } else if (g == GR_USER) {
        uint32_t cur = ui.uslot;
        n = UP_SLOTS;
        fmt_int(m->pos, (int32_t)cur + 1);
        str_cpy(m->pos + str_len(m->pos), "/", sizeof m->pos - str_len(m->pos));
        fmt_int(m->pos + str_len(m->pos), (int32_t)n);
        uxl_sv_cell(m, 0, 1, "SLOT", m->pos);
        uxl_sv_win(cur, n, &first);
        for (i = 0; i < 4u && first + i < n; i++) {
            up_slot_label(tag, first + i);
            str_cpy(m->r[i].tag, tag, sizeof m->r[i].tag);
            m->r[i].used = (uint8_t)up_used(first + i);
            if (m->r[i].used) up_name(first + i, nm); else str_cpy(nm, "EMPTY", sizeof nm);
            str_cpy(m->r[i].name, nm, sizeof m->r[i].name);
            m->r[i].sel = first + i == cur;
        }
        uxl_sv_cell(m, 1, 2, "LOAD", 0);
        uxl_sv_cell(m, 2, 2, "ERASE", 0);
        uxl_sv_cell(m, 3, 2, "SAVE", 0);
    } else if (g == GR_PATS) {
        uint32_t cur = pat_pick();
        n = pat_count();
        fmt_int(m->pos, (int32_t)cur + 1);
        str_cpy(m->pos + str_len(m->pos), "/", sizeof m->pos - str_len(m->pos));
        fmt_int(m->pos + str_len(m->pos), (int32_t)n);
        uxl_sv_cell(m, 0, 1, "PAT", m->pos);
        uxl_sv_win(cur, n, &first);
        for (i = 0; i < 4u && first + i < n; i++) {
            pat_label(first + i, tag, nm);
            str_cpy(m->r[i].tag, tag, sizeof m->r[i].tag);
            str_cpy(m->r[i].name, nm, sizeof m->r[i].name);
            m->r[i].used = 1;
            m->r[i].sel = first + i == cur;
        }
        uxl_sv_cell(m, 1, 2, "LOAD", 0);
    } else if (g == GR_SLOTS) {
        uint32_t sel = (uint32_t)song.g[G_SLOT] - 1u;
        m->pos[0] = (char)('A' + sel % 4u);
        m->pos[1] = 0;
        uxl_sv_cell(m, 0, 1, "SLOT", m->pos);
        for (i = 0; i < 4u; i++) {
            int used = graph_project_used(i);
            const char *pn = graph_project_name(i);
            m->r[i].tag[0] = (char)('A' + i);
            m->r[i].tag[1] = 0;
            m->r[i].used = (uint8_t)(used != 0);
            str_cpy(m->r[i].name, !used ? "EMPTY" : pn[0] ? pn : "USED", sizeof m->r[i].name);
            m->r[i].sel = i == sel % 4u;
        }
#if CARAVEL_UX_LAYERS
        if (!play_project_save_only) {
            uxl_sv_cell(m, 1, 2, ui.proj_erase ? "ERASE" : "INIT", 0);
            uxl_sv_cell(m, 2, 2, "LOAD", 0);
        }
#else
        uxl_sv_cell(m, 1, 2, ui.proj_erase ? "ERASE" : "INIT", 0);
        uxl_sv_cell(m, 2, 2, "LOAD", 0);
#endif
        uxl_sv_cell(m, 3, 2, "SAVE", 0);
    } else {                                           /* TOOLS */
        static const char *const NAMES[4] = {"PAT", "SOUND", "SEC", "SONG"};
        static const char *const DO[4] = {"CLEAR", "INIT", "DELETE", "CLEAR"};
        m->kind = 1;
        for (i = 0; i < 4u; i++) {
            m->r[i].tag[0] = 'K';
            m->r[i].tag[1] = (char)('1' + i);
            str_cpy(m->r[i].name, NAMES[i], sizeof m->r[i].name);
            str_cpy(m->r[i].st, i == 1u && ui.tl_clean && act_col() == 2u ? "CLEAN" : DO[i], sizeof m->r[i].st);
            m->r[i].used = 1;
            m->r[i].sel = act_col() == i + 1u;
        }
    }
    if (g == GR_SLOTS)
#if CARAVEL_UX_LAYERS
        str_cpy(m->what, play_project_save_only ? "SAVE PROJECT ONLY" : "SAVES EVERYTHING", sizeof m->what);
#else
        str_cpy(m->what, "SAVES EVERYTHING", sizeof m->what);
#endif
    else if (g == GR_USER) { str_cpy(m->what, "SAVES T1 SOUND + STEPS", sizeof m->what); m->what[7] = (char)('1' + song.sel); }
    else if (g == GR_BROWSE) { str_cpy(m->what, "LOADS A SOUND TO T1", sizeof m->what); m->what[17] = (char)('1' + song.sel); }
    else if (g == GR_PATS) { str_cpy(m->what, "LOADS A PATTERN TO T1", sizeof m->what); m->what[19] = (char)('1' + song.sel); }
    else if (g == GR_TOOLS) str_cpy(m->what, "SAVES NOTHING", sizeof m->what);
    if (m->kind != 2) {
        char a[16], b[16];
        foot_hint(a, b);
        str_cpy(m->ha, a, sizeof m->ha);
        str_cpy(m->hb, b, sizeof m->hb);
#if CARAVEL_UX_LAYERS
        m->edit = !play_project_save_only && foot_rename() != 0u;
#else
        m->edit = foot_rename() != 0u;
#endif
        m->picked = (uint8_t)act_col();
        m->ready = (uint8_t)(act_ready() != 0);
    }
}
static void uxl_sv_tab_name(const char *title, char *o, uint32_t n)   /* "PRESETS" -> "PRESET": the five names must fit one line */
{
    uint32_t l;
    str_cpy(o, title, n);
    l = str_len(o);
    if (l >= 7u && o[l - 1u] == 'S')
        o[l - 1u] = 0;
}
static void uxl_sv_tabs(void)                          /* the family's pages as tabs: the current one a filled pill */
{
#if CARAVEL_UX_LAYERS
    if (play_project_save_only) {
        cv_rrect(7, 30, 79, 19, 9, T_ACCENT, T_BG);
        cv_text_on(16, 33, &AF_S, "PROJECT", T_INK, T_ACCENT);
        cv_text(157, 33, &AF_S, "FROM PLAY", T_THEME);
        return; /* no USER / PRESETS / PHRASES / TOOLS shortcut */
    }
#endif
    uint32_t k, cnt = 0;
    int32_t sum = 0, gap, x = 9;
    char nm[12];
    for (k = 0; k < NPAGES; k++)
        if (PAGES[k].fam == cur_page()->fam && page_visible(k) && !dual_bottom(&PAGES[k])) {
            uxl_sv_tab_name(PAGES[k].title, nm, sizeof nm);
            sum += text_w(&AF_S, nm);
            cnt++;
        }
    gap = cnt > 1u ? (240 - 18 - sum - 2) / (int32_t)(cnt - 1u) : 8;
    if (gap > 14) gap = 14;
    for (k = 0; k < NPAGES; k++) {
        int32_t w;
        if (PAGES[k].fam != cur_page()->fam || !page_visible(k) || dual_bottom(&PAGES[k]))
            continue;
        uxl_sv_tab_name(PAGES[k].title, nm, sizeof nm);
        w = text_w(&AF_S, nm);
        if (&PAGES[k] == cur_page()) {
            cv_rrect(x - 3, 30, w + 6, 19, 9, T_ACCENT, T_BG);
            cv_text_on(x, 33, &AF_S, nm, T_INK, T_ACCENT);
        } else {
            cv_text(x, 33, &AF_S, nm, T_MID);
        }
        x += w + gap;
    }
}
static void uxl_save_paint(const uxl_save_t *m)
{
    const int32_t Y0 = m->kind == 2 ? 53 : 67, PITCH = m->kind == 2 ? 35 : 31, H = m->kind == 2 ? 32 : 28, BY = 195, BH = 25;
    const int32_t TY = (H - 16) / 2;
    uint32_t i, c;
    char f[24];
    uint16_t half = ux_mix(T_THEME, T_INK, 35);
    cv_rect(0, 0, 240, 240, T_BG);
    uxl_sv_tabs();
    if (m->what[0])                                                   /* what the page saves or loads, in the larger face under the tabs */
        cv_text(8, 50, &AF_M, m->what, T_TEXT);
    for (i = 0; i < 4u; i++) {
        const uxl_sv_row_t *r = &m->r[i];
        int32_t y = Y0 + (int32_t)i * PITCH, xr = 230;
        uint16_t bg = r->sel ? T_THEME : T_SURF, ink = r->sel ? T_INK : T_TEXT, dim = r->sel ? half : T_MID;
        if (!r->tag[0] && !r->name[0])
            continue;
        cv_rrect(3, y, 234, H, 5, bg, T_BG);
        if (m->kind == 2) {                                           /* the MIXER: a track */
            int32_t bw = 56, fill = (int32_t)m->mlv[i] * bw / 127;
            uint16_t on = m->mmute[i] ? (r->sel ? half : T_DIM) : r->sel ? T_INK : T_ACCENT;
            cv_text(11, y + 8, &AF_M, r->tag, m->mmute[i] ? dim : r->sel ? T_INK : T_THEME);
            text_fit(f, sizeof f, r->name, &AF_M, 80);
            cv_text(40, y + 1, &AF_M, f, m->mmute[i] ? dim : ink);
            text_fit(f, sizeof f, r->st, &AF_S, 80);
            cv_text(40, y + 17, &AF_S, f, dim);
            cv_rrect(126, y + 6, bw, 7, 3, r->sel ? half : T_RAISE, bg);
            if (fill > 2)
                cv_rrect(126, y + 6, fill, 7, 3, on, r->sel ? half : T_RAISE);
            uxl_text_r(xr, y + 2, &AF_S, m->mx[i][0], m->mmute[i] ? dim : ink);
            if (m->mmute[i]) {
                cv_rrect(126, y + 17, 40, 11, 3, r->sel ? T_INK : T_KEY, bg);
                cv_text_on(131, y + 17, &AF_S, "MUTED", r->sel ? T_THEME : T_INK, r->sel ? T_INK : T_KEY);
            } else {
                cv_text(126, y + 17, &AF_S, "PAN", dim);
                cv_text(126 + text_w(&AF_S, "PAN "), y + 17, &AF_S, m->mx[i][1], ink);
            }
            cv_text(178, y + 17, &AF_S, "REV", dim);
            uxl_text_r(xr, y + 17, &AF_S, m->mx[i][2], ink);
            continue;
        }
        if (m->smalltag)
            cv_text(11, y + TY + 2, &AF_S, r->tag, r->sel ? T_INK : T_THEME);
        else
            cv_text(11, y + TY, &AF_M, r->tag, r->sel ? T_INK : r->used || m->kind == 1 ? T_THEME : T_DIM);
        if (m->kind == 1) {                                           /* TOOLS: the action's word at the right, the picked one filled */
            int32_t w = text_w(&AF_S, r->st) + 14;
            cv_text(48, y + TY, &AF_M, r->name, ink);
            if (r->sel) {
                cv_rrect(xr - w, y + TY - 2, w, 19, 9, m->ready ? T_INK : half, bg);
                cv_text_on(xr - w + 7, y + TY + 1, &AF_S, r->st, m->ready ? T_THEME : ux_mix(T_THEME, T_INK, 70), m->ready ? T_INK : half);
            } else {
                uxl_text_r(xr - 7, y + TY + 2, &AF_S, r->st, T_MID);
            }
            continue;
        }
        text_fit(f, sizeof f, r->name, &AF_M, xr - 58);
        cv_text(m->smalltag ? 54 : 52, y + TY, &AF_M, f, r->sel ? T_INK : r->used ? T_TEXT : T_DIM);
    }
    if (m->kind == 1)
        uxl_text_c(120, 200, &AF_S, "TURN K1-K4 TO PICK, OCT+ DOES IT", T_DIM);
    if (m->kind != 1) {                                               /* the bar under the rows: K1 .. K4, what each turns or does */
        for (c = 0; c < 4u; c++) {
            int32_t x = 4 + (int32_t)c * 58;
            uint16_t fill = T_SURF;
            if (!m->ck[c])
                continue;
            if (m->ck[c] == 2) {                                      /* an action: filled when picked */
                int pk = m->picked == c + 1u;
                fill = pk ? (m->ready ? T_ACCENT : T_LINE) : T_RAISE;
                cv_rrect(x, BY, 54, BH, 6, fill, T_BG);
                cv_text_on(x + 27 - text_w(&AF_S, m->cl[c][0]) / 2, BY + 7, &AF_S, m->cl[c][0], pk ? (m->ready ? T_INK : T_DIM) : T_TEXT, fill);
            } else {
                cv_rrect(x, BY, 54, BH, 6, fill, T_BG);
                cv_text_on(x + 5, BY + 1, &AF_S, m->cl[c][0], T_DIM, fill);
                text_fit(f, sizeof f, m->cl[c][1], &AF_S, 46);
                cv_text_on(x + 5, BY + 12, &AF_S, f, T_TEXT, fill);
            }
        }
    }
    if (m->kind == 2)
        return;
    {                                                                 /* the keys as keycaps: OCT+ (when it does something), EDIT NAME, OCT- */
        const char *wa = m->ha + 5, *wb = m->hb + 5;
        int32_t y = 222;
        if (!(wa[0] == '-' || !wa[0]))
            cv_key_hint(6, y, KC_OCTUP, wa, m->ready ? 1 : 0, T_BG);          /* (dim while the picked action would do nothing) */
        if (m->edit)
            cv_key_hint(120 - kh_w(KC_EDIT, "NAME") / 2, y, KC_EDIT, "NAME", 1, T_BG);
        cv_key_hint(234 - kh_w(KC_OCTDN, wb), y, KC_OCTDN, wb, 1, T_BG);
    }
}
static void uxl_save_build(void)
{
    static uxl_save_t m;
    static uint32_t last;
    uint32_t h = 2166136261u, i;
    const uint8_t *b;
    uxl_save_model(&m);
    b = (const uint8_t *)&m;
    for (i = 0; i < sizeof m; i++)
        h = (h ^ b[i]) * 16777619u;
    h ^= (uint32_t)(uintptr_t)cur_page() * 31u + (uint32_t)ui.hot_t;
    if (!ui.force && h == last)
        return;
    last = h;
    cv_begin(240, 120, T_BG);
    cv_bg = T_BG;
    cv_oy = -28;
    uxl_y0 = 28;
    uxl_save_paint(&m);
    cv_oy = 0;
    cv_blit(0, 28);
    cv_begin(240, 92, T_BG);
    cv_bg = T_BG;
    cv_oy = -148;
    uxl_y0 = 148;
    uxl_save_paint(&m);
    cv_oy = 0;
    cv_blit(0, 148);
}
/* ---- BUILD in PLAY's manner: the SEL (SCL) and GLO layers, held ----
 * A layer is the map of the keys while its button is held. Here: the layer's header (BUILD band, its name), the four knobs round a small
 * ring (SEL: ROOT SCL CHRD VOIC; GLO: the four tracks' LEVEL on the arcs, the tempo in the middle), the keys as fields below
 * (SEL: the sixteen scales, the one now chosen lit; GLO: SOLO 1-4, ALL LEARN FILL TAP, MUTE 1-4), and the footer the layer always had. */
static const page_t UXL_LY_SCL = {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_CHRD, P_VOIC}};
static int uxl_eu_on(void);
static void uxl_layer_body_more(uint32_t l);
static int uxl_layer_page(void)
{
    if (uxl.play || ui.menu || ui.confirm || name_on() || ui.uboot || !ui.layer)
        return 0;
    if (ui.layer == LAYER_SEQ)                          /* TOOLS: the map, but not with EUCLID open (its cells keep the old drawing) */
        return !uxl_eu_on();
    return ui.layer == LAYER_SCL || ui.layer == LAYER_GLO || ui.layer == LAYER_FX || ui.layer == LAYER_EDIT || ui.layer == LAYER_REC;
}
/* BUILD's header pieces, shared by the layers' header and the menu's: the chip (white, BUILD, with the roadworks sign) on the band, and the
 * hazard strip (the first four rows of the canvas being drawn) */
static int32_t uxl_chip(uint16_t band)                   /* returns the chip's width */
{
    int32_t cw = text_w(&AF_M, "BUILD") + 28, r;
    cv_rrect(2, 3, cw, 18, 4, T_TEXT, band);
    for (r = 0; r < 12; r++) {                              /* the roadworks sign: an amber triangle, an ink exclamation mark */
        int32_t hw = (r * 6) / 11;
        cv_rect(11 - hw, 5 + r, 2 * hw + 1, 1, T_THEME);
    }
    cv_rect(11, 9, 1, 4, T_INK);
    cv_rect(11, 14, 1, 1, T_INK);
    cv_text_on(22, 4, &AF_M, "BUILD", T_INK, T_TEXT);
    return cw;
}
/* The BUILD title and its warning stripe are one 240x28 LCD region.
 * In particular, no intermediate "old title" or cleared stripe may be sent
 * before restoring the BUILD image: the FM-1 exposes each LCD transfer. */
static uint32_t uxl_build_head_hash;
static uint8_t uxl_build_head_owner;              /* 0: other screen, 1: BUILD page, 2: BUILD key layer */
static void uxl_hazard_rows(void)
{
    uint32_t px, py;
    for (py = 0; py < 4u; py++)
        for (px = 0; px < 240u; px++)
            cv_px[(24u + py) * 240u + px] = ((px + py) / 6u) & 1u ? swap16(T_BG) : swap16(T_THEME);
}
static void uxl_build_head_flush(uint8_t owner)
{
    uint32_t k, hash = 2166136261u;
    uxl_hazard_rows();
    for (k = 0; k < 240u * 28u; k++)
        hash = (hash ^ cv_px[k]) * 16777619u;
    /* Redraw on content or screen ownership changes, not every 15-ms frame. */
    if (uxl_build_head_owner != owner || uxl_build_head_hash != hash || ui.force) {
        cv_blit(0, 0);
        uxl_build_head_hash = hash;
        uxl_build_head_owner = owner;
    }
}
static int uxl_menu_style(void) { return !uxl.play; }
static int uxl_build_now(void) { return !uxl.play; }
static void uxl_layer_header(void)
{
    static const char *const FAM[LAYER_N] = {"", "FX", "GLO", "SEL", "EDIT", "SEQ", "REC"}, *const TITLE[LAYER_N] = {"", "FX HOLD", "GLOBAL SET", "SCALE SET", "EDIT SET", "SEQ TOOLS", "REC SET"};
    const char *fam = FAM[ui.layer % LAYER_N], *title = TITLE[ui.layer % LAYER_N];
    uint16_t band = ux_mix(T_SURF, T_ACCENT, 20);
    int32_t cw;
    cv_begin(240, 28, band);
    cv_bg = band;
    cw = uxl_chip(band);
    if (ui.msg_t) {                                         /* a message takes the place of both names */
        cv_free_text(cw + 8, 4, &AF_M, ui.msg, T_TEXT, band, 238 - cw - 8);
    } else {
        cv_text(cw + 8, 4, &AF_M, fam, T_TEXT);
        uxl_text_r(236, 4, &AF_M, title, T_ACCENT);
    }
    uxl_build_head_flush(2u);
}
static void uxl_layer_field(int32_t x, int32_t y, int32_t w, const char *t, int on, int dim)
{
    uint16_t fill = on ? T_THEME : T_RAISE, ink = on ? T_INK : dim ? T_DIM : T_MID;
    cv_rrect(x, y, w, 15, 4, fill, T_BG);
    cv_text_on(x + (w - text_w(&AF_S, t)) / 2, y + 1, &AF_S, t, ink, fill);
}
static void uxl_layer_paint(uint32_t l)
{
    static const int16_t START[4] = {277, 7, 187, 97};
    uint32_t k;
    int32_t cx = 120, cy = 76;
    char val[24], big[16], small[16];
    const char *unit;
    cv_rect(0, 0, 240, 240, T_BG);
    for (k = 0; k < 4u; k++) {                                         /* the four knobs: an arc each, the name and the value at its corner */
        int32_t v = 0, ratio = 0, x = (k & 1u) == 0u ? 4 : 236, y = k < 2u ? 34 : 78;
        const param_desc_t *d;
        int16_t *vp = 0;
        char tag[4];
        const char *label;
        int dim = 0;
        if (l == LAYER_GLO) {
            tag[0] = 'T';
            tag[1] = (char)('1' + k);
            tag[2] = 0;
            label = tag;
            d = &TP[P_LEVEL];
            v = trk[k].p[P_LEVEL];
            dim = trk[k].p[P_MUTE] || (perf_solo && !((perf_solo >> k) & 1u));
        } else {
            d = page_desc(&UXL_LY_SCL, k, &vp);
            if (d && vp)
                v = *vp;
            label = d ? d->label : "";
        }
        if (!d)
            continue;
        param_format(d, v, val, &unit);
        ratio = d->max > d->min ? (v - d->min) * 1000 / (d->max - d->min) : 0;
        if (unit && unit[0]) {
            str_cpy(val + str_len(val), " ", sizeof val - str_len(val));
            str_cpy(val + str_len(val), unit, sizeof val - str_len(val));
        }
        uxl_ring(cx, cy, 38, 49, START[k], START[k] + 76, START[k], START[k] + ratio * 76 / 1000, T_LINE, dim ? T_DIM : T_THEME);
        if (x < 120) {
            cv_text(x, y, &AF_M, label, dim ? T_MID : T_TEXT);
            cv_text(x, y + 20, &AF_S, val, T_MID);
        } else {
            uxl_text_r(x, y, &AF_M, label, dim ? T_MID : T_TEXT);
            uxl_text_r(x, y + 20, &AF_S, val, T_MID);
        }
    }
    if (l == LAYER_SCL) {                                              /* the scale in the middle, its root under it */
        const param_desc_t *dr = page_desc(&UXL_LY_SCL, 0, 0);
        int16_t *vr = 0;
        uint32_t sc = (uint32_t)TSEL->p[P_SCALE];
        str_cpy(big, TP[P_SCALE].names[sc <= (uint32_t)TP[P_SCALE].max ? sc : 0u], sizeof big);
        dr = page_desc(&UXL_LY_SCL, 0, &vr);
        str_cpy(small, "ROOT ", sizeof small);
        if (dr && vr) {
            param_format(dr, *vr, val, &unit);
            str_cpy(small + 5, val, sizeof small - 5u);
        }
    } else {                                                           /* the tempo, the transport's state */
        fmt_int(big, song.g[G_BPM]);
        str_cpy(small, song.playing ? "PLAYING" : "STOPPED", sizeof small);
    }
    uxl_text_c(cx, cy - 14, &AF_M, big, l == LAYER_GLO ? T_THEME : T_TEXT);
    uxl_text_c(cx, cy + 6, &AF_S, small, T_MID);
    if (l == LAYER_SCL) {                                              /* the sixteen scales, four a row */
        uint32_t i, sc = (uint32_t)TSEL->p[P_SCALE];
        for (i = 0; i < 16u && i <= (uint32_t)TP[P_SCALE].max; i++)
            uxl_layer_field(6 + (int32_t)(i % 4u) * 58, 126 + (int32_t)(i / 4u) * 19, 54, TP[P_SCALE].names[i], i == sc, 0);
    } else {
        static const char *const SOLO[4] = {"SOLO 1", "SOLO 2", "SOLO 3", "SOLO 4"}, *const MUTE[4] = {"MUTE 1", "MUTE 2", "MUTE 3", "MUTE 4"};
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            uxl_layer_field(6 + (int32_t)i * 58, 126, 54, SOLO[i], (perf_solo >> i) & 1u, 0);
            uxl_layer_field(6 + (int32_t)i * 58, 164, 54, MUTE[i], trk[i].p[P_MUTE] != 0, 0);
        }
        uxl_layer_field(6, 145, 54, "ALL", 0, 0);
        uxl_layer_field(64, 145, 54, "LEARN", ui.ml.on != 0, 0);
        uxl_layer_field(122, 145, 54, "FILL", seq_fill != 0, 0);
        uxl_layer_field(180, 145, 54, song.g[G_CLOCK] ? "SYNC" : "TAP", 0, 0);
    }
}
static void uxl_layer_body(uint32_t l)
{
    static uint32_t last;
    uint32_t h = 2166136261u, k, e;
    if (l != LAYER_SCL && l != LAYER_GLO) {
        uxl_layer_body_more(l);
        return;
    }
    for (k = 0; k < 4u; k++) {
        int16_t *vp = 0;
        const param_desc_t *d = l == LAYER_SCL ? page_desc(&UXL_LY_SCL, k, &vp) : 0;
        h = (h ^ (uint32_t)(l == LAYER_GLO ? trk[k].p[P_LEVEL] : (d && vp ? *vp : 0)) ^ ((uint32_t)trk[k].p[P_MUTE] << 12)) * 16777619u;
    }
    e = l == LAYER_SCL ? (uint32_t)TSEL->p[P_SCALE] : perf_solo * 31u + (uint32_t)song.g[G_BPM] * 7u + (uint32_t)song.playing * 5u + (uint32_t)ui.ml.on * 3u +
        (uint32_t)seq_fill * 11u + (uint32_t)song.g[G_CLOCK] * 13u;
    h = (h ^ e ^ (l << 24) ^ (uint32_t)song.sel << 8) * 16777619u;
    if (!ui.force && h == last)
        return;
    last = h;
    cv_begin(240, 120, T_BG);
    cv_bg = T_BG;
    cv_oy = -28;
    uxl_y0 = 28;
    uxl_layer_paint(l);
    cv_oy = 0;
    cv_blit(0, 28);
    cv_begin(240, 54, T_BG);
    cv_bg = T_BG;
    cv_oy = -148;
    uxl_y0 = 148;
    uxl_layer_paint(l);
    cv_oy = 0;
    cv_blit(0, 148);
}
/* a BUILD page that is not rebuilt yet (STEP, DETAIL, AUTOMATION, MIXER, INSERT, SLICER, HARMONY, LINK ...): drawn flat, its boxes' fill gone */
static int uxl_flat_page(void)
{
    if (uxl.play || ui.home || ui.menu || ui.confirm || name_on() || ui.uboot || ui.layer)
        return 0;
    return !uxl_ring_page() && !uxl_save_page() && cur_page()->graph != GR_EVOLVE && !uxl_c_page();
}
/* The BUILD frame and header cover only the old header and outermost pixels.
 * Parameter banks, engine graphics and the existing EDIT/VIEW selection stay. */
static void uxl_build_mark(void)
{
    char count[12], line[40]; uint32_t i, n = 0, at = 0;
    int cpg = 0;
    const page_t *count_page=dual_top_page(cur_page());
    if(!count_page)count_page=cur_page();
    if (uxl.play || ui.menu || ui.confirm || name_on() || ui.uboot) {
        uxl_build_head_owner = 0;                 /* screen may have replaced these rows */
        return;
    }
    if (ui.layer) {
#if CARAVEL_UX_EVOLVE
        if (!uxl_layer_page())
#endif
            uxl_build_head_owner = 0;             /* a legacy layer owns the LCD */
        return;
    }
#if CARAVEL_UX_EVOLVE
    cpg = uxl_ring_page() ? 0 : uxl_c_page();
#endif
    if (!ui.home)
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == cur_page()->fam && page_visible(i) && !dual_bottom(&PAGES[i])) {
                n++; if (&PAGES[i] == count_page) at = n;
            }
    fmt_int(count, (int32_t)(at ? at : 1));
    str_cpy(count + str_len(count), "/", sizeof count - str_len(count));
    fmt_int(count + str_len(count), (int32_t)(n ? n : 1));
    uint16_t band = ux_mix(T_SURF, T_ACCENT, 20);       /* BUILD's header band: the surface tinted with the accent (a cool slate on AMBER, grey on GREY) */
    cv_begin(240, 28, band); cv_bg = band;
    {   /* one row in the larger face: the chip, the block (named like its PLAY area), the screen's name at the right; a message takes the place of both
         * names while it shows. (The pages' own foot lines and tabs say where in the block a page is; the second small line is gone.) */
        static const char *const BLK[FAM_COUNT] = {"TRACKS", "SHAPE", "MOVE", "SPACE", "HARM", "SOUND", "MIX", "SAVE", "ARP", "SEQ", "TRK"};
        const char *blk = ui.home ? "TRACKS" : BLK[cur_page()->fam < FAM_COUNT ? cur_page()->fam : 0u];
        int32_t cw = uxl_chip(band), x0 = cw + 8;
        char tt[28];
        const char *ttl = ui.home ? "OVERVIEW" : cur_page()->title;
        str_cpy(tt, ttl, sizeof tt);
        if (!ui.home && cur_page()->graph == GR_EVOLVE) {     /* (these pages have no foot line: the place in the family goes after the title) */
            char ps[12];
            uxl_page_pos(ps, sizeof ps);
            str_cpy(tt + str_len(tt), " ", sizeof tt - str_len(tt));
            str_cpy(tt + str_len(tt), ps, sizeof tt - str_len(tt));
        }
#if CARAVEL_UX_EVOLVE
        if (uxl.inter) {                                    /* INTERACT: the count, and the four columns: in the set (filled), not (open) or no target (dash) */
            char hd[24];
            uint32_t c;
            str_cpy(hd, "INTERACT ", sizeof hd);
            fmt_int(hd + str_len(hd), (int32_t)(uxl.uset & (1u << song.sel) ? uxl_set_count(song.sel) : 0u));
            str_cpy(hd + str_len(hd), "/32", sizeof hd - str_len(hd));
            cv_text(x0, 4, &AF_M, hd, T_TEXT);
            if (!ui.home && !cpg)
                for (c = 0; c < 4u; c++) {
                    hp_ref_t r;
                    int32_t x = x0 + text_w(&AF_M, hd) + 10 + (int32_t)c * 22;
                    if (x + 14 > 238)
                        break;
                    if (!uxl_inter_ref(c, &r))
                        cv_rect(x + 2, 12, 10, 1, T_MID);
                    else if (uxl_inter_in(c))
                        cv_rect(x, 6, 14, 12, T_ACCENT);
                    else
                        cv_frame(x, 6, 14, 12, T_TEXT);
                }
        } else
#endif
        if (ui.msg_t) {
            cv_free_text(x0, 4, &AF_M, ui.msg, T_TEXT, band, 238 - x0);
        } else {
            int32_t tw = text_w(&AF_M, tt), bw = text_w(&AF_M, blk);
            if (x0 + bw + 10 + tw > 238)                    /* too long for both: the title alone, at the left */
                cv_free_text(x0, 4, &AF_M, tt, T_TEXT, band, 238 - x0);
            else {
                cv_text(x0, 4, &AF_M, blk, T_TEXT);
                uxl_text_r(236, 4, &AF_M, tt, T_ACCENT);
            }
        }
    }
    uxl_build_head_flush(1u);
#if CARAVEL_UX_EVOLVE
    if (!ui.home && cur_page()->graph != GR_EVOLVE && !cpg && !uxl_ring_page() && !uxl_save_page()) {   /* a dot on each card whose parameter RANDOM and JOURNEY move (the ring pages mark their own) */
        uint32_t c, bottom = 0;
        int32_t y = 31, dx = 48;
        if (dual_page(cur_page())) {                    /* two banks: the dot on the one the knobs act on (its label row) */
            (void)dual_of(cur_page(), &bottom);
            y = bottom ? 108 : 58;
            dx = 46;
        }
        for (c = 0; c < 4u; c++) {
            hp_ref_t r;
            if (uxl_inter_ref(c, &r) && uxl_morph_has(r))
                lcd_fill(3 + 59 * (int32_t)c + dx, y, 6, 6, T_THEME);
        }
    }
#endif
    /* (no outer frame any more: every page block that was blitted full width over it, each frame, made it flicker on a panel,
     * 67 times a second, and the owner does not want it; BUILD is told apart by its header, and is to be rebuilt as a whole, see
     * docs/BUILD_NEUE_UI_PLAN) */
}
#endif
