/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (SCL > CHORD: P_CHRD, P_VOIC): one key, MIDI note or arp input plays a chord. Included by seq.c
 * after kb_map; runs in the audio ISR with the rest of the note input.
 *   CHRD OFF; DIA3 / DIA7: the triad / seventh of the track's scale (ROOT, SCALE) built on the key, every tone
 *        in key (a key outside the scale takes the scale note below it, as QNT SNAP does; CHR builds on the
 *        major scale of ROOT); MAJ MIN DOM7 MAJ7 MIN7 SUS4 POW: that shape on the key, whatever the scale.
 *   VOIC CLOSE (root position); OPEN (the second tone an octave up: 1-5-3); INV1 / INV2 (the lowest tone, then
 *        the next, an octave up); +OCT (the root an octave down added; a seventh drops its fifth). At most
 *        CHORD_MAX notes, notes outside 0..127 left out.
 * The key is mapped first (kb_map: QNT SNAP / WHITE, TRN, the octave), the chord is built on the note it gives.
 * MONO / LEGATO / UNISON parts play the chord's root only; a kit (the DRUM engine, slices: an
 * engine that maps the keys itself) ignores CHRD. Every source keeps the notes it started (kb_chord for a
 * key, mchord for a MIDI note) and its release ends exactly those, so CHRD / VOIC changed while it is held
 * leave nothing hanging. A note several sources hold sounds once and ends with the last of them. */
enum { CH_OFF, CH_DIA3, CH_DIA7, CH_MAJ, CH_MIN, CH_DOM7, CH_MAJ7, CH_MIN7, CH_SUS4, CH_POW, CH_BUS };
enum { VC_CLOSE, VC_OPEN, VC_INV1, VC_INV2, VC_BASS };
#define CHORD_MAX 4u
#define MCHORD_N 24u                     /* MIDI notes held as chords at once (more: their root alone) */

static const int8_t CHORD_SHAPE[CH_POW - CH_MAJ + 1][CHORD_MAX] = {
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 5, 7, -1}, {0, 7, 12, -1},
};

static uint8_t kb_chord[27][CHORD_MAX], kb_chn[27];   /* the notes key k started (kb_chn 0: none) */
static uint8_t kb_midi[27][CHORD_MAX], kb_midin[27];  /* the notes key k last sent to MIDI OUT: retriggers move
                                                       * only the local list, so the external NoteOns stay matched */
static uint8_t kb_live[27];                            /* this press may still retrigger (panic clears it until re-press) */
typedef struct { uint8_t id, ch, src, n, note[CHORD_MAX]; } mchord_t;   /* id: track + 1, 0 = free */
static mchord_t mchord[MCHORD_N];
/* the last chord played per track (the CHORD page shows it): its root, notes, and the shape's tones above the
 * root (bit i = i semitones, 0..11) */
static struct { uint8_t root, key, n, note[CHORD_MAX]; uint16_t mask; uint8_t gen, vel; } chord_last[NTRK];
static uint8_t chord_live[NTRK];        /* an owner still holds the CHRD BUS chord: eligibility for retrigger */

/* 1 = the track's keys are a kit: no chords */
static int chord_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}

/* the tones of the chord (semitones above *root, ascending, iv[0] = 0) -> their number; DIA may move *root
 * down onto the scale */
static uint32_t chord_tones(const track_t *t, uint32_t mode, int32_t *root, int32_t *iv)
{
    uint32_t n = 0, i;
    if (mode == CH_BUS) {
        uint16_t bm = harmony_mask(t), rel;
        if (bm) {                                     /* the HARMONIC BUS chord, at the octave nearest the key */
            int32_t br = (int32_t)harmony_live_desc(harmony_chord_at(harmony_bar), &rel);   /* effective root + relative tones */
            while (br + 12 <= *root + 6)
                br += 12;
            while (br > *root + 6)
                br -= 12;
            *root = br;
            iv[n++] = 0;
            for (i = 1; i < 12u && n < CHORD_MAX; i++)
                if ((rel >> i) & 1u)
                    iv[n++] = (int32_t)i;
            return n;
        }
        mode = CH_DIA3;                               /* bus off: the scale's triad, as DIA3 */
    }
    if (mode >= CH_MAJ) {
        for (i = 0; i < CHORD_MAX && CHORD_SHAPE[mode - CH_MAJ][i] >= 0; i++)
            iv[n++] = CHORD_SHAPE[mode - CH_MAJ][i];
        return n;
    }
    {
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t r = t->p[P_ROOT];
        if (mask == 0xFFFu)
            mask = SCALE_MASK[1];                       /* CHR: no key of its own, the major scale of ROOT */
        while (guard-- && !((mask >> (uint32_t)((*root - r + 120) % 12)) & 1u))
            (*root)--;                                  /* outside the scale: the scale note below */
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((*root - r + 120) % 12))
                    at = c;
                deg[c++] = i;
            }
        for (i = 0; i < (mode == CH_DIA7 ? 4u : 3u); i++) {   /* every other note of the scale */
            uint32_t k = at + 2u * i;
            iv[n++] = (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
        }
        return n;
    }
}

/* the chord on note `root` as track t plays it now -> out (ascending), the number of notes (1..CHORD_MAX);
 * *rp its root, *maskp its tones above the root (bit i: i semitones; 0 = no chord). The root alone when CHRD
 * is OFF, on a kit, or for MONO / LEGATO / UNISON (their chord's root) */
static uint32_t chord_make(const track_t *t, uint32_t root, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    int32_t iv[CHORD_MAX + 1u], r = (int32_t)root, x;
    uint32_t mode = (uint32_t)t->p[P_CHRD], n, i, j, m = 0;
    uint16_t mask = 0;
    *rp = r;
    *maskp = 0;
    if (!mode || mode > CH_BUS || chord_kit(t)) {
        out[0] = (uint8_t)root;
        return 1;
    }
    n = chord_tones(t, mode, &r, iv);
    for (i = 0; i < n; i++)
        mask |= (uint16_t)(1u << (iv[i] % 12));
    *rp = r;
    *maskp = mask;
    if (trk_vmode(t) != V_POLY) {                       /* one voice: the root */
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    switch (t->p[P_VOIC]) {
    case VC_OPEN:                                       /* 1-5-3(-7): the second tone an octave up */
        if (n >= 3u)
            iv[1] += 12;
        break;
    case VC_INV2:
    case VC_INV1:
        for (j = 0; j < (t->p[P_VOIC] == VC_INV2 ? 2u : 1u); j++) {   /* the lowest tone an octave up */
            x = iv[0] + 12;
            for (i = 1; i < n; i++)
                iv[i - 1u] = iv[i];
            iv[n - 1u] = x;
        }
        break;
    case VC_BASS:                                       /* the root an octave down; a seventh drops its fifth */
        if (n == CHORD_MAX) {
            iv[2] = iv[3];
            n--;
        }
        for (i = n; i > 0; i--)
            iv[i] = iv[i - 1u];
        iv[0] = -12;
        n++;
        break;
    default:
        break;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            x = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = x;
        }
    for (i = 0; i < n && m < CHORD_MAX; i++) {          /* inside 0..127, each note once */
        x = r + iv[i];
        if (x < 0 || x > 127 || (m && out[m - 1u] == (uint8_t)x))
            continue;
        out[m++] = (uint8_t)x;
    }
    if (!m) {
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    return m;
}

/* ------------------------------------------------------------------------------------------------------------------
 * CHORD EXT: HARM reads the current bus chord; T1..T4 remember a chord from
 * real panel/MIDI input (chord_source.c). A source never starts a follower by itself.
 *   ext_state / ext_on: one answer for the audio ISR and for ext_info (the UI). OFF; N/A (a kit, a pair-morph follower);
 *        WAIT BUS (HARM with HARMONY off: no DIA3 fallback); WAIT / AMBIG for live input; OK. EXT overrides CHRD and FOLLOW (their values stay stored
 *        and are not read for an EXT track).
 *   ext_pool: the target tones 7 -> 9 -> 13 (EXT level 1..3) of the chord's kind, minus the tones the chord has
 *        already (any octave), once each, never padded; the scale only picks between a primary and an alternate interval.
 *   placement: each pool tone at the follower's register (the octave nearest the key / SEQ note / ARP anchor, a tie goes
 *        to the lower), then OCT * 12 (folded back by octaves into 0..127). An empty pool / WAIT: the note is dropped.
 * Every started note is kept in the owners' lists (kb_chord, mchord, arp_note, seq_notes) as before: a release never
 * asks the resolver again. Not inlined: this runs per note event only, outside the ISR's loops. */
#if !FELUCCA_FM4
#define EXT_POOL_API 1                              /* (tests/chordext_api.h: ext_pool exists) */
enum { XK_MAJ, XK_MIN, XK_DIM, XK_AUG, XK_SUS2, XK_SUS4, XK_POW };
#include "chord_source.c"
/* the semitones above the root of the 7th, 9th and 13th: primary | alternate << 4 (0: none) */
static const uint8_t EXT_CAND[7][3] = {
    {11 | 10 << 4, 2, 9},               /* maj: maj7, b7 on a V chord */
    {10 | 11 << 4, 2 | 1 << 4, 9 | 8 << 4},   /* min */
    {10 | 9 << 4, 2 | 1 << 4, 8},       /* dim: no 6 */
    {10 | 11 << 4, 2, 0},               /* aug: no 13 (it rubs the #5) */
    {10 | 11 << 4, 0, 9},               /* sus2: the 9 is its 2 */
    {10 | 11 << 4, 2, 9},               /* sus4 */
    {10 | 11 << 4, 2, 9},               /* power chord */
};

/* the extension tones missing from a chord: rel = its tones above the root (bit i: i semitones), root its pitch class,
 * kind XK_*, level 1..3 (7, 7+9, 7+9+13), scale = absolute pitch classes of the source's scale (0: unknown: primaries) ->
 * pc[] (absolute pitch classes, in order 7, 9, 13) and their number */
static __attribute__((noinline)) uint32_t ext_pool(uint32_t rel, uint32_t root, uint32_t kind, uint32_t level, uint32_t scale, uint8_t *pc)
{
    uint32_t s, n = 0, i, seven = (rel & 0xC00u) || (kind == XK_DIM && (rel & 0x200u));
    if (kind > XK_POW)
        return 0;
    for (s = 0; s < level && s < 3u; s++) {
        uint32_t cand = EXT_CAND[kind][s], pri = cand & 15u, alt = cand >> 4, pick = pri, c;
        if (!pri || (s == 0 && seven))                  /* none / a seventh is there: b7 or maj7 (a dim also its 6) */
            continue;
        if (scale && scale != 0xFFFu && alt && !((scale >> ((root + pri) % 12u)) & 1u) && ((scale >> ((root + alt) % 12u)) & 1u))
            pick = alt;                                 /* the primary is outside the scale, the alternate inside it */
        if ((rel >> pick) & 1u)
            continue;                                   /* played already: not missing */
        c = (root + pick) % 12u;
        for (i = 0; i < n && pc[i] != c; i++)
            ;
        if (i == n)
            pc[n++] = (uint8_t)c;
    }
    return n;
}

/* what CHORD EXT does on track t now: EXT_ST_OFF, _NA (why), _WAITBUS (why), or _OK (a source chord is there) */
static __attribute__((noinline)) uint32_t ext_state(const track_t *t, int src, uint32_t *why)
{
    uint32_t w = EXT_WHY_NONE, st = EXT_ST_OK;
    if (src < 1 || src > 5)
        st = EXT_ST_OFF;
    else if (chord_kit(t)) {
        st = EXT_ST_NA;
        w = EXT_WHY_KIT;
    }
#if FELUCCA_PAIR_MORPH
    else if (pair_follower(t)) {
        st = EXT_ST_NA;
        w = EXT_WHY_PAIR;
    }
#endif
    else if (src >= 2) {
        uint32_t f = trk_index(t), source = (uint32_t)src - 2u;
        w = ext_live_why(f, source);
        if (w) st = EXT_ST_NA;
        else if (xext[f].shadow != src) { st = EXT_ST_WAIT; w = EXT_WHY_PLAY; }
        else {
            uint32_t chord = xext[f].chord;
            st = chord & XS_AMBIG ? EXT_ST_AMBIG : chord & XS_VALID ? EXT_ST_OK : EXT_ST_WAIT;
            w = st == EXT_ST_AMBIG && (chord & XS_VALID) ? EXT_WHY_KEPT : st == EXT_ST_OK ? EXT_WHY_NONE : EXT_WHY_PLAY;
        }
    } else if (!harmony_on) {
        st = EXT_ST_WAITBUS;
        w = EXT_WHY_BUSOFF;
    }
    if (why)
        *why = w;
    return st;
}
/* EXT governs the track's notes (the pool, or silence): the ISR's one question */
static __attribute__((noinline)) int ext_on(const track_t *t, int src)   /* (declared in seq.c, with ext_cfg / EXT_ON; src: the caller's one read of SOURCE) */
{
    uint32_t st = ext_state(t, src, 0);
    return st != EXT_ST_OFF && st != EXT_ST_NA;
}

/* the whole answer: state, why, the source chord and the pool (see ext_info_t) */
static __attribute__((noinline)) void ext_resolve(const track_t *t, const ext_cfg_t *c, ext_info_t *o)
{
    uint32_t context = c->src >= 2 && c->src <= 5 ? xext[trk_index(t)].chord : 0u;
    uint32_t why, st = ext_state(t, c->src, &why), root, kind, rel;
    uint16_t r16;
    if (c->src >= 2 && c->src <= 5 && st != EXT_ST_NA && xext[trk_index(t)].shadow == c->src) {
        st = context & XS_AMBIG ? EXT_ST_AMBIG : context & XS_VALID ? EXT_ST_OK : EXT_ST_WAIT;
        why = st == EXT_ST_AMBIG && (context & XS_VALID) ? EXT_WHY_KEPT : st == EXT_ST_OK ? EXT_WHY_NONE : EXT_WHY_PLAY;
    }
    memset(o, 0, sizeof *o);
    o->src = (uint8_t)c->src;
    o->state = (uint8_t)st;
    o->why = (uint8_t)why;
    o->mono = trk_vmode(t) != V_POLY;
    if (st != EXT_ST_OK && !(st == EXT_ST_AMBIG && (context & XS_VALID)))
        return;
    if (c->src >= 2) {
        root = (context >> 12) & 15u;
        r16 = (uint16_t)ext_relative(context & 0xFFFu, root);
    } else root = harmony_live_desc(harmony_chord_at(harmony_bar), &r16);
    rel = r16;
    if (!rel) {                                         /* the scale has no triad: no chord to read */
        o->state = EXT_ST_WAITBUS;
        return;
    }
    o->have_chord = 1;
    o->root = (uint8_t)root;
    o->mask = r16;
    /* a known root: the kind by its third (and its fifth: a dim, an aug), not the strict recogniser */
    kind = c->src >= 2 ? (context >> 16) & 7u
         : (rel & 0x10u) ? ((rel & 0x80u) || !(rel & 0x100u) ? XK_MAJ : XK_AUG)
         : (rel & 0x08u) ? ((rel & 0x80u) || !(rel & 0x40u) ? XK_MIN : XK_DIM) : XK_POW;
    o->want = (uint8_t)(chordext_disp((uint32_t)c->ext) + 1u);
    uint32_t scale = c->src >= 2 ? scale_mask(&trk[c->src - 2]) : harmony_scale_abs();
    o->npc = (uint8_t)ext_pool(rel, root, kind, o->want, scale, o->pc);
    if (st == EXT_ST_AMBIG) {
        o->state = EXT_ST_AMBIG;
    } else if (!o->npc) {
        o->state = EXT_ST_EMPTY;
        o->why = EXT_WHY_INSRC;
    } else
        o->state = EXT_ST_OK;
}

/* AMBIG is a UI status, not silence: a valid remembered pool stays playable. */
static inline int ext_ready(const ext_info_t *o)
{
    return o->state == EXT_ST_OK ||
           (o->state == EXT_ST_AMBIG && o->have_chord && o->npc);
}
/* pitch class pc nearest to x (a tie goes to the lower), inside 0..127 (folded by octaves) */
static int32_t ext_near(int32_t x, uint32_t pc)
{
    int32_t d = (int32_t)((pc + 12u - (uint32_t)x % 12u) % 12u), n = d > 6 ? x + d - 12 : d == 6 ? x - 6 : x + d;
    if (n < 0)
        n += 12;
    if (n > 127)
        n -= 12;
    return n;
}
/* OCT: n moved by whole octaves, then folded back into 0..127 (the pitch class stays) */
static int32_t ext_shift(const ext_cfg_t *c, int32_t n)
{
    n += 12 * clamp((int32_t)c->oct, -2, 2);
    while (n < 0)
        n += 12;
    while (n > 127)
        n -= 12;
    return n;
}
/* the pool tone nearest x, OCT applied; -1: none */
static int32_t ext_nearest(const ext_cfg_t *c, const ext_info_t *o, int32_t x)
{
    int32_t best = -1, bd = 999, y, d;
    uint32_t i;
    if (!ext_ready(o))
        return -1;
    for (i = 0; i < o->npc; i++) {
        y = ext_near(x, o->pc[i]);
        d = y > x ? y - x : x - y;
        if (d < bd || (d == bd && y < best)) {
            bd = d;
            best = y;
        }
    }
    return best < 0 ? -1 : ext_shift(c, best);
}
/* SEQ / mono: note x -> the pool tone nearest it (0..127), or -1: no note */
static __attribute__((noinline)) int32_t ext_map(const track_t *t, const ext_cfg_t *c, int32_t x)
{
    ext_info_t o;
    ext_resolve(t, c, &o);
    return ext_nearest(c, &o, x);
}
/* a live note (key, MIDI) with the ARP off: the notes to start, ascending, once each (0..3, none: WAIT / empty). A mono part
 * gets the one nearest tone. With the ARP on the key itself is the anchor (arp_list resolves it at every step) */
static __attribute__((noinline)) uint32_t ext_build(const track_t *t, const ext_cfg_t *c, uint32_t root, uint8_t *out)
{
    ext_info_t o;
    uint32_t n = 0, i, j;
    int32_t y;
    if (t->p[P_AMODE]) {
        out[0] = (uint8_t)root;
        return 1;
    }
    ext_resolve(t, c, &o);
    if (!ext_ready(&o))
        return 0;
    if (o.mono) {
        y = ext_nearest(c, &o, (int32_t)root);
        if (y >= 0)
            out[n++] = (uint8_t)y;
        return n;
    }
    for (i = 0; i < o.npc; i++) {
        y = ext_shift(c, ext_near((int32_t)root, o.pc[i]));
        for (j = 0; j < n && out[j] != (uint8_t)y; j++)
            ;
        if (j == n)
            out[n++] = (uint8_t)y;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && out[j - 1u] > out[j]; j--) {
            uint8_t x = out[j];
            out[j] = out[j - 1u];
            out[j - 1u] = x;
        }
    return n;
}
/* the ARP's anchors (list[0..n)) -> the pool tones near each (at most 8 anchors), once each; *last: the last anchor's tone
 * (REPEAT). Returns the new number of notes (0: nothing to play) */
static __attribute__((noinline)) uint32_t ext_arp(const track_t *t, const ext_cfg_t *c, uint32_t *list, uint32_t n, uint32_t *last)
{
    ext_info_t o;
    uint32_t a[8], i, j, k, m = 0, na = n < 8u ? n : 8u, lastanchor = n ? list[n - 1u] : 0u;
    int32_t y;
    ext_resolve(t, c, &o);
    if (!ext_ready(&o))
        return 0;
    for (i = 0; i < na; i++)
        a[i] = list[i];
    for (i = 0; i < na; i++)
        for (k = 0; k < o.npc; k++) {
            y = ext_shift(c, ext_near((int32_t)a[i], o.pc[k]));
            for (j = 0; j < m && list[j] != (uint32_t)y; j++)
                ;
            if (j == m)
                list[m++] = (uint32_t)y;
        }
    if (n)
        *last = (uint32_t)ext_nearest(c, &o, (int32_t)lastanchor);
    return m;
}
#else
static void chordext_reset_runtime(void) {}
static void ext_window_clear(uint32_t source) { (void)source; }
static void ext_sources_stop(void) {}
static void ext_source_tick(void) {}
static inline uint32_t ext_build(const track_t *t, const ext_cfg_t *c, uint32_t root, uint8_t *out) { (void)t; (void)c; (void)root; (void)out; return 0; }
static inline int32_t ext_map(const track_t *t, const ext_cfg_t *c, int32_t x) { (void)t; (void)c; (void)x; return -1; }
static inline uint32_t ext_arp(const track_t *t, const ext_cfg_t *c, uint32_t *list, uint32_t n, uint32_t *last) { (void)t; (void)c; (void)list; (void)last; return n; }
#endif

/* chord_make for a note played now: a chord is kept in chord_last (the CHORD page shows it) */
static void latch_bus_capture(track_t *t);
static int latch_would_toggle(const track_t *t, const uint8_t *nn, uint32_t n);
static uint32_t chord_build_c(track_t *t, const ext_cfg_t *c, uint32_t root, uint8_t *out)   /* (c: the caller's one read of SOURCE / EXT / OCT for this event) */
{
    if (ext_is_on(t, c))                                 /* CHORD EXT: the pool (or the anchor for the ARP, or nothing); not chord_make, which step entry calls too */
        return ext_build(t, c, root, out);
    int32_t r;
    uint16_t mask;
    uint32_t m = chord_make(t, root, out, &r, &mask), i, k = trk_index(t);
#if !FELUCCA_FM4
    if (ext_sources && !latch_would_toggle(t, out, m)) ext_source_note(t, root, r, mask);
#endif
    if (m > 1u && !latch_would_toggle(t, out, m)) {      /* (a LATCH STACK toggle starts no new chord: chord_last stays) */
        latch_bus_capture(t);
        chord_last[k].root = (uint8_t)clamp(r, 0, 127);
        chord_last[k].key = (uint8_t)clamp((int32_t)root, 0, 127);
        chord_last[k].mask = mask;
        chord_last[k].n = (uint8_t)m;
        chord_last[k].vel = (uint8_t)(kb_vel ? kb_vel : 96u);
        for (i = 0; i < CHORD_MAX; i++)
            chord_last[k].note[i] = i < m ? out[i] : 0u;
        chord_last[k].gen++;
        if (t->p[P_CHRD] == CH_BUS)
            chord_live[k] = 1;
    }
    return m;
}
static __attribute__((unused)) uint32_t chord_build(track_t *t, uint32_t root, uint8_t *out)   /* (the host tests; key_on / midi_play use chord_build_c with their one SOURCE / EXT / OCT read) */
{
    ext_cfg_t c = ext_cfg(t);
    return chord_build_c(t, &c, root, out);
}

/* a chord's name from its root and tones (bit i: i semitones above the root): "C", "Cm7", "F#m7b5", "Gsus4",
 * "A5" -> b (12 bytes) */
static void chord_name(char *b, uint32_t root, uint32_t mask)
{
    uint32_t mi = (mask >> 3) & 1u, ma = (mask >> 4) & 1u, b5 = (mask >> 6) & 1u, p5 = (mask >> 7) & 1u;
    uint32_t s5 = (mask >> 8) & 1u, d7 = (mask >> 9) & 1u, m7 = (mask >> 10) & 1u, M7 = (mask >> 11) & 1u;
    const char *q, *x = "";
    str_cpy(b, N_NOTE[root % 12u], 12);
    if (mi && !ma && b5 && !p5)
        q = d7 ? "dim7" : m7 ? "m7b5" : "dim";
    else if (ma && !mi && s5 && !p5)
        q = m7 ? "+7" : M7 ? "+M7" : "aug";
    else if (ma && !mi)
        q = m7 ? "7" : M7 ? "maj7" : d7 ? "6" : "";
    else if (mi)
        q = m7 ? "m7" : M7 ? "mM7" : d7 ? "m6" : "m";
    else if ((mask >> 5) & 1u) {                        /* no third: suspended */
        q = m7 ? "7" : "";
        x = "sus4";
    } else if ((mask >> 2) & 1u) {
        q = m7 ? "7" : "";
        x = "sus2";
    } else
        q = "5";
    str_cpy(b + str_len(b), q, 12 - str_len(b));
    str_cpy(b + str_len(b), x, 12 - str_len(b));
}

/* the MIDI note (ch, src) of track id - 1 that plays a chord, 0 = none */
static mchord_t *mchord_of(uint32_t ch, uint32_t src, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id && mchord[i].ch == ch && mchord[i].src == src)
            return &mchord[i];
    return 0;
}

/* a MIDI-held chord of track id - 1 sounds note */
static int mchord_held(uint32_t id, uint32_t note)
{
    uint32_t i, j;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id)
            for (j = 0; j < mchord[i].n; j++)
                if (mchord[i].note[j] == note)
                    return 1;
    return 0;
}

static void mchord_forget(uint32_t track)               /* panic: the track's MIDI chords are gone */
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == track + 1u)
            mchord[i].id = 0;
}


/* STOP/panic: a bus chord started or retriggered by the HARMONY engine is not in the key/sequence owner lists;
 * release it explicitly so no tone can hang. */
static int chord_plain_owned(uint32_t i, uint32_t note)   /* a key or MIDI chord of the track still lists the note */
{
    uint32_t k, m, j;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == i)
            for (j = 0; j < kb_chn[k]; j++)
                if (kb_chord[k][j] == note)
                    return 1;
    for (m = 0; m < MCHORD_N; m++)
        if (mchord[m].id == i + 1u)
            for (j = 0; j < mchord[m].n; j++)
                if (mchord[m].note[j] == note)
                    return 1;
    return 0;
}
static void chord_panic(uint32_t i);
static uint32_t lat_eat[NTRK][4];       /* (seq.c: bit n = the press of note n ended it; declared here for chord_stop_all) */
static void chord_stop_all(void)
{
    uint32_t i, j;
    for (i = 0; i < NTRK; i++) {
        uint32_t bus = trk[i].p[P_CHRD] == CH_BUS && !EXT_ON(&trk[i]);   /* (an EXT track has no CHRD BUS chord, whatever CHRD stores) */
        if (!bus && !chord_live[i] && !chord_last[i].n)
            continue;                                 /* round 7: a CHRD BUS chord is the engine's: it ends; a plain chord a player still holds (a key or MIDI list names the note) sounds on, as in 0.11.0; a note of the last chord nobody holds any more (an orphan) still ends, so STOP cleans up as before */
        for (j = 0; j < chord_last[i].n; j++)
            if (chord_last[i].note[j] && (bus || !chord_plain_owned(i, chord_last[i].note[j])))
                trk_note_off(&trk[i], chord_last[i].note[j]);
        if (bus) {
            chord_last[i].n = 0;
            memset(lat_eat[i], 0, sizeof lat_eat[i]);   /* (a kept key's 'this press ended it' mark: its release is no longer a live one) */
            chord_panic(i);
        }
    }
}

/* RETRIG (LINK row K2: OFF, HARM = a chord change of the bus at a bar line, SCALE = a key change of the LINK source, BOTH; the values 1 / 2
 * were named CHORD / LINK before): the last CHRD BUS chord of the track is released and sounded again with the bus' new
 * chord / the linked scale; with HARM / BOTH an EXT voicing, a held single note (CHRD OFF, FOLLOW) and latched notes follow the bar's chord
 * too (retrigger_follow, seq.c), and SCALE does nothing on an EXT track. Per track (the sequencer and ARP retrigger
 * their own steps anyway). */
static void retrigger_latched(uint32_t i);
static void retrigger_owners(uint32_t i, const uint8_t *old, uint32_t oldn, const uint8_t *new, uint32_t newn);
static void retrigger_follow(uint32_t i);
static void chord_live_sync(uint32_t i);
static void chord_panic(uint32_t i);
static int chord_owner_matches(uint32_t i, const uint8_t *old, uint32_t oldn);
static int chord_owner_fallback(uint32_t i, uint32_t *src, uint8_t *old, uint32_t *oldn);
static int chord_note_held(uint32_t i, uint32_t note);
static void link_retrigger(uint32_t i)
{
    track_t *t = &trk[i];
    if (t->p[P_AMODE])
        return;                                        /* round 7: with the ARP on, the held list is the ARP's and it follows HARMONY on every step; rewriting the key's chord would leave the old notes in that list (an arpeggio that outlives its key) */
    if (EXT_ON(t)) {                                   /* an EXT voicing follows the bar's chord only: a LINK key change does not move the pool, SCALE has no effect here (nothing sounds again) */
        if (ret_harm)
            retrigger_follow(i);                       /* RETRIG HARM / BOTH: rebuilt per owner from the new pool (seq.c) */
        return;
    }
    if (ret_harm && t->p[P_CHRD] == CH_OFF && harmony_on && harmony_follow[i] && !chord_kit(t)) {
        retrigger_follow(i);                           /* a held single note / the latched pitches are snapped again to the bar's chord */
        return;
    }
    uint32_t k = trk_index(t), j, nn = 0, oldn = chord_last[k].n, src = chord_last[k].key;
    uint8_t out[CHORD_MAX], old[CHORD_MAX];
    int32_t r = 0;
    uint16_t mask = 0;
    for (j = 0; j < oldn; j++)
        old[j] = chord_last[k].note[j];
    if (!chord_live[k] || !chord_owner_matches(k, old, oldn)) {
        if (!chord_owner_fallback(k, &src, old, &oldn)) {   /* no real owner: no resurrection */
            chord_live[k] = 0;
            retrigger_latched(i);
            return;
        }
        chord_live[k] = 1;
    }
    if (t->p[P_CHRD] != CH_BUS || !oldn) {
        retrigger_latched(i);                          /* LATCH without CHRD BUS (an EXT track never gets here: it returned above) */
        return;
    }
    /* Rebuild the bus chord for the new harmony chord; reusing the old chord_last notes would repeat the old
     * root/fifth and never sound the new third. */
    nn = chord_make(t, src, out, &r, &mask);
    chord_last[k].key = (uint8_t)clamp((int32_t)src, 0, 127);
    /* Move this chord's owners first. Other voicings may still own one of its old pitches. */
    retrigger_owners(i, old, oldn, out, nn);
    for (j = 0; j < oldn; j++)
        if (!chord_note_held(k, old[j]))
            trk_note_off(t, old[j]);
    chord_last[k].n = (uint8_t)nn;
    chord_last[k].root = (uint8_t)clamp(r, 0, 127);
    chord_last[k].mask = mask;
    for (j = 0; j < CHORD_MAX; j++)
        chord_last[k].note[j] = j < nn ? out[j] : 0u;
    for (j = 0; j < nn; j++)
        if (out[j])
            trk_note_on(t, out[j], chord_last[k].vel ? chord_last[k].vel : 96u);
}

/* CHORD EXT status for the UI (see ext_info_t in core.h); f: the follower track. Read-only, safe from the main loop: it
 * reads the same stored values and bus state as the audio ISR */
static void ext_info(uint32_t f, ext_info_t *o)
{
    memset(o, 0, sizeof *o);
#ifdef EXT_INFO_HOOK
    if (EXT_INFO_HOOK(f, o))
        return;
#endif
#if !FELUCCA_FM4
    if (f < NTRK) {
        ext_cfg_t c = ext_cfg(&trk[f]);
        ext_resolve(&trk[f], &c, o);
    }
#else
    (void)f;
#endif
}
