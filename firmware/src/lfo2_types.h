/* SPDX-License-Identifier: GPL-3.0-only */
/* Custom: a second, track-local control-rate LFO. Virtual IDs never overlap
 * the byte IDs used by sequencer motion / performance FX. */
#define L2_FIRST 400u
enum { L2_RATE, L2_WAVE, L2_PHASE, L2_FADE, L2_SYNC, L2_TRIG, L2_POL, L2_N };
typedef struct { uint8_t p[L2_N]; } lfo2_config_t;
typedef struct { uint32_t ph, rnd; int32_t val, fade; int16_t prev; } lfo2_state_t;
static lfo2_config_t lfo2_config[NTRK];
static lfo2_state_t lfo2_state[NTRK];
static int lfo2_target(uint32_t id) { return id >= L2_FIRST && id < L2_FIRST + L2_N; }
static int lfo2_valid(const lfo2_config_t *c)
{
    static const uint8_t max[L2_N] = {127, 4, 127, 127, 10, 1, 1};
    for (uint32_t i = 0; i < L2_N; i++) if (c->p[i] > max[i]) return 0;
    return 1;
}
static int lfo2_nonzero(const lfo2_config_t *c)
{
    for (uint32_t i = 0; i < L2_N; i++) if (c->p[i]) return 1;
    return 0;
}
