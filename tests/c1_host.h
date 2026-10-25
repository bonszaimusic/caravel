/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 gospodindilly-ops */
/* The second core on the host (after Jangada's tests/c1_host.h, github.com/zednaked/jangada, GPL-3.0): a thread plays
 * core 1 (hal/fm1_cpu1.h) and renders the parts fx.c mix_block hands it, at the same time as the main thread renders
 * the others. Included before the firmware: tests/dualcore_test.c, tests/regress.c built with -DC1_HOST
 * (run_tests.sh). Built with -fsanitize=thread, any state two parts share while they render is reported as a race.
 * env C1_MASK: which parts go to the thread each block: "auto" (fx.c mix_split, the firmware's choice), "rot"
 * (the default: every split in turn, block by block), or a fixed mask (1..14) */
#ifndef FELUCCA_CPU2
#define FELUCCA_CPU2 1                              /* (the dual-core build: src/core.h) */
#endif
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t c1h_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c1h_cv = PTHREAD_COND_INITIALIZER;
static void (*c1h_fn)(uint32_t);
static uint32_t c1h_arg, c1h_job, c1h_done, c1h_up, c1h_blocks, c1h_taken;
#ifndef C1_HOST_OFF
#define C1_HOST_OFF 0
#endif
static int c1h_off = C1_HOST_OFF;                                 /* 1: no second core (the reference renders) */

static void *c1h_main(void *x)
{
    (void)x;
    pthread_mutex_lock(&c1h_mu);
    for (;;) {
        void (*fn)(uint32_t);
        uint32_t arg;
        while (c1h_done == c1h_job)
            pthread_cond_wait(&c1h_cv, &c1h_mu);
        fn = c1h_fn;
        arg = c1h_arg;
        pthread_mutex_unlock(&c1h_mu);
        fn(arg);
        pthread_mutex_lock(&c1h_mu);
        c1h_done = c1h_job;
        pthread_cond_broadcast(&c1h_cv);
    }
    return 0;
}

static int c1h_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (!c1h_up) {                                  /* started in the child that renders (the tests fork) */
        pthread_t th;
        if (pthread_create(&th, 0, c1h_main, 0))
            return 0;
        c1h_up = 1;
    }
    pthread_mutex_lock(&c1h_mu);
    c1h_fn = fn;
    c1h_arg = arg;
    c1h_job++;
    c1h_taken++;
    pthread_cond_broadcast(&c1h_cv);
    pthread_mutex_unlock(&c1h_mu);
    return 1;
}

static int c1h_wait(void)
{
    pthread_mutex_lock(&c1h_mu);
    while (c1h_done != c1h_job)
        pthread_cond_wait(&c1h_cv, &c1h_mu);
    pthread_mutex_unlock(&c1h_mu);
    return 0;
}

static uint32_t mix_split(void);
static uint32_t c1h_split(void)
{
    static const char *e;
    uint32_t b = c1h_blocks++;
    if (!b)
        e = getenv("C1_MASK");
    if (e && !strcmp(e, "auto"))
        return mix_split();
    if (e && *e && strcmp(e, "rot"))
        return (uint32_t)atoi(e) & 15u;
    return (b * 7u + 3u) % 15u;                     /* every split in turn (0 too: one core), not in step with the
                                                     * patterns */
}

#define C1_ON() (!c1h_off)
#define C1_RUN(fn, arg) c1h_run(fn, arg)
#define C1_WAIT() c1h_wait()
#define C1_SPLIT() c1h_split()
