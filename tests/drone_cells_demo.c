/* SPDX-License-Identifier: GPL-3.0-only */
/* Dry A/B: identical controls/gestures, legacy RING vs opt-in CELLRING.
 * Native host render, not hardware or a subjective sound-quality test. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void demo(uint32_t cells)
{
    static const uint8_t notes[] = {48, 55, 63, 60, 48};
    static const uint32_t starts[] = {12, 32, 52, 72, 92};
    ui_power_on(); host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    for (uint32_t k = 0; k < NPART; k++) trk_all_off(&trk[k]);
    track_t *t = &trk[0]; host_preset(t, ENGI_DRONE, 3);
    t->p[P_E3] = cells ? 5 : 2;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ITYPE] = t->p[P_AMODE] = 0;
    engine_block(t);
    char path[128];
    snprintf(path, sizeof path, "build/drone_cells_demo/%s.wav", cells ? "cells" : "legacy");
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    const uint32_t frames = 128u * FS;
    wav_hdr(f, frames); trk_note_on(t, 36, 100);
    uint32_t last_sec = 0;
    for (uint32_t pos = 0; pos < frames;) {
        uint32_t sec = pos / FS;
        if (sec != last_sec) {
            for (uint32_t k = 0; k < NELEM(notes); k++) {
                if (sec == starts[k]) trk_note_on(t, notes[k], 72 + k * 10);
                if (sec == starts[k] + 10) trk_note_off(t, notes[k]);
            }
            if (sec == 120) trk_note_off(t, 36);
            last_sec = sec;
        }
        /* Same slow coupling edits in both versions, not a P2-only comparison. */
        if (sec >= 40 && sec < 56) t->p[P_E2] = 30 + (sec - 40) * 4;
        else if (sec >= 72 && sec < 88) t->p[P_E2] = (87 - sec) * 5;
        else if (sec >= 92) t->p[P_E2] = 78;
        uint32_t n = frames - pos < CTL ? frames - pos : CTL;
        int32_t out[2 * CTL]; mix_block(out, n);
        for (uint32_t i = 0; i < n; i++) wav_put(f, out[2*i], out[2*i+1]);
        pos += n;
    }
    if (ferror(f) || fclose(f)) exit(1);
    printf("%s: identical DARK RING settings except TOPO, C2 anchor 120 s + 8 s release.\n", path);
}
int main(void)
{
    demo(0); demo(1);
    return 0;
}
