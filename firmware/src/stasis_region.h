/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS2 source-region geometry. No allocations, per-sample division, or
 * dependence on UI/project structures. All coordinates are source frames
 * measured backwards from the capture writer (newest = age zero).
 *
 * The read anchor is the oldest address of a whole forward/reverse grain.
 * Interpolation accesses [anchor - span + 1, anchor] in age coordinates.
 * While LIVE, reserve another complete span before the writer can wrap.
 * Calls occur at block/head boundaries, not in the sample hot loop.
 */
#ifndef CARAVEL_STASIS_REGION_H
#define CARAVEL_STASIS_REGION_H

enum { STREG_WAIT, STREG_FILL, STREG_FIT, STREG_LIMIT };
typedef struct {
    uint32_t young, old, span, anchor_min, anchor_max, effective_length;
    uint8_t status;
} stasis_region_t;

static inline uint32_t streg_min(uint32_t a, uint32_t b)
{ return a < b ? a : b; }
static inline uint32_t streg_max(uint32_t a, uint32_t b)
{ return a > b ? a : b; }
static inline uint32_t streg_bound(uint32_t x, uint32_t lo, uint32_t hi)
{ return x < lo ? lo : x > hi ? hi : x; }

/* valid <= capacity. quality_factor = 1, 2 or 4. pos/length = 0..127.
 * Status is FILL when contiguous source history is insufficient, LIMIT if
 * requested LENGTH is reduced to fit, FIT when it can be honored, and WAIT
 * when even a minimally guarded grain cannot be scheduled.
 */
static inline stasis_region_t stasis_region_plan(uint32_t valid,
    uint32_t capacity, uint32_t output_grain, uint32_t quality_factor,
    uint32_t pos, uint32_t length, uint32_t frozen)
{
    const uint32_t guard = 256u; /* > interpolation and stereo neighbor */
    stasis_region_t r = {0};
    uint32_t oldest, width, min_len, theoretical, requested, offset;
    if (!quality_factor || !capacity || output_grain >= capacity)
        return r;
    if (valid > capacity) valid = capacity;
    if (pos > 127u) pos = 127u;
    if (length > 127u) length = 127u;
    r.span = (output_grain + quality_factor - 1u) / quality_factor + 2u;
    if (r.span + 3u * guard >= capacity)
        return r;
    r.young = guard;
    min_len = r.span + guard; /* at least one grain and a tap-change margin */
    oldest = valid > guard ? valid - guard : 0u;
    if (!frozen)
        oldest = streg_min(oldest, capacity - r.span - guard);
    if (oldest <= r.young || oldest - r.young < min_len) {
        r.status = valid < capacity ? STREG_FILL : STREG_WAIT;
        return r;
    }
    width = oldest - r.young;
    theoretical = capacity - r.span - 2u * guard;
    if (theoretical < min_len)
        return r;
    requested = min_len + ((theoretical - min_len) * length) / 127u;
    r.effective_length = streg_min(requested, width);
    r.status = requested > width ? STREG_LIMIT : STREG_FIT;
    offset = ((width - r.effective_length) * pos) / 127u;
    r.young += offset;
    r.old = r.young + r.effective_length;
    r.anchor_min = r.young + r.span;
    r.anchor_max = r.old;
    return r;
}

/* MOTN scatter: where in the REGION window a grain starts. width = anchor_max - anchor_min, motion 0..127 (the
 * smoothed MOTN), r16 a fresh 16-bit random number per grain start. The scatter grows with MOTN^2, so the default
 * (45) uses ~12 % of the window and 127 all of it; 0 is exactly the fixed start. Never exceeds the window, and
 * stasis_region_anchor() clamps anyway. 65535 * 65536 and width * 127^2 both fit in 32 bits (width <= 65536). */
static inline uint32_t stasis_region_wander(stasis_region_t r, uint32_t motion, uint32_t r16)
{
    uint32_t width, span;
    if (r.status < STREG_FIT || r.anchor_max <= r.anchor_min) return 0u;
    if (motion > 127u) motion = 127u;
    width = r.anchor_max - r.anchor_min;
    span = (width * (motion * motion)) / 16129u;
    return (r16 * span) >> 16;
}

/* Tap offsets are measured in captured sample frames (not output frames).
 * Clamp both stereo channels independently within the same selected region.
 * The previous source reader is never allowed to see new/uninitialized data.
 */
static inline uint32_t stasis_region_anchor(stasis_region_t r,
    uint32_t preferred_age)
{
    if (r.status < STREG_FIT) return 0u;
    return streg_bound(preferred_age, r.anchor_min, r.anchor_max);
}
#endif
