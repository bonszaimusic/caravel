/* SPDX-License-Identifier: GPL-3.0-only */
/* Isolate new DSP for strict signed-overflow UBSan (legacy synth code is not
 * executed here). Also test the explicit additive/aliased render contract. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "felucca_tables.h"
#ifdef __APPLE__
/* The host test has no retained boot state; .noinit is an ELF firmware section, not a Mach-O section. */
#define __attribute__(x)
#endif
#include "../firmware/src/core.h"
#ifdef __APPLE__
#undef __attribute__
#endif
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/va_blocks.c"
#include "../firmware/src/eng_hades.c"
int main(void)
{
    track_t t = {0}; voice_t *v = &t.v[0];
    uint32_t seed = 0x754D2E11u;
    int32_t peak = 0, click = 0;
    for (uint32_t test = 0; test < 12000; test++) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t note = seed >> 25;
        for (uint32_t k = 0; k < 8; k++) {
            seed = seed * 1664525u + 1013904223u;
            t.p[P_E0 + k] = k ? (int16_t)(seed >> 25) : (int16_t)(seed % 3u);
        }
        memset(v, 0, sizeof *v); hades_note_on(&t, v);
        vmod_t m = {.inc = pitch_inc(note * 16), .pitch16 = note * 16, .amp0 = 32767, .amp1 = 32767,
                    .shape = 64 * 256};
        for (uint32_t b = 0; b < 40; b++) {
            int32_t out[CTL] = {0};
            m.cutoff = (int32_t)(b % 3u) * 127 * 256 - 127 * 256;
            hades_render(&t, v, out, CTL, &m);
            for (uint32_t i = 0; i < CTL; i++) if (abs(out[i]) > peak) peak = abs(out[i]);
        }
    }
    /* Output may alias the voice-state memory (first eight output samples).
     * Restore of state at the end must match an unaliased render exactly. */
    for (uint32_t k = 0; k < 8; k++) t.p[P_E0 + k] = HADES_PRESETS[4].e[k];
    memset(v, 0, sizeof *v); hades_note_on(&t, v);
    vmod_t m = {.inc = pitch_inc(36 * 16), .pitch16 = 36 * 16, .amp0 = 16384, .amp1 = 16384, .shape = 64 * 256};
    for (uint32_t b = 0; b < 100; b++) { int32_t out[CTL] = {0}; hades_render(&t, v, out, CTL, &m); }
    voice_t reference = *v, alias = *v;
    int32_t out[8]; memcpy(out, alias.s, sizeof out);
    hades_render(&t, &reference, out, 8, &m);
    hades_render(&t, &alias, alias.s, 8, &m);
    assert(!memcmp(&alias, &reference, sizeof alias));
    v->env_out = 32767;
    reference = *v;
    hades_note_on(&t, v);
    assert(!memcmp(v->s, reference.s, sizeof v->s));
    v->env_out = 0;
    /* Compare the first changed sample against an unchanged clone: isolate
     * a parameter jump from the oscillator's own BLEP transitions. */
    for (uint32_t control = 0; control < 8; control++) {
        for (uint32_t k = 0; k < 8; k++) t.p[P_E0 + k] = HADES_PRESETS[1].e[k];
        memset(v, 0, sizeof *v); hades_note_on(&t, v);
        for (uint32_t b = 0; b < 500; b++) {
            voice_t normal = *v; int32_t plain[CTL] = {0}, changed[CTL] = {0};
            hades_render(&t, &normal, plain, CTL, &m);
            int16_t old = t.p[P_E0 + control];
            t.p[P_E0 + control] = b & 1 ? 0 : ENG_HADES.edit[control].max;
            hades_render(&t, v, changed, CTL, &m);
            t.p[P_E0 + control] = old;
            if (abs(changed[0] - plain[0]) > click) click = abs(changed[0] - plain[0]);
        }
    }
    printf("HADES strict DSP: 12000 randomized notes/settings; peak %d; largest immediate control deviation %d\n", peak, click);
    assert(peak < 200000 && click < 4000);
    return 0;
}
