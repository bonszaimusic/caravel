/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* Live CHORD sources: audio context only. One aligned word publishes each
 * follower's remembered chord; the UI never writes collector state. */
#define EXT_LIVE_API 1
#define EXT_GAP 41u
#define EXT_CAP 138u
#define XS_VALID (1u << 19)
#define XS_AMBIG (1u << 20)
typedef struct {
    volatile uint32_t chord; /* pcs[0..11], root[12..15], kind[16..18], valid/ambig */
    uint16_t pcs;
    uint8_t bass, gap, cap, shadow;
} xext_t;
_Static_assert(sizeof(xext_t) == 12u, "bounded live chord state");
static xext_t xext[NTRK];
static uint8_t ext_sources;
static int ext_real_input(void);

static uint32_t ext_bits(uint32_t p)
{
    uint32_t n = 0;
    while (p) { p &= p - 1u; n++; }
    return n;
}
static uint32_t ext_relative(uint32_t pcs, uint32_t root)
{
    return ((pcs >> root) | (pcs << (12u - root))) & 0xFFFu;
}
static uint32_t ext_kind(uint32_t rel)
{
    return rel & 16u ? (rel & 128u || !(rel & 256u) ? XK_MAJ : XK_AUG)
         : rel & 8u ? (rel & 128u || !(rel & 64u) ? XK_MIN : XK_DIM)
         : rel & 4u ? XK_SUS2 : rel & 32u ? XK_SUS4 : XK_POW;
}
/* Rule recogniser. Core tones outrank bass; a bass-root match then breaks ties.
 * Tensions stay in pcs so that ext_pool never invents an already played tone. */
static __attribute__((noinline)) int ext_recognise(uint32_t pcs, uint32_t bass, uint32_t *root, uint32_t *kind)
{
    uint32_t n = ext_bits(pcs), best = 0;
    bass %= 12u;
    if (n < 2u) return 0;
    if (n == 2u) {
        uint32_t rel = ext_relative(pcs, bass);
        if (rel == 129u) *root = bass;
        else if (rel == 33u) *root = (bass + 5u) % 12u;
        else return 0;
        *kind = XK_POW;
        return 1;
    }
    for (uint32_t step = 0; step < 12u; step++) {
        uint32_t r = (bass + step) % 12u, rel, k, core = 1u, allowed, seventh, score;
        if (!((pcs >> r) & 1u)) continue;
        rel = ext_relative(pcs, r);
        if ((rel & 24u) == 24u || (rel & 3072u) == 3072u) continue;
        k = ext_kind(rel);
        if (rel & 24u) {
            core |= rel & 24u;
            if (rel & 128u) core |= 128u;
            else if (k == XK_DIM) core |= 64u;
            else if (k == XK_AUG) core |= 256u;
            else if (!(rel & 3072u)) continue; /* a root/third/seventh shell */
        } else {
            if (!(rel & 128u)) continue;
            core |= 128u;
            if (k == XK_SUS2) core |= 4u;
            else if (k == XK_SUS4) core |= 32u;
        }
        allowed = core | 4u | 512u | 3072u;
        if (rel & 24u) allowed |= 32u;
        if (rel & ~allowed) continue;
        seventh = rel & (k == XK_DIM ? 3584u : 3072u);
        core |= seventh;
        if (!seventh && (k == XK_MAJ || k == XK_MIN)) core |= rel & 512u;
        score = ext_bits(core) * 4096u + (r == bass) * 2048u
              + (12u - ext_bits(rel & ~core)) * 64u + !!seventh * 8u + (6u - k);
        if (score > best) { best = score; *root = r; *kind = k; }
    }
    return best != 0;
}

static uint32_t ext_live_why(uint32_t follower, uint32_t source)
{
    if (source == follower) return EXT_WHY_SELF;
    if (trk[source].p[P_CSRC]) return EXT_WHY_SRCEXT;
    if (chord_kit(&trk[source])) return EXT_WHY_KIT;
#if FELUCCA_PAIR_MORPH
    if (pair_follower(&trk[source])) return EXT_WHY_PAIR;
#endif
    return EXT_WHY_NONE;
}
static void ext_window_clear(uint32_t source) { xext[source].pcs = 0; }
static void ext_sources_stop(void)
{
    for (uint32_t i = 0; i < NTRK; i++) ext_window_clear(i);
}
static void chordext_reset_runtime(void)
{
    memset(xext, 0, sizeof xext);
    ext_sources = 0;
}
static __attribute__((noinline)) void ext_commit(uint32_t source, uint32_t pcs, uint32_t root, uint32_t kind, int valid)
{
#ifdef EXT_COMMIT_HOOK
    EXT_COMMIT_HOOK(source, valid);
#endif
    for (uint32_t f = 0; f < NTRK; f++) {
        uint32_t old, next;
        if (trk[f].p[P_CSRC] != (int32_t)source + 2 || ext_live_why(f, source)) continue;
        old = xext[f].chord;
        next = valid ? pcs | root << 12 | kind << 16 | XS_VALID : old | XS_AMBIG;
        xext[f].chord = next; /* the single aligned publication */
        if (valid && (old & ~XS_AMBIG) != next && (link_retrig[f] & 1u)) {
            uint8_t was = ret_harm;
            ret_harm = 1;
            link_retrigger(f);
            ret_harm = was;
        }
    }
}
/* Called before this block's panel/MIDI inputs. A note arriving exactly at the
 * expiry belongs to the next gesture; no wall-clock/debounce timer is used. */
static __attribute__((noinline)) void ext_source_tick(void)
{
    uint32_t wanted = 0;
    for (uint32_t f = 0; f < NTRK; f++) {
        int32_t src = trk[f].p[P_CSRC];
        uint32_t s = src >= 0 && src <= 5 ? (uint32_t)src : 0u;
        if (xext[f].shadow != s) {
            xext[f].shadow = (uint8_t)s;
            xext[f].chord = 0;
            ext_window_clear(f);
        }
        if (s >= 2u && !ext_live_why(f, s - 2u) && !chord_kit(&trk[f])
#if FELUCCA_PAIR_MORPH
            && !pair_follower(&trk[f])
#endif
        ) wanted |= 1u << (s - 2u);
    }
    ext_sources = (uint8_t)wanted;
    for (uint32_t s = 0; s < NTRK; s++) {
        xext_t *x = &xext[s];
        if (!((wanted >> s) & 1u)) { x->pcs = 0; continue; }
        if (!x->pcs) continue;
        if (++x->gap < EXT_GAP && ++x->cap < EXT_CAP) continue;
        uint32_t root = 0, kind = 0, pcs = x->pcs;
        x->pcs = 0;
        if (ext_bits(pcs) > 1u) {
            int valid = ext_recognise(pcs, x->bass, &root, &kind);
            ext_commit(s, pcs, root, kind, valid);
        }
    }
}
static __attribute__((noinline)) void ext_source_note(track_t *t, uint32_t note, int32_t known_root, uint32_t rel)
{
    uint32_t s = trk_index(t);
    xext_t *x = &xext[s];
    if (!((ext_sources >> s) & 1u) || !ext_real_input()) return;
    if (rel) {
        uint32_t root = ((uint32_t)(known_root + 120)) % 12u;
        uint32_t pcs = ((rel << root) | (rel >> (12u - root))) & 0xFFFu;
        x->pcs = 0;
        ext_commit(s, pcs, root, ext_kind(rel), 1);
        return;
    }
    if (!x->pcs) { x->bass = (uint8_t)note; x->cap = 0; }
    if (note < x->bass) x->bass = (uint8_t)note;
    x->gap = 0;
    x->pcs |= (uint16_t)(1u << (note % 12u));
}
