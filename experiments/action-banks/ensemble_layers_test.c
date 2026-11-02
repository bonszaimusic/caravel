/* SPDX-License-Identifier: GPL-3.0-only */
/* ENSEMBLE layer recipes on the real firmware in the host harness. An ENSEMBLE preset belongs to one track; a second engine under or over it is a
 * second track, and Pair Morph is the way to play both from the same notes (it sends each note to both members and crossfades them with the MORPH
 * position). The recipes below are ENSEMBLE preset + a second engine's preset + a MORPH position; each is checked for: both tracks sound with a four-note
 * chord (four voices each, the budget of eight is full and nothing is stolen; a DRONE cell is one voice), neither engine is buried (each alone at its share of the morph is within
 * 8 dB of the other), the sum has headroom, and a fifth note takes a voice instead of exceeding the budget. A MIDI channel is heard by one track only
 * (seq.c midi_slot), so a keyboard that should play two tracks sends on two channels: that is checked too. PRINT=1 prints the numbers.
 * The numbers describe the signal; nothing here says how a recipe sounds. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN ensemble_layers_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <math.h>
#define ENGI_FM6 12u
#define SECS 8u
#define NS (SECS * 44100u)
static float L[NS];
typedef struct { const char *name; uint8_t ens, eng, other; int16_t morph; const char *role; } recipe_t;
/* the second track's engine and preset: SAMPLE 0 = PIANO (the five-zone set), FM6 1 = BELL, FM6 0 = TINE EP, FM6 5 = MARIMBA, DRONE 11 = SUB FOG (TOPO 4), DRONE 0 = STILL AIR (TOPO 0).
 * morph: 0 .. 32767 (0 = the ENSEMBLE alone, 32767 = the second engine alone) */
static const recipe_t RECIPES[] = {
    {"PIANO UNDER STRINGS", 5, ENGI_SAMPLE, 0, 22937, "STRING MACHINE + the PIANO set, MORPH 70 % toward the piano"},
    {"BELLS IN GLASS", 8, ENGI_FM6, 1, 24576, "GLASS SHIMMER + FM6 BELL, MORPH 75 %"},
    {"EP IN THE CHOIR", 6, ENGI_FM6, 0, 16384, "BREATH CHOIR + FM6 TINE EP, MORPH 50 %"},
    {"PIANO ON THE DEEP", 7, ENGI_SAMPLE, 0, 19660, "DEEP DRONE + the PIANO set, MORPH 60 %: the piano speaks first, the drone swells under it"},
    {"STRINGS ON A BED", 2, ENGI_DRONE, 11, 22937, "DUSK ORCHESTRA + DRONE SUB FOG (a cell TOPO: the pair keeps four slots), MORPH 70 %"},
};
static const uint8_t CHORD[2][4] = {{40, 47, 52, 55}, {52, 55, 59, 64}};
static uint32_t voices_on(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active; return n; }
static uint32_t total_on(void) { uint32_t n = 0; for (uint32_t t = 0; t < NTRK; t++) n += voices_on(&trk[t]); return n; }
static void silence_all(void)
{
    web_pair_off(); web_pair_panic(); run_ms(300);
    for (uint32_t t = 0; t < NTRK; t++) trk_all_off(&trk[t]);
    for (uint32_t g = 0; g < 400u; g++) { run_ms(50); if (!total_on()) break; }
    run_ms(800);
}
typedef struct { double rms, late, peak; uint32_t va, vb, total; } take_t;
/* one take: the recipe at `morph`, the chord held 5 s; rms is the first second (0.15 .. 1.15 s: where a struck second engine speaks), late the settled part (3 .. 5 s);
 * the voices are counted 0.3 s in */
static take_t take(const recipe_t *r, int32_t morph, const uint8_t *notes, uint32_t nn)
{
    take_t k; double e = 0, pk = 0;
    memset(&k, 0, sizeof k);
    silence_all();
    CHECK(web_pair_prepare(0, 1, ENGI_ENSEMBLE, r->eng));
    host_preset(&trk[0], ENGI_ENSEMBLE, r->ens);
    host_preset(&trk[1], r->eng, r->other);
    for (uint32_t t = 0; t < 2u; t++) {
        trk[t].p[P_VOICE] = V_POLY; trk[t].p[P_SPRD] = 0;
        trk[t].p[P_DIST] = trk[t].p[P_CHOR] = trk[t].p[P_DLY] = trk[t].p[P_REV] = 0; trk[t].p[P_LEVEL] = 100;
    }
    web_pair_morph(0, morph);
    run_ms(100);
    memset(L, 0, sizeof L);
    for (uint32_t i = 0; i < nn; i++) trk_note_on(&trk[0], notes[i], 100);
    for (uint32_t ms = 0, at = 0; ms < 5000u; ms++) {
        web_render(44);
        for (uint32_t j = 0; j < 44u; j++, at++) {
            CHECK(isfinite(out_l[j]) && isfinite(out_r[j]) && fabsf(out_l[j]) <= 1.0f && fabsf(out_r[j]) <= 1.0f);
            if (at < NS) L[at] = out_l[j];
        }
        if (ms == 300u) { k.va = (uint32_t)web_pair_get(7, 0); k.vb = (uint32_t)web_pair_get(7, 1); k.total = total_on(); }
    }
    for (uint32_t i = 6615u; i < 6615u + 44100u; i++) e += (double)L[i] * L[i];
    k.rms = sqrt(e / 44100.0);
    e = 0;
    for (uint32_t i = 3u * 44100u; i < 5u * 44100u; i++) e += (double)L[i] * L[i];
    k.late = sqrt(e / (2.0 * 44100.0));
    for (uint32_t i = 0; i < 5u * 44100u; i++) if (fabs(L[i]) > pk) pk = fabs(L[i]);
    k.peak = pk;
    for (uint32_t i = 0; i < nn; i++) trk_note_off(&trk[0], notes[i]);
    run_ms(400);
    return k;
}
int main(void)
{
    int pr = getenv("PRINT") != NULL;
    web_nor_erase(); web_boot(); run_ms(1400); normal_off(); pair_disable();
    const char *try_ = getenv("TRY");          /* design aid: TRY="ens,engine,other,morph" prints the numbers of one recipe */
    if (try_) {
        int a, b, c, m;
        if (sscanf(try_, "%d,%d,%d,%d", &a, &b, &c, &m) == 4) {
            recipe_t r = {"try", (uint8_t)a, (uint8_t)b, (uint8_t)c, (int16_t)m, ""};
            for (int ch = 0; ch < 2; ch++) {
                take_t both = take(&r, m, CHORD[ch], 4), ea = take(&r, 0, CHORD[ch], 4), eb = take(&r, 32767, CHORD[ch], 4);
                printf("chord %d: mix rms %.4f (late %.4f) peak %.3f voices %u+%u (%u)   A alone %.4f (late %.4f)  B alone %.4f (late %.4f)  share A %.1f dB B %.1f dB\n", ch, both.rms, both.late, both.peak, both.va, both.vb, both.total,
                       ea.rms, ea.late, eb.rms, eb.late, 20 * log10(ea.rms * (1.0 - m / 32767.0) / both.rms), 20 * log10(eb.rms * (m / 32767.0) / both.rms));
            }
        }
        return 0;
    }
    CHECK(NELEM(RECIPES) == 5u);
    for (uint32_t q = 0; q < NELEM(RECIPES); q++) {
        const recipe_t *r = &RECIPES[q];
        CHECK(pair_engine_ok(ENGI_ENSEMBLE) && pair_engine_ok(r->eng));
        CHECK(r->ens < ENG_ENSEMBLE.npresets && r->other < ENGINES[r->eng]->npresets);
        for (int ch = 0; ch < 2; ch++) {
            take_t both = take(r, r->morph, CHORD[ch], 4), ea = take(r, 0, CHORD[ch], 4), eb = take(r, 32767, CHORD[ch], 4);
            double fa = 20 * log10((ea.rms * (1.0 - r->morph / 32767.0) + 1e-9) / (both.rms + 1e-9)), fb = 20 * log10((eb.rms * (r->morph / 32767.0) + 1e-9) / (both.rms + 1e-9));
            if (pr) printf("%-20s chord %d: mix rms %.4f (late %.4f) peak %.3f voices %u+%u (%u of %u)  A alone %.4f B alone %.4f (late %.4f)  shares %.1f / %.1f dB\n", r->name, ch, both.rms, both.late, both.peak, both.va, both.vb, both.total, NVOICE,
                           ea.rms, eb.rms, eb.late, fa, fb);
            CHECK(both.rms > 0.012 && both.peak < 0.9);                        /* it sounds (-38 dB or more: the pair crossfade halves each member and FM6 is quiet), with headroom */
            CHECK(ea.rms > 0.01 && eb.rms > 0.01);                             /* each member sounds on its own */
            CHECK(fabs(fa - fb) < 8.0);                                        /* neither is buried at the recipe's MORPH position */
            CHECK(both.total <= NVOICE);                                       /* the budget is never exceeded */
            if (r->eng != ENGI_DRONE) CHECK(both.va == 4u && both.vb == 4u);   /* a four-note chord: four voices on each track */
            else CHECK(both.va == 4u && both.vb >= 1u);                        /* a DRONE preset is MONO: one voice under the chord */
        }
    }
    /* a fifth note in a pair takes a voice: the budget of eight is never exceeded, and each member keeps its four */
    {
        const recipe_t *r = &RECIPES[0];
        static const uint8_t five[5] = {40, 47, 52, 55, 59};
        take_t t5 = take(r, r->morph, five, 5);
        if (pr) printf("five notes: voices %u+%u (%u of %u)\n", t5.va, t5.vb, t5.total, NVOICE);
        CHECK(t5.total <= NVOICE && t5.va >= 3u && t5.vb >= 3u && t5.peak < 0.9);
    }
    /* DRONE has one voice per part (engine_t.poly = 1): in a pair with a DRONE of TOPO 0..2 the pair is one voice (a drone under a melody, not under a chord);
     * a cell TOPO (3..5) keeps the pair's four slots (pair_voice.c pair_budget), which is what the STRINGS ON A BED recipe uses */
    {
        recipe_t r = {"drone pair", 7, ENGI_DRONE, 0, 16384, ""};
        take_t t = take(&r, r.morph, CHORD[0], 4);
        if (pr) printf("ENSEMBLE + DRONE pair: voices %u+%u, pair budget %d\n", t.va, t.vb, (int)web_pair_get(3, 0));
        CHECK(t.va == 1u && t.vb == 1u && t.rms > 0.01);
    }
    /* two tracks from one keyboard without a pair: a MIDI channel is heard by one track only, so the keyboard sends on two channels */
    {
        silence_all();
        web_pair_off();
        host_preset(&trk[0], ENGI_ENSEMBLE, 5); host_preset(&trk[1], ENGI_SAMPLE, 0);
        for (uint32_t t = 0; t < 2u; t++) { trk[t].p[P_VOICE] = V_POLY; trk[t].p[P_DIST] = trk[t].p[P_CHOR] = trk[t].p[P_DLY] = trk[t].p[P_REV] = 0; trk[t].p[P_LEVEL] = 100; }
        song.g[G_ROUTE] = ROUTE_TRACK; mch_set(0, 0); mch_set(1, 0);    /* T1 and T2 on the same channel: the first track hears it */
        run_ms(60);
        for (uint32_t i = 0; i < 4u; i++) midi(0x90, CHORD[0][i], 100);
        run_ms(300);
        CHECK(voices_on(&trk[0]) == 4u && voices_on(&trk[1]) == 0u);
        for (uint32_t i = 0; i < 4u; i++) midi(0x80, CHORD[0][i], 0);
        run_ms(1500);
        mch_set(1, 1);                                                   /* T2 on channel 2: one keyboard, two channels */
        run_ms(60);
        for (uint32_t i = 0; i < 4u; i++) { midi(0x90, CHORD[0][i], 100); midi(0x91, CHORD[0][i], 100); }
        run_ms(300);
        if (pr) printf("two channels: T1 %u voices, T2 %u voices (%u of %u)\n", voices_on(&trk[0]), voices_on(&trk[1]), total_on(), NVOICE);
        CHECK(voices_on(&trk[0]) == 4u && voices_on(&trk[1]) == 4u && total_on() == NVOICE);
        for (uint32_t i = 0; i < 4u; i++) { midi(0x80, CHORD[0][i], 0); midi(0x81, CHORD[0][i], 0); }
        run_ms(1500);
        song.g[G_ROUTE] = 0; mch_set(0, 0); mch_set(1, 0);
    }
    printf("ENSEMBLE LAYERS PASS: %u checks, %u recipes.\n", checks, (unsigned)NELEM(RECIPES));
    return 0;
}
