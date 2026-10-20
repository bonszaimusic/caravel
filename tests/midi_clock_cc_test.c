/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* TRS MIDI clock (24 ppqn) with a controller sharing the wire: the tempo the firmware measures, the BPM it
 * shows and the sequencer's position against the wall clock.
 *
 * The sender is modelled at the byte level (31250 baud, 10 bit times = 320 us per byte): a CC (status, 2
 * data bytes; optionally with running status) occupies the wire, and a Clock byte that falls due meanwhile
 * either waits for the whole message ("message" mode) or slips in after the byte in flight ("byte" mode,
 * realtime inside a running-status CC). The receiver is the real chain: simulated RX DMA ring, the real
 * TIMER5 ISR (poll at 2 kHz, also while the audio ISR renders), um_byte, midi_enqueue (1 ms fm1_ms stamps),
 * events_block with the real consumer (midi_clock_pulse / midi_clock_advance) and the sequencer.
 *
 * Measured per scenario:
 *   stamp   spread (max - min, ms) of the Clock stamps against the ideal pulse time, once a constant is removed
 *   bpm     min .. max of the tempo the clock measured (60 * FS / midi_beat_samples) and the shown G_BPM
 *   err     sequencer position (track 1, 1/16 steps) against the wall clock after the first beats: the
 *           min .. max of the error in ms, in MIDI_CLOCK_GRID units, and the slope in ms per minute
 * Usage: midi_clock_cc_test              the checked matrix
 *        midi_clock_cc_test bpm cc_us mode rs burst seconds    one scenario, printed only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 1
#define TIMER5_SYNTH_TEST 1
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_uart.h"
static uint32_t rx_pending, rx_write, rx_lose;           /* rx_lose: the take reports one byte too few, once */
static uint32_t host_rx_take(void)
{
    uint32_t n = rx_pending;
    rx_pending = 0;
    if (rx_lose && n) { rx_lose = 0; n--; }
    return n;
}
#define fm1_uart1_rx_take() host_rx_take()
#define UI_TEST_NO_MAIN 1
#define felucca_dbg ui_debug_stub
#define fm1_delay_ms unused_timer_delay
#include "ui_test.c"
#undef fm1_delay_ms
#undef felucca_dbg
static void fm1_timer5_ack(void) {}
static void fm1_input_tick(void) {}
static void host_usb_poll(void) {}
#define usb_poll() host_usb_poll()
#undef FELUCCA_UAC
#define FELUCCA_UAC 1
static void uac_service(void) {}
static struct { uint32_t timer_irqs, nested, in_audio; } felucca_dbg;
static uint32_t t5_nested_ticks;
#include "../firmware/src/timer5.c"

/* --- the sender ------------------------------------------------------------------------------------- */
typedef struct { double bpm; uint32_t cc_us, mode, rs, burst, seconds, lose_ms; double sweep; } scen_t;      /* sweep: the tempo goes +-sweep (fraction) around bpm, 6 s a round */
typedef struct { uint8_t v; double done_us; int clock; double ideal_us; } wbyte_t;

static wbyte_t *wire;
static uint32_t wire_n;

/* Build the whole byte stream. CC values change on every message (a sweep). cc_us == 0: no CC. */
static void sender(const scen_t *s)
{
    double clk_period = 60e6 / (s->bpm * 24.0), end = s->seconds * 1e6, byte_us = 320.0;
    double t = 0, next_clk = 20000.0, next_cc = 7300.0;     /* arbitrary phases */
    uint32_t value = 0, rs_have = 0, mlen = 0, mpos = 0, burst_left = 0;
    uint8_t msg[3];
    wire_n = 0;
    wire = realloc(wire, (size_t)(end / byte_us + 1000) * sizeof *wire);
    while (t < end) {
        int clock_due = next_clk <= t, in_msg = mlen && mpos < mlen;
        if (clock_due && (!in_msg || s->mode == 1u)) {
            wire[wire_n++] = (wbyte_t){0xF8u, t + byte_us, 1, next_clk};
            next_clk += s->sweep ? 60e6 / (s->bpm * (1.0 + s->sweep * sin(6.283185307 * next_clk / 6e6)) * 24.0) : clk_period;
            t += byte_us;
        } else if (in_msg || (s->cc_us && next_cc <= t)) {
            if (!in_msg) {
                uint32_t n = 0;
                if (!(s->rs && rs_have)) msg[n++] = 0xB0u;
                msg[n++] = 12u;
                msg[n++] = (uint8_t)(value++ & 127u);
                mlen = n; mpos = 0; rs_have = 1;
                if (s->burst && ++burst_left < s->burst) next_cc = t;           /* back to back */
                else { burst_left = 0; next_cc += (double)s->cc_us * (s->burst ? s->burst : 1u); }
            }
            wire[wire_n++] = (wbyte_t){msg[mpos++], t + byte_us, 0, 0};
            t += byte_us;
        } else {                                             /* idle: on to the next due time */
            double nx = s->cc_us && next_cc < next_clk ? next_cc : next_clk;
            t = nx > t ? nx : t + 1.0;
        }
    }
}

/* --- the receiver ----------------------------------------------------------------------------------- */
typedef struct {
    double stamp_spread, bpm_lo, bpm_hi, shown_lo, shown_hi, err_lo, err_hi, slope, grid_lo, grid_hi;
    double nominal_dev, sl_lo, sl_hi, sl_slope;
    uint32_t clocks_sent, clocks_seen, cc_seen, drops;
} res_t;

static double seq_beats(const track_t *t, uint32_t *steps, uint32_t *prev)
{
    static const double value[] = {1., .5, .25, .125, 1. / 3., 1. / 6., 2., 4., 8., 16.};
    *steps += (t->seq_idx + (uint32_t)t->p[P_SLEN] - *prev) % (uint32_t)t->p[P_SLEN];
    *prev = t->seq_idx;
    return (*steps + (double)t->seq_pos / div_samples((uint32_t)t->p[P_SDIV])) * value[t->p[P_SDIV]];
}

static void run(const scen_t *s, res_t *r)
{
    static uint32_t base_us;                                 /* host time keeps running across scenarios */
    double ideal_first = 0, clk_period = 60e6 / (s->bpm * 24.0), warm_us = 4e6;
    double st_lo = 1e18, st_hi = -1e18, n_e = 0, sx = 0, sy = 0, sxx = 0, sxy = 0, blk_us = CTL * 1e6 / FS, next_blk = 0;
    uint32_t steps = 0, prev = 0, wi = 0, clocks_seen = 0, started = 0, first_seen = 0;
    uint32_t sl_steps = 0, sl_prev = 0, sl_init = 0;
    double sl_off = 0, sl_n = 0, sl_sx = 0, sl_sy = 0, sl_sxx = 0, sl_sxy = 0;
    memset(r, 0, sizeof *r);
    r->bpm_lo = r->shown_lo = r->err_lo = r->grid_lo = 1e9;
    r->bpm_hi = r->shown_hi = r->err_hi = r->grid_hi = -1e9;
    r->sl_lo = 1e9; r->sl_hi = -1e9;
    sender(s);
    for (uint32_t i = 0; i < wire_n; i++) r->clocks_sent += wire[i].clock;
    ui_power_on(); ml_clear_all(); memset(&um, 0, sizeof um); memset((void *)um_ring, 0, sizeof um_ring); memset(&midi_clock, 0, sizeof midi_clock);
    mi_r = mi_w = 0; midi_beat_samples = 0; rx_pending = rx_write = 0; midi_in_overflow = 0;
    song.g[G_CLOCK] = 2; song.g[G_ROUTE] = ROUTE_TRACK; song.g[G_BPM] = 100;
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = 64; trk[k].p[P_SDIV] = 2;
        trk[k].p[P_SLCR] = 1; trk[k].p[P_SLPAT] = 1; trk[k].p[P_SLRATE] = 1; trk[k].p[P_SLDEPTH] = 127;   /* SLICER GATE, 1/16 */
        for (uint32_t i = 0; i < NSTEP; i++) trk[k].step[i] = (step_t){{(uint8_t)(60 + k)}, 1, ST_NOTE, 0, 100};
    }
    host_ticks = base_us; felucca_dbg.in_audio = 0;
    fm1_timer5_irq();                                        /* fm1_ms catches up with the host clock first */
    events_block(CTL);
    um_byte(0xFA);                                           /* Start: the next clock is step zero */
    events_block(CTL);
    for (uint32_t us = 0; us <= s->seconds * 1000000u; us += 100) {
        host_ticks = base_us + us;
        while (wi < wire_n && wire[wi].done_us <= us) {
            um_ring[rx_write++ % UM_RING] = wire[wi].v;
            rx_pending++;
            wi++;
        }
        if (s->lose_ms && us == s->lose_ms * 1000u) rx_lose = 1;
        felucca_dbg.in_audio = (us % 726u) < 520u;           /* the render covers most of a block */
        fm1_timer5_irq();
        if ((double)us >= next_blk) {
            next_blk += blk_us;
            for (uint32_t q = mi_r; q != mi_w; q++) {        /* stamps of the clocks about to be consumed */
                uint32_t at = q % MQ;
                if (((midi_in_q[at] >> 8) & 255u) == 0xF8u) {
                    double ideal = 20000.0 + clocks_seen * clk_period;
                    double e = (double)midi_in_ms[at] * 1000.0 - (base_us + ideal);
                    if (!first_seen) { ideal_first = ideal; first_seen = 1; }
                    if (e < st_lo) st_lo = e;
                    if (e > st_hi) st_hi = e;
                    clocks_seen++;
                } else
                    r->cc_seen++;
            }
            events_block(CTL);
            slicer_track(&trk[0], 0, CTL);                    /* the audio block's step clock, after the tick as audio.c */
            double beats = seq_beats(&trk[0], &steps, &prev), now = (double)us - 0;
            if (!started && midi_clock.have_pulse) started = 1;
            if (started) {                                    /* the SLICER's 1/16 step clock, unwrapped from the start */
                if (!sl_init) { sl_init = 1; sl_prev = sl[0].idx; }
                sl_steps += (sl[0].idx + 16u - sl_prev) & 15u;
                sl_prev = sl[0].idx;
            }
            if (started && now > warm_us) {
                double want = (now - ideal_first) / (clk_period * 24.0);
                double err_ms = (beats - want) * 60e3 / s->bpm;
                double bpm = 60.0 * FS / (midi_beat_samples ? midi_beat_samples : 1u);
                if (err_ms < r->err_lo) r->err_lo = err_ms;
                if (err_ms > r->err_hi) r->err_hi = err_ms;
                if (bpm < r->bpm_lo) r->bpm_lo = bpm;
                if (bpm > r->bpm_hi) r->bpm_hi = bpm;
                if (song.g[G_BPM] < r->shown_lo) r->shown_lo = song.g[G_BPM];
                if (song.g[G_BPM] > r->shown_hi) r->shown_hi = song.g[G_BPM];
                n_e += 1; sx += now; sy += err_ms; sxx += now * now; sxy += now * err_ms;
                if (sl[0].len) {                                  /* SLICER beats against the sequencer's, from the warm-up on */
                    double sb = ((double)sl_steps + (double)sl[0].pos / sl[0].len) * 0.25, d = (sb - beats) * 60e3 / s->bpm;
                    if (!sl_n) sl_off = d;
                    d -= sl_off;
                    if (d < r->sl_lo) r->sl_lo = d;
                    if (d > r->sl_hi) r->sl_hi = d;
                    sl_n += 1; sl_sx += now; sl_sy += d; sl_sxx += now * now; sl_sxy += now * d;
                }
            }
        }
    }
    for (uint32_t i = 0; i < 40; i++) { host_ticks += 100; felucca_dbg.in_audio = 0; fm1_timer5_irq(); }
    r->stamp_spread = (st_hi - st_lo) / 1000.0;
    r->slope = (n_e * sxy - sx * sy) / (n_e * sxx - sx * sx) * 60e6;      /* ms of error per minute */
    r->clocks_seen = clocks_seen;
    r->sl_slope = (sl_n * sl_sxy - sl_sx * sl_sy) / (sl_n * sl_sxx - sl_sx * sl_sx) * 60e6;
    r->grid_lo = r->err_lo * s->bpm / 60e3 * MIDI_CLOCK_GRID;
    r->grid_hi = r->err_hi * s->bpm / 60e3 * MIDI_CLOCK_GRID;
    r->drops = um.drops;
    r->nominal_dev = fmax(s->bpm - r->bpm_lo, r->bpm_hi - s->bpm);
    base_us += s->seconds * 1000000u + 1000000u;
    (void)ideal_first;
}

static void report(const char *name, const scen_t *s, const res_t *r)
{
    printf("%-34s stamp spread %4.2f ms | tempo %6.2f..%6.2f BPM (shown %3.0f..%3.0f) | seq err %+6.2f..%+6.2f ms = %+5.0f..%+5.0f units, %+5.2f ms/min\n",
           name, r->stamp_spread, r->bpm_lo, r->bpm_hi, r->shown_lo, r->shown_hi, r->err_lo, r->err_hi, r->grid_lo, r->grid_hi, r->slope);
    printf("%-34s SLICER 1/16 against the sequencer: %+6.2f..%+6.2f ms, %+5.2f ms/min\n", "", r->sl_lo, r->sl_hi, r->sl_slope);
    (void)s;
}

int main(int argc, char **argv)
{
    int bad = 0;
    res_t r;
    if (argc >= 7) {          /* one scenario, printed only (lose_ms: the DMA count loses one byte then) */
        scen_t s = {atof(argv[1]), (uint32_t)atoi(argv[2]), (uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]),
                    (uint32_t)atoi(argv[5]), (uint32_t)atoi(argv[6]), argc > 7 ? (uint32_t)atoi(argv[7]) : 0u};
        run(&s, &r);
        report("scenario", &s, &r);
        return 0;
    }
    static const double bpms[] = {60, 90, 120, 140, 174};
    static const struct { const char *name; uint32_t cc_us, mode, rs, burst; } load[] = {
        {"no CC", 0, 0, 0, 0}, {"CC every 12 ms", 12000, 0, 0, 0}, {"CC every 5 ms, running status", 5000, 0, 1, 0},
        {"CC every 7 ms, RS, Clock inside the CC", 7000, 1, 1, 0}, {"bursts of 8 CCs every 40 ms", 40000, 0, 0, 8}};
    int tempo_ok = 1, shown_ok = 1, seq_ok = 1, slope_ok = 1, wire_ok = 1;
    for (uint32_t b = 0; b < 5; b++)
        for (uint32_t l = 0; l < 5; l++) {
            scen_t s = {bpms[b], load[l].cc_us, load[l].mode, load[l].rs, load[l].burst, 20, 0};
            char name[96];
            run(&s, &r);
            snprintf(name, sizeof name, "%3.0f BPM, %s", bpms[b], load[l].name);
            report(name, &s, &r);
            tempo_ok &= r.nominal_dev < 0.8;                          /* the measured tempo */
            shown_ok &= r.shown_lo == bpms[b] && r.shown_hi == bpms[b];  /* the BPM on the display never moves */
            seq_ok &= r.err_hi - r.err_lo < 3.0;                        /* sequencer position against the wall clock */
            slope_ok &= fabs(r.slope) < 0.2;                            /* and no drift over 16 s */
            wire_ok &= r.drops == 0 && r.clocks_seen + 2u >= r.clocks_sent;
        }
    {   /* the SLICER's step clock is free running audio time, the sequencer's follows the pulses: they must not part (a tempo that is not a whole BPM) */
        static const double odd[] = {120.5, 97.3, 174.4, 63.7};
        int sl_ok = 1;
        for (uint32_t b = 0; b < 4; b++) {
            scen_t s = {odd[b], 12000, 0, 0, 0, 120, 0};
            char name[96];
            run(&s, &r);
            snprintf(name, sizeof name, "%5.1f BPM, CC every 12 ms, 120 s", odd[b]);
            report(name, &s, &r);
            sl_ok &= r.sl_hi - r.sl_lo < 4.5 && fabs(r.sl_slope) < 0.2;
        }
        {   /* the tempo swept up and down by a quarter every 6 s (a fast BPM knob): the SLICER follows through it */
            scen_t s = {120, 12000, 0, 0, 0, 120, 0, 0.25};
            run(&s, &r);
            report("120 BPM +-25 % every 6 s, CC 12 ms", &s, &r);
            sl_ok &= r.sl_hi - r.sl_lo < 8.0 && fabs(r.sl_slope) < 0.3;
        }
        bad += check("SLICER step clock stays with the sequencer over 120 s (< 4.5 ms peak to peak, no drift)", sl_ok);
    }
    bad += check("measured tempo stays within 0.8 BPM of the sender at 60..174 BPM with and without CCs", tempo_ok);
    bad += check("the displayed BPM never leaves the sender's tempo", shown_ok);
    bad += check("sequencer position keeps within 3 ms peak to peak of the wall clock", seq_ok);
    bad += check("no drift (< 0.2 ms per minute)", slope_ok);
    bad += check("every Clock arrives, no drops", wire_ok);
    /* The CDC counters the device offers to look at what its wire and poll deliver. */
    bad += check("clock gap diagnostics saw the stream", midi_clk_max > 0 && midi_clk_min > 0 && midi_clk_min <= midi_clk_max);
    printf("MIDI CLOCK + CC: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
