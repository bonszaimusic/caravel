/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SAMPLE's reduced factory sets (0.13.0 V1+V3), rendered by the real host path.
 * 1. PIANO (SET 0; aliases 1/3/4) uses unshortened C1/C3/C5 samples:
 *    three zones, 22,050 Hz, 0.75 s at root; GRAIN SRC 0 shares them.
 * 2. FLUTE (SET 2) uses C4/C6, two looped zones with original loop points.
 * 3. SLICE PIANO references the factory middle-C zone (no duplicate data).
 * 4. PIANO HD user export stays five zones as before and plays in USR1.
 *    Only notes choosing the same root as factory PIANO remain bit-identical;
 *    this change intentionally invalidates old affected SAMPLE/GRAIN golden hashes. */
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3, as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails;
static void check(const char *what, int ok)
{
    printf("sample: %-88s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static voice_t *voice_of(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note)
            return &t->v[i];
    return 0;
}

#define SECS 2u
static int32_t buf[FS * SECS];
/* track 1 on engine eng, SET / SRC src (the engine's preset 0 otherwise, sends off), note held SECS s -> buf;
 * *ended_at the first sample after which the voice had stopped (one-shot end), 0 = still on; *zone the voice's zone */
static void render(uint32_t eng, int16_t src, uint32_t note, uint32_t *ended_at, int32_t *zone)
{
    track_t *t = &trk[0];
    voice_t *v;
    uint32_t i, k, n = 0;
    host_tracks_init();
    host_preset(t, eng, 0);
    t->p[P_E0] = src;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
    t->p[P_ATK] = 0;
    t->p[P_DEC] = 127;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 10;
    trk_note_on(t, note, 100);
    v = voice_of(t, note);
    if (zone)
        *zone = v ? v->s[eng == ENGI_SAMPLE ? 4 : 0] : -1;
    if (ended_at)
        *ended_at = 0;
    for (k = 0; k < FS * SECS / CTL; k++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[n++] = o[2 * i];
        if (ended_at && !*ended_at && v && (!v->active || v->s[6]))
            *ended_at = n;
    }
    trk_note_off(t, note);
    for (k = 0; k < FS / 2u / CTL; k++) {                 /* the release: nothing left sounding */
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
}

/* render(ENGI_SAMPLE, src, note) in a forked child, from this process's state; its buf -> dst (shared memory) */
static void render_child(int16_t src, uint32_t note, int32_t *dst)
{
    pid_t pid = fork();
    if (pid == 0) {
        render(ENGI_SAMPLE, src, note, 0, 0);
        memcpy(dst, buf, sizeof buf);
        _exit(0);
    }
    if (pid > 0)
        waitpid(pid, 0, 0);
}

/* YIN (cumulative mean normalised difference, threshold 0.15, parabolic) on n samples of buf from a */
static double yin_hz(uint32_t a, uint32_t n, double lo_hz, double hi_hz)
{
    static double d[4096];
    uint32_t tmin = (uint32_t)(FS / hi_hz), tmax = (uint32_t)(FS / lo_hz), t, i, best = 0;
    double sum = 0.0;
    if (tmax >= 4096u)
        tmax = 4095u;
    d[0] = 1.0;
    for (t = 1; t <= tmax; t++) {
        double s = 0.0;
        for (i = 0; i < n; i++) {
            double x = (double)buf[a + i] - (double)buf[a + i + t];
            s += x * x;
        }
        sum += s;
        d[t] = sum > 0.0 ? s * t / sum : 1.0;
    }
    for (t = tmin; t < tmax; t++)
        if (d[t] < 0.15) {
            while (t + 1u < tmax && d[t + 1u] < d[t])
                t++;
            best = t;
            break;
        }
    if (!best) {
        double m = 1e9;
        for (t = tmin; t < tmax; t++)
            if (d[t] < m)
                m = d[t], best = t;
    }
    if (best > 1u && best + 1u < 4096u) {
        double y0 = d[best - 1u], y1 = d[best], y2 = d[best + 1u], den = y0 - 2.0 * y1 + y2;
        return FS / (best + (den != 0.0 ? 0.5 * (y0 - y2) / den : 0.0));
    }
    return 0.0;
}

static double rms(uint32_t a, uint32_t b)
{
    double s = 0.0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)buf[i] * buf[i];
    return b > a ? sqrt(s / (b - a)) : 0.0;
}

static void wav_out(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, FS * SECS);
    for (i = 0; i < FS * SECS; i++)
        wav_put(f, buf[i], buf[i]);
    fclose(f);
}

static long load(const char *path, void *dst, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(dst, 1, (size_t)max, f);
    fclose(f);
    return n;
}

/* the note's pitch from buf (0.2 s on), within 15 cents */
static int pitch_ok(const char *what, uint32_t note)
{
    char m[160];
    double want = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0), hz = yin_hz(FS / 5u, 4096u, want / 1.6, want * 1.6);
    int ok = hz > 0.0 && fabs(1200.0 * log2(hz / want)) < 15.0;
    snprintf(m, sizeof m, "%s: note %u at its pitch (%.1f Hz, want %.1f, %+.0f ct)", what, note, hz, want,
             hz > 0.0 ? 1200.0 * log2(hz / want) : 0.0);
    check(m, ok);
    return ok;
}

int main(int argc, char **argv)
{
    static const uint8_t NOTES[] = {36, 45, 48, 55, 59, 60, 64, 69, 72, 79, 84, 96};
    const char *dir = argc > 2 ? argv[2] : "build/sample_demo";
    char path[512], m[160];
    uint32_t i, ended;
    int32_t zone, zone2;
    if (argc < 2) {
        printf("usage: sample_test PIANO_HD_PREFIX [DEMO_DIR]\n");
        return 2;
    }

    /* 1: PIANO factory V1, three full-length recordings */
    {
        static const uint8_t ROOT[3] = {36, 60, 84}, LO[3] = {0, 49, 73}, HI[3] = {48, 72, 127};
        const smp_set_t *s = &SMP_SETS[0];
        const smp_zone_t *z = &SMP_ZONES[s->z0];
        uint32_t bytes = 0, ok = s->nz == 3u;
        check("PIANO: SET 0, its aliases 1, 3 (once SAX) and 4 the same zones; FLUTE 2, USR1..3 5..7; 4 presets",
              SMP_NSETS == 5u && str_eq(SMP_SETS[0].name, "PIANO") && SMP_SETS[1].z0 == s->z0 && SMP_SETS[3].z0 == s->z0 &&
              SMP_SETS[4].z0 == s->z0 && SMP_SETS[1].nz == s->nz && SMP_SETS[3].nz == s->nz && SMP_SETS[4].nz == s->nz &&
              str_eq(SMP_SETS[2].name, "FLUTE") && str_eq(SMP_SETS[3].name, "PIANO") && SMP_SET_ORIG[3] == 0u && SMP_NALIAS == 2 && str_eq(SMP_ALL_NAMES[5], "USR1") && ENG_SAMPLE.edit[0].max == 7 &&
              ENG_SAMPLE.npresets == 4u);
        for (i = 0; ok && i < 3u; i++) {
            bytes += (z[i].n + 1u) / 2u;
            ok &= z[i].rate == 32768u && z[i].root16 == ROOT[i] * 16u && z[i].lo == LO[i] && z[i].hi == HI[i] &&
                  !z[i].looped && z[i].n == 16537u;
        }
        snprintf(m, sizeof m, "PIANO: 3 full-length zones at 22,050 Hz, roots 36 60 84, 0.75 s (%u B ADPCM)", bytes);
        check(m, ok && bytes == 24807u);
        render(ENGI_SAMPLE, 0, 48, 0, &zone);
        render(ENGI_SAMPLE, 0, 49, 0, &zone2);
        check("PIANO: note 48 uses root 36 and note 49 uses root 60 (new split)",
              zone == (int32_t)s->z0 && zone2 == (int32_t)s->z0 + 1);
        for (i = 0; i < sizeof NOTES; i++) {
            render(ENGI_SAMPLE, 0, NOTES[i], 0, 0);
            pitch_ok("PIANO", NOTES[i]);
            snprintf(path, sizeof path, "piano_%02u", NOTES[i]);
            wav_out(dir, path);
        }
        render(ENGI_SAMPLE, 0, 60, &ended, 0);
        snprintf(m, sizeof m, "PIANO: a root plays 0.75 s, faded out (ends %.3f s; RMS %.0f at 0.2 s, %.0f in its last 20 ms)",
                 (double)ended / FS, rms(FS / 5u, FS / 4u), rms(FS * 73u / 100u, FS * 75u / 100u));
        check(m, ended > FS * 74u / 100u && ended < FS * 77u / 100u && rms(FS * 77u / 100u, FS * SECS) < 2.0 &&
                 rms(FS * 73u / 100u, FS * 75u / 100u) < 0.05 * rms(FS / 5u, FS / 4u));
        render(8u, 0, 60, 0, &zone);
        check("PIANO: GRAIN SRC 0 takes its zones (the middle C zone for note 60, not the sine)",
              zone == 1 && rms(FS / 2u, FS) > 100.0);
    }

    /* 2: FLUTE factory V3, original full-length looping samples. */
    {
        static const uint8_t ROOT[2] = {72, 96}, LO[2] = {0, 85}, HI[2] = {84, 127};
        const smp_set_t *s = &SMP_SETS[2];
        const smp_zone_t *z = &SMP_ZONES[s->z0];
        uint32_t bytes = 0, ok = s->nz == 2u;
        for (i = 0; ok && i < 2u; i++) {
            bytes += (z[i].n + 1u) / 2u;
            ok &= z[i].rate == 32768u && z[i].root16 == ROOT[i] * 16u &&
                  z[i].lo == LO[i] && z[i].hi == HI[i] && z[i].looped &&
                  z[i].n == 20947u && z[i].ls == 8820u && z[i].le == 20946u;
        }
        check("FLUTE: 2 zones at C4/C6, original lengths/loops and MIDI boundaries",
              ok && bytes == 20948u);
        render(ENGI_SAMPLE, 2, 84, 0, &zone);
        render(ENGI_SAMPLE, 2, 85, 0, &zone2);
        check("FLUTE: note 84 uses root 72 and note 85 uses root 96 (new split)",
              zone == (int32_t)s->z0 && zone2 == (int32_t)s->z0 + 1);
    }

    /* 3: SLICE's PIANO shares the retained factory middle C zone. */
    check("SLICE PIANO: retained SAMPLE middle C zone, 16537 samples, no copy",
          SLC_PIANO.len == 16537u && SLC_PIANO.rate == 32768u &&
          SLC_PIANO.seg[0].off == SMP_ZONES[SMP_SETS[0].z0 + 1u].off &&
          SLC_PIANO.len == SMP_ZONES[SMP_SETS[0].z0 + 1u].n);

    /* All source ADPCM bytes: 3 * 8270 (PIANO padded) + 2 * 10474 (FLUTE). */
    check("factory PIANO+FLUTE ends after 45758 bytes, aliases have no separate data",
          SMP_ZONES[SMP_SETS[2].z0 + 1u].off +
          (SMP_ZONES[SMP_SETS[2].z0 + 1u].n + 1u) / 2u == 45758u);

    /* 4: PIANO HD user file keeps all 5 original zones; no forced factory identity. */
    snprintf(path, sizeof path, "%s.hdr", argv[1]);
    if (load(path, host_slots, SMP_USER_DATA) != (long)sizeof(smp_user_hdr_t)) {
        printf("sample: no %s (gen_samples.py --user-slot PIANO \"PIANO HD\" ...)\n", path);
        return 1;
    }
    snprintf(path, sizeof path, "%s.bin", argv[1]);
    if (load(path, (uint8_t *)host_slots + SMP_USER_DATA, SMP_USER_SIZE - SMP_USER_DATA) <= 0)
        return 1;
    smp_user_scan(0);
    {
        /* both from the same state (a fork each: the mix keeps state from render to render) */
        int32_t *ref = mmap(0, 2u * sizeof buf, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0), *usr = ref + FS * SECS;
        const smp_zone_t *z = &usr_zone[0][2];
        static const uint8_t SAME_ROOT[] = {1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1};
        uint32_t same = 0, common = 0;
        check("PIANO HD: USR1 holds 5 zones (roots 36 .. 84, 22,050 Hz, 0.75 s), 41 KB of a 79.5 KB slot",
              usr_nz[0] == 5u && usr_zone[0][0].root16 == 36 * 16 && usr_zone[0][4].root16 == 84 * 16 &&
              z->root16 == 60 * 16 && z->rate == 32768u && z->n == 16537u);
        if (ref == MAP_FAILED)
            return 1;
        for (i = 0; i < sizeof NOTES; i++) {
            render_child(0, NOTES[i], ref);
            render_child((int16_t)SMP_NSETS, NOTES[i], usr);
            memcpy(buf, usr, sizeof buf);
            if (SAME_ROOT[i]) {
                common++;
                same += !memcmp(ref, usr, sizeof buf) && rms(0, FS / 2u) > 100.0;
            }
            snprintf(path, sizeof path, "piano_hd_%02u", NOTES[i]);
            wav_out(dir, path);
        }
        snprintf(m, sizeof m, "PIANO HD: 5-zone user file remains byte-identical where factory and user select the same recording (%u/%u)",
                 same, common);
        check(m, common == 8u && same == common);
    }
    printf("sample: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
