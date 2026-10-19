/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible, dry listening renders: same bass phrase and common envelope,
 * then the eight HADES presets. Run in fresh processes (DSP state starts zero).
 * Usage: hades_audio engine preset mode output.wav
 * mode 0: factory patch; 1: matched single-saw filter comparison; 2: live sweep.
 * Audio is synthetic, not taken from the Dreadbox reference videos. */
#define main hostsim_main
#include "hostsim.c"
#undef main

int main(int argc, char **argv)
{
    if (argc != 5) return 2;
    uint32_t engine = (uint32_t)atoi(argv[1]), preset = (uint32_t)atoi(argv[2]), mode = (uint32_t)atoi(argv[3]);
    if (engine >= NENGINES) return 2;
    host_tracks_init(); track_t *t = &trk[0]; host_preset(t, engine, preset);
    for (uint32_t p = 0; p < NPART; p++) {
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
        trk[p].p[P_ITYPE] = 0;
    }
    t->p[P_GLIDE] = 0;
    if (mode == 1u) {
        t->p[P_ATK] = 0; t->p[P_DEC] = 65; t->p[P_SUS] = 72; t->p[P_REL] = 24;
        t->p[P_ED_FLT] = 28; t->p[P_VOICE] = V_MONO;
        for (uint32_t k = 0; k < 8; k++) t->p[P_E0 + k] = 0;
        if (engine == ENGI_HADES) { t->p[P_E0] = 1; t->p[P_E1] = 64; }
        t->p[P_E4] = 56; t->p[P_E5] = 30;
    }
    FILE *f = fopen(argv[4], "wb"); if (!f) return 2;
    uint32_t frames = FS * 6u, written = 0;
    wav_hdr(f, frames);
    const uint8_t notes[] = {36, 36, 43, 39, 36, 48, 34, 31};
    uint32_t unit = (FS / 2u + CTL - 1u) / CTL, blocks = (frames + CTL - 1u) / CTL;
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t step = b / unit, at = b % unit;
        if (step < NELEM(notes) && at == 0) trk_note_on(t, notes[step], 100);
        if (step < NELEM(notes) && at == unit * 3u / 4u) trk_note_off(t, notes[step]);
        if (mode == 2u && step < NELEM(notes)) {
            t->p[P_E4] = 25 + (int32_t)(b % (unit * 4u)) * 85 / (int32_t)(unit * 4u);
            t->p[P_E6] = (int32_t)b * 127 / (int32_t)(unit * NELEM(notes));
        }
        int32_t out[CTL * 2]; mix_block(out, CTL);
        for (uint32_t i = 0; i < CTL && written < frames; i++, written++) wav_put(f, out[2 * i], out[2 * i + 1]);
    }
    fclose(f); return 0;
}
