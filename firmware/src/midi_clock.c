/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI clock contributions by ChanceTheMaker and keremimo (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Audio ISR state. Input timestamps keep USB/TRS jitter outside the render loop.
 * pos/rendered use fixed musical units (GRID per quarter), independent of BPM.
 * Sequencers/ARPs convert those units with fractional carries; tempo changes
 * cannot change the clock's accumulated musical phase. */
static struct {
    uint32_t pos, rendered, last_ms, start_ms, interval_ms;
    uint32_t pulse_units, interp_q8, tempo_ms;
    uint32_t resync_after;
    uint8_t mode, have_pulse, tempo_valid, tempo_n;
    uint8_t bar_tick;
    volatile uint8_t bar_valid, resync;             /* 1 bar, 2 next pulse; audio owns the state */
    uint8_t resync_start;                          /* armed while stopped: remain silent until the pulse */
} midi_clock;
enum { RS_NONE, RS_BAR, RS_PULSE, RS_CANCEL, RS_WAIT, RS_STOP, RS_REC, RS_SONG, RS_DONE, RS_START_BAR, RS_START_PULSE };
static volatile uint8_t midi_resync_notice;

static void midi_resync_cancel(void)
{
    if (midi_clock.resync) midi_resync_notice = RS_CANCEL;
    midi_clock.resync = midi_clock.resync_start = 0;
}

/* UI publishes a transport request; validation and state changes stay in the
 * audio consumer. A repeated request cancels; a different mode replaces it. */
static __attribute__((noinline)) void midi_resync_request(uint32_t mode)
{
    if (mode == 3u || midi_clock.resync == mode) {
        midi_resync_cancel();
        midi_resync_notice = RS_CANCEL;
        return;
    }
    midi_clock.resync = midi_clock.resync_start = 0; /* replacement must not leave the old mode armed */
    if (!song.g[G_CLOCK] || !midi_clock.tempo_valid ||
               fm1_ms - midi_clock.last_ms > 500u) {
        midi_resync_notice = RS_STOP;
    } else if (chain.running || chain.armed) {
        midi_resync_notice = RS_SONG;
    } else if ((song.rec & ~step_record_mask) || motion_fx_recording) {
        midi_resync_notice = RS_REC;
    } else if (mode == 1u && !midi_clock.bar_valid) {
        midi_resync_notice = RS_WAIT;
    } else {
        midi_clock.resync = (uint8_t)mode;
        midi_clock.resync_start = !song.playing;
        midi_clock.resync_after = fm1_ms;           /* do not trigger on older queued clocks */
        midi_resync_notice = midi_clock.resync_start ? (mode == 1u ? RS_START_BAR : RS_START_PULSE) :
                              mode == 1u ? RS_BAR : RS_PULSE;
    }
}

/* Keep rare transport/UI validation out of the inlined audio block's loops. */
static __attribute__((noinline)) void midi_resync_poll(void)
{
    if (transport_req >= 4u && transport_req <= 6u) {
        uint32_t request = transport_req;
        transport_req = 0;
        midi_resync_request(request - 3u);
    }
    if (midi_clock.resync && ((!song.playing && !midi_clock.resync_start) || chain.running || chain.armed ||
        fm1_ms - midi_clock.last_ms > 500u ||
        (song.rec & ~step_record_mask) || motion_fx_recording))
        midi_resync_cancel();
}

static __attribute__((noinline)) void midi_clock_transport(uint32_t status, uint32_t ms)
{
    if (status != 0xFAu && status != 0xFBu && status != 0xFCu) return;
    midi_resync_cancel();
    midi_clock.bar_valid = 0;                      /* Continue has no song-position reference */
    if (status == 0xFAu) {                         /* Start: step zero */
        midi_clock.bar_valid = 1;
        midi_clock.bar_tick = 95;                  /* first following clock is the downbeat */
        midi_clock.pos = midi_clock.rendered = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        seq_start();
    } else if (status == 0xFBu) {                  /* Continue: preserve the step */
        midi_clock.pos = midi_clock.rendered;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        if (!song.playing)
            motion_begin();
        song.playing = 1;
    } else if (status == 0xFCu) {
        seq_stop();
    }
}

static uint32_t midi_clock_scale(uint32_t value, uint32_t new_period, uint32_t old_period)
{
    return (uint32_t)(((uint64_t)value * new_period + old_period / 2u) / old_period);
}
static uint32_t midi_clock_period(uint32_t quarter, uint32_t div)
{
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}
static uint32_t midi_clock_scale_phase(uint32_t value, uint32_t new_period, uint32_t old_period, midi_grid_t *grid)
{
    uint64_t phase = (uint64_t)value * MIDI_CLOCK_GRID + grid->remainder;
    phase = (phase * new_period + old_period / 2u) / old_period;
    grid->remainder = (uint32_t)(phase % MIDI_CLOCK_GRID);
    return (uint32_t)(phase / MIDI_CLOCK_GRID);
}

static __attribute__((noinline)) void midi_clock_pulse(uint32_t ms)
{
    if (midi_clock.tempo_valid && ms - midi_clock.last_ms > 500u) {
        midi_clock.bar_valid = 0;
        midi_resync_cancel();
    }
    if (midi_clock.bar_valid) {
        if (++midi_clock.bar_tick == 96u) midi_clock.bar_tick = 0;
    }
    if (!midi_clock.tempo_valid) {
        midi_clock.tempo_valid = 1;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
    } else if (++midi_clock.tempo_n == 6u) {
        uint32_t dt = ms - midi_clock.tempo_ms;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
        /* Six clocks are a quarter of a beat. Reject gaps and corrupt bursts. */
        if (dt >= 62u && dt <= 375u) {
            uint32_t old_beat = beat_samples(), new_beat = (uint32_t)FS * dt / 250u;
            if (song.playing && new_beat != old_beat) {
                uint32_t i;
                /* Preserve each track's fractional step phase when the master
                 * changes tempo. The next pulse then lands on its new boundary. */
                for (i = 0; i < NTRK; i++) {
                    uint32_t div = (uint32_t)trk[i].p[P_SDIV];
                    uint32_t old_period = midi_clock_period(old_beat, div), new_period = midi_clock_period(new_beat, div);
                    if (trk[i].seq_pos < 0x7FFFFFFFu)
                        trk[i].seq_pos = midi_clock_scale_phase(trk[i].seq_pos, new_period, old_period, &midi_seq_grid[i]);
                    if (trk[i].seq_off)
                        trk[i].seq_off = midi_clock_scale(trk[i].seq_off, new_period, old_period);
                    if (trk[i].nd_late)            /* (NUDGE: the latched plays, seq_pos + 1, with the step) */
                        trk[i].nd_late = midi_clock_scale(trk[i].nd_late - 1u, new_period, old_period) + 1u;
                    if (trk[i].nd_early)
                        trk[i].nd_early = midi_clock_scale(trk[i].nd_early - 1u, new_period, old_period) + 1u;
                    div = (uint32_t)trk[i].p[P_ARATE];
                    old_period = midi_clock_period(old_beat, div); new_period = midi_clock_period(new_beat, div);
                    if (trk[i].arp_pos < 0xFFFFFFFu)
                        trk[i].arp_pos = midi_clock_scale_phase(trk[i].arp_pos, new_period, old_period, &midi_arp_grid[i]);
                    if (trk[i].arp_off)
                        trk[i].arp_off = midi_clock_scale(trk[i].arp_off, new_period, old_period);
                }
                if (clk_pos != CLK_START)          /* (the metronome's beat as the steps) */
                    clk_pos = midi_clock_scale_phase(clk_pos, new_beat / 4u, old_beat / 4u, &midi_click_grid);
                beat_pos = midi_clock_scale_phase(beat_pos, new_beat, old_beat, &midi_beat_grid);
            }
            midi_beat_samples = new_beat;
            song.g[G_BPM] = (int16_t)clamp((int32_t)((15000u + dt / 2u) / dt), 40, 240);
        }
    }
    if (song.playing) {
        if (!midi_clock.have_pulse) {
            /* Enter a fresh start before later clocks in the same queue are
             * consumed; the start sentinel must not discard their progress. */
            for (uint32_t i = 0; i < NTRK; i++) {
                seq_tick(&trk[i], 0);
                if (trk[i].arp_pos == 0xFFFFFFFu) arp_step(&trk[i], 0, 0);
            }
        }
        if (midi_clock.have_pulse) {
            uint32_t interval = ms - midi_clock.last_ms;
            if (interval >= 8u && interval <= 80u)
                midi_clock.interval_ms = interval;
            midi_clock.pos += MIDI_CLOCK_GRID / 24u;
        }
        midi_clock.have_pulse = 1;
    }
    midi_clock.pulse_units = MIDI_CLOCK_GRID / 24u;
    if (!midi_clock.interval_ms)
        midi_clock.interval_ms = beat_samples() * 1000u / ((uint32_t)FS * 24u);
    midi_clock.interp_q8 = midi_clock.pulse_units * 256u / midi_clock.interval_ms;
    midi_clock.last_ms = ms;
    if (midi_clock.resync && (int32_t)(ms - midi_clock.resync_after) > 0 && (midi_clock.resync == 2u ||
        (midi_clock.bar_valid && !midi_clock.bar_tick))) {
        midi_clock.resync = 0;
        midi_clock.resync_start = 0;
        midi_clock.have_pulse = 1;
        midi_clock.start_ms = ms;
        song.playing = 1;
        midi_clock.pos = midi_clock.rendered = 0;
        seq_resync();
        midi_resync_notice = RS_DONE;
    }
}

static uint32_t midi_clock_advance(uint32_t now)
{
    uint32_t target, elapsed, offset, n;
    if (!midi_clock.have_pulse)
        return 0;
    elapsed = now - midi_clock.last_ms;
    /* Interpolate to the next pulse, never across it before it arrives. */
    if (elapsed > midi_clock.interval_ms)
        elapsed = midi_clock.interval_ms;
    offset = elapsed * midi_clock.interp_q8 >> 8;
    if (offset >= midi_clock.pulse_units)
        offset = midi_clock.pulse_units - 1u;
    target = midi_clock.pos + offset;
    n = (int32_t)(target - midi_clock.rendered) > 0 ? target - midi_clock.rendered : 0u;
    if (n > MIDI_CLOCK_GRID / 2u)
        n = MIDI_CLOCK_GRID / 2u;
    midi_clock.rendered += n;
    return n;
}

