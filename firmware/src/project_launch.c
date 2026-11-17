/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* One pending live project launch, independent from the removed four-source
 * Song/Chain cache. Main loop fills it; the ISR sees only the published slot. */
typedef struct {
    step_t step[NTRK][NSTEP];
    int16_t timing[NTRK][4];
    uint8_t qnt[NTRK];                       /* each track's QUANTIZE (P_SQNT) as saved */
    motion_store_t motion;
} project_pattern_t;
static struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    uint8_t preset[NTRK];
    lfo2_config_t lfo2[NTRK];
} live_sound __attribute__((section(".pool")));

static struct {
    project_pattern_t pattern;
    uint32_t carry;
    volatile uint8_t live_pending, live_notice;
    uint8_t live_applied, live_engine[NTRK], live_full;
} project_launch;
#define LAUNCH_STATE project_launch
#define LAUNCH_PATTERN project_launch.pattern
static const step_t *seq_steps(const track_t *t) { return t->step; }
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);
