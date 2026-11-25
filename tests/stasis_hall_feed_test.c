/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS 2.0: HALL (return level of the built-in hall) and FEED (grains into the hall).
 * Build with -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DCARAVEL_STASIS2=1. */
#if !CARAVEL_STASIS2 || !CARAVEL_STASIS_EXPAND || !CARAVEL_REVERB_MINIMAL
#error "needs STASIS2, expanded capture and minimal reverb"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <math.h>

#define TAIL (ST_RING_LEN / 2u)
static int32_t out_l[TAIL], out_r[TAIL];
static uint32_t cursor, keep;
static void render(uint32_t frames, int32_t sample)
{
    int32_t zero[CTL] = {0}, bus[CTL], wet[CTL];
    for (uint32_t n = 0; n < frames; n += CTL) {
        for (uint32_t j = 0; j < CTL; j++) { bus[j] = sample; wet[j] = 0; }
        fx_buses(zero, zero, bus, wet, CTL);
        if (keep)
            for (uint32_t j = 0; j < CTL && cursor < TAIL; j++, cursor++) {
                out_l[cursor] = wet[j] + rev_side[j]; out_r[cursor] = wet[j] - rev_side[j];
            }
    }
}
/* Fill the ring, freeze, then record the tail with no input. Everything is deterministic. */
static void scene(uint32_t hall, uint32_t feed)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx); memset(&hl, 0, sizeof hl); memset(&st, 0, sizeof st);
    memset(dly_buf, 0, sizeof dly_buf); rev_clear();
    st_expand.grain = 1; st_expand.quality = 0; st_expand.scan = 0; st_expand.reverse = 0; st_expand.length = 64;
    song.g[G_RTYPE] = 3; song.g[G_DRLVL] = 0; song.g[G_RSIZE] = 96; song.g[G_DRREV] = 75;
    st_hall_ret = (uint8_t)hall; st_hall_feed = (uint8_t)feed;
    keep = 0; render(ST_RING_LEN + CTL * 4u, 16000);
    song.g[G_DRLVL] = 1;
    render(ST_RING_LEN / 4u, 0);
    cursor = 0; keep = 1; render(TAIL, 0); keep = 0;
}
static int32_t ref_l[TAIL], ref_r[TAIL];
static uint64_t diff_from_ref(void)
{
    uint64_t d = 0;
    for (uint32_t i = 0; i < TAIL; i++) d += (uint64_t)abs(out_l[i] - ref_l[i]) + (uint64_t)abs(out_r[i] - ref_r[i]);
    return d;
}
static void keep_ref(void) { memcpy(ref_l, out_l, sizeof ref_l); memcpy(ref_r, out_r, sizeof ref_r); }
/* CHORUS/DELAY already sit in wet when the hall is added: HALL must not touch them. */
static uint64_t send_scene(uint32_t hall)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx); memset(&hl, 0, sizeof hl); memset(&st, 0, sizeof st);
    memset(dly_buf, 0, sizeof dly_buf); rev_clear();
    song.g[G_RTYPE] = 3; song.g[G_RSIZE] = 100; st_hall_ret = (uint8_t)hall; st_hall_feed = 64;
    uint64_t e = 0; uint32_t p = 0;
    for (uint32_t b = 0; b < 400u; b++) {
        int32_t cho[CTL], dly[CTL], zero[CTL] = {0}, wet[CTL];
        for (uint32_t j = 0; j < CTL; j++) { cho[j] = (int32_t)(9000.0 * sin(2 * 3.14159265 * 330.0 * p / 44100.0)); dly[j] = cho[j] / 2; wet[j] = 0; p++; }
        fx_buses(cho, dly, zero, wet, CTL);
        for (uint32_t j = 0; j < CTL; j++) e += (uint64_t)abs(wet[j]);
    }
    return e;
}
int main(void)
{
    scene(64, 64);  keep_ref();
    scene(64, 64);  assert(diff_from_ref() == 0);                    /* deterministic */
    scene(0, 64);   uint64_t hall_part = diff_from_ref();            /* default minus hall-less = the hall's own share */
    scene(32, 64);  uint64_t half_part = diff_from_ref();
    scene(127, 64); uint64_t loud_part = diff_from_ref();
    scene(0, 0);    uint64_t both0 = diff_from_ref();
    scene(64, 0);   uint64_t feed_part = diff_from_ref();
    scene(64, 127); uint64_t feed_up = diff_from_ref();
    fprintf(stderr, "hall_part=%llu half(32)=%llu loud(127)=%llu both0=%llu feed0=%llu feed127=%llu\n", (unsigned long long)hall_part,
            (unsigned long long)half_part, (unsigned long long)loud_part, (unsigned long long)both0,
            (unsigned long long)feed_part, (unsigned long long)feed_up);
    assert(hall_part > 0);
    /* HALL is linear in amplitude: 32 gives half the hall share (difference to default = half), 127 gives 3x (difference = 2x) */
    assert(half_part > hall_part * 45u / 100u && half_part < hall_part * 55u / 100u);
    assert(loud_part > hall_part * 17u / 10u && loud_part < hall_part * 23u / 10u);
    assert(both0 == hall_part);                                       /* FEED is irrelevant once HALL is 0 */
    assert(feed_part > 0 && feed_up > 0);                              /* FEED reaches the hall in both directions */
    /* a send that only runs through CHORUS/DELAY is untouched by HALL */
    send_scene(64);                                                   /* warm up the FX statics outside fx (LFO phases) */
    uint64_t s0 = send_scene(0), s64 = send_scene(64), s127 = send_scene(127);
    assert(s0 > 0 && s0 == s64 && s64 == s127);
    puts("stasis hall/feed: HALL 0/32/64/127 scale only the hall share (0, 1/2, 1, 3x), FEED feeds it, CHORUS/DELAY untouched");
    return 0;
}
