/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1.4: two-stage pull/return with legacy or tempo-synced oscillations.
 * A turn schedules a pull instead of immediately changing the live parameter.
 * End of PULL starts a bounded oscillatory return around the saved anchor.
 * PULL is 16ms..45s; finite RETURN is 16ms..300s, plus unattenuated INF.
 * Musical BOUNCE uses the internal/filtered external transport BPM; old
 * untagged BOUNCE values keep their v4 cycle-count behavior.
 */
#ifndef CARAVEL_HOME_ELASTIC_TRAJECTORY_H
#define CARAVEL_HOME_ELASTIC_TRAJECTORY_H
#include "home_elastic_core.h"
#include "home_elastic_p13.h"
#include "home_elastic_sync.h"

typedef struct {
    int32_t start_q8, goal_q8;
    uint32_t elapsed_ms, last_ms;
    uint32_t pull_duration_ms, return_duration_ms;
    uint16_t bounce_snapshot; /* legacy 1..1000 or tagged tempo division */
    hp_es_phase_t sync_phase; /* phase and BPM carry; preserved by HOLD */
    uint8_t return_infinite;
    uint8_t reflect_snapshot; /* 0..100 restitution, latched per gesture */
    uint8_t phase; /* 0 idle, 1 pull, 2 return */
} hp_et_t;

typedef struct {
    uint8_t pull,back;  /* PULL 0..100; RETURN 0..100 or INF=101 */
    uint16_t bounce;    /* legacy 1..1000 or HP_ES_SYNC_FLAG | division */
    uint8_t reflect;    /* boundary restitution 0..100 */
    uint8_t couple;     /* row-only pair influence 0..100; 0=OFF */
    uint8_t seed;       /* deterministic pair map 0..255 */
} hp_et_cfg_t;
#define HP_ET_DEFAULT ((hp_et_cfg_t){30u,70u,HP_ES_DEFAULT,HP_EC_REFLECT_DEFAULT,0u,0u})

/* PULL: 0=16ms, 20=80ms, 40=400ms, 60=2s, 80=10s, 100=45s.
 * RETURN preserves the identical short-time feel through 80, then adds a
 * musically readable long tail: 90=45s, 94=1m30s, 95=2m, 100=5m.
 * The gesture snapshots both durations and BOUNCE selection; live changes
 * affect subsequent gestures, while transport tempo continuously retimes
 * a running tagged musical cycle without resetting oscillator phase. */
static inline uint32_t hp_et_ms(uint32_t control)
{
    static const uint32_t steps[6]={16u,80u,400u,2000u,10000u,45000u};
    if(control>=100u)return 45000u;
    uint32_t i=control/20u,rem=control%20u;
    uint32_t ms=steps[i]+(steps[i+1u]-steps[i])*rem/20u;
    return ms<16u?16u:ms;
}
/* Dedicated 5-minute return map; PULL remains 45 seconds max.
 * Above 90, discrete landmarks make the slow ambient end easy to select:
 * 50s, 1m, 1m15, 1m30, 2m, 2m30, 3m, 3m30, 4m15, 5m.
 * All values fit uint32_t millisecond clocks. */
static inline uint32_t hp_et_return_ms(uint32_t control)
{
    if(control<=80u)return hp_et_ms(control);
    if(control<=90u)return 10000u+(control-80u)*3500u;
    static const uint32_t tail[10]={
        50000u,60000u,75000u,90000u,120000u,
        150000u,180000u,210000u,255000u,300000u
    };
    return tail[control>=100u?9u:control-91u];
}
static inline uint32_t hp_et_ease(uint32_t x)
{
    if(x>=65536u)return 65536u;
    /* x < 65536: both products fit uint32. Round after multiplication
     * without overflowing when the result rounds up to 65536. */
    uint32_t xx=(x*x+32768u)>>16;
    uint32_t product=xx*(196608u-2u*x);
    return (product>>16)+((product&65535u)>=32768u);
}
/* Exact floor(elapsed * 65536 / duration) using only 32-bit divide.
 * PULL <= 45,000 ms and RETURN <= 300,000 ms, so both 8-bit radix
 * numerators are <= 76,800,000. Preserves the former uint64 result. */
static inline uint32_t hp_et_fraction_q16(uint32_t elapsed,uint32_t duration)
{
    if(elapsed>=duration)return 65536u;
    uint32_t n=elapsed<<8;
    uint32_t hi=n/duration;
    uint32_t rem=n-hi*duration;
    return (hi<<8)+((rem<<8)/duration);
}
static inline int32_t hp_et_clamp_x(const hp_elastic_t *e,int32_t q8,hp_elastic_cfg_t cfg)
{
    int32_t travel=hp_elastic_limit(e,cfg)*256;
    int32_t lo=((int32_t)e->low-e->anchor)*256;
    int32_t hi=((int32_t)e->high-e->anchor)*256;
    lo=hp_elastic_clamp(lo,-travel,travel);
    hi=hp_elastic_clamp(hi,-travel,travel);
    return hp_elastic_clamp(q8,lo,hi);
}
static inline void hp_et_reset(hp_et_t *t)
{
    t->start_q8=t->goal_q8=0;
    t->elapsed_ms=t->last_ms=0u;
    t->pull_duration_ms=t->return_duration_ms=0u;
    t->bounce_snapshot=0u;t->reflect_snapshot=0u;t->phase=0u;
    t->sync_phase=(hp_es_phase_t){0};t->return_infinite=0u;
}
static inline void hp_et_cancel(hp_elastic_t*e,hp_et_t*t)
{
    hp_elastic_cancel(e);hp_et_reset(t);
}
static inline void hp_et_touch(hp_elastic_t*e,hp_et_t*t,int32_t raw,uint32_t now,
                               hp_et_cfg_t times,hp_elastic_cfg_t cfg)
{
    if(!raw||e->paused)return;
    if(!hp_elastic_limit(e,cfg))return;
    raw=hp_elastic_clamp(raw,-8,8);
    uint32_t dt=e->touched?now-e->last_touch_ms:120u;
    uint32_t speed=dt<24u?4u:dt<60u?3u:dt<120u?2u:1u;
    if(dt>500u)speed=1u;
    e->touched=1u;e->last_touch_ms=now;
    int32_t scale=(int32_t)(1u+speed/2u); /* slow 1, fast 3 */
    int32_t requested=t->phase? t->goal_q8:e->x;
    requested+=raw*256*scale;
    requested=hp_et_clamp_x(e,requested,cfg);
    if(requested==e->x && !t->phase)return;
    t->start_q8=e->x;t->goal_q8=requested;
    t->elapsed_ms=0u;t->last_ms=now;t->phase=1u;
    t->pull_duration_ms=hp_et_ms(times.pull);
    t->return_duration_ms=hp_et_return_ms(times.back);
    /* Old host initializers use only {pull,back}: preserve legacy playback. */
    t->bounce_snapshot=(uint16_t)(times.bounce?times.bounce:1u+cfg.bounce/50u);
    if(!hp_es_is_sync(t->bounce_snapshot) &&
       t->bounce_snapshot>HP_EC_BOUNCE_MAX)t->bounce_snapshot=HP_EC_BOUNCE_MAX;
    t->return_infinite=(uint8_t)(hp_es_is_sync(t->bounce_snapshot) &&
                                times.back==HP_ES_RETURN_INF);
    t->sync_phase=(hp_es_phase_t){0};
    t->reflect_snapshot=times.bounce?times.reflect:0u; /* legacy tests clamp */
    e->active=1u;e->v=0;
}
/* Smoothed, sign-alternating cos-like wave; no sin(), floats or LUT. */
static inline int32_t hp_et_osc(uint32_t u,uint32_t cycles)
{
    /* u <= 65536 and cycles <= 1000 => product <= 131072000. */
    uint32_t half=u*cycles*2u;
    uint32_t index=half>>16;
    uint32_t frac=half&65535u;
    int32_t v=65536-(int32_t)(2u*hp_et_ease(frac));
    return (index&1u)?-v:v;
}
static inline void hp_et_poll_bpm(hp_elastic_t*e,hp_et_t*t,uint32_t now,
                                  hp_et_cfg_t times,hp_elastic_cfg_t cfg,
                                  uint32_t bpm)
{
    (void)times;
    if(!t->phase||!e->active){t->last_ms=now;return;}
    if(e->paused){t->last_ms=now;return;}
    uint32_t remain=now-t->last_ms;t->last_ms=now;
    if(!remain)return;
    for(uint32_t transitions=0;transitions<2u && t->phase;transitions++){
        uint32_t duration=t->phase==1u?t->pull_duration_ms:t->return_duration_ms;
        uint32_t endless=t->phase==2u&&t->return_infinite;
        uint32_t left=endless?remain:duration-t->elapsed_ms;
        uint32_t advance=remain<left?remain:left;
        t->elapsed_ms+=advance;remain-=advance;
        uint32_t u=endless?0u:hp_et_fraction_q16(t->elapsed_ms,duration);
        if(t->phase==1u){
            int32_t ease=(int32_t)hp_et_ease(u);
            e->x=t->start_q8+(int32_t)(((int64_t)(t->goal_q8-t->start_q8)*ease)/65536);
            e->x=hp_et_clamp_x(e,e->x,cfg);
            if(t->elapsed_ms>=duration){t->phase=2u;t->elapsed_ms=0u;
                t->start_q8=t->goal_q8=e->x;}
        }else{
            int32_t envelope=(int32_t)(endless?65536u:65536u-hp_et_ease(u));
            int32_t osc;
            if(hp_es_is_sync(t->bounce_snapshot)){
                hp_es_advance(&t->sync_phase,advance,bpm,t->bounce_snapshot);
                osc=hp_et_osc(hp_es_phase_q16(&t->sync_phase,t->bounce_snapshot),1u);
            }else{
                uint32_t cycles=hp_ec_cycles(t->bounce_snapshot,duration);
                osc=cycles?hp_et_osc(u,cycles):65536;
            }
            int32_t amp=(int32_t)(((int64_t)t->goal_q8*envelope)/65536);
            int32_t projected=(int32_t)(((int64_t)amp*osc)/65536);
            int32_t travel=hp_elastic_limit(e,cfg)*256;
            int32_t lo=hp_elastic_clamp(((int32_t)e->low-e->anchor)*256,-travel,travel);
            int32_t hi=hp_elastic_clamp(((int32_t)e->high-e->anchor)*256,-travel,travel);
            e->x=hp_ec_reflect_x(projected,lo,hi,t->reflect_snapshot);
            if(endless)t->elapsed_ms=0u;
            else if(t->elapsed_ms>=duration){hp_et_cancel(e,t);break;}
        }
        if(!remain)break;
    }
}
/* Original host tests and older integrations keep their legacy 120 BPM call.
 * Production P1.4 calls hp_et_poll_bpm with the actual transport BPM. */
static inline void hp_et_poll(hp_elastic_t*e,hp_et_t*t,uint32_t now,
                              hp_et_cfg_t times,hp_elastic_cfg_t cfg)
{
    hp_et_poll_bpm(e,t,now,times,cfg,120u);
}
static inline void hp_et_hold(hp_elastic_t*e,hp_et_t*t,uint8_t hold,uint32_t now)
{
    e->paused=!!hold;t->last_ms=now;
}
#endif
