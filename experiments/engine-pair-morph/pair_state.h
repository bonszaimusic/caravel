/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental emulator and opt-in FM-1 test build; native parameter IDs stay unchanged. */
#if !defined(FELUCCA_WEB) && !defined(PAIR_TEST) && !FELUCCA_PERF_HW
#error Engine Pair Morph is a host/emulator experiment, not a firmware build
#endif
#if FELUCCA_CPU2 && !CARAVEL_DUALCORE_D1
#error Pair with CPU2 requires the explicit CARAVEL_DUALCORE_D1 test build
#endif
typedef struct {
    uint8_t on, a, b, budget;
    uint8_t mode, slots, live_slots, dispatch;
    uint8_t cells;                   /* POLY shared CELL network mask: bit 0 A, bit 1 B */
    uint8_t fade_block;              /* keep final pair weights for the legacy one-block voice fade */
    uint8_t keys[128], key_count;   /* one logical held-note order, including DRONE cell phrases */
    uint8_t endpoint;              /* current block: 0 generic, 1 A unity/B zero, 2 B unity/A zero */
    int16_t base, level, pan, mute; /* Q15 morph, existing level/pan units */
    int32_t effective;
    uint8_t note[4], used[4];
    uint32_t age[4], clock;
    int16_t lane[NSTEP];           /* -1 = use base; one lane, owned by member a */
    int16_t weight[2][CTL];
    int16_t shared[32];
    uint8_t motion;                /* optional curve + middle-of-morph tone movement, default OFF */
    int32_t motion_mix;            /* Q15, slewed on/off independently from the morph position */
} engine_pair_t;
static engine_pair_t ep;
static uint8_t pair_cleanup_pending;
static voice_t *ep_alloc;         /* only set during an atomic pair note dispatch */
static track_t *ep_alloc_track;
static int pair_member(const track_t *t) { return ep.on && (t == &trk[ep.a] || t == &trk[ep.b]); }
static int pair_mix_member(const track_t *t) { return (ep.on || ep.fade_block) && (t == &trk[ep.a] || t == &trk[ep.b]); }
static track_t *pair_owner(track_t *t) { return pair_member(t) ? &trk[ep.a] : t; }
static int pair_follower(const track_t *t) { return ep.on && t == &trk[ep.b]; }
static void pair_disable(void);
static void pair_validate(void);
static void pair_stolen(voice_t *v);
static void pair_mix_begin(uint32_t n);
static uint32_t pair_tone_begin(track_t *t, int16_t *keep);

/* Preserve the caller's interrupt mask: these helpers are also reached from audio. */
static inline uint32_t perf_irq_save(void) {
#if FELUCCA_PERF_HW && FM1_IRQ_TARGET
 uint32_t f=fm1_icfg();fm1_irq_off();return f;
#else
 return 0;
#endif
}
static inline void perf_irq_restore(uint32_t f) {
#if FELUCCA_PERF_HW && FM1_IRQ_TARGET
 __asm__ volatile("csync" ::: "memory");fm1_icfg_set(f);
#else
 (void)f;
#endif
}
