/* SPDX-License-Identifier: GPL-3.0-only */
/* ENSEMBLE audition renders from the actual firmware DSP (host, offline): WAV files to listen to, plus plausibility numbers on stdout.
 *   cc -O2 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_REVERB_MINIMAL=1 \
 *      -DCARAVEL_STASIS_EXPAND=1 -DFELUCCA_SLICE=0 -DFELUCCA_PERF_HW=1 -DFELUCCA_CPU2=0 tools/ensemble_audition.c -lm -o build/ensemble-audition
 *   build/ensemble-audition            (writes build/ensemble-audio/*.wav; the folder must exist)
 *   build/ensemble-audition presets-v2 (writes build/ensemble-audio/presets-v2/*.wav; the folder must exist): the presets from PROFILE A on, each held as a low
 *                                      chord (E2 B2 E3 G3) and a high chord (E3 G3 B3 E4) 8 s and released, with the preset's own sends; a MONO preset (SOLO DRIFT)
 *                                      also as a line; and the five layer recipes of ensemble_layers_test (ENSEMBLE + a second engine in a Pair, MORPH as in the recipe)
 * Scenes (an E minor chord of four voices, E3 G3 B3 E4):
 *   NN-<PRESET>-ensemble-on.wav / -off.wav   each factory preset held 7 s and released, with the ensemble (the preset's MIX) and with MIX 0 (the dry voices)
 *   morph-sweep-on.wav / -off.wav            VINTAGE STRINGS, MORPH from 0 (strings) through 64 (choir) to 127 (dark) over 14 s
 *   stasis-hold.wav                          GLASS CHOIR into the REV send, STASIS HOLD on the chord, the keys released, a second layer of ABYSS PAD
 *                                            sweeping its MORPH above the frozen sound (the auditory brief of the feasibility study, not a verdict)
 * The numbers printed are measures of the signal (level, energy shares below about 200 Hz / up to 1 kHz / above, the spread of the 20 ms levels),
 * not a judgement of how it sounds. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN ensemble_audition_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
#include <math.h>
static const uint8_t NOTES[4] = {52, 55, 59, 64};
static const uint8_t LOW[4] = {40, 47, 52, 55};
static const char *g_dir = "build/ensemble-audio";
static void chord(uint32_t tr, int on) { for (uint32_t i = 0; i < 4u; i++) { if (on) trk_note_on(&trk[tr], NOTES[i], 100); else trk_note_off(&trk[tr], NOTES[i]); } }
static void voice(uint32_t tr, uint32_t preset, int mix0, uint32_t rev)
{
    host_preset(&trk[tr], ENGI_ENSEMBLE, preset);
    trk[tr].p[P_LEVEL] = 100; trk[tr].p[P_REV] = (int16_t)rev; trk[tr].p[P_CHOR] = trk[tr].p[P_DLY] = 0; trk[tr].p[P_DIST] = trk[tr].p[P_ITYPE] = 0;
    if (mix0) trk[tr].p[P_E7] = 0;
}
typedef struct { FILE *f; char name[160]; double e, pk, la, lb, e1, e2, e3, win[2000], we; uint32_t wn, nw, frames; } rec_t;
static void rec_open(rec_t *r, const char *name, uint32_t seconds)
{
    char path[240];
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", name);
    snprintf(path, sizeof path, "%s/%s.wav", g_dir, name);
    r->f = fopen(path, "wb"); CHECK(r->f);
    r->frames = seconds * FS;
    wav_hdr(r->f, r->frames);
}
static uint32_t rec_block(rec_t *r, uint32_t done)
{
    uint32_t n = r->frames - done < 44u ? r->frames - done : 44u;
    web_render(n);
    for (uint32_t i = 0; i < n; i++) {
        double x = out_l[i];
        CHECK(isfinite(out_l[i]) && isfinite(out_r[i]));
        int32_t a = (int32_t)(out_l[i] * 32767.0), b = (int32_t)(out_r[i] * 32767.0);
        CHECK(a >= -32768 && a <= 32767 && b >= -32768 && b <= 32767);
        wav_put(r->f, a, b);
        r->e += x * x; if (fabs(x) > r->pk) r->pk = fabs(x);
        r->la += (x - r->la) * 0.03; r->lb += (x - r->lb) * 0.15;
        r->e1 += r->la * r->la; r->e2 += (r->lb - r->la) * (r->lb - r->la); r->e3 += (x - r->lb) * (x - r->lb);
        r->we += x * x;
        if (++r->wn == 882u) { if (r->nw < 2000u) r->win[r->nw++] = sqrt(r->we / 882.0); r->we = 0; r->wn = 0; }
    }
    return n;
}
static void rec_close(rec_t *r, uint32_t from_s, uint32_t to_s)
{
    double mean = 0, var = 0, t = r->e1 + r->e2 + r->e3;
    uint32_t a = from_s * 50u, b = to_s * 50u < r->nw ? to_s * 50u : r->nw;
    for (uint32_t i = a; i < b; i++) mean += r->win[i];
    mean /= (b > a ? b - a : 1u);
    for (uint32_t i = a; i < b; i++) var += (r->win[i] - mean) * (r->win[i] - mean);
    fclose(r->f);
    CHECK(r->pk > 0.01 && r->pk < 0.99);
    printf("%-34s rms %.4f  peak %.3f  bands %.2f/%.2f/%.2f  level spread (%u..%u s) %.3f\n", r->name, sqrt(r->e / (r->frames ? r->frames : 1u)), r->pk,
           t > 0 ? r->e1 / t : 0, t > 0 ? r->e2 / t : 0, t > 0 ? r->e3 / t : 0, from_s, to_s, mean > 0 ? sqrt(var / (b > a ? b - a : 1u)) / mean : 0);
}
static void settle(void) { web_pair_panic(); run_ms(300); for (uint32_t t = 0; t < NTRK; t++) trk_all_off(&trk[t]); run_ms(1200); }
static void held_scene(uint32_t preset, int mix0)
{
    rec_t r; char name[160];
    snprintf(name, sizeof name, "%02u-%s-ensemble-%s", preset + 1u, ENG_ENSEMBLE.presets[preset].name, mix0 ? "off" : "on");
    for (char *c = name; *c; c++) if (*c == ' ') *c = '_';
    settle(); voice(0, preset, mix0, 0);
    run_ms(100);
    rec_open(&r, name, 10);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        if (ms == 200) chord(0, 1);
        if (ms == 7200) chord(0, 0);
        done += rec_block(&r, done);
    }
    rec_close(&r, 2, 7);
}
static void morph_scene(int mix0)
{
    rec_t r;
    settle(); voice(0, 0, mix0, 0);
    run_ms(100);
    rec_open(&r, mix0 ? "morph-sweep-off" : "morph-sweep-on", 18);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        if (ms == 200) chord(0, 1);
        if (ms >= 1000 && ms <= 15000) trk[0].p[P_E1] = (int16_t)((ms - 1000u) * 127u / 14000u);
        if (ms == 15200) chord(0, 0);
        done += rec_block(&r, done);
    }
    rec_close(&r, 1, 15);
}
static void stasis_scene(void)
{
    rec_t r;
    settle();
    song.g[G_RTYPE] = 3; song.g[G_RSIZE] = 112; song.g[G_DRREV] = 45; song.g[G_STAMT] = 42; song.g[G_STTIME] = 6;
    voice(0, 1, 0, 116);
    voice(1, 3, 0, 60);
    run_ms(100);
    rec_open(&r, "stasis-hold", 24);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        if (ms == 0) chord(0, 1);
        if (ms == 2500) song.g[G_DRLVL] = 1;                       /* STASIS HOLD: the ensembled chord is what the REV send carries */
        if (ms == 3000) chord(0, 0);
        if (ms == 4200) trk[0].p[P_MUTE] = 1;
        if (ms == 6000) chord(1, 1);                               /* a second layer above the frozen sound */
        if (ms >= 6000 && ms <= 16000) trk[1].p[P_E1] = (int16_t)(40 + (ms - 6000u) * 87u / 10000u);
        if (ms == 16500) chord(1, 0);
        if (ms == 17000) song.g[G_DRREV] = 85;
        if (ms == 20000) stasis_transport_stop();
        done += rec_block(&r, done);
    }
    rec_close(&r, 5, 16);
}
static void tagged(char *name, size_t n, uint32_t index, const char *preset, const char *tag)
{
    snprintf(name, n, "%02u-%s-%s", index + 1u, preset, tag);
    for (char *c = name; *c; c++) if (*c == ' ') *c = '_';
}
/* a preset as the device loads it (its sends included), a chord held 8 s and released, 4 s of tail */
static void v2_preset(uint32_t preset, const uint8_t *notes, const char *tag)
{
    rec_t r; char name[160];
    tagged(name, sizeof name, preset, ENG_ENSEMBLE.presets[preset].name, tag);
    settle(); host_preset(&trk[0], ENGI_ENSEMBLE, preset); trk[0].p[P_LEVEL] = 100;
    run_ms(100);
    rec_open(&r, name, 12);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        if (ms == 200) for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[0], notes[i], 100);
        if (ms == 8200) for (uint32_t i = 0; i < 4u; i++) trk_note_off(&trk[0], notes[i]);
        done += rec_block(&r, done);
    }
    rec_close(&r, 2, 8);
}
/* the MONO solo preset as a line: E3 G3 B3 E4 B3 G3 E3 each 0.6 s, legato */
static void v2_line(uint32_t preset)
{
    static const uint8_t LINE[8] = {52, 55, 59, 64, 59, 55, 52, 47};
    rec_t r; char name[160];
    tagged(name, sizeof name, preset, ENG_ENSEMBLE.presets[preset].name, "line");
    settle(); host_preset(&trk[0], ENGI_ENSEMBLE, preset); trk[0].p[P_LEVEL] = 100;
    run_ms(100);
    rec_open(&r, name, 8);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        uint32_t k = ms >= 200u ? (ms - 200u) / 600u : 99u, ph = ms >= 200u ? (ms - 200u) % 600u : 0u;
        if (k < 8u && ph == 0u) { if (k) trk_note_off(&trk[0], LINE[k - 1u]); trk_note_on(&trk[0], LINE[k], 100); }
        if (ms == 5200u) trk_note_off(&trk[0], LINE[7]);
        done += rec_block(&r, done);
    }
    rec_close(&r, 1, 5);
}
typedef struct { const char *name; uint8_t ens, eng, other; int16_t morph; } recipe_t;
static const recipe_t RECIPES[] = {   /* the recipes of tests: experiments/action-banks/ensemble_layers_test.c */
    {"piano-under-strings", 5, ENGI_SAMPLE, 0, 22937}, {"bells-in-glass", 8, 12, 1, 24576},
    {"ep-in-the-choir", 6, 12, 0, 16384}, {"piano-on-the-deep", 7, ENGI_SAMPLE, 0, 19660}, {"strings-on-a-bed", 2, ENGI_DRONE, 11, 22937},
};
static void v2_layer(uint32_t q, const uint8_t *notes, const char *tag)
{
    rec_t r; char name[160];
    const recipe_t *l = &RECIPES[q];
    snprintf(name, sizeof name, "layer-%u-%s-%s", q + 1u, l->name, tag);
    web_pair_off(); settle();
    CHECK(web_pair_prepare(0, 1, ENGI_ENSEMBLE, l->eng));
    host_preset(&trk[0], ENGI_ENSEMBLE, l->ens); host_preset(&trk[1], l->eng, l->other);
    for (uint32_t t = 0; t < 2u; t++) { trk[t].p[P_VOICE] = V_POLY; trk[t].p[P_SPRD] = 0; trk[t].p[P_LEVEL] = 100; }
    web_pair_morph(0, l->morph);
    run_ms(100);
    rec_open(&r, name, 12);
    for (uint32_t ms = 0, done = 0; done < r.frames; ms++) {
        if (ms == 200) for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[0], notes[i], 100);
        if (ms == 8200) for (uint32_t i = 0; i < 4u; i++) trk_note_off(&trk[0], notes[i]);
        done += rec_block(&r, done);
    }
    rec_close(&r, 1, 8);
    web_pair_off();
}
int main(int argc, char **argv)
{
    web_nor_erase(); web_boot(); run_ms(1400); web_master(850);
    CHECK(NENGINES == 18u && ENGINES[ENGI_ENSEMBLE] == &ENG_ENSEMBLE);
    if (argc > 1 && !strcmp(argv[1], "presets-v2")) {
        g_dir = "build/ensemble-audio/presets-v2";
        for (uint32_t p = 4; p < ENG_ENSEMBLE.npresets; p++) {
            v2_preset(p, LOW, "low"); v2_preset(p, NOTES, "high");
            if (ENG_ENSEMBLE.presets[p].mono) v2_line(p);
        }
        for (uint32_t q = 0; q < NELEM(RECIPES); q++) { v2_layer(q, LOW, "low"); v2_layer(q, NOTES, "high"); }
        printf("ENSEMBLE AUDITION (presets-v2): %u checks; WAV files in build/ensemble-audio/presets-v2/\n", checks);
        return 0;
    }
    for (uint32_t p = 0; p < ENG_ENSEMBLE.npresets; p++) { held_scene(p, 0); held_scene(p, 1); }
    morph_scene(0); morph_scene(1);
    stasis_scene();
    printf("ENSEMBLE AUDITION: %u checks; WAV files in build/ensemble-audio/\n", checks);
    return 0;
}
