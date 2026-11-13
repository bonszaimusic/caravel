/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P0: no storage, UI, DSP, or allocation. Pure control-rate prototype.
 * All parameter displacements are signed Q8 units; raw encoder steps only.
 * This header is intentionally standalone so that host tests use the exact math.
 */
#ifndef CARAVEL_HOME_ELASTIC_CORE_H
#define CARAVEL_HOME_ELASTIC_CORE_H
#include <stdint.h>

typedef struct {
    int32_t x, v;             /* position and velocity, Q8 parameter units */
    uint32_t last_ms, last_touch_ms;
    int16_t anchor, low, high;
    uint8_t active, paused, touched;
} hp_elastic_t;

typedef struct {
    uint8_t response, decay, bounce, range; /* each 0..100; caller validates */
} hp_elastic_cfg_t;

#define HP_ELASTIC_DEFAULT ((hp_elastic_cfg_t){50u, 80u, 50u, 18u})

static inline int32_t hp_elastic_clamp(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}
static inline int32_t hp_elastic_abs(int32_t x) { return x < 0 ? -x : x; }
static inline int32_t hp_elastic_div_round(int32_t x)
{
    return x < 0 ? -((-x + 128) / 256) : (x + 128) / 256;
}
static inline void hp_elastic_reset(hp_elastic_t *e, int16_t anchor,
                                    int16_t low, int16_t high, uint32_t now_ms)
{
    if (high < low) { int16_t t = low; low = high; high = t; }
    e->anchor = (int16_t)hp_elastic_clamp(anchor, low, high);
    e->low = low; e->high = high;
    e->x = e->v = 0;
    e->last_ms = e->last_touch_ms = now_ms;
    e->active = e->paused = e->touched = 0;
}
static inline int32_t hp_elastic_limit(const hp_elastic_t *e, hp_elastic_cfg_t cfg)
{
    int32_t span = (int32_t)e->high - e->low;
    int32_t range = hp_elastic_clamp(cfg.range, 0, 100);
    int32_t limit = (span * range + 50) / 100;
    return limit > 0 ? limit : (span > 0 && range > 0 ? 1 : 0);
}
/* Invalidate state on target replacement, project load or panic; never writes a preset. */
static inline void hp_elastic_cancel(hp_elastic_t *e)
{
    e->x = e->v = 0;
    e->active = e->paused = e->touched = 0;
}
static inline int16_t hp_elastic_value(const hp_elastic_t *e)
{
    int32_t v = (int32_t)e->anchor + hp_elastic_div_round(e->x);
    return (int16_t)hp_elastic_clamp(v, e->low, e->high);
}
/* Turn event: time between RAW detents determines the impulse, not UI acceleration.
 * Positive steps push upward, negative downward; a new gesture adds momentum. */
static inline void hp_elastic_touch(hp_elastic_t *e, int32_t raw_steps,
                                    uint32_t now_ms, hp_elastic_cfg_t cfg)
{
    int32_t limit = hp_elastic_limit(e, cfg);
    if (!raw_steps || !limit || e->paused) return;
    raw_steps = hp_elastic_clamp(raw_steps, -8, 8);
    uint32_t dt = e->touched ? now_ms - e->last_touch_ms : 120u;
    uint32_t speed = dt < 16u ? 8u : dt < 32u ? 6u :
                     dt < 64u ? 4u : dt < 120u ? 2u : 1u;
    /* A long idle gap is not interpreted as a rapid encoder flick. */
    if (dt > 500u) speed = 1u;
    e->last_touch_ms = now_ms; e->touched = 1;
    int32_t response = hp_elastic_clamp(cfg.response, 0, 100);
    int32_t kick = (int32_t)speed * (16 + response) / 32;
    e->x += raw_steps * 256;
    e->v += raw_steps * 128 * kick;
    int32_t maxq = limit * 256;
    e->x = hp_elastic_clamp(e->x, -maxq, maxq);
    e->v = hp_elastic_clamp(e->v, -maxq, maxq);
    e->active = 1;
    e->last_ms = now_ms;
}
/* Fixed-size control ticks; 16ms makes the numerical integrator deterministic.
 * Bounded catch-up prevents a stalled UI frame from spending unbounded time. */
static inline void hp_elastic_poll(hp_elastic_t *e, uint32_t now_ms,
                                   hp_elastic_cfg_t cfg)
{
    if (!e->active) { e->last_ms = now_ms; return; }
    if (e->paused) { e->last_ms = now_ms; return; }
    uint32_t elapsed = now_ms - e->last_ms;
    uint32_t ticks = elapsed / 16u;
    if (ticks > 8u) ticks = 8u;
    if (!ticks) return;
    e->last_ms = now_ms - (elapsed % 16u);
    int32_t bounce = hp_elastic_clamp(cfg.bounce, 0, 100);
    int32_t decay = hp_elastic_clamp(cfg.decay, 0, 100);
    int32_t spring = 4 + bounce * 8 / 100;       /* 4..12 / 256: a slow pendulum */
    int32_t damping = 4 + (100 - decay) * 28 / 100; /* 4..32 / 256: FALL = long */
    int32_t leak = 1 + (100 - decay) / 100;      /* 1..2: finite, click-free stop */
    int32_t maxq = hp_elastic_limit(e, cfg) * 256;
    for (uint32_t i = 0; i < ticks; ++i) {
        int32_t a = -(e->x * spring) / 256 - (e->v * damping) / 256;
        e->v = hp_elastic_clamp(e->v + a, -maxq, maxq);
        if (e->v > 0) e->v = e->v > leak ? e->v - leak : 0;
        else if (e->v < 0) e->v = e->v < -leak ? e->v + leak : 0;
        e->x = hp_elastic_clamp(e->x + e->v, -maxq, maxq);
        if (e->x > 0) e->x = e->x > leak ? e->x - leak : 0;
        else if (e->x < 0) e->x = e->x < -leak ? e->x + leak : 0;
        int32_t effective = (int32_t)e->anchor + hp_elastic_div_round(e->x);
        if (effective < e->low || effective > e->high) {
            e->x = (hp_elastic_clamp(effective, e->low, e->high) - e->anchor) * 256;
            e->v = 0;
        }
        if (hp_elastic_abs(e->x) < 4 && hp_elastic_abs(e->v) < 2) {
            e->x = e->v = 0; e->active = 0; break;
        }
    }
}
/* Commit is an explicit UI operation; normally the anchor never changes. */
static inline void hp_elastic_commit(hp_elastic_t *e)
{
    e->anchor = hp_elastic_value(e);
    hp_elastic_cancel(e);
}
static inline void hp_elastic_hold(hp_elastic_t *e, uint8_t hold, uint32_t now_ms)
{
    e->paused = !!hold; e->last_ms = now_ms;
}
#endif
