/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* The delay's TIME follows the tempo. On the external clock the measured beat (midi_beat_samples) is re-estimated every quarter
 * note and changes by a few samples; the delay's read position must glide, not jump (a jump is a click in the echo).
 * A held sine with the delay on, the beat changing as an estimate does (+-0.3 % every 1/4 note): the largest sample step of the
 * output stays within a few times that of the same sound with a steady beat. Run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int check_(const char *what, int ok)
{
    printf("delay ext clock: %-84s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* seconds of a held note through the delay; jitter: the measured beat (samples) steps by +-jitter every quarter note
 * (22050 samples at 120 BPM); returns the largest |step| of the left output after the first second */
static int32_t run(uint32_t dtime, int32_t jitter, uint32_t secs, int32_t *rms_out)
{
    track_t *t = &trk[0];
    uint32_t frames = secs * FS, pos, i, qn = 0, next_q = 0;
    int32_t prev = 0, mx = 0;
    double sq = 0;
    ui_power_on();
    host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    host_preset(t, ENGI_DRONE, 0);                       /* STILL AIR: sines */
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_REV] = 0;
    t->p[P_DLY] = 110;
    song.g[G_DTIME] = (int16_t)dtime;
    song.g[G_DFDBK] = 70; song.g[G_DMIX] = 90; song.g[G_DCOLOR] = 100;
    song.g[G_BPM] = 120; song.g[G_CLOCK] = 1;            /* external clock: the measured beat decides */
    song.master_q12 = 4096;
    midi_beat_samples = 22050u;
    engine_block(t);
    trk_note_on(t, 48, 100);
    for (pos = 0; pos < frames;) {
        int32_t o[2 * CTL];
        uint32_t n = frames - pos < CTL ? frames - pos : CTL;
        if (pos >= next_q) {                             /* a new estimate every quarter note */
            midi_beat_samples = (uint32_t)(22050 + ((qn++ & 1u) ? jitter : -jitter));
            next_q += 22050u;
        }
        mix_block(o, n);
        for (i = 0; i < n; i++) {
            if (pos + i > FS) {
                int32_t d = o[2 * i] - prev;
                if (d < 0) d = -d;
                if (d > mx) mx = d;
                sq += (double)o[2 * i] * o[2 * i];
            }
            prev = o[2 * i];
        }
        pos += n;
    }
    if (rms_out) *rms_out = (int32_t)sqrt(sq / (frames - FS));
    return mx;
}

int main(void)
{
    int bad = 0;
    int32_t rms, steady, jit, small;
    steady = run(0, 0, 8, &rms);                         /* 1/4 */
    small = run(0, 2, 8, 0);                             /* an estimate that moves by 2 samples */
    jit = run(0, 66, 8, 0);                              /* +-0.3 % */
    printf("  1/4 delay, steady beat: largest step %d (rms %d); +-2 samples: %d; +-66 samples: %d\n", steady, rms, small, jit);
    bad += check_("a steady beat: no step beyond the sine's own (few x rms)", steady < 6 * rms / 4 + 2000);
    bad += check_("the beat moving by 2 samples every quarter note: no step more than twice the steady one", small <= 2 * steady + 200);
    bad += check_("the beat moving by +-66 samples (0.3 %): no step more than twice the steady one", jit <= 2 * steady + 200);
    puts(bad ? "DELAY EXT CLOCK TEST FAILED" : "delay ext clock test passed");
    return bad != 0;
}
