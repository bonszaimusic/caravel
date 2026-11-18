/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* CHORD SOURCE = HARM / EXT / OCT (slice 1b engine) against the real keyboard, MIDI IN, ARP, SEQ and voice code, driven
 * block by block (tests/chordext_sim.h). T01-T16 of the test study (reports/phase2/ui_and_tests_study.md B.d); T17 checks the BUILD data/input interface separately. The oracle (the design's diatonic table, the extended-source table, the placement rule) is written down here as
 * plain C tables / a naive search, not derived from the firmware's resolver.
 *   T01 HARM pool C Am F G (+ Dm Em Bdim, A harmonic minor): the notes, ext_info      T02 dedupe / extended source / never padded
 *   T03 ARP normal + CHORD, phase unchanged     T04 SEQ mono / poly nearest pool tone, tie lower, no multiplication
 *   T05 SEQ non-destructive                     T06 every step, no later triad snapping
 *   T07 source change at ARP/SEQ boundaries, held keys / MIDI / LATCH / ARP HOLD (pitch stays, no re-attack)
 *   T08 release identity after SOURCE / EXT / OCT / engine change     T09 OCT and 0..127 folding
 *   T10 WAIT BUS (HARMONY off)      T11 DRUM / kit N/A, audio unchanged     T12 SOURCE 2..5 are OFF in this slice (and no self source)
 *   T13 MONO / LEGATO / UNISON one note     T14 CHRD / FOLLOW overridden but stored     T15 rapid knob changes
 *   T16 project round trip, behaviour after the load
 * Groups run alone: CE_GROUP=T07 (or a comma list). Every check is written so that it fails on b2a8d58 by behaviour (the three
 * settings are stored there and nothing reads them; the adapter tests/chordext_api.h compiles on both); the few checks that
 * hold there by design are marked "(invariant)". Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
#include "chordext_sim.h"
#include "chordext_api.h"

#if FELUCCA_FM4
int main(void) { printf("chordext test skipped (FELUCCA_FM4=1)\n"); return 0; }
#else

static int x_check(const char *what, int ok)
{
    printf("chordext: %-96s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static int x_on(const char *g)
{
    const char *o = getenv("CE_GROUP");
    size_t n = strlen(g);
    if (!o) return 1;
    for (const char *p = o; *p;) {
        const char *e = strchr(p, ',');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        if (l == n && !strncmp(p, g, n)) return 1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

#define K_C4 7u
#define K_E4 11u
#define K_G4 14u
#define K_A4 16u

/* ------------------------------------------------------------------ observer --- */
typedef struct { uint32_t blk; uint8_t trk, on, note, bar; } nev_t;
#define NEV_MAX 60000u
static nev_t nev[NEV_MAX];
static uint32_t nnev;
typedef struct { uint8_t on, note; uint32_t age; } xv_t;
static xv_t xv_prev[NTRK][NVOICE];
static void obs(uint32_t blk, const int32_t *a)
{
    uint32_t t, i;
    (void)a;
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[t].v[i];
            xv_t *p = &xv_prev[t][i];
            uint8_t on = (uint8_t)(v->active && v->gate);
            if (p->on && (!on || p->note != v->note || p->age != v->age) && nnev < NEV_MAX)
                nev[nnev++] = (nev_t){blk, (uint8_t)t, 0, p->note, (uint8_t)harmony_bar};
            if (on && (!p->on || p->note != v->note || p->age != v->age) && nnev < NEV_MAX)
                nev[nnev++] = (nev_t){blk, (uint8_t)t, 1, v->note, (uint8_t)harmony_bar};
            p->on = on; p->note = v->note; p->age = v->age;
        }
}

/* ------------------------------------------------------------------ scene --- */
static ce_ev_t E[512];
static uint32_t En;
static void ev_add(uint32_t us, uint32_t kind, uint32_t a, uint32_t b, uint32_t c)
{
    if (En >= 512u) { printf("too many events\n"); exit(2); }
    E[En++] = (ce_ev_t){us, (uint8_t)kind, (uint8_t)a, (uint8_t)b, (uint8_t)c};
}
static void u_key(uint32_t ms, uint32_t k, uint32_t dn) { ev_add(ms * 1000u, CE_KEY, k, dn, 0); }
static void u_usb(uint32_t ms, uint32_t st, uint32_t d1, uint32_t d2) { ev_add(ms * 1000u, CE_USB, st, d1, d2); }
static void run_to(uint32_t ms)
{
    if (En) { ce_sched(E, En); En = 0; }
    ce_run_until(ms * 1000u);
}
static void harm_const(uint32_t root)                      /* the same bus chord every bar */
{
    uint32_t i;
    for (i = 0; i < 4u; i++) { hroot_set(i, (uint8_t)root); hdur_set(i, 0); }
    harmony_on = 1; harmony_bar = 0;
}
static void harm_prog(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    hroot_set(0, (uint8_t)a); hroot_set(1, (uint8_t)b); hroot_set(2, (uint8_t)c); hroot_set(3, (uint8_t)d);
    for (uint32_t i = 0; i < 4u; i++) hdur_set(i, 0);
    harmony_on = 1; harmony_bar = 0;
}
static void setup(void)
{
    uint32_t k;
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners); memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target); memset(&midi_clock, 0, sizeof midi_clock);
    memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = mo_r = mo_w = 0; midi_in_overflow = 0; midi_beat_samples = 0;
    for (k = 0; k < NTRK; k++) latch_forget(k);
    latch_req = 0; panic_req = 0;
    fm1_in.notes = kb_prev = 0; fm1_ms = 0; usb.config = 1;
    memset(song.mch, 0, sizeof song.mch); song.g[G_ROUTE] = 0; midi_seen = 0;
    song.sel = 1; song.playing = 0; song.rec = 0; song.master_q12 = 4096;   /* the follower is T2 (index 1) */
    song.g[G_BPM] = 240;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memset(t->step, 0, sizeof t->step);
        t->p[P_VOICE] = V_POLY; t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_QUANT] = 0; t->p[P_MUTE] = 0;
        t->p[P_CHRD] = CH_OFF; t->p[P_VOIC] = VC_CLOSE; t->p[P_LATCH] = LT_OFF; t->p[P_AMODE] = 0; t->p[P_AHOLD] = 0;
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2;
        ce_set(k, CE_SRC, 0); ce_set(k, CE_EXT, 1); ce_set(k, CE_OCT, 0);
    }
    harmony_on = 0; harmony_bar = 0;
    events_block(CTL); events_block(CTL);
    memset(xv_prev, 0, sizeof xv_prev); nnev = 0;
    ce_reset();
    ce_render = 0; ce_post_block = obs;
    En = 0;
}

/* ------------------------------------------------------------------ helpers --- */
static uint32_t ngated(uint32_t t)                          /* gated voices */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += trk[t].v[i].active && trk[t].v[i].gate;
    return n;
}
/* the distinct gated notes of track t, ascending -> out, their number */
static uint32_t gnotes(uint32_t t, uint8_t *out)
{
    uint32_t i, j, n = 0;
    uint8_t all[128] = {0};
    for (i = 0; i < NVOICE; i++)
        if (trk[t].v[i].active && trk[t].v[i].gate) all[trk[t].v[i].note & 127u] = 1;
    for (j = 0; j < 128u; j++) if (all[j]) out[n++] = (uint8_t)j;
    return n;
}
static int notes_are(uint32_t t, const uint8_t *want, uint32_t nw)   /* want ascending */
{
    uint8_t g[128];
    uint32_t n = gnotes(t, g), i;
    if (n != nw) return 0;
    for (i = 0; i < n; i++) if (g[i] != want[i]) return 0;
    return 1;
}
static void show(const char *tag, uint32_t t)
{
    uint8_t g[128];
    uint32_t n = gnotes(t, g), i;
    printf("    %s gated:", tag);
    for (i = 0; i < n; i++) printf(" %u", g[i]);
    printf("\n");
}

/* the ORACLE. Pitch classes: C 0 D 2 E 4 F 5 G 7 A 9 B 11. Design table (C major, bus scale C major). */
static const struct { uint8_t root; uint16_t rel; uint8_t pc[3][3]; uint8_t n[3]; } ORC[] = {
    /* root  triad (rel)   7 | 7+9 | 7+9+13  (pitch classes; n tells how many are used) */
    {0,  0x091, {{11, 0, 0}, {11, 2, 0}, {11, 2, 9}}, {1, 2, 3}},   /* C   : B / B D / B D A */
    {9,  0x089, {{7, 0, 0},  {7, 11, 0}, {7, 11, 5}}, {1, 2, 3}},   /* Am  : G / G B / G B F */
    {5,  0x091, {{4, 0, 0},  {4, 7, 0},  {4, 7, 2}},  {1, 2, 3}},   /* F   : E / E G / E G D */
    {7,  0x091, {{5, 0, 0},  {5, 9, 0},  {5, 9, 4}},  {1, 2, 3}},   /* G   : F / F A / F A E */
    {2,  0x089, {{0, 0, 0},  {0, 4, 0},  {0, 4, 11}}, {1, 2, 3}},   /* Dm  : C / C E / C E B */
    {4,  0x089, {{2, 0, 0},  {2, 5, 0},  {2, 5, 0}},  {1, 2, 3}},   /* Em  : D / D F / D F C */
    {11, 0x049, {{9, 0, 0},  {9, 0, 0},  {9, 0, 7}},  {1, 2, 3}},   /* Bdim: A / A C / A C G */
};
#define NORC ((int)(sizeof ORC / sizeof ORC[0]))
#define orc_fix() ((void)0)
/* the note of pitch class pc nearest to x (a tie: the lower), folded into 0..127 (naive search over a wide range) */
static int or_near(int x, int pc)
{
    int best = -999, bd = 999, c;
    for (c = -30; c < 160; c++)
        if (((c % 12) + 12) % 12 == pc) {
            int d = c > x ? c - x : x - c;
            if (d < bd || (d == bd && c < best)) { bd = d; best = c; }
        }
    while (best < 0) best += 12;
    while (best > 127) best -= 12;
    return best;
}
static int or_fold(int n, int oct)
{
    n += 12 * oct;
    while (n < 0) n += 12;
    while (n > 127) n -= 12;
    return n;
}
static void or_sort(uint8_t *a, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = i + 1; j < n; j++)
            if (a[j] < a[i]) { uint8_t x = a[i]; a[i] = a[j]; a[j] = x; }
}
/* the notes the oracle expects for a key at x: the pool tones nearest x, OCT applied, distinct, ascending */
static uint32_t or_live(int x, const uint8_t *pcs, uint32_t n, int oct, uint8_t *out)
{
    uint32_t i, j, m = 0;
    for (i = 0; i < n; i++) {
        uint8_t y = (uint8_t)or_fold(or_near(x, pcs[i]), oct);
        for (j = 0; j < m && out[j] != y; j++) ;
        if (j == m) out[m++] = y;
    }
    or_sort(out, m);
    return m;
}
/* the single tone for a mono key / SEQ note: nearest pool tone (tie lower), OCT applied; -1 none */
static int or_one(int x, const uint8_t *pcs, uint32_t n, int oct)
{
    int best = -1, bd = 999;
    for (uint32_t i = 0; i < n; i++) {
        int y = or_near(x, pcs[i]), d = y > x ? y - x : x - y;
        if (d < bd || (d == bd && y < best)) { bd = d; best = y; }
    }
    return best < 0 ? -1 : or_fold(best, oct);
}
static const uint8_t *orc_pcs(int row, int level) { return ORC[row].pc[level]; }

/* ------------------------------------------------------------------ T01 --- */
static int t01(void)
{
    int bad = 0, r, lv, ok_all_notes = 1, ok_all_info = 1, ok_all_off = 1;
    orc_fix();
    for (r = 0; r < NORC; r++)
        for (lv = 0; lv < 3; lv++) {
            uint8_t want[3], g[8];
            uint32_t n;
            ext_info_t o;
            setup();
            harm_const(ORC[r].root);
            ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, lv);
            u_key(10, K_C4, 1);
            run_to(30);
            n = or_live(60, orc_pcs(r, lv), ORC[r].n[lv], 0, want);
            if (!notes_are(1, want, n)) {
                ok_all_notes = 0;
                printf("    chord root %d level %d: want %u notes (first %u)", ORC[r].root, lv, n, n ? want[0] : 0);
                show("", 1);
            }
            ce_info(1, &o);
            if (!(o.state == EXT_ST_OK && o.why == EXT_WHY_NONE && o.src == 1 && o.have_chord && o.root == ORC[r].root &&
                  o.mask == ORC[r].rel && o.want == lv + 1 && o.npc == ORC[r].n[lv] && !o.mono &&
                  !memcmp(o.pc, orc_pcs(r, lv), ORC[r].n[lv])))
                ok_all_info = 0;
            u_key(40, K_C4, 0);
            run_to(80);
            if (ngated(1)) ok_all_off = 0;
        }
    bad += x_check("T01 HARM: C Am F G Dm Em Bdim x (7, 7+9, 7+9+13) on key C4: the pool's tones nearest the key (21 cases)", ok_all_notes);
    bad += x_check("T01 ext_info: OK, source chord root + mask, want, npc, pc[] in order 7 9 13 (21 cases)", ok_all_info);
    bad += x_check("T01 the key's release ends exactly them (21 cases)", ok_all_off);
    {   /* A harmonic minor (scale HARM, root A): Am -> G# B F (the alternates), C aug (C E G#) -> B D, no 13 */
        uint8_t want[3], pcs1[3] = {8, 11, 5}, pcs2[2] = {11, 2};
        uint32_t n;
        ext_info_t o;
        int okA, okB;
        setup();
        trk[0].p[P_SCALE] = 7; trk[0].p[P_ROOT] = 9;
        harm_const(9);
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        u_key(10, K_C4, 1); run_to(30);
        n = or_live(60, pcs1, 3, 0, want);
        ce_info(1, &o);
        okA = notes_are(1, want, n) && o.state == EXT_ST_OK && o.npc == 3 && !memcmp(o.pc, pcs1, 3);
        bad += x_check("T01 A harmonic minor, Am: 7 = G# (the b7 is not in the scale, the maj7 is), 9 = B, 13 = F (not F#)", okA);
        u_key(40, K_C4, 0); run_to(60);
        harm_const(0);
        u_key(70, K_C4, 1); run_to(90);
        n = or_live(60, pcs2, 2, 0, want);
        ce_info(1, &o);
        okB = notes_are(1, want, n) && o.state == EXT_ST_OK && o.npc == 2 && o.want == 3 && !memcmp(o.pc, pcs2, 2) && o.mask == 0x111;
        bad += x_check("T01 A harmonic minor, C E G# (aug): 7 = B, 9 = D, no 13 (2 of 3)", okB);
    }
    return bad;
}

/* ------------------------------------------------------------------ T02 --- */
static int t02(void)
{
    int bad = 0, ok = 1, i, l;
    static const uint16_t CMAJ = 0xAB5;
    static const struct {
        uint8_t kind; uint16_t rel; uint16_t scale; int8_t pc[3][3]; uint8_t n[3];   /* -1 unused; pitch classes absolute, root C */
        const char *name;
    } TB[] = {
        /* Cmaj with its own extensions, scale C major (design: "source with own extensions") */
        {CE_MAJ, 1 << 0 | 1 << 4 | 1 << 7 | 1 << 11, CMAJ, {{-1}, {2, -1}, {2, 9, -1}}, {0, 1, 2}, "C E G B"},
        {CE_MAJ, 1 << 0 | 1 << 2 | 1 << 4 | 1 << 7 | 1 << 11, CMAJ, {{-1}, {-1}, {9, -1}}, {0, 0, 1}, "C E G B D"},
        {CE_MAJ, 1 << 0 | 1 << 2 | 1 << 4 | 1 << 7, CMAJ, {{11}, {11}, {11, 9}}, {1, 1, 2}, "C E G D (add9)"},
        {CE_MAJ, 1 << 0 | 1 << 4 | 1 << 7 | 1 << 10, CMAJ, {{-1}, {2}, {2, 9}}, {0, 1, 2}, "C E G Bb (b7 counts)"},
        {CE_MAJ, 1 << 0 | 1 << 4 | 1 << 7 | 1 << 9, CMAJ, {{11}, {11, 2}, {11, 2}}, {1, 2, 2}, "C E G A (6)"},
        {CE_DIM, 1 << 0 | 1 << 3 | 1 << 6 | 1 << 9, CMAJ, {{-1}, {2}, {2, 8}}, {0, 1, 2}, "C Eb Gb A (dim7: the 6 is the 7th; 13 = Ab, design 1a table)"},
        {CE_MIN, 1 << 0 | 1 << 3 | 1 << 7 | 1 << 10, CMAJ, {{-1}, {2}, {2, 9}}, {0, 1, 2}, "C Eb G Bb"},
        /* every kind on C, scale unknown (0): primaries */
        {CE_DIM, 1 << 0 | 1 << 3 | 1 << 6, 0, {{10}, {10, 2}, {10, 2, 8}}, {1, 2, 3}, "dim: Bb / Bb D / Bb D Ab"},
        {CE_AUG, 1 << 0 | 1 << 4 | 1 << 8, 0, {{10}, {10, 2}, {10, 2}}, {1, 2, 2}, "aug: no 13"},
        {CE_SUS2, 1 << 0 | 1 << 2 | 1 << 7, 0, {{10}, {10}, {10, 9}}, {1, 1, 2}, "sus2: the 9 is its 2"},
        {CE_SUS4, 1 << 0 | 1 << 5 | 1 << 7, 0, {{10}, {10, 2}, {10, 2, 9}}, {1, 2, 3}, "sus4"},
        {CE_POW, 1 << 0 | 1 << 7, 0, {{10}, {10, 2}, {10, 2, 9}}, {1, 2, 3}, "power chord"},
        {CE_MIN, 1 << 0 | 1 << 3 | 1 << 7, 0, {{10}, {10, 2}, {10, 2, 9}}, {1, 2, 3}, "min"},
        /* the alternates (primary outside the scale, alternate inside): A minor in C major is Am; V in C major is G */
        {CE_MIN, 1 << 0 | 1 << 3 | 1 << 7, 0xAB5, {{-1}}, {0, 0, 0}, "(placeholder, root C: Cm in C major scale: 7 = Bb primary outside, alternate B inside)"},
    };
    uint8_t pc[4];
    (void)i;
    for (int t = 0; t < (int)(sizeof TB / sizeof TB[0]) - 1; t++) {
        for (l = 0; l < 3; l++) {
            int n = ce_pool(TB[t].rel, 0, TB[t].kind, (uint32_t)l + 1u, TB[t].scale, pc), k, same = n == TB[t].n[l];
            for (k = 0; same && k < n; k++) same &= pc[k] == (uint8_t)TB[t].pc[l][k];
            if (!same) { ok = 0; printf("    %s level %d: got %d pcs (first %d), want %u\n", TB[t].name, l + 1, n, n > 0 ? pc[0] : -1, TB[t].n[l]); }
        }
    }
    bad += x_check("T02 pool of a source with its own extensions, other kinds, scale known / unknown: the design's tables (13 sources x 3)", ok);
    {   /* the alternates by scale: Cm (C Eb G) in C major (Bb outside: B; 9 D; 13 A is inside) */
        int n = ce_pool(1 << 0 | 1 << 3 | 1 << 7, 0, CE_MIN, 3, CMAJ, pc);
        int a = n == 3 && pc[0] == 11 && pc[1] == 2 && pc[2] == 9;
        /* G major chord (root 7) in C major: 7 = F (alternate b7, F#/maj7 is outside), 9 = A, 13 = E */
        n = ce_pool(1 << 0 | 1 << 4 | 1 << 7, 7, CE_MAJ, 3, CMAJ, pc);
        int b = n == 3 && pc[0] == 5 && pc[1] == 9 && pc[2] == 4;
        /* the same G with the scale unknown: the primaries F# A E */
        n = ce_pool(1 << 0 | 1 << 4 | 1 << 7, 7, CE_MAJ, 3, 0, pc);
        int c = n == 3 && pc[0] == 6 && pc[1] == 9 && pc[2] == 4;
        bad += x_check("T02 the scale only picks between primary and alternate (Cm in C: B; G in C: F; G with no scale: F#)", a && b && c);
    }
    {   /* properties over every chord shape: never padded, never a played tone, distinct, <= level, only candidate intervals */
        int okp = 1;
        for (uint32_t kind = 0; kind < 7u && okp; kind++)
            for (uint32_t rel = 1; rel < 4096u && okp; rel += 2u)
                for (uint32_t root = 0; root < 12u && okp; root += 5u)
                    for (uint32_t level = 1; level <= 3u && okp; level++)
                        for (uint32_t sc = 0; sc < 2u && okp; sc++) {
                            int n = ce_pool(rel, root, kind, level, sc ? CMAJ : 0, pc), k, j;
                            if (n < 0 || n > (int)level) okp = 0;
                            for (k = 0; okp && k < n; k++) {
                                uint32_t iv = (pc[k] + 12u - root) % 12u;
                                if ((rel >> iv) & 1u) okp = 0;                 /* played already */
                                if (iv == 0 || iv == 3 || iv == 4 || iv == 5 || iv == 6 || iv == 7) okp = 0;   /* a padding tone */
                                for (j = 0; j < k; j++) if (pc[j] == pc[k]) okp = 0;
                            }
                        }
        bad += x_check("T02 never padded: <= level tones, none already played (any octave), no root/third/fourth/fifth, distinct (all shapes)", okp);
    }
    return bad;
}

/* ------------------------------------------------------------------ ARP helper --- */
typedef struct { uint32_t n; uint32_t blk[256]; uint8_t note[256]; } rec_t;
static void collect(rec_t *r, uint32_t t, uint32_t from_blk)   /* the note-ons of track t */
{
    r->n = 0;
    for (uint32_t i = 0; i < nnev; i++)
        if (nev[i].on && nev[i].trk == t && nev[i].blk >= from_blk && r->n < 256u) {
            r->blk[r->n] = nev[i].blk; r->note[r->n] = nev[i].note; r->n++;
        }
}
static void arp_scene(int ext, int mode, int hold)
{
    setup();
    harm_const(0);
    song.g[G_BPM] = 240;
    trk[1].p[P_ARATE] = 2; trk[1].p[P_AMODE] = (int16_t)mode; trk[1].p[P_AOCT] = 1; trk[1].p[P_AHOLD] = hold;
    if (ext) { ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); }
    transport_req = 1;
    u_key(10, K_C4, 1);
    run_to(1200);
}
static int t03(void)
{
    int bad = 0;
    static rec_t off, ext;
    uint32_t i, same_blk = 1, pitch_ok = 1;
    uint8_t seen[128] = {0};
    uint32_t distinct = 0;
    arp_scene(0, AM_UP, 0); collect(&off, 1, 0);
    arp_scene(1, AM_UP, 0); collect(&ext, 1, 0);
    for (i = 0; i < ext.n; i++) {
        uint32_t pc = ext.note[i] % 12u;
        if (!(pc == 11 || pc == 2 || pc == 9)) pitch_ok = 0;
        if (!seen[ext.note[i]]) { seen[ext.note[i]] = 1; distinct++; }
    }
    same_blk = ext.n >= 8u && ext.n == off.n;
    for (i = 0; same_blk && i < ext.n; i++) same_blk &= ext.blk[i] == off.blk[i];
    bad += x_check("T03 ARP UP over EXT 7+9+13 on C: every step a pool tone (B D A near the held key), >= 3 distinct pitches", ext.n >= 8u && pitch_ok && distinct >= 3u);
    bad += x_check("T03 the step blocks are exactly those of SOURCE OFF (rate, gate, phase unchanged)", same_blk);
    {   /* ARP CHORD: the pool together, at most three, same step blocks */
        static rec_t eo, ex;
        uint32_t k, ok = 1, steps = 0;
        arp_scene(0, AM_CHORD, 0); collect(&eo, 1, 0);
        arp_scene(1, AM_CHORD, 0); collect(&ex, 1, 0);
        for (i = 0; i < ex.n;) {
            uint32_t j = i, pcs = 0;
            while (j < ex.n && ex.blk[j] == ex.blk[i]) { pcs |= 1u << (ex.note[j] % 12u); j++; }
            if (j - i != 3u || pcs != (1u << 11 | 1u << 2 | 1u << 9)) ok = 0;
            steps++;
            i = j;
        }
        for (k = 0; k < eo.n && ok; k++) ;
        {   /* off: one note per step, the same step blocks */
            uint32_t m = 0, same = 1;
            for (i = 0; i < ex.n; i += 3u, m++) same &= m < eo.n && ex.blk[i] == eo.blk[m];
            ok &= same && steps >= 4u;
        }
        bad += x_check("T03 ARP CHORD over EXT 7+9+13: the three pool tones together every step, the step blocks of SOURCE OFF", ok);
    }
    return bad;
}

/* ------------------------------------------------------------------ SEQ --- */
static void seq_scene(int ext, int level, uint32_t until_ms)
{
    setup();
    harm_const(0);
    if (ext) { ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, level); }
    transport_req = 1;
    run_to(until_ms);
}
static void stp(track_t *t, uint32_t i, uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t flags, uint32_t vel)
{
    t->step[i] = (step_t){{(uint8_t)a, (uint8_t)b, (uint8_t)c}, (uint8_t)n, ST_NOTE, (uint8_t)flags, (uint8_t)vel};
}
static uint32_t step_hash(const track_t *t)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof t->step; i++) h = (h ^ ((const uint8_t *)t->step)[i]) * 16777619u;
    return h;
}
static const uint8_t SEQ_NOTES[16] = {60, 61, 64, 66, 70, 72, 75, 48, 55, 58, 62, 67, 71, 74, 80, 69};
static void seq_setup_mono(void)
{
    setup();
    harm_const(0);
    for (uint32_t i = 0; i < 16u; i++) stp(&trk[1], i, 1, SEQ_NOTES[i], 0, 0, 0, 100);
}
/* the first n note-ons of T2 */
static int t04(void)
{
    int bad = 0, ok = 1;
    static rec_t r;
    uint8_t pcs[3] = {11, 2, 9};     /* C: B D A (7+9+13) */
    uint32_t i;
    seq_setup_mono();
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    run_to(2200);
    collect(&r, 1, 0);
    for (i = 0; i < 32u; i++) {
        int w = or_one(SEQ_NOTES[i % 16u], pcs, 3, 0);
        if (i >= r.n || r.note[i] != (uint8_t)w) {
            ok = 0;
            printf("    note-on %u: got %d, want %d (stored %u)\n", i, i < r.n ? r.note[i] : -1, w, SEQ_NOTES[i % 16u]);
            break;
        }
    }
    bad += x_check("T04 SEQ mono notes: every stored note plays as the nearest pool tone, a tie the lower (70 -> A 69), 32 steps", ok);
    {   /* one note-on per step: never two in a block, 2.2 s / 62.5 ms = 35.2 steps */
        uint32_t dup = 0, k;
        for (k = 1; k < r.n; k++) dup += r.blk[k] == r.blk[k - 1u];
        bad += x_check("T04 one stored note = one note-on (no multiplication): one per step (36 ons in 2.2 s, never two in a block)", r.n >= 35u && r.n <= 37u && !dup);
    }
    {   /* poly steps: 3 notes -> <= 3 outputs, two notes on one pool tone play once */
        uint32_t n0, n1, n2;
        static rec_t q;
        setup();
        harm_const(0);
        stp(&trk[1], 0, 3, 60, 64, 67, 0, 100);       /* 59 62 69 */
        stp(&trk[1], 1, 2, 59, 60, 0, 0, 100);        /* both on B 59: once */
        stp(&trk[1], 2, 1, 70, 0, 0, 0, 100);
        trk[1].p[P_SLEN] = 3;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        transport_req = 1;
        run_to(500);
        collect(&q, 1, 0);
        /* first three events at one block, then one, then one */
        n0 = n1 = n2 = 0;
        for (i = 0; i < q.n && q.blk[i] == q.blk[0]; i++) n0++;
        for (uint32_t j = i; j < q.n && q.blk[j] == q.blk[i]; j++) n1++;
        ok = n0 == 3u && q.note[0] == 59 && q.note[1] == 62 && q.note[2] == 69;
        ok &= n1 == 1u && q.note[3] == 59;
        n2 = q.n > 4u ? q.note[4] : 0;
        ok &= n2 == 69u;
        if (!ok) { printf("    poly: n0 %u n1 %u first %u %u %u %u %u\n", n0, n1, q.note[0], q.note[1], q.note[2], q.note[3], q.note[4]); }
        bad += x_check("T04 SEQ poly: 60 64 67 -> 59 62 69 once each; 59 + 60 both on B play once; 70 -> 69", ok);
    }
    return bad;
}
static int t05(void)
{
    int bad = 0, ok;
    static rec_t r;
    uint32_t h0, h1;
    seq_setup_mono();
    trk[1].step[3].flags = SF_SLIDE; trk[1].step[5].time = ST_REST; trk[1].step[9].time = ST_TIE; trk[1].step[7].flags = SF_ACCENT;
    h0 = step_hash(&trk[1]);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    run_to(2200);
    h1 = step_hash(&trk[1]);
    collect(&r, 1, 0);
    ok = h0 == h1 && r.n >= 8u && r.note[0] != SEQ_NOTES[0] && trk[1].step[0].note[0] == SEQ_NOTES[0] && trk[1].step[5].time == ST_REST;
    bad += x_check("T05 SEQ is not rewritten: the steps' hash is the same after 2 loops, yet what plays is the pool (59, not the stored 60)", ok);
    ok = trk[1].step[3].flags == SF_SLIDE && trk[1].step[7].flags == SF_ACCENT && trk[1].step[9].time == ST_TIE;
    bad += x_check("T05 rest / tie / slide / accent flags unchanged", ok);
    return bad;
}
static int t06(void)
{
    int bad = 0, ok = 1;
    static rec_t r;
    uint32_t i, strong_only = 0;
    seq_setup_mono();
    for (i = 0; i < 16u; i++) trk[1].step[i].note[0] = (uint8_t)(60 + (i * 7u) % 12u + 12u * (i % 3u));   /* mixed, mostly not B D A */
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    run_to(2200);
    collect(&r, 1, 0);
    for (i = 0; i < r.n; i++) {
        uint32_t pc = r.note[i] % 12u;
        if (!(pc == 11 || pc == 2 || pc == 9)) { ok = 0; strong_only++; }
    }
    bad += x_check("T06 every one of 16 steps x 2 loops plays a pool tone (not only steps 1 5 9 13, no triad snapping later)", ok && r.n >= 30u);
    return bad;
}

/* ------------------------------------------------------------------ T07 --- */
/* the pool pcs of the chord of bar b for the progression C Am F G (rows 0 1 2 3 of ORC order: C Am F G) */
static const uint8_t PROG[4] = {0, 9, 5, 7};
static int row_of_root(uint8_t root) { for (int r = 0; r < NORC; r++) if (ORC[r].root == root) return r; return -1; }
static int in_pool(uint32_t note, uint32_t bar, int level)
{
    int r = row_of_root(PROG[bar & 3u]);
    for (uint32_t k = 0; k < ORC[r].n[level]; k++) if (ORC[r].pc[level][k] == note % 12u) return 1;
    return 0;
}
static int t07(void)
{
    int bad = 0, ok;
    static rec_t r, off;
    uint32_t i, seen_mask = 0;
    orc_fix();
    /* (a) SEQ: each note-on takes the pool of the bar it plays in; the pool changes at the bar line */
    setup();
    harm_prog(0, 9, 5, 7);
    for (i = 0; i < 16u; i++) stp(&trk[1], i, 1, 60, 0, 0, 0, 100);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    run_to(4300);
    for (ok = 1, i = 0; i < nnev; i++)
        if (nev[i].on && nev[i].trk == 1) {
            if (!in_pool(nev[i].note, nev[i].bar, 2)) ok = 0;
            seen_mask |= 1u << (nev[i].bar & 3u);
        }
    bad += x_check("T07 SEQ over C Am F G: every note-on is a tone of the pool of the bar it plays in (4 bars, the pool follows)", ok && seen_mask == 15u);
    /* (b) a held key keeps its pitches over bar lines, no re-attack, its release ends exactly them */
    setup();
    harm_prog(0, 9, 5, 7);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    u_key(100, K_C4, 1);
    run_to(300);
    {
        uint8_t first[8], now[8];
        uint32_t n0 = gnotes(1, first), n1, ons = 0, bar0 = harmony_bar;
        run_to(3600);
        n1 = gnotes(1, now);
        for (i = 0; i < nnev; i++) ons += nev[i].on && nev[i].trk == 1;
        ok = n0 == 3u && n1 == n0 && !memcmp(first, now, n0) && ons == n0 && harmony_bar != bar0;
        bad += x_check("T07 a held key over 3 bar lines: the same three pitches, no new note-on (kept until its release)", ok);
        u_key(3700, K_C4, 0);
        run_to(3800);
        bad += x_check("T07 .. its release ends exactly them (nothing left sounding)", ngated(1) == 0);
    }
    /* (c) a held MIDI note (channel 2 = T2) */
    setup();
    harm_prog(0, 9, 5, 7);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    transport_req = 1;
    u_usb(100, 0x91, 60, 100);
    run_to(300);
    {
        uint8_t first[8], now[8];
        uint32_t n0 = gnotes(1, first), n1, ons = 0;
        run_to(3600);
        n1 = gnotes(1, now);
        for (i = 0; i < nnev; i++) ons += nev[i].on && nev[i].trk == 1;
        ok = n0 == 3u && n1 == n0 && !memcmp(first, now, n0) && ons == n0;
        u_usb(3700, 0x81, 60, 0);
        run_to(3800);
        bad += x_check("T07 a held MIDI note over bar lines keeps its pitches, no re-attack; the note-off ends exactly them", ok && ngated(1) == 0);
    }
    /* (d) LATCH NEW: pressed and let go, it sounds on over the bar line with its pitches */
    setup();
    harm_prog(0, 9, 5, 7);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    trk[1].p[P_LATCH] = LT_NEW;
    transport_req = 1;
    u_key(100, K_C4, 1); u_key(200, K_C4, 0);
    run_to(400);
    {
        uint8_t first[8], now[8];
        uint32_t n0 = gnotes(1, first), n1, ons = 0;
        run_to(3600);
        n1 = gnotes(1, now);
        for (i = 0; i < nnev; i++) ons += nev[i].on && nev[i].trk == 1;
        ok = n0 == 3u && n1 == n0 && !memcmp(first, now, n0) && ons == n0;
        trk[1].p[P_LATCH] = LT_OFF;
        run_to(3800);
        bad += x_check("T07 LATCH NEW over bar lines: the latched pitches stay, no re-attack; LATCH off ends exactly them", ok && ngated(1) == 0);
    }
    /* (e) ARP with HOLD: it follows the new pool at the next step, the step blocks are those of SOURCE OFF (no phase reset) */
    for (int e = 0; e < 2; e++) {
        setup();
        harm_prog(0, 9, 5, 7);
        trk[1].p[P_ARATE] = 2; trk[1].p[P_AMODE] = AM_UP; trk[1].p[P_AOCT] = 1; trk[1].p[P_AHOLD] = 1;
        if (e) { ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); }
        transport_req = 1;
        u_key(100, K_C4, 1); u_key(200, K_C4, 0);
        run_to(4300);
        collect(e ? &r : &off, 1, 0);
    }
    for (ok = r.n >= 40u && r.n == off.n, i = 0; ok && i < r.n; i++) {
        uint32_t bar = 0;
        for (uint32_t j = 0; j < nnev; j++) if (nev[j].on && nev[j].trk == 1 && nev[j].blk == r.blk[i]) { bar = nev[j].bar; break; }
        ok &= r.blk[i] == off.blk[i] && in_pool(r.note[i], bar, 2);
    }
    bad += x_check("T07 ARP HOLD over C Am F G: each step a tone of its bar's pool, step blocks identical to SOURCE OFF (no phase reset)", ok);
    return bad;
}

/* ------------------------------------------------------------------ T08 --- */
static int t08(void)
{
    int bad = 0, ok_all = 1, v;
    static const char *NAME[6] = {"SOURCE off", "EXT 7", "OCT +2", "engine switch", "HARMONY off", "all at once"};
    for (v = 0; v < 6; v++) {
        uint8_t a[8], b[8], expect[8];
        uint32_t na, nab, nb, i, m = 0, ok = 1;
        setup();
        harm_const(0);
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        u_key(10, K_C4, 1); run_to(30);
        na = gnotes(1, a);
        u_key(40, K_G4, 1); run_to(60);
        nab = gnotes(1, b);
        for (i = 0; i < nab; i++) { uint32_t j; for (j = 0; j < na && a[j] != b[i]; j++) ; if (j == na) expect[m++] = b[i]; }
        nb = m;
        /* the keys' own lists: key G4's pool tones (62 69 71) and C4's (57 59 62) overlap on 62 */
        switch (v) {
        case 0: ce_set(1, CE_SRC, 0); break;
        case 1: ce_set(1, CE_EXT, 0); break;
        case 2: ce_set(1, CE_OCT, 2); break;
        case 3: set_engine_of(&trk[1], trk[1].eng_req == ENGI_FM6 ? ENGI_FM6 + 0 : trk[1].eng_req); trk[1].p[P_VOICE] = V_MONO; break;
        case 4: harmony_on = 0; break;
        default: ce_set(1, CE_SRC, 0); ce_set(1, CE_EXT, 1); ce_set(1, CE_OCT, -2); harmony_on = 0; break;
        }
        run_to(80);
        u_key(90, K_C4, 0);
        run_to(110);
        {   /* C4's notes end, but 62 (shared with G4) and G4's own stay: exactly G4's list */
            uint8_t want[8], g[8];
            uint32_t n, w = 0, j;
            for (i = 0; i < nab; i++) {
                int inG = 0;
                static const uint8_t G4[3] = {62, 69, 71};
                for (j = 0; j < 3u; j++) inG |= b[i] == G4[j];
                if (inG) want[w++] = b[i];
            }
            n = gnotes(1, g);
            if (v == 3 || v == 5) ok = 1;   /* a voice-mode switch can panic: nothing may be left hanging below */
            else ok = n == w && !memcmp(g, want, w);
            if (!ok) { printf("    %s: after C4 up", NAME[v]); show("", 1); }
        }
        u_key(120, K_G4, 0);
        run_to(160);
        ok &= ngated(1) == 0;
        if (!ok) ok_all = 0;
        (void)nb; (void)expect;
    }
    bad += x_check("T08 two keys sound; SOURCE / EXT / OCT / voice / HARMONY changed before the releases: key 1 ends exactly its own notes, then all", ok_all);
    {   /* MIDI: note-on, settings changed, note-off ends exactly the started notes */
        int ok = 1;
        for (v = 0; v < 4; v++) {
            setup();
            harm_const(0);
            ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
            u_usb(10, 0x91, 60, 100); run_to(30);
            if (ngated(1) != 3u) ok = 0;
            if (v == 0) ce_set(1, CE_SRC, 0); else if (v == 1) ce_set(1, CE_OCT, -1); else if (v == 2) ce_set(1, CE_EXT, 0); else harmony_on = 0;
            u_usb(50, 0x81, 60, 0); run_to(80);
            ok &= ngated(1) == 0;
        }
        bad += x_check("T08 MIDI: note-on, SOURCE / OCT / EXT / HARMONY changed, note-off: exactly the started notes end (nothing left)", ok);
    }
    {   /* the raw key must not be released: a same-pitch voice another source holds (a latched note) survives an EXT NoteOff with no pool */
        setup();
        harm_const(0);
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        trk[1].p[P_LATCH] = LT_STACK;
        u_usb(10, 0x91, 60, 100); u_usb(20, 0x81, 60, 0);          /* latched: 57 59 62 */
        run_to(40);
        {
            uint8_t before[8], after[8];
            uint32_t nb = gnotes(1, before), na;
            harmony_on = 0;                                           /* WAIT BUS: a new note makes no notes (mapped, n = 0) */
            u_usb(50, 0x91, 57, 100); u_usb(70, 0x81, 57, 0);        /* raw key 57 = a latched pool tone */
            run_to(100);
            na = gnotes(1, after);
            bad += x_check("T08 WAIT note-on/off (no notes) leaves the same-pitch latched voice alone (the release does not fall back to the raw key)",
                           nb == 3u && na == nb && !memcmp(before, after, nb));
        }
    }
    return bad;
}

/* ------------------------------------------------------------------ T09 --- */
static int t09(void)
{
    int bad = 0, ok = 1, oct;
    static const uint8_t XS[7] = {0, 3, 60, 100, 124, 127, 12};
    uint8_t pcs[3] = {11, 2, 9};
    for (uint32_t xi = 0; xi < 7u; xi++)
        for (oct = -2; oct <= 2; oct++) {
            uint8_t want[3], g[8];
            uint32_t n, got;
            setup();
            harm_const(0);
            ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); ce_set(1, CE_OCT, oct);
            u_usb(10, 0x91, XS[xi], 100); run_to(30);
            n = or_live(XS[xi], pcs, 3, oct, want);
            got = gnotes(1, g);
            if (got != n || memcmp(g, want, n) || n != 3u) {
                ok = 0;
                printf("    key %u OCT %d: got %u notes, want %u", XS[xi], oct, got, n);
                show("", 1);
            }
            u_usb(40, 0x81, XS[xi], 0); run_to(60);
            if (ngated(1)) ok = 0;
        }
    bad += x_check("T09 OCT -2..+2 x keys 0 3 12 60 100 124 127: nearest tone then +-12*OCT, folded into 0..127, always 3 distinct tones", ok);
    {   /* a stored OCT outside -2..2 (junk) is clamped */
        uint8_t want[3];
        uint32_t n;
        setup(); harm_const(0);
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        trk[1].p[79] = 9;
        u_usb(10, 0x91, 60, 100); run_to(30);
        n = or_live(60, pcs, 3, 2, want);
        bad += x_check("T09 a stored OCT outside -2..2 (junk 9) is clamped: it acts as +2", notes_are(1, want, n));
    }
    return bad;
}

/* ------------------------------------------------------------------ T10 --- */
static int t10(void)
{
    int bad = 0;
    ext_info_t o;
    uint8_t want[3];
    uint8_t pcs[3] = {11, 2, 9};
    uint32_t n;
    setup();
    harmony_on = 0;
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    ce_info(1, &o);
    bad += x_check("T10 SOURCE HARM with HARMONY off: ext_info WAIT BUS, why BUS OFF, no chord", o.state == EXT_ST_WAITBUS && o.why == EXT_WHY_BUSOFF && !o.have_chord && o.src == 1);
    u_key(10, K_C4, 1); run_to(30);
    bad += x_check("T10 a key plays nothing (no DIA3 fallback, no raw note)", ngated(1) == 0);
    u_key(40, K_C4, 0); run_to(60);
    trk[1].p[P_CHRD] = CH_BUS;
    u_key(70, K_C4, 1); run_to(90);
    bad += x_check("T10 CHRD BUS on the EXT track: still nothing (the bus is off, no DIA3 triad)", ngated(1) == 0);
    u_key(100, K_C4, 0); run_to(120);
    harm_const(0);
    u_key(130, K_C4, 1); run_to(150);
    n = or_live(60, pcs, 3, 0, want);
    ce_info(1, &o);
    bad += x_check("T10 HARMONY on: the same setting plays the pool (OK)", notes_are(1, want, n) && o.state == EXT_ST_OK);
    u_key(160, K_C4, 0); run_to(180);
    ce_set(1, CE_SRC, 0); harmony_on = 0;
    u_key(190, K_C4, 1); run_to(210);
    {   /* (CHRD is still BUS from above: with SOURCE OFF and the bus off it is the DIA3 triad, the fallback EXT must not take) */
        const uint8_t tri[3] = {60, 64, 67};
        bad += x_check("T10 (invariant) SOURCE OFF with HARMONY off and CHRD BUS: the DIA3 triad as ever (the fallback EXT does not take)", notes_are(1, tri, 3));
    }
    return bad;
}

/* ------------------------------------------------------------------ T11 --- */
static int t11(void)
{
    int bad = 0;
    for (uint32_t e = 0; e < 6u; e++) {
        setup(); harm_const(0);
        set_engine_of(&trk[1], eng_unstore(e)); engine_block(&trk[1]); trk[1].p[P_VOICE] = V_POLY;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        ext_info_t o; ce_info(1, &o);
        uint8_t out[CHORD_MAX];
        bad += x_check("T11 FORMAT 2: every retained engine is pitched and resolves HARM (retired DRUM case replaced)",
                       !chord_kit(&trk[1]) && o.state == EXT_ST_OK && o.npc == 3u &&
                       chord_build(&trk[1], 60u, out) == 3u);
    }
    return bad;
}

/* ------------------------------------------------------------------ T12 --- */
static int t12(void)
{
    int bad = 0, s, ok = 1;
    ext_info_t o;
    /* Values beyond the implemented OFF/HARM/T1..T4 range remain OFF. */
    for (s = 6; s <= 9; s++) {
        setup();
        harm_const(0);
        trk[1].p[76] = (int16_t)s; ce_set(1, CE_EXT, 2);
        ce_info(1, &o);
        u_key(10, K_C4, 1); run_to(30);
        ok &= o.state == EXT_ST_OFF && o.src == (uint8_t)s && ngated(1) == 1 && trk[1].v[0].note == 60;
    }
    bad += x_check("T12 (invariant) SOURCE 6..9 are outside the implemented range and behave as OFF", ok);
    return bad;
}

/* ------------------------------------------------------------------ T13 --- */
static int t13(void)
{
    int bad = 0, ok = 1, vm;
    static const char *N[4] = {"POLY", "MONO", "LEGATO", "UNISON"};
    (void)N;
    for (vm = 0; vm < 4; vm++) {
        uint8_t g[128];
        ext_info_t o;
        uint32_t n;
        setup();
        harm_const(0);
        trk[1].p[P_VOICE] = (int16_t)vm;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        ce_info(1, &o);
        u_key(10, K_C4, 1); run_to(30);
        n = gnotes(1, g);
        if (vm == V_POLY) ok &= n == 3u && !o.mono;
        else ok &= n == 1u && g[0] == 59u && o.mono && o.npc == 3u;       /* the pool tone nearest C4: B3 */
        u_key(40, K_C4, 0); run_to(60);
        ok &= ngated(1) == 0;
    }
    bad += x_check("T13 POLY plays the 3 pool tones; MONO / LEGATO / UNISON one tone (the nearest, B3), ext_info mono (1 of 3); release ends it", ok);
    return bad;
}

/* ------------------------------------------------------------------ T14 --- */
static int t14(void)
{
    int bad = 0, ok = 1, chrd;
    uint8_t pcs[3] = {11, 2, 9}, want[3];
    uint32_t n = or_live(60, pcs, 3, 0, want);
    for (chrd = 0; chrd < 4; chrd++) {
        static const uint8_t C[4] = {CH_DIA3, CH_DIA7, CH_BUS, CH_MAJ7};
        setup();
        harm_const(0);
        trk[1].p[P_CHRD] = C[chrd]; trk[1].p[P_VOIC] = VC_OPEN; harmony_follow[1] = 1;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        u_key(10, K_C4, 1); run_to(30);
        ok &= notes_are(1, want, n);
        ok &= trk[1].p[P_CHRD] == C[chrd] && trk[1].p[P_VOIC] == VC_OPEN && harmony_follow[1] == 1;
        u_key(40, K_C4, 0); run_to(60);
        ok &= ngated(1) == 0;
    }
    bad += x_check("T14 CHRD DIA3 / DIA7 / BUS / MAJ7 + VOIC OPEN + FOLLOW with EXT: only the pool's 3 tones, no expansion; the values stay stored", ok);
    {   /* SOURCE off again: the stored CHRD DIA3 plays as it did (C E G) */
        uint8_t tri[3] = {60, 64, 67};
        setup();
        harm_const(0);
        trk[1].p[P_CHRD] = CH_DIA3;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        u_key(10, K_C4, 1); run_to(30);
        u_key(40, K_C4, 0); run_to(60);
        ce_set(1, CE_SRC, 0);
        u_key(70, K_C4, 1); run_to(90);
        bad += x_check("T14 after SOURCE OFF the stored CHRD DIA3 plays C E G again", notes_are(1, tri, 3));
    }
    return bad;
}

/* ------------------------------------------------------------------ T15 --- */
static int t15(void)
{
    int bad = 0, ok = 1;
    uint32_t seed = 12345u, ms, ext_notes = 0, i, held = 0;
    setup();
    harm_prog(0, 9, 5, 7);
    trk[1].p[P_AMODE] = AM_UP; trk[1].p[P_ARATE] = 2; trk[1].p[P_AOCT] = 2;
    for (i = 0; i < 16u; i++) stp(&trk[2], i, 1, (uint32_t)(60 + (int)i), 0, 0, 0, 100);
    ce_set(1, CE_SRC, 1); ce_set(2, CE_SRC, 1);
    transport_req = 1;
    for (ms = 21; ms < 3021; ms += 3u) {                 /* 1000 knob changes over 3 s */
        seed = seed * 1664525u + 1013904223u;
        switch ((seed >> 20) % 3u) {
        case 0: ce_set(1 + (seed >> 12) % 2u, CE_SRC, (int)((seed >> 16) & 1u)); break;
        case 1: ce_set(1 + (seed >> 12) % 2u, CE_EXT, (int)((seed >> 16) % 3u)); break;
        default: ce_set(1 + (seed >> 12) % 2u, CE_OCT, (int)((seed >> 16) % 5u) - 2); break;
        }
        if (!(ms % 90u)) { held ^= 1u; u_key(ms, K_C4, held); u_usb(ms, held ? 0x91 : 0x81, 64, 100); }
        run_to(ms);
    }
    u_key(3030, K_C4, 0); u_usb(3030, 0x81, 64, 0);
    run_to(3050);
    trk[1].p[P_AMODE] = 0; transport_req = 2;
    run_to(3800);
    for (i = 0; i < nnev; i++)
        if (nev[i].on && (nev[i].trk == 1 || nev[i].trk == 2)) {
            uint32_t pc = nev[i].note % 12u;
            (void)pc;
            ext_notes++;
        }
    ok = ngated(1) == 0 && ngated(2) == 0 && ext_notes > 30u;
    {   /* some of those notes must be EXT pool tones (a base run has the raw ARP notes 60 64 etc.) */
        uint32_t inpool = 0;
        for (i = 0; i < nnev; i++)
            if (nev[i].on && nev[i].trk == 1 && in_pool(nev[i].note, nev[i].bar, 2)) inpool++;
        ok &= inpool > 10u;
        if (!ok) printf("    gated T2 %u T3 %u, %u note-ons, %u in the pool\n", ngated(1), ngated(2), ext_notes, inpool);
    }
    bad += x_check("T15 1000 knob changes (SOURCE / EXT / OCT of T2, T3) over ARP + SEQ + keys + MIDI: nothing left sounding, EXT notes played", ok);
    return bad;
}

/* ------------------------------------------------------------------ T16 --- */
static int t16(void)
{
    int bad = 0, ok;
    static project_t p, q;
    static project_store_t st;
    uint32_t k;
    uint8_t pcs[3] = {11, 2, 9}, want[3];
    uint32_t n = or_live(60, pcs, 3, 0, want);
    setup();
    harm_const(0);
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); ce_set(1, CE_OCT, 0);
    ce_set(0, CE_SRC, 0); ce_set(0, CE_EXT, 0); ce_set(0, CE_OCT, -2);
    ce_set(2, CE_SRC, 1); ce_set(2, CE_EXT, 1); ce_set(2, CE_OCT, 2);
    project_capture(&p);
    ok = proj_pack(&st, &p) && proj_import(&q, &st, PROJ_STORE_SIZE);
    for (k = 0; k < NTRK; k++) { ce_set(k, CE_SRC, 0); ce_set(k, CE_EXT, 1); ce_set(k, CE_OCT, 0); }
    ok = ok && !project_restore_runtime(&q);
    ok = ok && ce_get(1, CE_SRC) == 1 && ce_get(1, CE_EXT) == 2 && ce_get(1, CE_OCT) == 0 && ce_get(0, CE_EXT) == 0 && ce_get(0, CE_OCT) == -2 &&
         ce_get(2, CE_SRC) == 1 && ce_get(2, CE_EXT) == 1 && ce_get(2, CE_OCT) == 2;
    bad += x_check("T16 project capture -> pack -> import -> restore: SOURCE / EXT / OCT of every track come back", ok);
    harm_const(0);
    u_key(10, K_C4, 1); run_to(30);
    bad += x_check("T16 .. and after the load the follower plays the pool (a loaded project works, not just stores)", notes_are(1, want, n));
    u_key(40, K_C4, 0); run_to(60);
    {   /* an old project (SOURCE 0): no EXT behaviour, the stored values only */
        ce_set(1, CE_SRC, 0); ce_set(1, CE_EXT, 2);
        project_capture(&p); proj_pack(&st, &p); proj_import(&q, &st, PROJ_STORE_SIZE);
        ce_set(1, CE_SRC, 1);
        project_restore_runtime(&q);
        u_key(70, K_C4, 1); run_to(90);
        bad += x_check("T16 SOURCE OFF in the project loads OFF over a live HARM: the key plays one note", ce_get(1, CE_SRC) == 0 && ngated(1) == 1);
    }
    return bad;
}


/* ------------------------------------------------------------------ T18 --- */
/* fuzz seed 109: a SEQ note held over by SLIDE into a step whose stored note equals a live-recorded note (rskip: "this note already
 * sounds live, do not play it again") lost its owner, because the comparison used the stored pitch while EXT plays the pool tone:
 * the held-over tone stayed in the voice (and in a UNISON / MONO stack) for good. The skip must compare the pitch that would sound */
static int t18(void)
{
    int bad = 0, vm;
    for (vm = 0; vm < 2; vm++) {
        setup();
        harm_const(0);
        trk[1].p[P_VOICE] = vm ? V_UNISON : V_POLY;
        stp(&trk[1], 0, 1, 60, 0, 0, SF_SLIDE, 100);      /* 60 -> B3 59, held over by the slide */
        stp(&trk[1], 1, 1, 60, 0, 0, 0, 100);             /* the same stored note again */
        trk[1].p[P_SLEN] = 2;
        ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        transport_req = 1;
        run_to(10);                                        /* (a transport start clears rskip) */
        trk[1].rskip_idx = 1; trk[1].rskip[0] = 60; trk[1].rskip_n = 1;   /* a live note 60 is recorded into step 1 and sounds */
        run_to(100);                                       /* step 0 (62.5 ms) and step 1 have started */
        transport_req = 2;
        run_to(900);
        u_usb(910, 0x91, 72, 100); u_usb(950, 0x81, 72, 0);       /* a later note: the stale tone must not come back when it ends */
        run_to(1100);
        bad += x_check(vm ? "T18 SEQ SLIDE into a step with a recorded live note (rskip), UNISON: nothing left sounding after STOP and a later note" :
                            "T18 SEQ SLIDE into a step with a recorded live note (rskip), POLY: nothing left sounding after STOP and a later note", ngated(1) == 0);
    }
    return bad;
}

/* ------------------------------------------------------------------ T19 - T25: RETRIG = HARM (slice 1c) --- */
/* an EXT voicing / a held single note / a latched note follows the bar's chord when the track's RETRIG is HARM (stored 1) or BOTH (3) and
 * is sounded again; the owner lists stay the release identity. Progression C Am F G, a bar each (1 s at 240 BPM). */
static uint32_t ons_since_bar(uint32_t t, uint32_t bar)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < nnev; i++) n += nev[i].on && nev[i].trk == t && nev[i].bar >= bar && nev[i].blk > 0;
    return n;
}
static void ret_setup(int vm, int lv, int rt)
{
    setup();
    harm_prog(0, 9, 5, 7);
    trk[1].p[P_VOICE] = (int16_t)vm;
    ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, lv);
    link_retrig[1] = (uint8_t)rt;
    transport_req = 1;
}
static int t19(void)
{
    int bad = 0, owner, vm, lv;
    static const char *ON[4] = {"key", "MIDI", "LATCH NEW", "LATCH STACK"};
    for (owner = 0; owner < 4; owner++) {
        int ok = 1;
        for (vm = 0; vm < 4; vm++)
            for (lv = 0; lv < 3; lv++) {
                uint8_t w0[4], w1[4], g[8];
                uint32_t n0, n1, ng, i;
                ret_setup(vm, lv, owner & 1 ? 3 : 1);               /* HARM, and BOTH for the MIDI case */
                if (owner >= 2) trk[1].p[P_LATCH] = owner == 2 ? LT_NEW : LT_STACK;
                if (owner == 1) u_usb(100, 0x91, 60, 100); else u_key(100, K_C4, 1);
                if (owner >= 2) u_key(200, K_C4, 0);
                run_to(600);
                if (vm == V_POLY) n0 = or_live(60, orc_pcs(0, lv), ORC[0].n[lv], 0, w0);
                else { int y = or_one(60, orc_pcs(0, lv), ORC[0].n[lv], 0); n0 = 1; w0[0] = (uint8_t)y; }
                if (!notes_are(1, w0, n0)) { ok = 0; printf("    %s vm %d lv %d: before the bar line\n", ON[owner], vm, lv); show("", 1); }
                run_to(1500);
                if (harmony_bar != 1u) { ok = 0; printf("    bar %u\n", harmony_bar); }
                if (owner >= 2) {   /* a latched pitch moves on its own: each to the nearest tone of the new pool */
                    uint32_t j, m = 0, q;
                    for (j = 0; j < n0; j++) {
                        uint8_t y = (uint8_t)or_one(w0[j], orc_pcs(1, lv), ORC[1].n[lv], 0);
                        for (q = 0; q < m && w1[q] != y; q++) ;
                        if (q == m) w1[m++] = y;
                    }
                    or_sort(w1, m); n1 = m;
                } else if (vm == V_POLY) n1 = or_live(60, orc_pcs(1, lv), ORC[1].n[lv], 0, w1);
                else { int y = or_one(60, orc_pcs(1, lv), ORC[1].n[lv], 0); n1 = 1; w1[0] = (uint8_t)y; }
                if (!notes_are(1, w1, n1)) { ok = 0; printf("    %s vm %d lv %d: after the bar line, want %u notes (first %u)", ON[owner], vm, lv, n1, w1[0]); show("", 1); }
                if ((n0 != n1 || memcmp(w0, w1, n0)) && ons_since_bar(1, 1) < n1) { ok = 0; printf("    %s vm %d lv %d: not sounded again\n", ON[owner], vm, lv); }
                /* the release ends exactly what sounds */
                if (owner == 1) u_usb(1600, 0x81, 60, 0); else if (owner == 0) u_key(1600, K_C4, 0); else trk[1].p[P_LATCH] = LT_OFF;
                run_to(1900);
                ng = gnotes(1, g);
                if (ng) { ok = 0; printf("    %s vm %d lv %d: %u notes left after the release\n", ON[owner], vm, lv, ng); }
                (void)i;
            }
        bad += x_check(owner == 0 ? "T19 RETRIG HARM, EXT voicing held by a KEY over a bar line: rebuilt from the new pool (POLY / MONO / LEGATO / UNISON x pool 1..3), sounded again, key-up frees it" :
                       owner == 1 ? "T19 .. held by a MIDI note (BOTH): the same" :
                       owner == 2 ? "T19 .. LATCH NEW: each latched pitch moves to the nearest new tone, sounded again, LATCH off frees it" :
                                    "T19 .. LATCH STACK: the same", ok);
    }
    return bad;
}
static int t20(void)
{
    int bad = 0, ok = 1;
    /* a single held note: CHRD OFF, FOLLOW on, not EXT. C chord: F#4 (66) snaps to G (67); Am: to E (64) */
    {
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; harmony_follow[1] = 1; transport_req = 1;
        u_key(100, 13, 1);                         /* key 13 = F#4 (66) */
        run_to(600);
        { uint8_t w[1] = {67}; ok &= notes_are(1, w, 1); }
        run_to(1500);
        { uint8_t w[1] = {64}; ok &= notes_are(1, w, 1) && harmony_bar == 1u && ons_since_bar(1, 1) >= 1u; if (!ok) show("single note after the bar", 1); }
        u_key(1600, 13, 0); run_to(1800);
        ok &= ngated(1) == 0;
        bad += x_check("T20 RETRIG HARM, CHRD off, FOLLOW on: a held single note is snapped again at the bar line (G -> E), sounded again, key-up frees it", ok);
    }
    {   /* RETRIG OFF: nothing changes (the note stays G); FOLLOW off: unaffected */
        for (int c = 0; c < 2; c++) {
            setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = c ? 1 : 0; harmony_follow[1] = c ? 0 : 1; transport_req = 1;
            u_key(100, 13, 1); run_to(1500);
            { uint8_t w[1] = {c ? 66 : 67}; ok = notes_are(1, w, 1); }
            u_key(1600, 13, 0); run_to(1800);
            bad += x_check(c ? "T20 (invariant) FOLLOW off: the held note is not snapped, not retriggered" : "T20 (invariant) RETRIG OFF: the held note stays", ok && ngated(1) == 0);
        }
    }
    {   /* a MIDI single note (note 66 is snapped to 67 first: a mapped entry), and one that is a chord tone (60, no entry) */
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; harmony_follow[1] = 1; transport_req = 1;
        u_usb(100, 0x91, 66, 100); u_usb(110, 0x91, 60, 100);
        run_to(600);
        { uint8_t w[2] = {60, 67}; ok = notes_are(1, w, 2); }
        run_to(1500);
        { uint8_t w[2] = {60, 64}; ok = notes_are(1, w, 2); }     /* C stays a chord tone of Am (A C E), G goes to E */
        u_usb(1600, 0x81, 66, 0); u_usb(1610, 0x81, 60, 0); run_to(1800);
        bad += x_check("T20 MIDI single notes (one mapped, one played as it came) follow too; the note-offs free exactly them", ok && ngated(1) == 0);
    }
    return bad;
}
static int t21(void)
{
    int bad = 0, ok = 1;
    {   /* CHRD BUS: the existing retrigger, unchanged (the Am triad at the bar line) */
        uint8_t w[3] = {57, 60, 64};
        setup(); harm_prog(0, 9, 5, 7); trk[1].p[P_CHRD] = CH_BUS; link_retrig[1] = 1; transport_req = 1;
        u_key(100, K_C4, 1); run_to(1500);
        ok = notes_are(1, w, 3);
        u_key(1600, K_C4, 0); run_to(1800);
        bad += x_check("T21 (invariant) CHRD BUS: the held chord follows the bus and is sounded again as before (Am), key-up frees it", ok && ngated(1) == 0);
    }
    {   /* an unchanged pool is no retrigger: C -> C bars */
        uint32_t before, after;
        setup(); harm_prog(0, 0, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); transport_req = 1;
        u_key(100, K_C4, 1); run_to(600);
        before = nnev; run_to(1500);
        after = nnev;
        ok = harmony_bar == 1u && before == after;
        u_key(1600, K_C4, 0); run_to(1800);
        bad += x_check("T21 an unchanged pool (C -> C): no note-off, no note-on at the bar line", ok && ngated(1) == 0);
    }
    {   /* RETRIG OFF with EXT: pitches stay (the T07 rule) */
        uint8_t w[3];
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 0; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); transport_req = 1;
        u_key(100, K_C4, 1); run_to(600);
        { uint32_t n = or_live(60, orc_pcs(0, 2), 3, 0, w); ok = notes_are(1, w, n); }
        run_to(1500);
        { uint32_t n = or_live(60, orc_pcs(0, 2), 3, 0, w); ok = ok && notes_are(1, w, n) && ons_since_bar(1, 1) == 0u; }
        bad += x_check("T21 RETRIG OFF with EXT: the held voicing keeps its pitches, no re-attack", ok);
    }
    {   /* the ARP on: no retrigger (the ARP follows at its own steps) */
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
        trk[1].p[P_AMODE] = AM_UP; trk[1].p[P_ARATE] = 2; trk[1].p[P_AHOLD] = 1; transport_req = 1;
        u_key(100, K_C4, 1); u_key(200, K_C4, 0); run_to(1500);
        { static rec_t r; collect(&r, 1, 0); ok = r.n > 8u; for (uint32_t i = 1; i < r.n; i++) ok &= r.blk[i] > r.blk[i - 1u]; }   /* one note per step, never two at once (no extra retrigger attack) */
        bad += x_check("T21 RETRIG HARM with the ARP on: no retrigger attack (one note per step)", ok);
    }
    return bad;
}
/* things that happen to a retriggered voicing */
static int t22(void)
{
    int bad = 0, v;
    static const char *N[6] = {"STOP", "panic_req", "CC120", "SOURCE off", "engine switch", "project load"};
    for (v = 0; v < 6; v++) {
        int ok = 1;
        static project_t p, q; static project_store_t st;
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); transport_req = 1;
        u_key(100, K_C4, 1); u_usb(110, 0x91, 67, 100);
        run_to(1500);                                         /* both retriggered (Am) */
        {   uint8_t wA[4], g[8]; uint32_t nA = or_live(60, orc_pcs(1, 2), 3, 0, wA), ng = gnotes(1, g), i, j;   /* C4's Am voicing sounds (retriggered) */
            for (i = 0; i < nA; i++) { for (j = 0; j < ng && g[j] != wA[i]; j++) ; ok &= j < ng; }
        }
        switch (v) {
        case 0: transport_req = 2; break;
        case 1: panic_req |= 2; break;
        case 2: u_usb(1510, 0xB1, 120, 0); break;
        case 3: ce_set(1, CE_SRC, 0); break;
        case 4: set_engine_of(&trk[1], trk[1].eng_req); trk[1].p[P_VOICE] = V_LEGATO; break;
        default: project_capture(&p); proj_pack(&st, &p); proj_import(&q, &st, PROJ_STORE_SIZE); project_restore_runtime(&q); break;
        }
        run_to(1700);
        u_key(1750, K_C4, 0); u_usb(1760, 0x81, 67, 0);
        run_to(2200);
        ok &= ngated(1) == 0;
        bad += x_check(v == 0 ? "T22 after a retrigger: STOP, then the key-up and note-off: nothing left sounding" :
                       v == 1 ? "T22 .. panic_req" : v == 2 ? "T22 .. CC120" : v == 3 ? "T22 .. SOURCE changed off" : v == 4 ? "T22 .. voice mode / engine request changed" : "T22 .. a project load", ok);
        (void)N;
    }
    return bad;
}
/* two held voicings: the newest and the survivor both follow, the release of the newest leaves the survivor exactly */
static int t23(void)
{
    int bad = 0, ok = 1;
    uint8_t wA[4], wG[4], g[8];
    uint32_t nA, nG, ng;
    setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); transport_req = 1;
    u_key(100, K_C4, 1); u_key(150, K_G4, 1);
    run_to(1500);
    nA = or_live(60, orc_pcs(1, 2), 3, 0, wA);            /* Am pool at C4: G B F -> 55 59 65 */
    nG = or_live(67, orc_pcs(1, 2), 3, 0, wG);            /* and at G4: 67 71 65 */
    {   uint8_t all[8]; uint32_t n = 0, i, j;
        for (i = 0; i < nA; i++) all[n++] = wA[i];
        for (i = 0; i < nG; i++) { for (j = 0; j < n && all[j] != wG[i]; j++) ; if (j == n) all[n++] = wG[i]; }
        or_sort(all, n);
        ok &= notes_are(1, all, n);
    }
    u_key(1550, K_G4, 0); run_to(1700);                    /* the newest goes: exactly C4's three remain (shared pitches stay) */
    ok &= notes_are(1, wA, nA);
    if (!ok) show("after G4 up", 1);
    u_key(1750, K_C4, 0); run_to(1900);
    ng = gnotes(1, g);
    bad += x_check("T23 two held EXT voicings over a bar line: both follow; the newest's release leaves the survivor exactly; then nothing", ok && ng == 0);
    return bad;
}

/* a LATCH STACK toggle press (it ended the latched notes) held over a bar line sounds nothing and swallows nothing; the ARP on: a held key's
 * snapped anchor is not rewritten (no direct note-on beside the ARP, nothing left when the key is up) */
static int t24(void)
{
    int bad = 0, ok = 1;
    {
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); trk[1].p[P_LATCH] = LT_STACK; transport_req = 1;
        u_key(100, K_C4, 1); u_key(150, K_C4, 0);            /* latched */
        run_to(400);
        ok &= ngated(1) == 3u;
        u_key(450, K_C4, 1);                                 /* the toggle: ends them, the key is still down */
        run_to(600);
        ok &= ngated(1) == 0u;
        run_to(1500);                                        /* a bar line: the held toggle key sounds nothing */
        ok &= harmony_bar == 1u && ngated(1) == 0u;
        u_key(1550, K_C4, 0); run_to(1700);
        ok &= ngated(1) == 0u;
        u_key(1750, K_C4, 1); u_key(1800, K_C4, 0); run_to(1950);      /* a fresh press latches again, not swallowed */
        ok &= ngated(1) == 3u;
        trk[1].p[P_LATCH] = LT_OFF; run_to(2100);
        bad += x_check("T24 a LATCH STACK toggle key held over a bar line sounds nothing; a later press latches again; LATCH off frees it", ok && ngated(1) == 0u);
    }
    {
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; harmony_follow[1] = 1; trk[1].p[P_AMODE] = AM_UP; trk[1].p[P_ARATE] = 2; transport_req = 1;
        u_key(100, 13, 1); run_to(1500);                     /* F#4 snapped by the ARP, held over a bar line */
        u_key(1550, 13, 0); run_to(2300);
        bad += x_check("T24 RETRIG HARM with the ARP on and a held key (single snapped note): nothing left sounding after the key-up (no rewritten anchor)", ngated(1) == 0u && trk[1].nheld == 0u);
    }
    return bad;
}

/* ------------------------------------------------------------------ T25 - T30: the first review's fixes --- */
static void gvel(uint32_t t, uint32_t *lo, uint32_t *hi)     /* the velocity range of the gated voices (0, 0: none) */
{
    uint32_t i, a = 999u, b = 0;
    for (i = 0; i < NVOICE; i++)
        if (trk[t].v[i].active && trk[t].v[i].gate) { if (trk[t].v[i].vel < a) a = trk[t].v[i].vel; if (trk[t].v[i].vel > b) b = trk[t].v[i].vel; }
    *lo = b ? a : 0u; *hi = b;
}
static uint32_t nstay(uint32_t t) { uint32_t n = 0, j; for (j = 0; j < 128u; j++) n += (uint32_t)lat_get(lat_stay[t], j); return n; }
static uint32_t nother(uint32_t t) { uint32_t n = 0, j; for (j = 0; j < 128u; j++) n += (uint32_t)lat_get(lat_bus_other[t], j); return n; }
/* review A F1: a MIDI note pressed as a LATCH STACK toggle (it ended the latched note) and held over a bar line sounds nothing, SOURCE OFF
 * (RETRIG HARM, CHRD OFF, FOLLOW) as with EXT; its release latches nothing */
static int t25(void)
{
    int bad = 0, ex;
    for (ex = 0; ex < 2; ex++) {
        int ok = 1;
        setup(); harm_prog(0, 2, 4, 5); harmony_follow[1] = 1; link_retrig[1] = 1; ce_set(1, CE_SRC, ex); ce_set(1, CE_EXT, 2);
        trk[1].p[P_LATCH] = LT_STACK; transport_req = 1;
        u_usb(100, 0x91, 60, 100); u_usb(150, 0x81, 60, 0);   /* latched */
        run_to(400);
        ok &= ngated(1) >= 1u && nstay(1) >= 1u;
        u_usb(450, 0x91, 60, 100);                            /* the toggle: ends it, the note is still down */
        run_to(600);
        ok &= ngated(1) == 0u;
        run_to(1500);                                         /* the bar line: the held toggle note sounds nothing */
        ok &= harmony_bar == 1u && ngated(1) == 0u;
        u_usb(1550, 0x81, 60, 0); run_to(1800);
        ok &= ngated(1) == 0u && nstay(1) == 0u;              /* and its release latches nothing */
        bad += x_check(ex ? "T25 EXT: a MIDI LATCH STACK toggle held over a bar line sounds nothing, its release latches nothing" :
                            "T25 SOURCE OFF (CHRD OFF, FOLLOW, RETRIG HARM): a MIDI LATCH STACK toggle held over a bar line sounds nothing, its release latches nothing", ok);
    }
    return bad;
}
/* review A F2: an EXT track with CHRD stored BUS and LATCH is not a BUS chord owner: a released EXT voicing is booked in lat_bus_other
 * nowhere and follows the bars exactly as with CHRD OFF; STOP leaves a held key following (review A F6) */
static int t26_scene(int chrd, int lat, uint8_t g[4][8], uint8_t *ng)
{
    int i, other = 0;
    setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2);
    trk[1].p[P_CHRD] = (int16_t)(chrd ? CH_BUS : CH_OFF); trk[1].p[P_LATCH] = (int16_t)lat; transport_req = 1;
    u_key(100, K_C4, 1); u_key(200, K_C4, 0);
    run_to(400); ng[0] = (uint8_t)gnotes(1, g[0]); other += (int)nother(1);
    for (i = 1; i < 4; i++) { run_to(400u + 1100u * (uint32_t)i); ng[i] = (uint8_t)gnotes(1, g[i]); other += (int)nother(1); }
    trk[1].p[P_LATCH] = LT_OFF; run_to(400u + 1100u * 4u);
    other += (int)ngated(1) * 100;
    return other;
}
static int t26(void)
{
    int bad = 0, lat;
    for (lat = LT_NEW; lat <= LT_STACK; lat++) {
        uint8_t a[4][8], b[4][8], na[4], nb[4];
        int ok = 1, i, oa = t26_scene(0, lat, a, na), ob = t26_scene(1, lat, b, nb);
        for (i = 0; i < 4; i++) ok &= na[i] == nb[i] && !memcmp(a[i], b[i], na[i]);
        ok &= oa == 0 && ob == 0;
        bad += x_check(lat == LT_NEW ? "T26 EXT + CHRD BUS + LATCH NEW: the released voicing follows the bars as with CHRD OFF, never booked as an older BUS chord, nothing left after LATCH off" :
                                       "T26 EXT + CHRD BUS + LATCH STACK: the same", ok);
    }
    {
        uint8_t a[2][8], na[2];
        int ok = 1, c;
        for (c = 0; c < 2; c++) {
            setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = 1; ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); trk[1].p[P_CHRD] = (int16_t)(c ? CH_BUS : CH_OFF);
            transport_req = 1; u_key(100, K_C4, 1); run_to(400);
            transport_req = 2; run_to(600); ok &= ngated(1) >= 1u;      /* a held key sounds on through STOP (a plain chord, as in 0.11.0) */
            transport_req = 1; run_to(1900);                            /* PLAY, a bar line: it follows */
            na[c] = (uint8_t)gnotes(1, a[c]);
            u_key(2000, K_C4, 0); run_to(2300); ok &= ngated(1) == 0u;
        }
        ok &= na[0] > 0 && na[0] == na[1] && !memcmp(a[0], a[1], na[0]);
        bad += x_check("T26 EXT + CHRD BUS: STOP with a held key leaves it sounding and following exactly as with CHRD OFF", ok);
    }
    return bad;
}
/* review A F3: a retriggered voicing / snapped note keeps the velocity it was pressed with (EXT and not, MIDI and key) */
static int t27(void)
{
    int bad = 0, v;
    static const char *N[4] = {"T27 EXT, MIDI v30: the retriggered voicing keeps v30", "T27 EXT, a key at VEL 40: the retriggered voicing keeps it",
                               "T27 SOURCE OFF follow, MIDI v30: the re-snapped note keeps v30", "T27 SOURCE OFF follow, a key at VEL 40: the re-snapped note keeps it"};
    for (v = 0; v < 4; v++) {
        int ok = 1;
        uint32_t lo0, hi0, lo1, hi1, want = (v & 1) ? 40u : 30u;
        setup(); harm_prog(0, 2, 4, 5); harmony_follow[1] = 1; link_retrig[1] = 1; ce_set(1, CE_SRC, v < 2); ce_set(1, CE_EXT, 2); transport_req = 1;
        kb_vel = 100;
        if (v & 1) { kb_vel = (uint8_t)want; u_key(100, K_C4, 1); } else u_usb(100, 0x91, 60, want);
        run_to(600); gvel(1, &lo0, &hi0);
        run_to(1500); gvel(1, &lo1, &hi1);
        ok &= hi0 != 0 && lo0 == hi0 && hi1 != 0 && lo1 == hi1 && hi1 == hi0 && hi0 != 100u && ons_since_bar(1, 1) >= 1u;
        if (!ok) printf("    before %u..%u after %u..%u\n", lo0, hi0, lo1, hi1);
        kb_vel = 100;
        bad += x_check(N[v], ok);
    }
    return bad;
}
/* review B F1: no MIDI note owned = no scan of the 2048 MIDI slots at a bar line (the bookkeeping midi_owners / owned says none: a stray slot is not looked at) */
static int t28(void)
{
    int ok = 1;
    setup(); harm_prog(0, 2, 4, 5); harmony_follow[1] = 1; link_retrig[1] = 1; transport_req = 1;
    run_to(300);
    midi_note_set(1, 61, 2);                                   /* a slot nobody owns by the counts */
    run_to(1500);
    ok &= harmony_bar == 1u && ngated(1) == 0u;
    midi_note_set(1, 61, 0);
    return x_check("T28 RETRIG HARM, CHRD OFF, nothing held: the bar line reads no MIDI slot (midi_owners / owned say none)", ok);
}
/* review B F2: EXT + RETRIG SCALE + LATCH: a LINK key change re-strikes nothing (SCALE has no effect on an EXT track) */
static int t29(void)
{
    int bad = 0, rt, n0, n1;
    for (rt = 0; rt < 2; rt++) {
        int ok = 1;
        setup(); harm_prog(0, 9, 5, 7); link_retrig[1] = (uint8_t)(rt ? 3 : 2); ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 2); trk[1].p[P_LATCH] = LT_NEW; transport_req = 1;
        u_key(100, K_C4, 1); u_key(200, K_C4, 0);
        run_to(500); ok &= ngated(1) == 3u;
        n0 = (int)nnev; link_retrig_req |= 2u; run_to(700); n1 = (int)nnev;
        ok &= n1 == n0 && ngated(1) == 3u;                      /* BOTH: the LINK trigger does not move or re-strike a latched voicing either */
        bad += x_check(rt ? "T29 EXT + RETRIG BOTH + LATCH: a LINK key change re-strikes no latched note" : "T29 EXT + RETRIG SCALE + LATCH: a LINK key change re-strikes no latched note", ok);
    }
    return bad;
}

static int t30(void) {
    setup(); harm_const(0); ce_set(1, CE_SRC, 1); ce_set(1, CE_EXT, 0);
    link_retrig[1] = 1; u_key(10, K_C4, 1); run_to(30);
    /* A previous STACK toggle may leave a mark on the pitch we move into. */
    lat_put(lat_eat[1], 55, 1);
    hroot_set(0, 9); ret_harm = 1; link_retrigger(1); ret_harm = 0;
    int ok = kb_chn[K_C4] == 1 && kb_chord[K_C4][0] == 55 && !lat_get(lat_eat[1],55);
    u_key(40, K_C4, 0); run_to(70);
    return x_check("T30 retrigger clears stale toggle mark on the destination pitch; release ends the new note",ok && !ngated(1));
}
static int t31(void) {
    setup(); harm_const(0); trk[1].p[P_CHRD] = CH_BUS;
    u_key(10,K_C4,1);run_to(30);
    int ok = chord_last[1].n && chord_live[1] && ngated(1);
    chord_stop_all();
    ok = ok && !chord_last[1].n && !chord_live[1] && !kb_live[K_C4] && !ngated(1);
    u_key(40,K_C4,0);run_to(60);
    return x_check("T31 STOP clears the live BUS chord, gates and held-key retrigger eligibility",ok && !ngated(1));
}

/* ------------------------------------------------------------------ T32-T34: physical EXT MIDI and held RETRIG --- */
static int t32(void)
{
    ext_info_t o;
    int ok = 1;
    setup();
    ce_set(1, CE_SRC, 2); /* T2 follows physical MIDI notes on T1 */
    ce_set(1, CE_EXT, 2);
    u_usb(10, 0x90, 60, 100); run_to(240);
    ce_info(1, &o);
    ok &= o.state == EXT_ST_AMBIG && ext_held[0][60] == 1u;
    u_usb(310, 0x90, 64, 100); run_to(540); /* > 200 ms after C */
    ce_info(1, &o);
    ok &= o.state == EXT_ST_AMBIG && ext_held[0][60] && ext_held[0][64];
    u_usb(610, 0x90, 67, 100); run_to(830); /* > 200 ms after E */
    ce_info(1, &o);
    ok &= o.state == EXT_ST_OK && o.have_chord && o.root == 0u &&
          (xext[1].chord & 0xFFFu) == ((1u << 0) | (1u << 4) | (1u << 7));
    u_usb(900, 0x80, 64, 0); run_to(1120);
    ce_info(1, &o);
    ok &= o.state == EXT_ST_AMBIG && o.have_chord; /* remembered valid chord */
    u_usb(1200, 0x80, 60, 0); u_usb(1200, 0x90, 67, 0); run_to(1420);
    ce_info(1, &o);
    ok &= o.state == EXT_ST_OK && o.have_chord && !ext_held[0][60] &&
          !ext_held[0][64] && !ext_held[0][67]; /* all up: last chord latches */
    u_usb(1500, 0x90, 57, 95); u_usb(1510, 0x90, 60, 95); u_usb(1520, 0x90, 64, 95);
    run_to(1760);
    ce_info(1, &o);
    ok &= o.state == EXT_ST_OK && o.root == 9u;
    return x_check("T32 sequential >200 ms physical MIDI C/E/G; ambiguous retains, velocity-zero release, all-up chord latch, new Am",ok);
}
static int t33(void)
{
    int ok;
    setup();
    ce_set(1, CE_SRC, 2);
    /* The shared sounding owner is one (channel,pitch), but the physical
     * harmonic owners are separate for USB and TRS. */
    midi_event_source(0x90u, 0u, 60u, 100u, 1u);
    midi_event_source(0x90u, 0u, 60u, 100u, 2u);
    midi_event_source(0x90u, 0u, 48u, 100u, 1u);
    ok = ext_held[0][60] == 2u && ext_held[0][48] == 1u &&
         midi_owners[0] == 2u;
    midi_event_source(0x80u, 0u, 60u, 0u, 1u);
    ok &= ext_held[0][60] == 1u && midi_owners[0] == 2u;
    midi_event_source(0x90u, 0u, 60u, 0u, 2u);
    ok &= ext_held[0][60] == 0u && ext_held[0][48] == 1u &&
          midi_owners[0] == 1u;
    midi_event_source(0x80u, 0u, 48u, 0u, 1u);
    ok &= !midi_owners[0] && !ext_held[0][48];
    return x_check("T33 USB/TRS independent holds, same pitch and octaves, final release alone clears voice ownership", ok);
}
static int t34(void)
{
    int bad = 0;
    for (int rt = 0; rt < 2; rt++) {
        int ok = 1;
        uint8_t before[128], after[128];
        uint32_t nb, na, ons;
        setup();
        ce_set(1, CE_SRC, 2);
        ce_set(1, CE_EXT, 2);
        link_retrig[1] = (uint8_t)rt; /* OFF versus HARM */
        u_usb(10, 0x90, 60, 100); u_usb(20, 0x90, 64, 100); u_usb(30, 0x90, 67, 100);
        run_to(240);
        u_key(260, K_C4, 1); run_to(400);
        nb = gnotes(1, before);
        ons = nnev;
        ok &= nb > 0u;
        u_usb(500, 0x80, 60, 0); u_usb(500, 0x80, 64, 0); u_usb(500, 0x80, 67, 0);
        u_usb(550, 0x90, 57, 100); u_usb(560, 0x90, 60, 100); u_usb(570, 0x90, 64, 100);
        run_to(830);
        na = gnotes(1, after);
        if (rt) ok &= na > 0u && (na != nb || memcmp(after, before, nb)) && nnev > ons;
        else ok &= na == nb && !memcmp(after, before, nb);
        u_key(850, K_C4, 0); run_to(1000);
        ok &= ngated(1) == 0u; /* original NoteOff remains valid after rebuild */
        bad += x_check(rt ? "T34 RETRIG HARM revoices held T2 panel note and releases cleanly" :
                            "T34 RETRIG OFF does not change held T2 panel note", ok);
    }
    return bad;
}

/* ------------------------------------------------------------------ T35-T36: sustain and actual HARM UI route --- */
static int t35(void)
{
    int ok;
    ext_info_t o;
    setup();
    ce_set(1, CE_SRC, 2);
    u_usb(10,0x90,60,100); u_usb(20,0x90,64,100); u_usb(30,0x90,67,100);
    run_to(240);
    ce_info(1,&o);
    ok = o.state == EXT_ST_OK && o.root == 0u;
    u_usb(250,0xB0,64,127); /* sustain pedal down */
    u_usb(300,0x80,60,0); u_usb(310,0x80,64,0); u_usb(320,0x80,67,0);
    run_to(550);
    ce_info(1,&o);
    ok &= o.state == EXT_ST_OK && o.root == 0u &&
          !ext_held[0][60] && !ext_held[0][64] && !ext_held[0][67] &&
          (midi_note_get(0,60) & MIDI_PEDAL_NOTE);
    u_usb(600,0xB0,64,0); run_to(730);
    ok &= !midi_owners[0] && !midi_note_get(0,60);
    return x_check("T35 CC64 sustains sounding voices, not physical harmony notes; pedal up releases existing MIDI owners",ok);
}
static int t36(void)
{
    int ok;
    setup();
    link_retrig[1] = 0;
    ce_set(1, CE_SRC, 2); ce_set(1,CE_EXT,2); ce_set(1,CE_OCT,1);
    open_family(FAM_SCL);
    for (uint32_t k = 0; k < 8u && !link_setup_row; k++) scl_page_turn(1);
    ok = link_setup_row && cur_page()->graph == GR_LINK &&
         harm_role(TSEL) == HROLE_EXT && trk[1].p[P_CSRC] == 2;
    edit_param(3u,1); /* spare K4 is an actual editable per-track RETRIG */
    ok &= link_retrig[1] == 1u && trk[1].p[P_CSRC] == 2 &&
          ce_get(1,CE_EXT) == 2 && ce_get(1,CE_OCT) == 1;
    edit_param(3u,1);
    ok &= link_retrig[1] == 2u; /* stored SCALE, inert in EXT: UI warns */
    scl_page_turn(-1);
    ok &= !link_setup_row && cur_page()->graph == GR_CHORD && chord_ext_row;
    return x_check("T36 HARM EXT RETRIG K4 via real page path; SOURCE/EXT/OCT preserved; SCALE does not change mode",ok);
}

/* ------------------------------------------------------------------ T37: held MIDI follower retains release identity --- */
static int t37(void)
{
    uint8_t before[128], after[128];
    uint32_t nb, na;
    int ok;
    setup();
    ce_set(1, CE_SRC, 2); ce_set(1, CE_EXT, 2);
    link_retrig[1] = 3u; /* BOTH acts like HARM on EXT changes */
    u_usb(10, 0x90, 60, 100); u_usb(20, 0x90, 64, 100); u_usb(30, 0x90, 67, 100);
    run_to(250);
    u_usb(270, 0x91, 72, 100); run_to(400);
    nb = gnotes(1, before);
    ok = nb > 0u;
    u_usb(500, 0x80, 60, 0); u_usb(500, 0x80, 64, 0); u_usb(500, 0x80, 67, 0);
    u_usb(550, 0x90, 57, 100); u_usb(560, 0x90, 60, 100); u_usb(570, 0x90, 64, 100);
    run_to(830);
    na = gnotes(1, after);
    ok &= na > 0u && (na != nb || memcmp(after, before, nb));
    u_usb(850, 0x81, 72, 0); run_to(1020);
    ok &= ngated(1) == 0u && !midi_owners[1];
    return x_check("T37 RETRIG BOTH revoices held T2 MIDI key; original MIDI NoteOff releases remapped notes",ok);
}

int main(void)
{
    int bad = 0;
    if (x_on("T01")) bad += t01();
    if (x_on("T02")) bad += t02();
    if (x_on("T03")) bad += t03();
    if (x_on("T04")) bad += t04();
    if (x_on("T05")) bad += t05();
    if (x_on("T06")) bad += t06();
    if (x_on("T07")) bad += t07();
    if (x_on("T08")) bad += t08();
    if (x_on("T09")) bad += t09();
    if (x_on("T10")) bad += t10();
    if (x_on("T11")) bad += t11();
    if (x_on("T12")) bad += t12();
    if (x_on("T13")) bad += t13();
    if (x_on("T14")) bad += t14();
    if (x_on("T15")) bad += t15();
    if (x_on("T16")) bad += t16();
    if (x_on("T18")) bad += t18();
    if (x_on("T19")) bad += t19();
    if (x_on("T20")) bad += t20();
    if (x_on("T21")) bad += t21();
    if (x_on("T22")) bad += t22();
    if (x_on("T23")) bad += t23();
    if (x_on("T24")) bad += t24();
    if (x_on("T25")) bad += t25();
    if (x_on("T26")) bad += t26();
    if (x_on("T27")) bad += t27();
    if (x_on("T28")) bad += t28();
    if (x_on("T29")) bad += t29();
    if (x_on("T30")) bad += t30();
    if (x_on("T31")) bad += t31();
    if (x_on("T32")) bad += t32();
    if (x_on("T33")) bad += t33();
    if (x_on("T34")) bad += t34();
    if (x_on("T35")) bad += t35();
    if (x_on("T36")) bad += t36();
    if (x_on("T37")) bad += t37();
    if (bad) { printf("chordext test FAILED (%d)\n", bad); return 1; }
    printf("chordext test passed\n");
    return 0;
}
#endif
