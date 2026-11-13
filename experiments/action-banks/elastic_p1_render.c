/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible offline audio render for ELASTIC P1 (web emulator firmware path).
 *   elastic_p1_render off out.wav
 *   elastic_p1_render on  out.wav
 * Both start from the same erased NOR and the same DRONE preset; "on" arms the
 * seventh ACTION bank and excites HOME K4 before the first sample. */
#define CARAVEL_ELASTIC_P1 1
#define CARAVEL_ACTION_BANKS 1
#define FELUCCA_RANDOM_MORPH 1
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN elastic_render_pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <math.h>
#include <string.h>

static void put16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }
static void put32(FILE *f, uint32_t v) { put16(f, (uint16_t)v); put16(f, (uint16_t)(v >> 16)); }
static void setup(void)
{
    web_nor_erase(); web_boot(); run_ms(1500);
    web_test_drone(14);                              /* RUST: CHAR 102, a bright drone */
    trk[0].p[P_E7] = 30;
    hp_apply(0, 1);
    song.sel = 0; go_home();
    action_enabled = 1; action_bank = ACT_BANK_ELASTIC; action_setup = 0;
    action_refresh();
    trk_note_on(&trk[0], 60u, 100u);
    run_ms(400);
}
int main(int argc, char **argv)
{
    if (argc != 3 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
        fprintf(stderr, "usage: %s on|off out.wav\n", argv[0]);
        return 2;
    }
    setup();
    FILE *f = fopen(argv[2], "wb");
    if (!f) { perror(argv[2]); return 4; }
    const uint32_t frames = 3u * FS;
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, FS); put32(f, FS * 4u);
    put16(f, 4); put16(f, 16); fwrite("data", 1, 4, f); put32(f, frames * 4u);
    uint32_t done = 0; int armed = 0, held = 0, released = 0; int held_value = 0;
    while (done < frames) {
        if (!armed && done >= 11025u && !strcmp(argv[1], "on")) {
            armed = 1;
            action_key(21u);
            if (!hp_el1.on) { fprintf(stderr, "arm failed\n"); fclose(f); return 3; }
            web_enc(EN_K1 + 3u, 20);          /* processed by the next UI frame */
        }
        if (armed && !held && done >= 11025u + 2000u) {
            held = 1; action_key(24u);        /* freeze the offset for the example */
            held_value = hp_elastic_value(&hp_el1.spring);
        }
        if (held && !released && done >= 2u * FS + FS / 4u) {
            released = 1; action_key(26u);    /* return to the stored base */
        }
        uint32_t n = frames - done < 1024u ? frames - done : 1024u;
        web_render(n);
        float *l = web_out_l(), *r = web_out_r();
        for (uint32_t i = 0; i < n; i++) {
            int32_t a = (int32_t)lrintf(l[i] * 32767.0f);
            int32_t b = (int32_t)lrintf(r[i] * 32767.0f);
            if (a > 32767) a = 32767; if (a < -32768) a = -32768;
            if (b > 32767) b = 32767; if (b < -32768) b = -32768;
            put16(f, (uint16_t)(int16_t)a); put16(f, (uint16_t)(int16_t)b);
        }
        done += n;
    }
    fclose(f);
    printf("ELASTIC P1 render %s: %u frames, base TONE %d, range %u, armed %d, held %d, final %d\n",
           argv[1], frames, trk[0].p[P_E7], hp_el1.cfg.range, hp_el1.on, held_value,
           hp_elastic_value(&hp_el1.spring));
    return 0;
}
