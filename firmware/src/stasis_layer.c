/* SPDX-License-Identifier: GPL-3.0-only
 * Included by fx.c: two capture banks in the existing tempo-delay allocation.
 * A frozen source is never edited under a grain. ADD/BLEND first crossfade
 * reads, then merge incrementally with an equivalent per-address read rule.
 */
#if CARAVEL_REVERB_MINIMAL
enum { STL_IDLE, STL_WAIT, STL_FADE, STL_RETURN, STL_MERGE };
static struct {
    uint32_t w, valid, signal_age, delta, merge;
    uint32_t duration, left, rem, err;
    int32_t mix, target, step, direction;
    uint32_t fade_samples;
    uint8_t mode;
#if CARAVEL_STASIS_EXPAND
    int32_t record_acc;
    int32_t record_lp[3]; /* same anti-alias process as primary bank */
    uint8_t record_n;
#endif
    volatile uint8_t phase, request;
} st_layer __attribute__((section(".pool")));

static void stasis_layer_reset(void) { memset(&st_layer, 0, sizeof st_layer); }
static int stasis_layer_can_start(void)
{
    return song.g[G_RTYPE] == 3u && fx.rtype == 3u && st.auto_hold &&
           !st.stopping && !st.releasing && st_layer.phase == STL_IDLE && !st_layer.request;
}
static int stasis_layer_can_cancel(void)
{
    return st_layer.phase == STL_WAIT || st_layer.phase == STL_FADE || st_layer.request == 1u;
}
/* UI writes only the byte mailbox; all capture and merge state belongs to audio. */
static int stasis_layer_command(uint32_t cancel)
{
    if (cancel ? !stasis_layer_can_cancel() : !stasis_layer_can_start()) return 0;
    st_layer.request = cancel ? 2u : 1u;
    return 1;
}
static const char *stasis_layer_status(void)
{
    return st_layer.request == 1u || st_layer.phase == STL_WAIT ? "WAIT" :
           st_layer.phase == STL_FADE ? "MIX" : st_layer.phase == STL_RETURN ? "BACK" :
           st_layer.phase == STL_MERGE ? "STORE" : stasis_layer_can_start() ? "READY" : "HOLD?";
}
static void stasis_layer_ramp(int32_t target, uint32_t samples, uint32_t phase)
{
    int32_t diff = target - st_layer.mix;
    uint32_t distance = (uint32_t)(diff < 0 ? -diff : diff);
    st_layer.target = target;
    st_layer.duration = st_layer.left = samples;
    st_layer.direction = diff < 0 ? -1 : 1;
    st_layer.step = (int32_t)(distance / samples) * st_layer.direction;
    st_layer.rem = distance % samples;
    st_layer.err = 0;
    st_layer.phase = (uint8_t)phase;
}
static void stasis_layer_freeze(void)
{
    st_layer.delta = (st_layer.w - st.w) & ST_RING_MASK;
    st_layer.merge = 0;
    stasis_layer_ramp(st_layer.target, st_layer.fade_samples, STL_FADE);
}
/* STOP/LIVE keep the currently audible mixture while the normal cloud fades.
 * Finish an already audible mixture transparently; do not resume a cancelled fade. */
static void stasis_layer_stop(void)
{
    st_layer.request = 0;
    if (st_layer.mix) {
        st_layer.phase = STL_MERGE;
        st_layer.left = 0;
    } else stasis_layer_reset();
}
static inline int32_t stasis_layer_mix_at(uint32_t p)
{
    int32_t old = dly_buf[p];
    int32_t incoming = dly_buf[ST_RING_LEN + ((p + st_layer.delta) & ST_RING_MASK)];
    return (old * (32768 - st_layer.mix) + incoming * st_layer.mix) >> 15u;
}
static inline int32_t stasis_read(uint32_t address)
{
    uint32_t p = address & ST_RING_MASK;
    if (!st_layer.mix || (st_layer.phase == STL_MERGE && p < st_layer.merge)) return dly_buf[p];
    return stasis_layer_mix_at(p);
}
static void stasis_layer_block(const int32_t *input, uint32_t n)
{
    uint32_t request = st_layer.request, i;
    st_layer.request = 0;
    if (request == 2u) {
        if (st_layer.phase == STL_WAIT) stasis_layer_reset();
        else if (st_layer.phase == STL_FADE)
            stasis_layer_ramp(0, FS / 5u, STL_RETURN);
    } else if (request == 1u && stasis_layer_can_start()) {
        st_layer.mode = song.g[G_STMODE] == 1;
        st_layer.target = st_layer.mode ? 32768 : clamp(song.g[G_STAMT], 0, 127) * 32768 / 127;
        st_layer.fade_samples = st_layer.mode ? (uint32_t)clamp(song.g[G_STTIME], 1, 30) * FS : FS / 5u;
        if (st_layer.target) st_layer.phase = STL_WAIT;
    }
    if ((!st.auto_hold || st.releasing || st.stopping) && st_layer.phase && st_layer.phase != STL_MERGE)
        stasis_layer_stop();
    if (st_layer.phase == STL_IDLE || st_layer.phase == STL_WAIT) {
        uint32_t peak = 0;
        for (i = 0; i < n; ++i) {
            int32_t x = input[i] / 4;
            uint32_t a = (uint32_t)(x < 0 ? -x : x);
            if (a > peak) peak = a;
        }
        /* -54 dBFS peak gate. A quiet input cannot erase a held texture. */
        if (peak >= 64u) st_layer.signal_age = 0;
        else if (st_layer.signal_age < ST_RING_LEN) st_layer.signal_age += n;
        if (st_layer.phase == STL_WAIT && st_layer.valid >= (
#if CARAVEL_STASIS_EXPAND
            stasis_min_history()
#else
            ST_MIN_VALID
#endif
            ) &&
            st_layer.signal_age < (
#if CARAVEL_STASIS_EXPAND
            stasis_grain_samples()
#else
            ST_GRAIN_LEN
#endif
            )) stasis_layer_freeze();
    }
    if (st_layer.phase == STL_MERGE) {
        /* At most one bank write per audio sample; no full-buffer copy in an ISR. */
        for (i = 0; i < n && st_layer.merge < ST_RING_LEN; ++i) {
            uint32_t p = st_layer.merge;
            dly_buf[p] = (int16_t)stasis_layer_mix_at(p);
            st_layer.merge++;
        }
        if (st_layer.merge == ST_RING_LEN) stasis_layer_reset();
    }
}
static inline void stasis_layer_tick(int32_t input)
{
    if (st.auto_hold && !st.stopping && !st.releasing && st_layer.phase <= STL_WAIT) {
#if CARAVEL_STASIS_EXPAND
        st_layer.record_acc+=stasis_record_prefilter(clamp(input/4,-32768,32767),
          st_layer.record_lp,st.active_quality);
        if(++st_layer.record_n>=stasis_record_factor()){
            dly_buf[ST_RING_LEN+(st_layer.w++&ST_RING_MASK)] =
              (int16_t)clamp(st_layer.record_acc/(int32_t)stasis_record_factor(),-32768,32767);
            st_layer.record_acc=0;st_layer.record_n=0;
            if(st_layer.valid<ST_RING_LEN)st_layer.valid++;
        }
#else
        dly_buf[ST_RING_LEN + (st_layer.w++ & ST_RING_MASK)] = (int16_t)clamp(input / 4, -32768, 32767);
        if (st_layer.valid < ST_RING_LEN) st_layer.valid++;
#endif
        if (st_layer.phase == STL_WAIT && st_layer.valid >= (
#if CARAVEL_STASIS_EXPAND
            stasis_min_history()
#else
            ST_MIN_VALID
#endif
            ) &&
            st_layer.signal_age < (
#if CARAVEL_STASIS_EXPAND
            stasis_grain_samples()
#else
            ST_GRAIN_LEN
#endif
            )) stasis_layer_freeze();
    }
    if (st_layer.phase == STL_FADE || st_layer.phase == STL_RETURN) {
        st_layer.mix += st_layer.step;
        st_layer.err += st_layer.rem;
        if (st_layer.err >= st_layer.duration) {
            st_layer.err -= st_layer.duration;
            st_layer.mix += st_layer.direction;
        }
        if (!--st_layer.left) {
            st_layer.mix = st_layer.target;
            if (st_layer.phase == STL_RETURN) stasis_layer_reset();
            else st_layer.phase = STL_MERGE;
        }
    }
}
#else
static inline void stasis_layer_reset(void) { }
static inline void stasis_layer_stop(void) { }
static inline void stasis_layer_block(const int32_t *input, uint32_t n) { (void)input; (void)n; }
static inline void stasis_layer_tick(int32_t input) { (void)input; }
static inline int32_t stasis_read(uint32_t address) { return dly_buf[address & ST_RING_MASK]; }
#endif
