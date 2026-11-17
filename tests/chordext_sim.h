/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* CHORD SOURCE / EXT host harness: a block-driven event scheduler (header only, no firmware code of its own).
 *
 * It drives the real audio-side control path, one events_block(CTL) per 32-sample block (CTL = 32 at 44100 Hz,
 * 0.7256 ms), and feeds the events that are due BEFORE the block, the way the firmware sees them: the keys are one
 * bit of fm1_in.notes (keyboard_block reads them at the start of the block), USB-MIDI is a midi_enqueue() packet,
 * TRS MIDI is bytes through um_byte() (with running status). Everything that arrives before the same block is
 * "simultaneous" for the firmware.
 *
 * Use: include it after tests/ui_test.c (UI_TEST_NO_MAIN 1), like tests/chord_test.c, tests/latch_test.c and
 * tests/midi_control_test.c do; the scene sets the tracks up with the firmware's own variables, then
 *     ce_reset();  ce_sched(ev, n);  ce_run_until(us);
 *
 * Time model (the only definition; later tests rely on it):
 *   block k starts at start(k) = k * 32 / 44100 s (a fraction of a microsecond; no rounding is stored).
 *   An event with at_us is fed before the FIRST block k with start(k) >= at_us, i.e. at_us * 44100 <= k * 32e6:
 *   an event exactly on a block start is fed before that block, one microsecond later before the next one.
 *   ce_us_of_block(k) is the start of block k in whole microseconds, rounded down: the last microsecond that is still
 *   fed before block k; one microsecond later the event waits for block k + 1. "W-1 / W / W+1 blocks" after block n are
 *   events at ce_us_of_block(n + W - 1 / W / W + 1), fed before exactly those blocks.
 *   ce_run_until(us) runs the blocks that start at or before us, so ce_run_until(ce_us_of_block(n)) leaves
 *   ce_block_no() == n (blocks 0 .. n-1 have run; the event at ce_us_of_block(n) is fed before block n by the next call).
 *   fm1_ms is set from time: at the moment an event is fed to
 *   its arrival time (at_us / 1000, what midi_enqueue stamps), at the block to the block's start / 1000.
 *   Events at the same time keep the order they were scheduled in.
 *
 * Observation: ce_post_block (if set) is called after every block with the block number (the one that just ran)
 * and, when ce_render is 1, the 2 * CTL rendered samples (mix_block(), which runs events_block itself once);
 * with ce_render 0 only events_block(CTL) runs and the audio pointer is 0. ce_fed_block(i) is the block the
 * i-th scheduled event (in scheduling order since ce_reset) was fed before.
 * Needs: events_block, mix_block, fm1_in, fm1_ms, midi_enqueue, um_byte, um, mi_r/mi_w (hostsim.c + seq.c). */
#ifndef CHORDEXT_SIM_H
#define CHORDEXT_SIM_H

enum { CE_KEY, CE_USB, CE_TRS, CE_TRS_RS, CE_TRS_B };
/* kinds and fields:
 *   CE_KEY     a = key 0..26, b = 1 down / 0 up   (one bit of fm1_in.notes)
 *   CE_USB     a = status byte (0x90 | ch ..), b = data 1, c = data 2   (one USB-MIDI packet, source 1)
 *   CE_TRS     a b c = three raw bytes through um_byte (status + 2 data; a status < 0xC0 of 3 bytes)
 *   CE_TRS_RS  b c = two raw data bytes (running status: the status byte came with an earlier CE_TRS)
 *   CE_TRS_B   a = one raw byte (a status byte alone, a realtime byte, or one data byte) */
typedef struct { uint32_t at_us; uint8_t kind, a, b, c; } ce_ev_t;

#define CE_MAX_EV 1024u
static ce_ev_t ce_q[CE_MAX_EV];                    /* the queue, sorted by time, stable */
static uint32_t ce_qn, ce_qr;                      /* events queued, events fed (ce_q[0 .. ce_qr) are fed) */
static uint32_t ce_fed[CE_MAX_EV];                 /* by scheduling index: the block it was fed before (~0u: not yet) */
static uint16_t ce_idx[CE_MAX_EV];                 /* queue position -> scheduling index */
static uint32_t ce_blk;                            /* blocks run since ce_reset */
static int ce_render;                              /* 1: mix_block (events + the audio), 0: events_block only */
static int32_t ce_audio[2u * CTL];
static void (*ce_post_block)(uint32_t blk, const int32_t *audio);

static uint32_t ce_block_no(void) { return ce_blk; }
static uint32_t ce_fed_block(uint32_t i) { return i < CE_MAX_EV ? ce_fed[i] : ~0u; }
/* the start of block k in whole microseconds (rounded down): the last microsecond whose event is still fed before block k
 * (one microsecond later it waits for block k + 1) */
static uint32_t ce_us_of_block(uint32_t k) { return (uint32_t)((uint64_t)k * 32000000u / 44100u); }
static uint32_t ce_block_start_us(uint32_t k) { return ce_us_of_block(k); }

static void ce_reset(void)
{
    uint32_t i;
    ce_qn = ce_qr = ce_blk = 0;
    for (i = 0; i < CE_MAX_EV; i++) ce_fed[i] = ~0u;
    memset(&um, 0, sizeof um);                     /* the TRS parser: no running status */
}

/* queue events (any order; stable by time). Events earlier than the blocks already run are fed at once */
static void ce_sched(const ce_ev_t *ev, uint32_t n)
{
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        if (ce_qn >= CE_MAX_EV) { printf("chordext_sim: queue full\n"); exit(2); }
        j = ce_qn++;
        while (j > ce_qr && ce_q[j - 1].at_us > ev[i].at_us) {        /* insert after the equal ones */
            ce_q[j] = ce_q[j - 1];
            ce_idx[j] = ce_idx[j - 1];
            j--;
        }
        ce_q[j] = ev[i];
        ce_idx[j] = (uint16_t)(ce_qn - 1u);        /* (the scheduling index counts what was scheduled so far) */
    }
}

static void ce_feed(const ce_ev_t *e, uint32_t blk)
{
    fm1_ms = e->at_us / 1000u;                     /* the arrival time (midi_enqueue stamps it) */
    switch (e->kind) {
    case CE_KEY:
        if (e->b) fm1_in.notes |= 1u << e->a; else fm1_in.notes &= ~(1u << e->a);
        break;
    case CE_USB:
        midi_enqueue((uint32_t)(e->a >> 4) | (uint32_t)e->a << 8 | (uint32_t)e->b << 16 | (uint32_t)e->c << 24, 1);
        break;
    case CE_TRS:
        um_byte(e->a); um_byte(e->b); um_byte(e->c);
        break;
    case CE_TRS_RS:
        um_byte(e->b); um_byte(e->c);
        break;
    case CE_TRS_B:
        um_byte(e->a);
        break;
    }
    (void)blk;
}

static void ce_run_until(uint32_t us)
{
    while ((uint64_t)ce_blk * 32000000u <= (uint64_t)us * 44100u) {          /* the block starts by us */
        uint32_t k = ce_blk;
        while (ce_qr < ce_qn && (uint64_t)ce_q[ce_qr].at_us * 44100u <= (uint64_t)k * 32000000u) {
            ce_feed(&ce_q[ce_qr], k);
            ce_fed[ce_idx[ce_qr]] = k;
            ce_qr++;
        }
        fm1_ms = ce_block_start_us(k) / 1000u;
        if (ce_render) {
            memset(ce_audio, 0, sizeof ce_audio);
            mix_block(ce_audio, CTL);
        } else
            events_block(CTL);
        ce_blk = k + 1u;
        if (ce_post_block)
            ce_post_block(k, ce_render ? ce_audio : 0);
    }
}

/* ---------------------------------------------------------------- self-test --- */
/* Proves the time model on a stopped, silent firmware state (no note sounds from these events but one USB note
 * and one key, whose gate is looked at): simultaneous events share a block, events one block apart are fed one
 * block apart, the events at W-1 / W / W+1 blocks land in exactly those blocks (W = 41 and the block 0 edge), an
 * event 1 microsecond after a block start waits for the next block, the three feeds really reach the firmware
 * in the block they are fed in (a key, USB-MIDI, TRS with running status), and fm1_ms follows time.
 * Returns the number of failures (printed). */
static uint32_t ce_cnt_calls, ce_cnt_last, ce_cnt_nonzero, ce_cnt_noaudio;
static void ce_selftest_count(uint32_t blk, const int32_t *a)
{
    ce_cnt_calls++;
    ce_cnt_last = blk;
    if (a) {
        uint32_t j;
        for (j = 0; j < 2u * CTL; j++) ce_cnt_nonzero |= a[j] != 0;
    } else
        ce_cnt_noaudio = 1;
}
static int ce_selftest(void)
{
    enum { W = 41, N0 = 100 };
    int bad = 0;
    uint32_t i;
    ce_ev_t ev[16];
#define CE_T(what, ok) do { if (!(ok)) { printf("chordext_sim self-test FAIL: %s\n", what); bad++; } } while (0)
    ce_render = 0; ce_post_block = 0;
    ce_reset();
    memset(ev, 0, sizeof ev);
    /* simultaneous: three events at the same time, one block */
    for (i = 0; i < 3; i++) ev[i] = (ce_ev_t){ce_us_of_block(N0), CE_USB, 0xB0, 20, (uint8_t)i};
    /* 1-block offset */
    ev[3] = (ce_ev_t){ce_us_of_block(N0 + 1), CE_USB, 0xB0, 21, 0};
    /* W-1, W, W+1 */
    ev[4] = (ce_ev_t){ce_us_of_block(N0 + W - 1), CE_USB, 0xB0, 22, 0};
    ev[5] = (ce_ev_t){ce_us_of_block(N0 + W), CE_USB, 0xB0, 23, 0};
    ev[6] = (ce_ev_t){ce_us_of_block(N0 + W + 1), CE_USB, 0xB0, 24, 0};
    /* 1 us after a block start: the next block; 1 us before: the same block */
    ev[7] = (ce_ev_t){ce_block_start_us(N0 + 50) + 1u, CE_USB, 0xB0, 25, 0};
    ev[8] = (ce_ev_t){ce_us_of_block(N0 + 60) - 1u, CE_USB, 0xB0, 26, 0};   /* 1 us before a block start: that block */
    /* scheduled out of order: the time wins, the order of equal times stays */
    ev[9] = (ce_ev_t){ce_us_of_block(N0 + 5), CE_USB, 0xB0, 27, 0};
    ev[10] = (ce_ev_t){ce_us_of_block(N0 + 5), CE_USB, 0xB0, 28, 0};
    ce_sched(ev + 9, 2);
    ce_sched(ev, 9);
    ce_run_until(ce_us_of_block(N0 + 70));
    CE_T("blocks run", ce_block_no() == N0 + 70u);
    CE_T("the order of equal times is kept (indices 0, 1 before 2)", ce_fed_block(0) == ce_fed_block(1));
    /* scheduling indices: ev 9, 10 were scheduled first (0, 1), then ev 0..8 (2..10) */
    CE_T("simultaneous events: one block", ce_fed_block(2) == N0 && ce_fed_block(3) == N0 && ce_fed_block(4) == N0);
    CE_T("1-block offset: exactly the next block", ce_fed_block(5) == N0 + 1u);
    CE_T("W-1 blocks", ce_fed_block(6) == N0 + W - 1u);
    CE_T("W blocks", ce_fed_block(7) == N0 + W);
    CE_T("W+1 blocks", ce_fed_block(8) == N0 + W + 1u);
    CE_T("1 us after a block start: the next block", ce_fed_block(9) == N0 + 51u);
    CE_T("1 us before a block: that block", ce_fed_block(10) == N0 + 60u);
    CE_T("out-of-order scheduling is fed by time", ce_fed_block(0) == N0 + 5u && ce_fed_block(1) == N0 + 5u);
    CE_T("block edges: ce_us_of_block is the last microsecond fed before the block", ce_us_of_block(0) == 0 &&
         (uint64_t)ce_us_of_block(N0) * 44100u <= (uint64_t)N0 * 32000000u &&
         (uint64_t)(ce_us_of_block(N0) + 1u) * 44100u > (uint64_t)N0 * 32000000u);
    CE_T("1000 blocks last 725.623 ms", ce_us_of_block(1000) == 725623u);       /* 1000 * 32 / 44100 s = 725.62358 ms */
    /* the feeds reach the firmware in their own block: a key, USB, TRS running status */
    ce_reset();
    {
        track_t *t = &trk[0];
        uint32_t seen_key = ~0u, seen_usb = ~0u, seen_trs = ~0u, seen_rs = ~0u, b;
        int gk[4];
        ui_power_on();
        memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord); memset(chord_last, 0, sizeof chord_last);
        fm1_in.notes = kb_prev = 0; mi_r = mi_w = 0; usb.config = 1; fm1_ms = 0; song.sel = 0; song.playing = 0;
        song.g[G_ROUTE] = 0;
        t->p[P_VOICE] = V_POLY; t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_CHRD] = CH_OFF; t->p[P_AMODE] = 0;
        t->p[P_LATCH] = 0; t->p[P_QUANT] = 0; t->p[P_MUTE] = 0;
        events_block(CTL);
        ce_reset();
        ev[0] = (ce_ev_t){ce_us_of_block(10), CE_KEY, 7, 1, 0};                   /* C4 = note 60 */
        ev[1] = (ce_ev_t){ce_us_of_block(20), CE_USB, 0x90, 64, 100};             /* E4 */
        ev[2] = (ce_ev_t){ce_us_of_block(30), CE_TRS, 0x90, 67, 100};             /* G4, TRS, status byte kept */
        ev[3] = (ce_ev_t){ce_us_of_block(31), CE_TRS_RS, 0, 69, 100};             /* A4: running status */
        ce_sched(ev, 4);
        for (b = 0; b < 40u; b++) {
            ce_run_until(ce_us_of_block(b + 1u));
            for (i = 0, gk[0] = gk[1] = gk[2] = gk[3] = 0; i < NVOICE; i++)
                if (t->v[i].active && t->v[i].gate) {
                    gk[0] |= t->v[i].note == 60u; gk[1] |= t->v[i].note == 64u;
                    gk[2] |= t->v[i].note == 67u; gk[3] |= t->v[i].note == 69u;
                }
            if (gk[0] && seen_key == ~0u) seen_key = b;
            if (gk[1] && seen_usb == ~0u) seen_usb = b;
            if (gk[2] && seen_trs == ~0u) seen_trs = b;
            if (gk[3] && seen_rs == ~0u) seen_rs = b;
        }
        CE_T("a key sounds in the block it is fed before", seen_key == 10u);
        CE_T("a USB note sounds in the block it is fed before", seen_usb == 20u);
        CE_T("a TRS note sounds in the block it is fed before", seen_trs == 30u);
        CE_T("a TRS running-status note sounds in the block it is fed before", seen_rs == 31u);
        CE_T("fm1_ms follows time (block 39: 28 ms)", fm1_ms == ce_block_start_us(39) / 1000u && fm1_ms == 28u);
    }
    {   /* with ce_render the audio comes with the block, once per block */
        song.master_q12 = 4096;
        ce_cnt_calls = ce_cnt_nonzero = ce_cnt_noaudio = 0;
        ce_render = 1; ce_post_block = ce_selftest_count;
        ce_run_until(ce_us_of_block(ce_block_no() + 20u));
        CE_T("ce_post_block: once per block, the block number", ce_cnt_calls == 20u && ce_cnt_last == ce_block_no() - 1u);
        CE_T("ce_render: the held notes are audible", !ce_cnt_noaudio && ce_cnt_nonzero);
        ce_render = 0; ce_post_block = 0;
    }
#undef CE_T
    fm1_in.notes = kb_prev = 0;
    return bad;
}

#endif
