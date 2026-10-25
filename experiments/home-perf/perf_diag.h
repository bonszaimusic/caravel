/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded, volatile runtime diagnostics. No project, preset or flash changes. */
/* D1: worker diagnostics have their own writer. All merges happen after C1_WAIT. */
#if FELUCCA_CPU2
static uint32_t pd_worker_mask;
#endif
#define PD_WORDS 120u
#define PD_MAGIC 0x38474450u
#define PD_PERIOD (HALF_FRAMES * 544u) /* TIMER4 ticks per actual I2S half. */
static volatile uint32_t t5_nested_ticks;
#if FM1_IRQ_TARGET
#define PD_REAL_CLOCK 1u
#else
#define PD_REAL_CLOCK 0u
#endif
#ifndef PD_CLOCK
#if FM1_IRQ_TARGET
#define PD_CLOCK() fm1_ticks()
#else
#define PD_CLOCK() 0u /* Emulator time does not advance inside a render; never report it as CPU time. */
#endif
#endif
typedef struct { uint32_t clock, nested; } pd_stamp_t;
typedef struct { uint32_t sum, max, calls; } pd_engine_t;
typedef struct { uint32_t sum, max; } pd_track_t;
static struct {
    uint32_t session, state, seconds, start_ms, last_ms, halves;
    uint32_t total, maximum, nested, nested_max, late, shed, over85, shed_base, deadline;
    uint32_t entry, gap_max, gap_long, entry_seen;
    pd_engine_t engine[3];
#if FELUCCA_CPU2
    pd_engine_t worker_engine[3];
#endif /* DRONE, HADES, other: pure engine calls, exclusive of TIMER5. */
    pd_track_t track[NTRK]; /* complete part path, including engine/track FX, exclusive of TIMER5. */
} pd;
static volatile uint8_t pd_active, pd_request, pd_seconds;
static uint32_t pd_serial;
static pd_stamp_t pd_stamp(void)
{
    pd_stamp_t s;
    do { s.nested=t5_nested_ticks;s.clock=PD_CLOCK(); } while(s.nested!=t5_nested_ticks);
    return s;
}
static uint32_t pd_elapsed(pd_stamp_t start)
{
    pd_stamp_t end=pd_stamp();
    uint32_t all=end.clock-start.clock,nested=end.nested-start.nested;
    return all>=nested?all-nested:0;
}
static uint32_t pd_part_elapsed(uint32_t track,pd_stamp_t start)
{
#if FELUCCA_CPU2
    /* TIMER5 interrupts core 0 only. Do not subtract its time from worker wall time. */
    if(pd_worker_mask & (1u << track))return PD_CLOCK()-start.clock;
#endif
    return pd_elapsed(start);
}
static void pd_engine_done(uint32_t track,uint32_t engine,pd_stamp_t start)
{
    pd_engine_t *bank=pd.engine;
#if FELUCCA_CPU2
    if(pd_worker_mask & (1u << track))bank=pd.worker_engine;
#endif
    pd_engine_t *s=&bank[engine==ENGI_DRONE?0:engine==ENGI_HADES?1:2];
    uint32_t ticks=pd_part_elapsed(track,start);s->sum+=ticks;s->calls++;if(ticks>s->max)s->max=ticks;
}
static void pd_track_done(uint32_t track,pd_stamp_t start)
{
    pd_track_t *s=&pd.track[track];uint32_t ticks=pd_part_elapsed(track,start);
    s->sum+=ticks;if(ticks>s->max)s->max=ticks;
}
static void pd_begin_half(uint32_t now,uint32_t shed)
{
    if(pd_request==1) {
        memset(&pd,0,sizeof pd);pd.session=++pd_serial;pd.state=1;
        pd.seconds=pd_seconds;pd.start_ms=pd.last_ms=now;pd.shed_base=shed;
        pd_active=1;pd_request=0;
    } else if(pd_request==2) {
        if(pd_active){pd.state=2;pd.last_ms=now;}pd_active=0;pd_request=0;
    }
    if(pd_active && (uint32_t)(now-pd.start_ms)>=pd.seconds*1000u) {
        pd_active=0;pd.state=3;pd.last_ms=now;
    }
    if(pd_active){uint32_t tick=PD_CLOCK();if(pd.entry_seen){uint32_t gap=tick-pd.entry;if(gap>pd.gap_max)pd.gap_max=gap;pd.gap_long+=gap>PD_PERIOD*3u/2u;}pd.entry=tick;pd.entry_seen=1;}
}
static void pd_finish_half(uint32_t now,uint32_t total,uint32_t nested,uint32_t late,uint32_t shed)
{
    if(!pd_active)return;
    pd.halves++;pd.last_ms=now;pd.total+=total;pd.nested+=nested;
    if(total>pd.maximum)pd.maximum=total;if(nested>pd.nested_max)pd.nested_max=nested;
    pd.late+=!!late;pd.shed=shed-pd.shed_base;
    pd.over85+=total>(PD_PERIOD*85u/100u);pd.deadline+=total>=PD_PERIOD;
}
