/* SPDX-License-Identifier: GPL-3.0-only */
/* CHORD SOURCE (HARM and T1-T4) / EXT / OCT fuzzer: the review fuzzer (rv_fuzz.c of the HARMONY round-7 review: random event
 * stream on the complete emulator, ASan + UBSan) with the new operations: SOURCE / EXT / OCT per track, engine switch (DRUM -> N/A),
 * project save/load, preset load, panic, STOP / PLAY, HARMONY on/off, ARP / SEQ / LATCH / keys / MIDI.
 *   usage: chordext_fuzz seed iters qevery projmode profile      (profile 2 = A: the HARM slice; 3 = D: A with RETRIG HARM / BOTH on, bars of one chord; 4 = B: live gestures; 1 / 0 the review's own)
 *   Invariants: I1 nothing gated after quiesce + panic + STOP; I2 no stale owners; I3 a MIDI NoteOff ends exactly the notes its
 *   NoteOn started (shadow model, on the quiet cases: ARP / LATCH / keys / sustain / playing all off); I5 ranges (src <= 5, ext <= 2,
 *   -2 <= oct <= 2, gated notes 0..127, a preset load keeps the three); I6 the project round trip keeps the three.
 *   Mutations of the firmware (tools of experiments/action-banks/chordext_mutants.py) must turn it red. */
#ifndef RV_NOHOME
#define FELUCCA_HOME_PERF 1
#endif
#include <stdint.h>
static uint32_t live_commits, live_ambiguous;
#define EXT_COMMIT_HOOK(source, valid) do { (void)(source); if (valid) live_commits++; else live_ambiguous++; } while (0)
#define PAIR_TEST_MAIN rv_unused_main
#include "../engine-pair-morph/pair_test.c"
static unsigned long step_no;
static uint32_t prof;
static unsigned long trace_from = ~0ul;
#include <stdarg.h>
static int rv_tr(const char *f, ...) { if (step_no >= trace_from) { va_list ap; va_start(ap, f); fprintf(stderr, "  @%lu ", step_no); vfprintf(stderr, f, ap); fprintf(stderr, "\n"); va_end(ap); } return 0; }
#define TR(...) rv_tr(__VA_ARGS__)
static uint64_t rs;
static uint32_t rvw_midi(uint32_t a, uint32_t b, uint32_t c) { TR("midi %02x %u %u", a, b, c); return web_midi(a, b, c); }
static void rvw_keys(uint32_t m) { TR("keys %07x", m); web_keys(m); }
static void rvw_buttons(uint32_t m) { TR("buttons %04x", m); web_buttons(m); }
static void rvw_enc(uint32_t r, int32_t n) { TR("enc %u %d", r, n); web_enc(r, n); }
#define web_midi rvw_midi
#define web_keys rvw_keys
#define web_buttons rvw_buttons
#define web_enc rvw_enc

static uint32_t rnd(void){ rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static uint32_t rn(uint32_t n){ return n ? rnd() % n : 0; }
static uint32_t pulse_us;
static uint32_t ext_mode;
static void clock_packet(uint32_t st){ midi_enqueue(15u|st<<8,1); }
static void adv(uint32_t n){
    if (!ext_mode) { run_ms(n); return; }
    while (n--) { pulse_us += 1000; while (pulse_us >= 20833u) { pulse_us -= 20833u; clock_packet(0xF8); } run_ms(1); }
}
/* model of MIDI notes held */
static uint8_t mheld[16][128];
static uint32_t kmask;
static unsigned long viol;
#define VIOL(...) do { viol++; fprintf(stderr, "VIOLATION seed-step %lu: ", (unsigned long)step_no); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); if (getenv("RV_STOP")) exit(3); } while (0)

static void sane(void){
    for (uint32_t i = 0; i < 4; i++) {
        if (harmony_root[i] > 11 || harmony_dur[i] > 3) VIOL("harmony root/dur out of range %u %u", harmony_root[i], harmony_dur[i]);
        if (link_src[i] > 4) VIOL("link_src %u", link_src[i]);
        if (link_retrig[i] > 3) VIOL("link_retrig %u", link_retrig[i]);
        if (harmony_follow[i] > 1) VIOL("follow %u", harmony_follow[i]);
        if (chord_last[i].n > CHORD_MAX) VIOL("chord_last n %u", chord_last[i].n);
        if (chord_last[i].n > 0) for (uint32_t j = 0; j < chord_last[i].n; j++) if (chord_last[i].note[j] > 127) VIOL("chord_last note");
    }
    for (uint32_t k = 0; k < 27; k++) {
        if (kb_chn[k] > CHORD_MAX || kb_midin[k] > CHORD_MAX) VIOL("kb_chn/midin k=%u %u %u", k, kb_chn[k], kb_midin[k]);
        for (uint32_t j = 0; j < kb_chn[k]; j++) if (kb_chord[k][j] > 127) VIOL("kb_chord note");
    }
    for (uint32_t m = 0; m < MCHORD_N; m++) if (mchord[m].id) {
        if (mchord[m].n > CHORD_MAX || mchord[m].id > NTRK) VIOL("mchord n/id %u %u", mchord[m].n, mchord[m].id);
    }
#if CARAVEL_ACTION_BANKS
    if (harmony_shift < -7 || harmony_shift > 7 || harmony_shift_want < -7 || harmony_shift_want > 7) VIOL("shift %d %d", harmony_shift, harmony_shift_want);
    if (harmony_pending > H_RESUME) VIOL("pending %u", harmony_pending);
#endif
    for (uint32_t t = 0; t < NTRK; t++) {
        if ((uint16_t)trk[t].p[P_CSRC] > 5u || (uint16_t)trk[t].p[P_CEXT] > 2u || trk[t].p[P_COCT] < -2 || trk[t].p[P_COCT] > 2) VIOL("I5 SOURCE/EXT/OCT out of range T%u: %d %d %d", t + 1, trk[t].p[P_CSRC], trk[t].p[P_CEXT], trk[t].p[P_COCT]);
        for (uint32_t v = 0; v < NVOICE; v++) if (trk[t].v[v].active && trk[t].v[v].gate && trk[t].v[v].note > 127) VIOL("I5 gated note %u > 127 on T%u", trk[t].v[v].note, t + 1);
    }
    if (ui.page >= NPAGES) VIOL("ui.page");
    if (song.sel >= NTRK) VIOL("song.sel");
}
/* I3 shadow: MIDI notes started on a quiet track (ARP, LATCH, keys, sustain, playing all off) -> the notes they sounded */
static uint32_t i3_epoch = 1;
static struct { uint32_t epoch; uint8_t trk; uint8_t n; uint8_t note[CHORD_MAX]; } i3s[16][128];
static unsigned long i3_checked, i3_ons;
static uint8_t pedal[16];
static int i3_quiet(void) {
    if (song.playing || kmask) return 0;
    for (uint32_t t = 0; t < NTRK; t++) if (trk[t].p[P_AMODE] || trk[t].p[P_LATCH] || trk[t].nheld) return 0;
    for (uint32_t c = 0; c < 16; c++) if (pedal[c]) return 0;
    return 1;
}
static uint32_t gate_set(uint8_t g[NTRK][128]) { uint32_t n = 0; memset(g, 0, NTRK * 128); for (uint32_t t = 0; t < NTRK; t++) for (uint32_t v = 0; v < NVOICE; v++) if (trk[t].v[v].active && trk[t].v[v].gate) { g[t][trk[t].v[v].note & 127u] = 1; n++; } return n; }
static void i3_midi_on(uint32_t c, uint32_t n, uint32_t vel) {
    static uint8_t b[NTRK][128], a[NTRK][128];
    int q = i3_quiet() && !mheld[c][n];
    if (q) gate_set(b);
    web_midi(0x90 | c, n, vel); mheld[c][n] = 1;
    i3s[c][n].epoch = 0;
    if (!q) return;
    run_ms(8);
    if (gate_set(a) > 6 || !i3_quiet()) return;
    {   uint32_t tt = NTRK, k = 0; uint8_t nn[CHORD_MAX]; int multi = 0;
        for (uint32_t t = 0; t < NTRK; t++) for (uint32_t j = 0; j < 128; j++) if (a[t][j] && !b[t][j]) { if (tt != NTRK && tt != t) multi = 1; tt = t; if (k < CHORD_MAX) nn[k] = (uint8_t)j; k++; }
        if (multi || tt == NTRK || k > CHORD_MAX || ENGINES[trk[tt].engine]->oneshot) return;   /* (a drum hit ignores its NoteOff) */
        i3s[c][n].epoch = i3_epoch; i3s[c][n].trk = (uint8_t)tt; i3s[c][n].n = (uint8_t)k; memcpy(i3s[c][n].note, nn, k); i3_ons++;
    }
}
static void i3_midi_off(uint32_t c, uint32_t n) {
    static uint8_t b[NTRK][128], a[NTRK][128];
    int q = i3s[c][n].epoch == i3_epoch && i3_quiet();
    uint32_t tt = i3s[c][n].trk, k = i3s[c][n].n; uint8_t nn[CHORD_MAX]; memcpy(nn, i3s[c][n].note, sizeof nn);
    i3s[c][n].epoch = 0;
    if (q) { if (gate_set(b) > 6) q = 0; }
    web_midi(0x80 | c, n, 0); mheld[c][n] = 0;
    if (!q) return;
    run_ms(8);
    gate_set(a);
    for (uint32_t t = 0; t < NTRK; t++) for (uint32_t j = 0; j < 128; j++) {
        int mine = 0, other = 0;
        for (uint32_t i = 0; i < k; i++) mine |= t == tt && nn[i] == j;
        for (uint32_t cc = 0; cc < 16; cc++) for (uint32_t m = 0; m < 128; m++) if (mheld[cc][m] && i3s[cc][m].epoch == i3_epoch && i3s[cc][m].trk == t) for (uint32_t i = 0; i < i3s[cc][m].n; i++) other |= i3s[cc][m].note[i] == j;
        if (b[t][j] && !mine && !a[t][j]) { VIOL("I3 NoteOff of ch%u note %u ended T%u note %u that it did not start (started %u: %u %u %u %u; src %d ext %d oct %d vm %d chrd %d rt %u follow %u hon %u)", c, n, t + 1, j, k, nn[0], nn[1], nn[2], nn[3], trk[tt].p[P_CSRC], trk[tt].p[P_CEXT], trk[tt].p[P_COCT], trk[tt].p[P_VOICE], trk[tt].p[P_CHRD], link_retrig[tt], harmony_follow[tt], harmony_on); }
        if (mine && !other && a[t][j]) { VIOL("I3 NoteOff of ch%u note %u left T%u note %u sounding (S=%u notes first %u; src %d ext %d oct %d vm %d chrd %d hon %u amode %d latch %d nheld %u route %d)", c, n, t + 1, j, k, nn[0], trk[t].p[P_CSRC], trk[t].p[P_CEXT], trk[t].p[P_COCT], trk[t].p[P_VOICE], trk[t].p[P_CHRD], harmony_on, trk[t].p[P_AMODE], trk[t].p[P_LATCH], trk[t].nheld, song.g[G_ROUTE]); }
    }
    if (getenv("I3_DBG") && viol) { for (uint32_t t = 0; t < NTRK; t++) { fprintf(stderr, " T%u before:", t+1); for (uint32_t j = 0; j < 128; j++) if (b[t][j]) fprintf(stderr, " %u", j); fprintf(stderr, " after:"); for (uint32_t j = 0; j < 128; j++) if (a[t][j]) fprintf(stderr, " %u", j); fprintf(stderr, "\n"); } exit(3); }
    i3_checked++;
}
static void dump_state(const char *tag){
    fprintf(stderr, "== STATE %s (step %lu) playing=%u harmony_on=%u bar=%u\n", tag, step_no, song.playing, harmony_on, harmony_bar);
    for (uint32_t t = 0; t < 4; t++) {
        fprintf(stderr, " T%u chrd %d voic %d latch %d amode %d vce %d follow %u link %u/%u | chord_last n=%u [", t + 1, trk[t].p[P_CHRD], trk[t].p[P_VOIC], trk[t].p[P_LATCH], trk[t].p[P_AMODE], trk[t].p[P_VOICE], harmony_follow[t], link_src[t], link_retrig[t], chord_last[t].n);
        for (uint32_t j = 0; j < chord_last[t].n; j++) fprintf(stderr, "%u ", chord_last[t].note[j]);
        fprintf(stderr, "] live=%u lat_last=%u nheld=%u arp_note=%u nmono=%u\n", chord_live[t], lat_bus_last[t], trk[t].nheld, trk[t].arp_note, trk[t].nmono);
        fprintf(stderr, "    stay:"); for (uint32_t n = 0; n < 128; n++) if (lat_get(lat_stay[t], n)) fprintf(stderr, " %u", n);
        fprintf(stderr, " | other:"); for (uint32_t n = 0; n < 128; n++) if (lat_get(lat_bus_other[t], n)) fprintf(stderr, " %u", n);
        fprintf(stderr, " | eat:"); for (uint32_t n = 0; n < 128; n++) if (lat_get(lat_eat[t], n)) fprintf(stderr, " %u", n);
        fprintf(stderr, "\n    ext src %d ext %d oct %d | seq_n %u hold %u off %u notes %u %u %u %u | arp_ch %u %u %u rat %u", trk[t].p[P_CSRC], trk[t].p[P_CEXT], trk[t].p[P_COCT], trk[t].seq_n, trk[t].seq_hold, trk[t].seq_off, trk[t].seq_notes[0], trk[t].seq_notes[1], trk[t].seq_notes[2], trk[t].seq_notes[3], trk[t].arp_ch[0], trk[t].arp_ch[1], trk[t].arp_ch[2], trk[t].rat_left);
        fprintf(stderr, "\n    voices:"); for (uint32_t v = 0; v < NVOICE; v++) if (trk[t].v[v].active) fprintf(stderr, " [%u n%u g%u st%u]", v, trk[t].v[v].note, trk[t].v[v].gate, trk[t].v[v].stage);
        fprintf(stderr, "\n");
    }
    for (uint32_t k = 0; k < 27; k++) if (kb_chn[k] || kb_live[k] || kb_midin[k]) { fprintf(stderr, " key %u trk %u chn %u live %u midin %u [", k, kb_trk[k], kb_chn[k], kb_live[k], kb_midin[k]); for (uint32_t j = 0; j < kb_chn[k]; j++) fprintf(stderr, "%u ", kb_chord[k][j]); fprintf(stderr, "]\n"); }
    for (uint32_t m = 0; m < MCHORD_N; m++) if (mchord[m].id) { fprintf(stderr, " mchord %u id %u ch %u src %u n %u [", m, mchord[m].id, mchord[m].ch, mchord[m].src, mchord[m].n); for (uint32_t j = 0; j < mchord[m].n; j++) fprintf(stderr, "%u ", mchord[m].note[j]); fprintf(stderr, "]\n"); }
}
static void quiesce(int check){
    if (step_no >= trace_from) dump_state("before quiesce");
    web_keys(0); kmask = 0; adv(80);
    for (uint32_t c = 0; c < 16; c++) {
        for (uint32_t n = 0; n < 128; n++) if (mheld[c][n]) { web_midi(0x80 | c, n, 0); mheld[c][n] = 0; }
        web_midi(0xB0 | c, 64, 0); pedal[c] = 0;
    }
    i3_epoch++;
    adv(40);
    for (uint32_t t = 0; t < NTRK; t++) { trk[t].p[P_LATCH] = LT_OFF; trk[t].p[P_AMODE] = 0; trk[t].p[P_AHOLD] = 0; }
    adv(60);
    transport_req = 2; adv(60); song.g[G_CLOCK] = 0; ext_mode = 0; adv(600);
    if (check) {
        for (uint32_t k = 0; k < 27; k++) if (kb_chn[k] || kb_live[k] || kb_midin[k]) VIOL("STALE key ownership after quiesce: key %u chn %u live %u midin %u", k, kb_chn[k], kb_live[k], kb_midin[k]);
        for (uint32_t m = 0; m < MCHORD_N; m++) if (mchord[m].id) VIOL("STALE mchord %u id %u", m, mchord[m].id);
        for (uint32_t t = 0; t < NTRK; t++) {
            if (lat_any(lat_stay[t]) || lat_any(lat_bus_other[t]) || lat_bus_last[t]) VIOL("STALE latch state trk %u", t);
            if (chord_live[t]) VIOL("STALE chord_live trk %u", t);
        }
        for (uint32_t t = 0; t < NTRK; t++) for (uint32_t v = 0; v < NVOICE; v++)
            if (trk[t].v[v].gate) { if (step_no >= trace_from) dump_state("after quiesce"); VIOL("HANGING NOTE after quiesce: trk %u voice %u note %u", t, v, trk[t].v[v].note); }
    }
}
struct hs { uint8_t root[4], dur[4], on, ls[4], lr[4], fo[4]; int16_t x[4][3]; };
static void hs_get(struct hs *h){ memset(h, 0, sizeof *h); for (uint32_t t = 0; t < 4; t++) { h->x[t][0] = trk[t].p[P_CSRC]; h->x[t][1] = trk[t].p[P_CEXT]; h->x[t][2] = trk[t].p[P_COCT]; } memcpy(h->root, harmony_root, 4); memcpy(h->dur, harmony_dur, 4); h->on = harmony_on; memcpy(h->ls, link_src, 4); memcpy(h->lr, link_retrig, 4); memcpy(h->fo, harmony_follow, 4); }
/* field by field: the struct has padding bytes (after fo[], before x[]) that differ between compilers, a memcmp of the whole struct is not a comparison of the state */
static int hs_eq(const struct hs *a, const struct hs *b){ return !memcmp(a->root, b->root, sizeof a->root) && !memcmp(a->dur, b->dur, sizeof a->dur) && a->on == b->on && !memcmp(a->ls, b->ls, sizeof a->ls) && !memcmp(a->lr, b->lr, sizeof a->lr) && !memcmp(a->fo, b->fo, sizeof a->fo) && !memcmp(a->x, b->x, sizeof a->x); }
static void rand_params(void){
    uint32_t t = rn(4);
    track_t *k = &trk[t];
    if (prof == 1 && rn(3) == 0) { uint32_t q = rn(6); if (q == 0) k->p[P_LATCH] = 1 + rn(2); else if (q == 1) k->p[P_CHRD] = CH_BUS; else if (q == 2) k->p[P_VOIC] = rn(5); else if (q == 3) k->p[P_AMODE] = rn(8) ? 0 : 1 + rn(15); else if (q == 4) k->p[P_VOICE] = rn(5) ? V_POLY : rn(4); else { harmony_follow[t] = rn(6) != 0; link_retrig[t] = 1 + rn(3); } return; }
    TR("params(t=%u) before: chrd %d voic %d latch %d amode %d voice %d scale %d root %d quant %d follow %u link %u/%u hon %u", t, k->p[P_CHRD], k->p[P_VOIC], k->p[P_LATCH], k->p[P_AMODE], k->p[P_VOICE], k->p[P_SCALE], k->p[P_ROOT], k->p[P_QUANT], harmony_follow[t], link_src[t], link_retrig[t], harmony_on);
    switch (rn(16)) {
    case 0: k->p[P_CHRD] = rn(3) ? CH_BUS : rn(11); break;
    case 1: k->p[P_VOIC] = rn(5); break;
    case 2: k->p[P_LATCH] = rn(3); break;
    case 3: k->p[P_AMODE] = rn(3) ? 0 : rn(16); break;
    case 4: k->p[P_VOICE] = rn(4); break;
    case 5: k->p[P_SCALE] = rn(16); break;
    case 6: k->p[P_ROOT] = rn(12); break;
    case 7: k->p[P_QUANT] = rn(5); break;
    case 8: harmony_follow[t] = rn(2); break;
    case 9: link_src[t] = rn(5); break;
    case 10: link_retrig[t] = rn(4); break;
    case 11: harmony_perf_clear(); harmony_on = rn(4) != 0; harmony_base_bar = perf_bars_q16 >> 16; harmony_bar = 0; break;
    case 12: hroot_set(rn(4), rn(12)); hdur_set(rn(4), rn(4)); break;
    case 13: k->p[P_AHOLD] = rn(2); k->p[P_ARATE] = rn(10); k->p[P_AOCT] = 1 + rn(4); break;
    case 14: song.sel = rn(4); break;
    case 15: song.g[G_ROUTE] = rn(6); break;
    }
}
static const uint8_t PRESETS_N = 4;
static void rand_ext(void){
    uint32_t t = rn(4);
    track_t *k = &trk[t];
    switch (rn(12)) {
    case 0: case 1: k->p[P_CSRC] = (int16_t)rn(prof == 4 ? 6 : 2); break;
    case 2: k->p[P_CEXT] = (int16_t)rn(3); break;
    case 3: k->p[P_COCT] = (int16_t)rn(5) - 2; break;
    case 4: { uint32_t e = eng_unstore(rn(6)); set_engine_of(k, e); i3_epoch++; break; }
    case 5: { int16_t s = k->p[P_CSRC], x = k->p[P_CEXT], o = k->p[P_COCT]; apply_preset_to(k, rn(ENGINES[eng_idx(k->eng_req)]->npresets)); i3_epoch++;
              if (k->p[P_CSRC] != s || k->p[P_CEXT] != x || k->p[P_COCT] != o) VIOL("I5 a preset load changed SOURCE/EXT/OCT of T%u: %d %d %d -> %d %d %d", t + 1, s, x, o, k->p[P_CSRC], k->p[P_CEXT], k->p[P_COCT]); break; }
    case 6: panic_req |= (uint8_t)(rn(2) ? 15u : 1u << t); i3_epoch++; break;
    case 7: harmony_on = rn(4) != 0; break;
    case 8: { uint32_t i = rn(16); step_t *s = &k->step[i]; s->n = (uint8_t)(1 + rn(3)); for (uint32_t j = 0; j < 3; j++) s->note[j] = (uint8_t)(36 + rn(60)); s->time = rn(8) ? ST_NOTE : rn(2) ? ST_REST : ST_TIE; s->vel = 100; break; }
    case 9: transport_req = rn(3) ? 1 : 2; if (transport_req == 2) i3_epoch++; break;
    case 10: k->p[P_AMODE] = rn(3) ? 0 : 1 + rn(15); k->p[P_AHOLD] = rn(3) == 0; k->p[P_ARATE] = rn(10); k->p[P_AOCT] = 1 + rn(3); break;
    case 11: k->p[P_LATCH] = rn(3); k->p[P_VOICE] = rn(4); i3_epoch++; break;
    }
}
/* an I3 probe: a quiet panel (stopped, no keys, ARP / LATCH off, pedals up, nothing held), one MIDI note on, the settings and the
 * bus bar changed under it, the note off: the off must end exactly the notes the on started */
static void i3_probe(void){
    uint32_t c = rn(4), n = rn(8) == 0 ? rn(128) : 30 + rn(70), k;
    transport_req = 2; web_keys(0); kmask = 0;
    for (uint32_t t = 0; t < NTRK; t++) { trk[t].p[P_AMODE] = 0; trk[t].p[P_LATCH] = LT_OFF; trk[t].p[P_AHOLD] = 0; }
    for (uint32_t cc = 0; cc < 16; cc++) { web_midi(0xB0 | cc, 64, 0); pedal[cc] = 0; for (uint32_t m = 0; m < 128; m++) if (mheld[cc][m]) { web_midi(0x80 | cc, m, 0); mheld[cc][m] = 0; } }
    uint32_t played = 0;
    c = rn(3); song.g[G_ROUTE] = 0;          /* channel c = track c (T1..T3: a melodic one) on EXT, POLY, no CHRD: what the shadow can judge */
    trk[c].p[P_VOICE] = V_POLY; trk[c].p[P_CHRD] = CH_OFF; trk[c].p[P_CSRC] = 1; harmony_on = 1; trk[c].p[P_QUANT] = QN_OFF;
    adv(120); i3_epoch++;
    i3_midi_on(c, n, 1 + rn(127));
    for (k = rn(4); k; k--) {
        uint32_t t = rn(4);
        switch (rn(6)) {
        case 0: trk[t].p[P_CSRC] = (int16_t)rn(2); break;
        case 1: trk[t].p[P_CEXT] = (int16_t)rn(3); break;
        case 2: trk[t].p[P_COCT] = (int16_t)rn(5) - 2; break;
        case 3: harmony_on = rn(4) != 0; break;
        case 4: harmony_bar = rn(4); hroot_set(rn(4), rn(12)); break;
        case 5: trk[t].p[P_SCALE] = rn(16); break;
        }
        adv(1 + rn(40));
        played |= song.playing;                  /* (an external clock may have started the transport: a bar line can retrigger what is held, legitimately) */
    }
    if (i3_quiet() && !played) i3_midi_off(c, n); else { web_midi(0x80 | c, n, 0); mheld[c][n] = 0; i3s[c][n].epoch = 0; }
}
/* profile D: a held note over bar lines with RETRIG on (a key or a MIDI note, a latch, EXT or a single snapped note): after its release (and, for
 * STACK, the toggle) nothing of that track may sound. I7. */
static void hold_probe(void){
    uint32_t c = rn(3), kind = rn(3), lat = rn(3), g, n = 40 + rn(50), key = rn(20);
    track_t *t = &trk[c];
    transport_req = 1; web_keys(0); kmask = 0;
    for (uint32_t cc = 0; cc < 16; cc++) { web_midi(0xB0 | cc, 64, 0); pedal[cc] = 0; for (uint32_t m = 0; m < 128; m++) if (mheld[cc][m]) { web_midi(0x80 | cc, m, 0); mheld[cc][m] = 0; } }
    memset(t->step, 0, sizeof t->step);
    panic_req |= 15u; adv(60);                   /* (a clean start: stale mono stacks of the random stream are another matter) */
    song.sel = c; song.g[G_ROUTE] = 0; song.g[G_CLOCK] = 0; ext_mode = 0;
    t->p[P_AMODE] = 0; t->p[P_AHOLD] = 0; t->p[P_LATCH] = LT_OFF; t->p[P_VOICE] = rn(4); t->p[P_CHRD] = rn(4) ? CH_OFF : CH_BUS; t->p[P_QUANT] = QN_OFF;
    t->p[P_CSRC] = (int16_t)(rn(3) != 0); t->p[P_CEXT] = (int16_t)rn(3); t->p[P_COCT] = (int16_t)rn(5) - 2;
    link_retrig[c] = (uint8_t)(1 + (rn(2) ? 2u : 0u)); harmony_follow[c] = 1; harmony_on = 1;
    for (uint32_t i = 0; i < 4; i++) { hroot_set(i, rn(12)); hdur_set(i, 0); }
    adv(200);
    t->p[P_LATCH] = (int16_t)lat;
    if (kind != 1) { kmask = 1u << key; web_keys(kmask); }
    if (kind != 0) { web_midi(0x90 | c, n, 90); mheld[c][n] = 1; }
    adv(1300 + rn(1500));
    if (kind != 1) { kmask = 0; web_keys(0); }
    if (kind != 0) { web_midi(0x80 | c, n, 0); mheld[c][n] = 0; }
    adv(100);
    if (lat == LT_STACK && kind != 1) { kmask = 1u << key; web_keys(kmask); adv(60); kmask = 0; web_keys(0); adv(60); }   /* the same key again: the toggle ends the latched notes */
    if (lat == LT_STACK && kind != 0) { web_midi(0x90 | c, n, 90); adv(60); web_midi(0x80 | c, n, 0); adv(60); }
    t->p[P_LATCH] = LT_OFF;
    adv(300); transport_req = 2; adv(400);         /* (STOP: a pattern of the song chain / other sequencers must not count) */
    for (g = 0; g < NVOICE; g++) if (t->v[g].active && t->v[g].gate) { VIOL("I7 a note held over a retriggered bar line hangs on T%u after its release: note %u (kind %u latch %u vm %d src %d ext %d oct %d chrd %d rt %u)", c + 1, t->v[g].note, kind, lat, t->p[P_VOICE], t->p[P_CSRC], t->p[P_CEXT], t->p[P_COCT], t->p[P_CHRD], link_retrig[c]); break; }
    i3_epoch++;
}
static void rand_midi(void){
    uint32_t c = rn(4) < 3 ? rn(4) : rn(16);
    switch (rn(8)) {
    case 0: case 1: case 2: { uint32_t n = rn(10) == 0 ? rn(128) : 40 + rn(50); { web_midi(0x90 | c, n, 1 + rn(127)); mheld[c][n] = 1; } break; }
    case 3: case 4: { for (uint32_t n = 0; n < 128; n++) if (mheld[c][n] && rn(3) == 0) { { web_midi(0x80 | c, n, 0); mheld[c][n] = 0; } break; } break; }
    case 5: { uint32_t pv = rn(2) * 127; web_midi(0xB0 | c, 64, pv); pedal[c] = (uint8_t)(pv != 0); if (pv) i3_epoch++; break; }
    case 6: if (rn(6) == 0) { web_midi(0xB0 | c, rn(2) ? 123 : 120, 0); i3_epoch++; } break;
    case 7: web_midi(0xB0 | c, 1 + rn(120), rn(128)); break;
    }
}
static void rand_ui(void){
    switch (rn(7)) {
    case 0: case 1: { uint32_t b = rn(NB); uint32_t m = 1u << b; if (rn(5) == 0) m |= 1u << rn(NB); web_buttons(m); adv(20 + rn(80)); web_buttons(0); break; }
    case 2: case 3: web_enc(rn(NE), (int32_t)rn(9) - 4); break;
    case 4: web_enc(rn(NE), (int32_t)rn(41) - 20); break;
    case 5: { uint32_t b = rn(NB); web_buttons(1u << b); adv(600 + rn(300)); web_buttons(0); break; }   /* a long press */
    case 6: if (prof == 1 && rn(2) == 0) { uint32_t g = rn(3) == 0 ? GR_LINK : GR_HARMONY; for (uint32_t i = 0; i < NPAGES; i++) if (PAGES[i].graph == g) { ui.page = i; ui.home = 0; page_entered(); ui.force = 1; break; } } else if (rn(3) == 0) { go_home();
#if CARAVEL_ACTION_BANKS
 if (rn(2)) action_enabled = 1;
#endif
 } break;
    }
}
static void rand_keys(void){
    uint32_t n = 1 + rn(2);
    while (n--) kmask ^= 1u << (rn(8) ? rn(20) : 20 + rn(7));
    if (rn(8) == 0) kmask = 0;
    web_keys(kmask);
}
static void rand_transport(void){
    uint32_t tc = rn(8);
    TR("transport case %u (playing %u clock %d)", tc, song.playing, song.g[G_CLOCK]);
    switch (tc) {
    case 0: transport_req = 1; break;
    case 1: if (prof != 1 || rn(4) == 0) transport_req = 2; break;
    case 2: bar_reset_req = 1; break;
#if CARAVEL_ACTION_BANKS
    case 3: harmony_command = 1 + rn(4); break;
    case 4: harmony_shift_want = (int8_t)(rn(15)) - 7; harmony_shift_dirty = 1; break;
#endif
    case 5: if (rn(3) == 0) { song.g[G_CLOCK] = rn(2); ext_mode = song.g[G_CLOCK]; if (ext_mode) { clock_packet(0xFA); } } break;
    case 6: if (ext_mode && rn(3) == 0) clock_packet(rn(2) ? 0xFC : 0xFB); break;
    case 7: song.g[G_BPM] = prof == 1 ? 180 + rn(120) : 40 + rn(200); break;
    }
}
static void rand_project(void){
    struct hs a, b;
    if (song.playing) return;
    transport_req = 2; adv(40);
    hs_get(&a);
    if (!project_save(rn(4))) { /* saved */ }
    uint32_t slot = proj_cur;
    harmony_on = !harmony_on; hroot_set(0, rn(12)); hdur_set(1, rn(4)); link_src[1] = rn(5); link_retrig[2] = rn(4); harmony_follow[3] ^= 1;
    project_load(slot); adv(40);
    hs_get(&b);
    if (memcmp(a.x, b.x, sizeof a.x)) VIOL("I6 project round trip: SOURCE/EXT/OCT differ");
    i3_epoch++;
    if (!hs_eq(&a, &b)) VIOL("project roundtrip: harmony state differs: on %u->%u roots %u,%u,%u,%u->%u,%u,%u,%u dur %u%u%u%u->%u%u%u%u link %u%u%u%u->%u%u%u%u rt %u%u%u%u->%u%u%u%u fo %u%u%u%u->%u%u%u%u",
        a.on, b.on, a.root[0],a.root[1],a.root[2],a.root[3],b.root[0],b.root[1],b.root[2],b.root[3],
        a.dur[0],a.dur[1],a.dur[2],a.dur[3],b.dur[0],b.dur[1],b.dur[2],b.dur[3],
        a.ls[0],a.ls[1],a.ls[2],a.ls[3],b.ls[0],b.ls[1],b.ls[2],b.ls[3],
        a.lr[0],a.lr[1],a.lr[2],a.lr[3],b.lr[0],b.lr[1],b.lr[2],b.lr[3],
        a.fo[0],a.fo[1],a.fo[2],a.fo[3],b.fo[0],b.fo[1],b.fo[2],b.fo[3]);
}
/* B: real timed MIDI gestures mixed with the same transport/owner stress. */
static void live_gesture(void) {
    uint32_t source=rn(4), follower=(source+1u+rn(3))%4u, root=36u+rn(49), minor=rn(2);
    static const uint8_t arrival[]={0,1,5,12,29,45};
    i3_epoch++;
    trk[source].p[P_CSRC]=0;trk[source].p[P_CHRD]=CH_OFF;trk[source].p[P_QUANT]=QN_OFF;
    trk[follower].p[P_CSRC]=(int16_t)(source+2u);link_retrig[follower]=rn(4);
    harmony_follow[source]=0;song.g[G_ROUTE]=0;song.mch[source]=(uint8_t)(source+1u);
    adv(2);
    uint32_t nn[3]={root,root+(minor?3u:4u),root+7u};
    for(uint32_t k=0;k<3;k++) {web_midi(0x90u|source,nn[k],40u+rn(88));mheld[source][nn[k]]=1;adv(arrival[rn(6)]);}
    adv(32u+rn(20));
    for(uint32_t k=0;k<3;k++) {web_midi(0x80u|source,nn[k],0);mheld[source][nn[k]]=0;}
    adv(2);
}
int main(int argc, char **argv){
    if (getenv("RV_TRACE_FROM") && *getenv("RV_TRACE_FROM")) trace_from = strtoul(getenv("RV_TRACE_FROM"), 0, 10);
    uint64_t seed = argc > 1 ? strtoull(argv[1], 0, 10) : 1;
    unsigned long iters = argc > 2 ? strtoul(argv[2], 0, 10) : 2000;
    uint32_t qevery = argc > 3 ? (uint32_t)strtoul(argv[3], 0, 10) : 150;
    uint32_t projmode = argc > 4 ? (uint32_t)strtoul(argv[4], 0, 10) : 1;
    prof = argc > 5 ? (uint32_t)strtoul(argv[5], 0, 10) : 0;
    rs = seed * 0x9E3779B97F4A7C15ull + 12345;
    web_nor_erase(); web_boot(); web_test_build(); run_ms(1400);   /* (the emulator starts in PLAY like the device; this harness was written for BUILD: random PLAY input fills the HOME capsule until a save says PROJECT TOO LARGE) */
    for (uint32_t t = 0; t < 4; t++) { trk[t].p[P_CHRD] = CH_BUS; trk[t].p[P_VOICE] = V_POLY; }
    harmony_on = 1;
    if (prof >= 2) {   /* A (2): the HARM slice; D (3): the same with RETRIG HARM / BOTH on every track, bars of one chord each: BPM fast, tracks on EXT, SEQ patterns on the followers */
        song.g[G_BPM] = 240; trk[0].p[P_SCALE] = 1;
        for (uint32_t i = 0; i < 4; i++) { hroot_set(i, rn(12)); hdur_set(i, rn(2)); trk[i].p[P_CSRC] = (int16_t)rn(2); trk[i].p[P_CEXT] = (int16_t)rn(3); trk[i].p[P_COCT] = (int16_t)rn(5) - 2; trk[i].p[P_CHRD] = rn(3) ? CH_OFF : CH_BUS;
            for (uint32_t j = 0; j < 16; j++) { step_t *st = &trk[i].step[j]; st->n = (uint8_t)(1 + rn(3)); for (uint32_t m = 0; m < 3; m++) st->note[m] = (uint8_t)(40 + rn(50)); st->time = rn(6) ? ST_NOTE : ST_REST; st->vel = 100; } }
        if (prof == 3) for (uint32_t i = 0; i < 4; i++) { link_retrig[i] = (uint8_t)(1 + rn(3) % 2 * 2); harmony_follow[i] = rn(4) != 0; hdur_set(i, 0); }
        transport_req = 1;
    }
    if (prof == 1) { song.g[G_BPM] = 240; for (uint32_t i = 0; i < 4; i++) { hroot_set(i, rn(12)); hdur_set(i, 0); trk[i].p[P_LATCH] = 1 + rn(2); link_retrig[i] = 1 + rn(3); } trk[0].p[P_SCALE] = 1; transport_req = 1; }
    for (step_no = 0; step_no < iters; step_no++) {
        if (prof == 4 && rn(4) == 0) live_gesture();
        for (uint32_t r = 1 + rn(3); r; r--) {
            switch (rn(prof >= 2 ? 14 : 10)) {
            case 10: case 11: case 12: rand_ext(); break;
            case 13: if (prof == 3 && rn(2) == 0) hold_probe(); else if (rn(2) == 0) i3_probe(); break;
            case 0: case 1: rand_params(); break;
            case 2: case 3: case 4: rand_midi(); break;
            case 5: case 6: rand_keys(); break;
            case 7: rand_ui(); break;
            case 8: rand_transport(); break;
            case 9: if (projmode && rn(12) == 0) rand_project(); break;
            }
        }
        adv(1 + rn(30));
        sane();
        if (qevery && step_no % qevery == qevery - 1) { quiesce(1); sane(); if (getenv("RV_LOGX")) fprintf(stderr, "[seed %llu] step %lu quiesced; viol=%lu\n", (unsigned long long)seed, step_no, viol); }
    }
    quiesce(1);
    if (prof == 4 && !live_commits) VIOL("live profile exercised no recognised chord commits");
    if (prof == 4) printf("LIVE commits %u ambiguous %u\n",live_commits,live_ambiguous);
    printf("FUZZ seed %llu iters %lu violations %lu (I3: %lu shadowed ons, %lu checked offs)\n", (unsigned long long)seed, iters, viol, i3_ons, i3_checked);
    return viol ? 1 : 0;
}
