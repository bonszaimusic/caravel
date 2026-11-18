/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 *
 * CARAVEL_UX_CAPTURE (enabled in the 1.0 target: plays notes from the block that counts the bars and keeps
 * 4 KB of event rings: 4 tracks x (live ring + loop) x 128 events x 4 bytes): CAPTURE, E8 / E38 / E41 of the decisions log. Each track has a LIVE ring of the notes played
 * (keys and MIDI IN, while the clock runs, raw timing) and one LOOP. REC takes the last 1/2/4/8/16/32 bars of the ring as the
 * loop (an existing loop receives an overdub; E45, the live ring continues); the loop's notes go
 * through input_on / input_off, so ARP, LATCH and HARMONY act on them as on played notes, and are not recorded again.
 * Times are perf_bars_q16 (bars in Q16), which runs only while the clock does. Not saved in the project (E8).
 *
 * ONE OWNER (device plan C-a): the rings, the loops, the phases and the serial numbers are read and written only by the
 * audio context (the key and MIDI inputs, cap_idle at the top of a block, cap_tick). The UI never touches them: it posts
 * commands (cap_post: REC, TOGGLE, WIPE, ALL_PAUSE, QUANT) into a small queue that cap_idle empties at the start of the next
 * block, and takes the outcomes (cap_result, for its message) from a second queue, in the same order. Each queue has one
 * writer and one reader and volatile indices, as transport_req and panic_req.
 *
 * THE DISPLAY (C-b) reads a snapshot, never the live state: once per block (end of cap_idle) the audio context publishes
 * three words per track (cap_pub: state, loop events, ring events and head, bars, phase, quant); the UI takes
 * them with cap_view and then reads the event arrays only (a note being written shows one frame late; cap_gen counts the
 * writes so that the UI can draw again when one fell inside a frame). cap_dring is gone: the band's notes of a running
 * loop are derived from the loop itself (they are the notes as captured, not as HARMONY or CHRD made them). */
#if CARAVEL_UX_CAPTURE
#define CAP_N 128u
#define CAP_HN (CAP_N / 2u)            /* every admitted raw note reserves its end */
/* Four bytes per event. The unsigned bit-fields are local RAM, never serialized or sent via USB.
 * An 18-bit clock at 1/4096 bar wraps every 64 bars: a 32-bar live window is unambiguous.
 * Keeping the previous 1/4096-bar granularity avoids a timing downgrade for short loops.
 * Vel=0 means note off; seven bits suffice for MIDI pitch and velocity (0..127).
 * The compile-time size assertion catches target ABI changes before a firmware image is used. */
typedef struct { uint32_t t : 18; uint32_t note : 7; uint32_t vel : 7; } cap_ev_t;
typedef char cap_ev_size_must_be_4[sizeof(cap_ev_t) == 4 ? 1 : -1];
#define CAP_U(q16) ((uint32_t)(q16) >> 4)                        /* bars in Q16 -> 1/4096 bar */
#define CAP_TMASK ((1u << 18) - 1u)
#define CAP_KEEP_U (32u << 12)                                  /* longest retrospective REC window */
static uint32_t cap_age(uint32_t now_q16, uint32_t t) { return (CAP_U(now_q16) - t) & CAP_TMASK; }
static cap_ev_t cap_ring[NTRK][CAP_N], cap_loop[NTRK][CAP_N];
/* Post-capture QUANT: cap_loop is immutable ORIGINAL timing. A separate
 * playback list holds the currently committed grid, and ONE shared scratch
 * list is prepared in bounded per-block slices. The UI never edits either.
 * Added event memory: (NTRK+1)*CAP_N*4 = 2560 B, plus small job state. */
static cap_ev_t cap_qloop[NTRK][CAP_N], cap_qscratch[CAP_N];
static uint8_t cap_qpair[CAP_N];                  /* scratch ON index -> OFF */
static uint32_t cap_qused[CAP_N / 32u];          /* which raw OFFs were paired */
static uint8_t cap_qactive[NTRK], cap_qwant[NTRK], cap_qready[NTRK], cap_qdirty[NTRK];
static uint8_t cap_qn[NTRK];                   /* active transformed list count: may lag an overdub */
static uint8_t cap_qowner = NTRK, cap_qstage, cap_qpos;
static uint8_t cap_qfailed; /* deferred UI outcome: avoids audio-worker/UI writes */
enum { CAP_QPAIR=1, CAP_QCHECK, CAP_QSORT, CAP_QREADY };
static const uint16_t CAP_GRID[6] = {0u, 256u, 512u, 1024u, 2048u, 4096u};
/* The clock is fixed 4 quarter-note beats/bar: 1/1 = 4096 units = one
 * four-beat bar. All loop lengths are multiples of that bar. */
static volatile uint8_t cap_gen[NTRK];                           /* counts every write to a ring or a loop (audio): the display's check */
static uint8_t cap_rn[NTRK], cap_rh[NTRK];                       /* the ring: events kept, next to write */
static uint8_t cap_full[NTRK], cap_rejected;               /* full/rejected capture: no partial takes */
static uint8_t cap_wait[NTRK];                           /* paused by transport, not by SEQ/PANIC */
static uint8_t cap_ln[NTRK], cap_st[NTRK];                       /* the loop: events; 0 none, 1 running, 2 paused */
static uint32_t cap_len[NTRK], cap_o[NTRK], cap_pph[NTRK];       /* length (Q16 bars), the bar clock at its start, paused phase */
static uint32_t cap_serial[NTRK], cap_taken_serial[NTRK]; /* event watermark: equal-time REC must not copy old notes */
static uint32_t cap_tt[NTRK];                                    /* when the last take (or overdub) was: an overdub adds only what came after */
static int32_t cap_last[NTRK];                                   /* the phase the last block ended at (-1: none yet) */
static uint32_t cap_held[NTRK][4];                               /* the loop's notes sounding (bit per note) */
static uint8_t cap_replay;                                       /* 1 while the loop plays a note: not recorded */
/* The block-time counter (device plan C-e): what CAPTURE costs in the audio block, in timer ticks (FM1_TICKS_PER_US per us), read on
 * the device with the console command "cap" (and reset with "cap reset"). cap_p_cur is the block being run (cap_idle opens it,
 * cap_tick adds to it); the previous one is closed into last / max / sum when the next begins. */
static uint32_t cap_p_cur, cap_p_fired, cap_p_all;               /* this block: ticks, events fired, 1 = all four loops run */
static volatile uint32_t cap_p_last, cap_p_max, cap_p_sum, cap_p_blocks, cap_p_fired_max, cap_p_busy_max, cap_p_take_last, cap_p_take_max, cap_p_takes;
static uint8_t cap_li[NTRK] = {1, 1, 1, 1};                    /* per-owner next-take length, default 2 bars */
static const uint8_t CAP_BARS[6] = {1, 2, 4, 8, 16, 32};
static void input_on(track_t *t, uint32_t note, uint32_t vel);
static void input_off(track_t *t, uint32_t note);
static uint32_t fm1_ticks(void);                                  /* the timer (hal/fm1_time.h; the emulator's own): for the block counter */

static uint32_t cap_owner(uint32_t i)
{
#if FELUCCA_PAIR_MORPH
    return trk_index(pair_owner(&trk[i]));
#else
    return i;
#endif
}
static uint32_t cap_selected(void) { return cap_owner(song.sel); }

/* Observe the monotonic audio clock to distinguish first boot from its Q16 wrap. */
static uint32_t cap_clock_prev;
static uint8_t cap_clock_wrapped;
static uint32_t cap_clock_now(void)
{
    uint32_t now=perf_bars_q16;
    if(now<cap_clock_prev)cap_clock_wrapped=1;
    cap_clock_prev=now;
    return now;
}
static uint32_t cap_open[NTRK][4];                               /* the notes sounding in the live ring (bit per note): each gets its end */
static void cap_log(uint32_t i, uint32_t note, uint32_t vel)
{
    cap_ev_t *e = &cap_ring[i][cap_rh[i]];
    cap_serial[i]++;
    cap_gen[i]++;
    e->t = CAP_U(cap_clock_now()) & CAP_TMASK;
    e->note = (uint8_t)note;
    e->vel = (uint8_t)vel;
    cap_rh[i] = (uint8_t)((cap_rh[i] + 1u) % CAP_N);
    if (cap_rn[i] < CAP_N)
        cap_rn[i]++;
}
static void cap_put(uint32_t i, uint32_t note, uint32_t vel)
{
    uint32_t w = (note >> 5) & 3u, b = 1u << (note & 31);
    if (cap_replay || i >= NTRK)
        return;
    if (vel) {                                                   /* a note begins: only while the clock runs */
        if (!song.playing)
            return;
        if (cap_open[i][w] & b)                                  /* (again without an end: the old one ends first) */
            cap_log(i, note, 0);
        cap_open[i][w] |= b;
        cap_log(i, note, vel);
    } else if (cap_open[i][w] & b) {                             /* its end: always, also after the clock stopped */
        cap_open[i][w] &= ~b;
        cap_log(i, note, 0);
    }
}
static void cap_close_live(uint32_t i)                           /* every sounding live note ends (STOP, PANIC: their key-ups may never come) */
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if (cap_open[i][n >> 5] & (1u << (n & 31)))
            cap_log(i, n, 0);
    memset(cap_open[i], 0, sizeof cap_open[i]);
}
static const cap_ev_t *cap_ring_at(uint32_t i, uint32_t k)       /* k-th oldest */
{
    return &cap_ring[i][(cap_rh[i] + CAP_N - cap_rn[i] + k) % CAP_N];
}
/* The loop plays RAW notes (the keys as pressed, E8); HARMONY, the chord and ARP / LATCH act on them as it plays, as on keys:
 * the note follows the HARMONY chord of the moment (as key_on does), CHRD builds its chord, input_on does the rest. What a note
 * became is kept (cap_hn) so its end ends the same notes, and cap_held / the band show the notes that sound. */
typedef struct { uint8_t raw, n, note[CHORD_MAX]; } cap_hn_t;
static cap_hn_t cap_hn[NTRK][CAP_HN];
static uint32_t cap_latched[NTRK][4]; /* released loop pitches still sounding through LATCH */
static uint8_t cap_raw[27];                                      /* the key's note as pressed, before HARMONY (seq.c key_on) */
static uint8_t cap_skip;                                         /* 1 while key_off ends the notes of a key (it logged the raw end itself) */
static int cap_note_held(const track_t *t,uint32_t note)
{
    return (cap_held[trk_index(t)][note>>5]>>(note&31u))&1u;
}
static int cap_mapping_held(uint32_t i,uint32_t note)
{
    uint32_t j,k;
    for(j=0;j<CAP_HN;j++)for(k=0;k<cap_hn[i][j].n;k++)
        if(cap_hn[i][j].note[k]==note)return 1;
    return 0;
}
static void cap_play_on(uint32_t i, uint32_t raw, uint32_t vel)
{
    track_t *t = &trk[i];
    cap_hn_t *h = 0;
    uint8_t ch[CHORD_MAX];
    uint32_t n, j, note = raw;
    for (j = 0; j < CAP_HN; j++)
        if (!cap_hn[i][j].n) { h = &cap_hn[i][j]; break; }
    if (!h) return;                      /* cannot occur for an admitted <=64-event loop */
    /* E8: each new loop set releases the old latched set, even in STACK.
     * Still-held live/MIDI/loop owners are protected by the normal input guards. */
    if(latch_mode(t)!=LT_OFF) {
        if(!t->p[P_AMODE]) {
            latch_free(t);
            memset(cap_latched[i],0,sizeof cap_latched[i]);
        } else if(!t->arp_phys) {
            t->nheld=0;
        }
    }
    if (harmony_on && harmony_follow[i] && t->p[P_CHRD] != CH_BUS && !chord_kit(t))
        note = harmony_snap(t, note);
    cap_replay = 1; /* harmonic mapping is replay too: it cannot teach a live CHORD source */
    n = chord_build(t, note, ch);
    if (n > CHORD_MAX) n = CHORD_MAX;
    h->raw = (uint8_t)raw;
    h->n = (uint8_t)n;
    for (j = 0; j < n; j++) {
        uint32_t x=ch[j],w=x>>5,b=1u<<(x&31u);
        h->note[j] = ch[j];
        if(!(cap_held[i][w]&b)) {
            if(!midi_local_held(t,x) && !midi_note_held(t,x)) input_on(t,x,vel);
        }
        cap_held[i][w]|=b;
        cap_latched[i][w]&=~b;
    }
    cap_replay = 0;
}
static void cap_play_off(uint32_t i, uint32_t raw)
{
    uint32_t j, k;
    for (j = 0; j < CAP_HN; j++)
        if (cap_hn[i][j].n && cap_hn[i][j].raw == raw) {
            uint32_t n=cap_hn[i][j].n;
            cap_hn[i][j].n=0;             /* other mapped loop inputs can still own the same pitch */
            cap_replay = 1;
            for (k = 0; k < n; k++) {
                uint32_t x = cap_hn[i][j].note[k],w=x>>5,b=1u<<(x&31u);
                if(!cap_mapping_held(i,x)) {
                    cap_held[i][w]&=~b;
                    input_off(&trk[i],x);
                    if(!midi_local_held(&trk[i],x) && !midi_note_held(&trk[i],x) && lat_get(lat_stay[i],x))
                        cap_latched[i][w]|=b;
                }
            }
            cap_replay = 0;
            return;
        }
}
static void cap_fire(uint32_t i, const cap_ev_t *e)
{
    cap_p_fired++;
    if (e->vel)
        cap_play_on(i, e->note, e->vel);
    else
        cap_play_off(i, e->note);
}
static void cap_release(uint32_t i)                              /* end only this loop's input ownership */
{
    uint32_t n,held[4],latched[4];
    track_t *t=&trk[i];
    memcpy(held,cap_held[i],sizeof held);memcpy(latched,cap_latched[i],sizeof latched);
    memset(cap_held[i],0,sizeof cap_held[i]);memset(cap_hn[i],0,sizeof cap_hn[i]);
    memset(cap_latched[i],0,sizeof cap_latched[i]);
    cap_replay = 1;
    for (n = 0; n < 128u; n++) {
        uint32_t w=n>>5,b=1u<<(n&31u);
        if(held[w]&b) input_off(t,n);
        if(((held[w]|latched[w])&b) && !midi_local_held(t,n) && !midi_note_held(t,n)) {
            arp_drop(t,n);
            lat_put(lat_stay[i],n,0);lat_put(lat_bus_other[i],n,0);
            trk_note_off(t,n);
        }
    }
    if(t->p[P_AMODE] && !t->nheld)arp_silence(t);
    cap_replay = 0;
}
/* All quantisation planning is audio-owned, bounded to at most two source
 * events per audio block. There is no copying, sorting or note pairing in
 * the UI; no unbounded insertion sort in the audio callback. The one scratch
 * buffer remains reserved while a completed job waits for a loop boundary.
 * Other tracks' requests queue and are processed after it is committed. */
static void cap_quant_abort(uint32_t i)
{
    if (cap_qowner == i) { cap_qowner = NTRK; cap_qstage = 0u; }
    cap_qready[i] = 0u;
}
static void cap_quant_dirty(uint32_t i)                 /* original loop changed by a successful overdub */
{
    if (!cap_qwant[i] && !cap_qactive[i]) return;
    cap_quant_abort(i);
    cap_qdirty[i] = cap_qwant[i] != 0u;
    cap_qready[i] = cap_qwant[i] == 0u;
}
static uint32_t cap_quant_request(uint32_t i, uint32_t mode)
{
    if (i >= NTRK || mode > 5u || !cap_ln[i] || !cap_len[i]) return 0u;
    cap_quant_abort(i);
    cap_qwant[i] = (uint8_t)mode;
    cap_qdirty[i] = mode != 0u;
    cap_qready[i] = mode == 0u;
    cap_gen[i]++;
    return 1u;
}
static void cap_quant_fail(uint32_t i)
{
    /* No partial transformed loop is ever published. On an ambiguous
     * overlap, schedule original timing at the next boundary instead. */
    cap_qwant[i] = 0u;
    cap_qready[i] = 1u;
    cap_qdirty[i] = 0u;
    cap_qowner = NTRK;
    cap_qstage = 0u;
    cap_qfailed = 1u;
    cap_gen[i]++;
}
static void cap_quant_apply(uint32_t i)
{
    uint32_t changed = cap_qactive[i] != cap_qwant[i];
    if (!cap_qready[i]) return;
    if (cap_qwant[i]) {
        if (cap_qowner != i || cap_qstage != CAP_QREADY) return;
        /* The source loop cannot change while a ready result is waiting:
         * overdub cancels this ready result in cap_quant_dirty(). */
        memcpy(cap_qloop[i], cap_qscratch, cap_ln[i] * sizeof(cap_ev_t));
        cap_qn[i] = cap_ln[i];
        cap_qowner = NTRK;
        cap_qstage = 0u;
        changed = 1u; /* source update even if grid setting is unchanged */
    }
    if (changed && cap_st[i] == 1u)
        cap_release(i);       /* one safe transition release, never a hanging old note */
    cap_qactive[i] = cap_qwant[i];
    cap_qready[i] = 0u;
    cap_gen[i]++;
}
static uint32_t cap_quant_overlap(uint32_t a, uint32_t da,
                                  uint32_t b, uint32_t db, uint32_t len)
{
    /* Circular half-open intervals [ON,OFF); touching endpoints are fine.
     * A later event with the same pitch cannot share a held raw-note owner. */
    /* CAPTURE lengths are 1/2/4/8/16/32 bars, thus always powers
     * of two in CAP_U. Use a mask, never integer DIV in the ISR worker. */
    return (((b + len - a) & (len - 1u)) < da) ||
           (((a + len - b) & (len - 1u)) < db);
}
static uint32_t cap_quant_before(cap_ev_t a, cap_ev_t b)
{
    if (a.t != b.t) return a.t < b.t;
    if (!!a.vel != !!b.vel) return !a.vel; /* release first for adjacent same-pitch notes */
    return 0u;                              /* stable order between identical-time events */
}
static void cap_quant_work(void)
{
    uint32_t i = cap_qowner, n, len, k;
    if (i == NTRK) {
        for (i = 0; i < NTRK; i++)
            if (cap_qdirty[i] && cap_qwant[i] && cap_ln[i] && cap_len[i])
                break;
        if (i == NTRK) return;
        cap_qowner = (uint8_t)i;
        cap_qdirty[i] = 0;
        cap_qstage = CAP_QPAIR;
        cap_qpos = 0;
        memset(cap_qpair, 255, sizeof cap_qpair);
        memset(cap_qused, 0, sizeof cap_qused);
        memcpy(cap_qscratch, cap_loop[i], cap_ln[i] * sizeof(cap_ev_t));
    }
    if (cap_qstage == CAP_QREADY) return;
    n = cap_ln[i];
    len = CAP_U(cap_len[i]);
    if (!n || !len || !cap_qwant[i]) { cap_quant_abort(i); return; }
    if (len & (len - 1u)) { cap_quant_fail(i); return; } /* unsupported loop length */
    /* Budget max TWO event operations per 128-frame audio block. Even
     * worst-case matching/sorting is at most 2*128 comparisons/moves. */
    for (k = 0; k < 2u && cap_qpos < n; k++, cap_qpos++) {
        uint32_t idx = cap_qpos;
        if (cap_qstage == CAP_QPAIR) {
            const cap_ev_t *e = &cap_loop[i][idx];
            uint32_t j, match = CAP_N, step, new_on, duration;
            if (!e->vel) continue;
            for (j = 1u; j <= n; j++) {
                uint32_t x = (idx + j) % n;
                if (cap_loop[i][x].note != e->note) continue;
                if (cap_loop[i][x].vel) { cap_quant_fail(i); return; }
                match = x; break;
            }
            if (match >= n || (cap_qused[match >> 5] & (1u << (match & 31u)))) {
                cap_quant_fail(i); return;
            }
            duration = (cap_loop[i][match].t + len - e->t) & (len - 1u);
            if (!duration) { cap_quant_fail(i); return; } /* no zero-length release can be made safe */
            cap_qused[match >> 5] |= 1u << (match & 31u);
            cap_qpair[idx] = (uint8_t)match;
            step = CAP_GRID[cap_qwant[i]];
            new_on = ((e->t + step / 2u) / step * step) & (len - 1u);
            cap_qscratch[idx].t = new_on;
            cap_qscratch[match].t = (new_on + duration) & (len - 1u);
        } else if (cap_qstage == CAP_QCHECK) {
            uint32_t j, off = cap_qpair[idx], d;
            if (!cap_loop[i][idx].vel) {
                if (!(cap_qused[idx >> 5] & (1u << (idx & 31u)))) { cap_quant_fail(i); return; }
                continue;
            }
            if (off >= n) { cap_quant_fail(i); return; }
            d = (cap_qscratch[off].t + len - cap_qscratch[idx].t) & (len - 1u);
            for (j = idx + 1u; j < n; j++) {
                uint32_t oj = cap_qpair[j], dj;
                if (!cap_loop[i][j].vel || cap_loop[i][j].note != cap_loop[i][idx].note) continue;
                if (oj >= n) { cap_quant_fail(i); return; }
                dj = (cap_qscratch[oj].t + len - cap_qscratch[j].t) & (len - 1u);
                if (cap_quant_overlap(cap_qscratch[idx].t, d, cap_qscratch[j].t, dj, len)) {
                    cap_quant_fail(i); return;
                }
            }
        } else { /* CAP_QSORT: bounded stable insertion, at most 128 moves */
            cap_ev_t key = cap_qscratch[idx];
            uint32_t j = idx;
            while (j && cap_quant_before(key, cap_qscratch[j - 1u])) {
                cap_qscratch[j] = cap_qscratch[j - 1u];
                j--;
            }
            cap_qscratch[j] = key;
        }
    }
    if (cap_qpos >= n) {
        cap_qpos = 0u;
        if (cap_qstage == CAP_QPAIR) cap_qstage = CAP_QCHECK;
        else if (cap_qstage == CAP_QCHECK) cap_qstage = CAP_QSORT;
        else {
            cap_qstage = CAP_QREADY;
            cap_qready[i] = 1u;  /* do NOT switch mid-loop */
            cap_gen[i]++;
        }
    }
}
/* REC: the notes played live become the loop. No loop yet: the last nb bars of the ring, a loop starting now. A loop already
 * there: OVERDUB (E45): the notes played live during its last length are added where they fall in the loop (its phase), the
 * length stays. Returns the new notes (0: nothing played in the window); *over = 1 for an overdub. */
static uint32_t cap_take(uint32_t i, uint32_t nb, uint32_t *over)
{
    /* The 18-bit time field wraps every 64 bars; up to 32 bars fit without ambiguity.
     * Difference and phase are computed modulo the 18-bit clock, then modulo the loop. */
    uint32_t now = cap_clock_now(), k, n = 0, ons = 0, open[4] = {0, 0, 0, 0}, len, origin, window, now_u, len_u, origin_u, window_u, relnow;
    int add = cap_ln[i] && cap_len[i];
    cap_ev_t tmp[CAP_N];
    uint32_t pending = 0;
    cap_rejected = 0;
    *over = (uint32_t)add;
    if (add) {
        if(cap_serial[i]==cap_taken_serial[i])return 0;
        len = cap_len[i];
        origin = cap_st[i] == 2u ? now - cap_pph[i] : cap_o[i];
        memcpy(tmp, cap_loop[i], cap_ln[i] * sizeof tmp[0]);
        n = cap_ln[i];
    } else {
        len = nb << 16;
        origin = !cap_clock_wrapped && now < len ? 0u : now - len;
    }
    window = !cap_clock_wrapped && now < len ? now : len;
    if (add && now - cap_tt[i] < window)
        window = now - cap_tt[i];
    now_u = CAP_U(now); len_u = CAP_U(len); origin_u = CAP_U(origin); window_u = CAP_U(window);
    relnow = add ? (now_u - origin_u) % len_u : len_u - 1u;
    {   /* a note struck BEFORE the window and still sounding at its start (held, or let go inside it) goes in from the start (E49) */
        uint32_t pre[4] = {0, 0, 0, 0}, seen[4] = {0, 0, 0, 0}, relmin = ((now_u - window_u) - origin_u) % len_u;
        uint8_t pv[128];
        memset(pv, 100, sizeof pv);
        for (k = 0; k < cap_rn[i]; k++) {
            const cap_ev_t *e = cap_ring_at(i, k);
            if(add && (int32_t)(cap_serial[i]-cap_rn[i]+k+1u-cap_taken_serial[i])<=0)continue;
            uint32_t w = e->note >> 5, b = 1u << (e->note & 31);
            if (cap_age(now, e->t) <= window_u) {
                if (e->vel)
                    seen[w] |= b;
                continue;
            }
            if (e->vel) {
                pre[w] |= b;
                pv[e->note & 127u] = e->vel;
            } else {
                pre[w] &= ~b;
            }
        }
        for (k = 0; k < 128u; k++) {                             /* (held now, its begin lost from the ring: it counts too) */
            uint32_t w = k >> 5, b = 1u << (k & 31);
            if (!add && (cap_open[i][w] & b) && !(seen[w] & b))
                pre[w] |= b;
        }
        for (k = 0; k < 128u; k++)
            if (pre[k >> 5] & (1u << (k & 31))) {
                if (n + pending + 2u > CAP_N) goto full;
                pending++;
                tmp[n++] = (cap_ev_t){relmin, (uint8_t)k, pv[k]};
                open[k >> 5] |= 1u << (k & 31);
                ons++;
            }
    }
    for (k = 0; k < cap_rn[i]; k++) {
        const cap_ev_t *e = cap_ring_at(i, k);
        uint32_t w = e->note >> 5, b = 1u << (e->note & 31), rel;
        if ((add && (int32_t)(cap_serial[i]-cap_rn[i]+k+1u-cap_taken_serial[i])<=0) || cap_age(now, e->t) > window_u)
            continue;
        rel = ((e->t - origin_u) & CAP_TMASK) % len_u;
        if (e->vel) {
            if (n + pending + 2u > CAP_N) goto full;
            if (!(open[w] & b)) pending++;
            tmp[n++] = (cap_ev_t){rel, e->note, e->vel};
            open[w] |= b;
            ons++;
        } else if (open[w] & b) {
            tmp[n++] = (cap_ev_t){rel, e->note, 0};
            open[w] &= ~b;
            pending--;
        }
    }
    for (k = 0; k < 128u && n < CAP_N; k++)                      /* a note still held: ends with the window */
        if (open[k >> 5] & (1u << (k & 31)))
            tmp[n++] = (cap_ev_t){relnow, (uint8_t)k, 0};
    if (!ons)
        return 0;
    /* Stable phase order also keeps overdubs chronological. No audio-block sort. */
    for(k=1;k<n;k++) {
        cap_ev_t e=tmp[k];uint32_t j=k;
        while(j && tmp[j-1u].t>e.t) {tmp[j]=tmp[j-1u];j--;}
        tmp[j]=e;
    }
    cap_gen[i]++;
    memcpy(cap_loop[i], tmp, n * sizeof tmp[0]);
    cap_ln[i] = (uint8_t)n;
    cap_full[i] = n == CAP_N;
    cap_tt[i] = now;
    cap_taken_serial[i]=cap_serial[i];
    cap_quant_dirty(i);   /* new overdubs retain raw timing and rebuild only playback */
    if (!add) {
        cap_release(i);
        cap_len[i] = len;
        cap_o[i] = now;
        cap_last[i] = -1;
        cap_st[i] = 1;
    }
    return ons;
full:
    cap_full[i] = cap_rejected = 1;
    return 0;                       /* old loop, take time and origin untouched */
}
static void cap_pause(uint32_t i)
{
    cap_wait[i]=0;
    if (cap_st[i] == 1u) {
        cap_pph[i] = (perf_bars_q16 - cap_o[i]) % cap_len[i];
        cap_release(i);
        cap_st[i] = 2;
    }
}
static void cap_resume(uint32_t i)
{
    cap_wait[i]=0;
    if (cap_st[i] == 2u) {
        cap_o[i] = perf_bars_q16 - cap_pph[i];
        cap_last[i] = -1;
        cap_st[i] = 1;
    }
}
static void cap_clear(uint32_t i)
{
    cap_release(i);
    cap_gen[i]++;
    cap_ln[i] = 0;
    cap_full[i] = cap_wait[i] = 0;
    cap_st[i] = 0;
    cap_quant_abort(i);
    cap_qactive[i] = cap_qwant[i] = cap_qready[i] = cap_qdirty[i] = 0u;
    cap_qn[i] = 0u;
}
/* each block, with the bar clock: the loops' notes that fall since the last block */
static void cap_tick(void)
{
    uint32_t i, k, now = cap_clock_now(), t_in = fm1_ticks();
    for (i = 0; i < NTRK; i++) {
        uint32_t len, ph;
        int32_t last;
        if (cap_st[i] != 1u || !cap_ln[i] || !cap_len[i])
            continue;
        len = cap_len[i];
        ph = CAP_U((now - cap_o[i]) % len);                      /* the place in the loop, in 1/4096 bar like the events */
        last = cap_last[i];
        /* A wrapped block crosses the old tail, then commits the new grid
         * at the boundary, then plays the new head. Never switch mid-cycle. */
        if (last >= 0 && ph < (uint32_t)last) {
            const cap_ev_t *tail = cap_qactive[i] ? cap_qloop[i] : cap_loop[i];
            uint32_t tn = cap_qactive[i] ? cap_qn[i] : cap_ln[i];
            for (k = 0; k < tn; k++)
                if (tail[k].t > (uint32_t)last) cap_fire(i, &tail[k]);
            cap_quant_apply(i);
        }
        {   const cap_ev_t *src = cap_qactive[i] ? cap_qloop[i] : cap_loop[i];
            uint32_t nn = cap_qactive[i] ? cap_qn[i] : cap_ln[i];
            for (k = 0; k < nn; k++) {
                uint32_t r = src[k].t;
                if (r <= ph && (last < 0 || ph < (uint32_t)last || r > (uint32_t)last))
                    cap_fire(i, &src[k]);
            }
        }
        cap_last[i] = (int32_t)ph;
    }
    cap_p_cur += fm1_ticks() - t_in;
}
static void cap_all_pause(void) { uint32_t i; for (i = 0; i < NTRK; i++) { cap_pause(i); cap_close_live(i); } }
static void cap_wipe(uint32_t i)                                 /* SEQ held: the loop and the live notes are gone */
{
    cap_clear(i);
    cap_gen[i]++;
    cap_rn[i] = 0;
    cap_rh[i] = 0;
    memset(cap_open[i], 0, sizeof cap_open[i]);
}
/* ---- the UI's commands to the audio context, and the outcomes back (device plan C-a) ---- */
enum { CAP_OP_REC = 1, CAP_OP_TOGGLE, CAP_OP_WIPE, CAP_OP_ALL_PAUSE, CAP_OP_QUANT };
enum { CAPR_NONE, CAPR_CAPTURED, CAPR_OVERDUB, CAPR_NOTHING, CAPR_NOTHING_ADD, CAPR_FULL, CAPR_PAUSED, CAPR_RUNS,
       CAPR_NO_LOOP, CAPR_NO_CLOCK, CAPR_CLEARED, CAPR_QUANT_NEXT, CAPR_QUANT_OFF,
       CAPR_QUANT_CONFLICT };
#define CAP_Q 8u                                                 /* a power of two */
static volatile uint16_t cap_cq[CAP_Q];                          /* op | track << 4 | bars << 8 */
static volatile uint8_t cap_cq_w, cap_cq_r;                      /* UI writes cap_cq_w, audio writes cap_cq_r */
static volatile uint8_t cap_rq[CAP_Q];                           /* the outcomes, in the order of the commands */
static volatile uint8_t cap_rq_w, cap_rq_r;                      /* audio writes cap_rq_w, UI writes cap_rq_r */
/* UI: 1 = posted, 0 = the queue is full (cannot happen at the speed of buttons; the caller says so) */
static uint32_t cap_post(uint32_t op, uint32_t track, uint32_t bars)
{
    uint8_t w = cap_cq_w;
    if ((uint8_t)(w - cap_cq_r) >= CAP_Q)
        return 0;
    cap_cq[w & (CAP_Q - 1u)] = (uint16_t)(op | track << 4 | bars << 8);
    cap_cq_w = (uint8_t)(w + 1u);
    return 1;
}
/* UI: the next outcome (CAPR_*), 0 = none */
static uint32_t cap_result(void)
{
    uint8_t r = cap_rq_r;
    uint32_t code;
    if (r == cap_rq_w)
        return CAPR_NONE;
    code = cap_rq[r & (CAP_Q - 1u)];
    cap_rq_r = (uint8_t)(r + 1u);
    return code;
}
static void cap_answer(uint32_t code)
{
    uint8_t w = cap_rq_w;
    if ((uint8_t)(w - cap_rq_r) >= CAP_Q)
        return;                                                  /* (a message nobody took: the oldest stays) */
    cap_rq[w & (CAP_Q - 1u)] = (uint8_t)code;
    cap_rq_w = (uint8_t)(w + 1u);
}
/* audio, the top of a block: every posted command, in order */
static void cap_commands(void)
{
    while (cap_cq_r != cap_cq_w) {
        uint32_t c = cap_cq[cap_cq_r & (CAP_Q - 1u)], op = c & 15u, i = (c >> 4) & 15u, nb = c >> 8;
        cap_cq_r = (uint8_t)(cap_cq_r + 1u);
        if (op == CAP_OP_ALL_PAUSE) {
            cap_all_pause();
        } else if (i >= NTRK) {
            continue;
        } else if (op == CAP_OP_REC) {
            uint32_t over = 0, ons;
            if (!song.playing) {
                cap_answer(CAPR_NO_CLOCK);
                continue;
            }
            {   uint32_t t0 = fm1_ticks(), d;
                ons = cap_take(i, nb, &over);
                d = fm1_ticks() - t0;
                cap_p_take_last = d; cap_p_takes++;
                if (d > cap_p_take_max) cap_p_take_max = d;
            }
            cap_answer(ons ? (over ? CAPR_OVERDUB : CAPR_CAPTURED) : cap_rejected ? CAPR_FULL : over ? CAPR_NOTHING_ADD : CAPR_NOTHING);
        } else if (op == CAP_OP_TOGGLE) {
            if (!cap_st[i]) {
                cap_answer(CAPR_NO_LOOP);
            } else if (cap_st[i] == 1u) {
                cap_pause(i);
                cap_answer(CAPR_PAUSED);
            } else if (!song.playing) {
                cap_answer(CAPR_NO_CLOCK);
            } else {
                cap_resume(i);
                cap_answer(CAPR_RUNS);
            }
        } else if (op == CAP_OP_WIPE) {
            cap_wipe(i);
            cap_answer(CAPR_CLEARED);
        } else if (op == CAP_OP_QUANT) {
            if (!cap_quant_request(i, nb))
                cap_answer(CAPR_NO_LOOP);
            else
                cap_answer(nb ? CAPR_QUANT_NEXT : CAPR_QUANT_OFF);
        }
    }
}
/* ---- the snapshot for the display (device plan C-b) ---- */
typedef struct { volatile uint32_t w0, w1, w2; } cap_pub_t;
static cap_pub_t cap_pub[NTRK];
typedef struct { uint8_t st, full, ln, rn, rh, bars, wrapped;
                 uint8_t quant, requested, quant_pending; uint32_t o; } cap_view_t;   /* o: the bar clock at the loop's start (running) or its phase (paused) */
/* audio, once per block, after the commands and the idle logic */
static void cap_publish(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        uint32_t bars = cap_len[i] >> 16, code = 0;
        uint32_t w0;
        while (bars > 1u) { bars >>= 1; code++; } /* only 1/2/4/8/16/32 are admitted */
        /* 2 state + 1 full + 8 loop + 8 live + 7 head + 3 bars-code + 1 wrap = 30 bits */
        w0 = (uint32_t)cap_st[i] | (uint32_t)(cap_full[i] & 1u) << 2 | (uint32_t)cap_ln[i] << 3 |
             (uint32_t)cap_rn[i] << 11 | (uint32_t)cap_rh[i] << 19 |
             (code & 7u) << 26 | (uint32_t)(cap_clock_wrapped & 1u) << 29;
        cap_pub[i].w2 = (uint32_t)cap_qactive[i] | (uint32_t)cap_qwant[i] << 3 |
                         (uint32_t)(cap_qready[i] || cap_qdirty[i] || cap_qowner == i) << 6;
        cap_pub[i].w1 = cap_st[i] == 2u ? cap_pph[i] : cap_o[i];
        cap_pub[i].w0 = w0;
    }
}
/* UI: the track's snapshot (a retry when a block published while it was read) */
static void cap_view(uint32_t i, cap_view_t *v)
{
    uint32_t a, b, q, n = 0;
    do {
        a = cap_pub[i].w0;
        b = cap_pub[i].w1;
        q = cap_pub[i].w2;
    } while (a != cap_pub[i].w0 && ++n < 4u);
    v->st = (uint8_t)(a & 3u);
    v->full = (uint8_t)((a >> 2) & 1u);
    v->ln = (uint8_t)((a >> 3) & 255u);
    v->rn = (uint8_t)((a >> 11) & 255u);
    v->rh = (uint8_t)((a >> 19) & 127u);
    v->bars = v->ln ? (uint8_t)(1u << ((a >> 26) & 7u)) : 0u;
    v->wrapped = (uint8_t)((a >> 29) & 1u);
    v->quant = (uint8_t)(q & 7u);
    v->requested = (uint8_t)((q >> 3) & 7u);
    v->quant_pending = (uint8_t)((q >> 6) & 1u);
    v->o = b;
}
static const cap_ev_t *cap_view_ring(const cap_view_t *v, uint32_t i, uint32_t k)   /* k-th oldest of the viewed ring */
{
    return &cap_ring[i][(v->rh + CAP_N - v->rn + k) % CAP_N];
}
static void cap_idle_body(void)                                  /* the clock stopped: the loops' notes end, they wait */
{
    static uint8_t was;
    uint32_t i;
    if (song.playing) {
        was = 1;
        for(i=0;i<NTRK;i++)if(cap_wait[i] && cap_st[i]==2u)cap_resume(i);
        return;
    }
    if (was) {                                                   /* (once, as it stops: the live notes held end here) */
        was = 0;
        for (i = 0; i < NTRK; i++)
            cap_close_live(i);
    }
    for (i = 0; i < NTRK; i++)
        if (cap_st[i] == 1u) {
            cap_pause(i);
            cap_wait[i]=1;
        }
}
static void cap_trim(void)                                       /* audio: retain up to 32 bars in the 18-bit (64-bar modulo) live clock */
{
    uint32_t i, now = perf_bars_q16;
    for (i = 0; i < NTRK; i++) {
        uint8_t n = cap_rn[i];
        while (cap_rn[i] && cap_age(now, cap_ring_at(i, 0)->t) > CAP_KEEP_U)
            cap_rn[i]--;
        if (cap_rn[i] != n)
            cap_gen[i]++;
    }
}
static void cap_idle(void)                                       /* audio, at the top of every block */
{
    uint32_t t0 = fm1_ticks(), i, busy = 0;
    if (cap_p_blocks | cap_p_cur) {                                /* the previous block is over: close its numbers */
        cap_p_last = cap_p_cur;
        if (cap_p_cur > cap_p_max) cap_p_max = cap_p_cur;
        cap_p_sum += cap_p_cur;
        if (cap_p_fired > cap_p_fired_max) cap_p_fired_max = cap_p_fired;
        if (cap_p_all && cap_p_cur > cap_p_busy_max) cap_p_busy_max = cap_p_cur;   /* (the block the plan is about: all four loops running) */
    }
    cap_p_blocks++;
    cap_p_cur = 0;
    cap_p_fired = 0;
    cap_trim();
    cap_commands();
    cap_idle_body();
    cap_quant_work();
    if (cap_qfailed) {
        cap_answer(CAPR_QUANT_CONFLICT);
        cap_qfailed = 0u;
    }
    /* Paused loops have no next audible loop boundary. Apply when staged,
     * without modifying their saved phase or sounding a note. */
    for (i = 0; i < NTRK; i++)
        if (cap_st[i] == 2u) cap_quant_apply(i);
    cap_publish();
    for (i = 0; i < NTRK; i++)
        busy += cap_st[i] == 1u;
    cap_p_all = busy == NTRK;
    cap_p_cur += fm1_ticks() - t0;
}
#define cap_key_on(k, t) do { cap_raw[k] = kb_note[k]; cap_put(trk_index(t), kb_note[k], kb_vel); } while (0)
#define cap_key_off(k, t) do { cap_put(trk_index(t), cap_raw[k], 0); cap_skip = 1; } while (0)
#define cap_key_done() (cap_skip = 0)
#else
#define cap_put(i, n, v) ((void)0)
#define cap_tick() ((void)0)
#define cap_idle() ((void)0)
#define cap_key_on(k, t) ((void)0)
#define cap_key_off(k, t) ((void)0)
#define cap_key_done() ((void)0)
#define cap_skip 0
#endif
