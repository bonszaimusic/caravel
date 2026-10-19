/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Custom sound-port integration: compare dry PHASE to its retained old loop,
 * exercise real UI, storage, MOD, MIDI Learn and locks, including a 111-value
 * FUN10 file with engine locks and Bonszai FX events. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t legacy_fun10(project_store_t *wire, const project_t *q)
{
    project_store_t current;
    uint8_t *b = wire->raw;
    uint32_t pos = 68u, i, t, hash;
    if (!proj_pack(&current, q)) return 0;
    memcpy(wire, &current, sizeof *wire);
    b[66] = 111;
    for (t = 0; t < NTRK; t++) {
        uint32_t at = 68u + t * (P_COUNT + 2u + NSTEP * 9u);
        for (i = 0; i < 111u; i++)
            b[pos++] = current.raw[at + (i < 103u ? i : P_E0 + i - 103u)];
        memcpy(b + pos, current.raw + at + P_COUNT, 2u + NSTEP * 9u);
        pos += 2u + NSTEP * 9u;
    }
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    motion_store_t motion = q->motion;
    for (i = 0; i < motion.count; i++) {
        uint32_t id = MOTION_ID(&motion.event[i]);
        if (id >= P_E0 && id < P_COUNT)
            motion.event[i].param = (motion.event[i].param & MOTION_LOCK) | (id - (P_E0 - 103u));
    }
    memcpy(b + pos, &motion, sizeof motion); pos += sizeof motion;
    memset(b + pos, 0, PROJ_MIDI_OFF - pos);
    hash = proj_hash(b, sizeof *wire - 4u); memcpy(b + sizeof *wire - 4u, &hash, 4);
    return 1;
}

int main(void)
{
    int bad = 0, ok = 1;
    ui_power_on();
    track_t *t = TSEL;
    bad += check("sound slots: 114 values (0.3.0: FILT after them), separate from FM and PHASE FOLD", P_COUNT == 114 && P_E0 == 106 && P_FILT == 105 &&
        P_MORPH == 103 && P_PD_FB == 104 && P_ESYNC == 102 &&
        TP[P_MORPH].def == 0 && TP[P_PD_FB].def == 0 &&
        !strcmp(ENG_PHASE.edit[7].label, "FOLD") && ENG_PHASE.npresets == 6);
    set_engine_of(t, 2); t->engine = 2;
    for (uint32_t wave = 0; wave < 8u; wave++) for (uint32_t fold = 0; fold < 2u; fold++) {
        voice_t a = {0}, b; int32_t x[CTL], y[CTL];
        vmod_t m = {.pitch16 = 60 * 16, .inc = pitch_inc(60 * 16), .shape = 64 * 256,
                    .amp0 = 32767, .amp1 = 32767, .envq15 = 32767};
        t->p[P_E0] = wave; t->p[P_E1] = 8 - wave; t->p[P_E7] = fold ? 127 : 0;
        t->mo_cur = t->mo_prev = t->fb_cur = t->fb_prev = 0;
        phase_note_on(t, &a); b = a;
        for (uint32_t block = 0; block < 20u; block++) {
            memset(x, 0, sizeof x); memset(y, 0, sizeof y);
            uint32_t fg = fold ? pd_fold_gain(127, m.inc) : 0;
            phase_loop(t, &a, x, CTL, &m, fg);
            phase_loop_ext(t, &b, y, CTL, &m, fg);
            ok &= !memcmp(x, y, sizeof x) && !memcmp(a.ph, b.ph, sizeof a.ph);
        }
    }
    bad += check("PHASE extensions zero: all waves and FOLD match retained legacy loop", ok);

#if FELUCCA_PHASE_FB
    {
        voice_t v[3] = {{0}}; int32_t x[3][CTL]; uint64_t delta[2] = {0};
        vmod_t m = {.pitch16 = 48 * 16, .inc = pitch_inc(48 * 16), .shape = 64 * 256,
                    .amp0 = 32767, .amp1 = 32767};
        t->p[P_E0] = t->p[P_E1] = t->p[P_E4] = t->p[P_E5] = t->p[P_E6] = t->p[P_E7] = 0;
        t->p[P_E2] = 60; t->p[P_E3] = 0; t->mo_cur = t->mo_prev = 0;
        for (uint32_t k = 0; k < 3u; k++) phase_note_on(t, &v[k]);
        for (uint32_t b = 0; b < 30u; b++) {
            for (uint32_t k = 0; k < 3u; k++) {
                t->fb_cur = t->fb_prev = k * 55 * 256;
                memset(x[k], 0, sizeof x[k]); phase_dispatch(t, &v[k], x[k], CTL, &m);
            }
            for (uint32_t i = 0; i < CTL; i++) for (uint32_t k = 0; k < 2u; k++)
                delta[k] += abs(x[k + 1u][i] - x[0][i]);
        }
        bad += check("PHASE feedback: both depths alter the dry waveform and retain history", delta[0] > 10000 &&
            delta[1] > 10000 && (v[1].s[2] || v[1].s[3]) && (v[2].s[2] || v[2].s[3]));
    }
#endif

    /* New timbres differ at fixed controls; extreme wave/line/fold/sub cases are bounded. */
    ok = 1;
    for (uint32_t wave = 0; wave < 8u; wave++) for (uint32_t note = 12; note <= 108; note += 48) {
        voice_t v = {0}; int32_t x[CTL]; uint64_t energy = 0;
        vmod_t m = {.pitch16 = note * 16, .inc = pitch_inc(note * 16), .shape = 64 * 256,
                    .amp0 = 32767, .amp1 = 32767};
        t->p[P_E0] = wave; t->p[P_E1] = 8; t->p[P_E2] = 127;
        t->p[P_E3] = 127; t->p[P_E4] = 127; t->p[P_E5] = 1; t->p[P_E6] = 127; t->p[P_E7] = 127;
        t->mo_cur = t->mo_prev = FELUCCA_MORPH ? 127 * 256 : 0;
        t->fb_cur = t->fb_prev = FELUCCA_PHASE_FB ? 127 * 256 : 0;
        phase_note_on(t, &v);
        for (uint32_t block = 0; block < 60u; block++) {
            memset(x, 0, sizeof x); phase_dispatch(t, &v, x, CTL, &m);
            for (uint32_t i = 0; i < CTL; i++) {
                ok &= x[i] >= -131070 && x[i] <= 131070;
                energy += (uint32_t)(x[i] < 0 ? -x[i] : x[i]);
            }
            if (block % 9u == 0) t->p[P_E7] = t->p[P_E7] ? 0 : 127;
        }
        ok &= energy > 0;
    }
    bad += check("PHASE MORPH/feedback plus FOLD/RING/SUB: all waves and pitches bounded", ok);

#if FELUCCA_MORPH
    ok = 1;
    for (uint32_t engine = 0; engine <= 3u; engine++) {
        if (engine == 1u) continue;
        ui_power_on(); t = TSEL; set_engine_of(t, engine); t->engine = engine;
        int32_t out[2 * CTL]; uint64_t e = 0;
        song.master_q12 = 4096; t->p[P_MORPH] = 127;
        for (uint32_t b = 0; b < 8u; b++) mix_block(out, CTL);
        trk_note_on(t, 60, 100);
        for (uint32_t block = 0; block < 100u; block++) {
            mix_block(out, CTL);
            for (uint32_t i = 0; i < 2u * CTL; i++) e += abs(out[i]);
        }
        if (!(e > 1000 && t->mo_cur == 127 * 256)) fprintf(stderr,"engine %u energy %llu morph %u requested %d\n",engine,(unsigned long long)e,t->mo_cur,t->p[P_MORPH]);
        ok &= e > 1000 && t->mo_cur == 127 * 256;
        t->p[P_MORPH] = 0;
        for (uint32_t block = 0; block < 45u; block++) mix_block(out, CTL);
        ok &= t->mo_cur == 0;
    }
    bad += check("MORPH: ANALOG PHASE LOFI sound, block slew reaches both endpoints", ok);
    ok = 1;
    for (uint32_t ph = 0; ph < 0xF0000000u; ph += 0x0F000000u)
        ok &= an_morph(0, ph, 1u << 24) == osc_saw(ph, 1u << 24) &&
              an_morph(768, ph, 1u << 24) == osc_sine(ph);
    bad += check("ANALOG MORPH endpoints: original SAW to SIN", ok);
#ifdef LOFI_FORMULA_STATE
    {
        ui_power_on(); t = TSEL; set_engine_of(t, 3); t->engine = 3;
        voice_t a = {0}, b; int32_t x[CTL], y[CTL];
        vmod_t m = {.pitch16 = 60 * 16, .inc = pitch_inc(60 * 16), .shape = 64 * 256,
                    .amp0 = 32767, .amp1 = 32767};
        ok = 1;
        for (uint32_t wave = RW_BYTE; wave <= RW_FLOAT; wave++) {
            t->p[P_E1] = wave; lofi_note_on(t, &a); b = a;
            for (uint32_t block = 0; block < 20u; block++) {
                memset(x, 0, sizeof x); memset(y, 0, sizeof y);
                t->mo_cur = t->mo_prev = 0; lofi_render(t, &a, x, CTL, &m);
                t->mo_cur = t->mo_prev = 127 * 256; lofi_render(t, &b, y, CTL, &m);
                ok &= !memcmp(x, y, sizeof x) && !memcmp(&a, &b, sizeof a);
            }
        }
        t->p[P_E1] = RW_TRI; memset(y, 0, sizeof y); lofi_render(t, &b, y, CTL, &m);
        ok &= ((uint32_t)b.s[5] & 0xFFFF0000u) != LOFI_FORMULA_STATE;
        bad += check("MORPH leaves BYTE/FLOAT bit-identical; live return to chip initializes state", ok);
    }
#endif
#endif
    ui_power_on(); t = TSEL; set_engine_of(t, 2); t->engine = 2;
    t->p[P_MORPH] = FELUCCA_MORPH ? 70 : 0; t->p[P_PD_FB] = FELUCCA_PHASE_FB ? 55 : 0;
    t->p[P_E7] = 93;
    song.mch[0] = 16; song.g[G_ROUTE] = 5;
    motion_set_lock(t, 0, P_E7, 80);
    project_t a, b; project_store_t wire;
    project_capture(&a);
    ok = proj_pack(&wire, &a) && proj_import(&b, &wire, sizeof wire) &&
        b.t[0].p[P_MORPH] == t->p[P_MORPH] && b.t[0].p[P_PD_FB] == t->p[P_PD_FB] &&
        b.t[0].p[P_E7] == 93 && b.mch[0] == 16 && MOTION_ID(&b.motion.event[0]) == P_E7;
    bad += check("new project: MORPH/feedback/FOLD, engine lock and MIDI channel round trip", ok);
    ok = legacy_fun10(&wire, &a) && proj_import(&b, &wire, sizeof wire) &&
        b.t[0].p[P_MORPH] == 0 && b.t[0].p[P_PD_FB] == 0 && b.t[0].p[P_FILT] == 0 &&
        b.t[0].p[P_E7] == 93 && MOTION_ID(&b.motion.event[0]) == P_E7 && b.mch[0] == 16;
    bad += check("old 111-value FUN10: new values zero, FOLD/engine locks/MIDI preserved", ok);
#ifdef MOTION_FX_FIRST
    motion_fx_put(t, 0, PF_LPF, 173, 1); motion_fx_put(t, 1, PF_LPF, 201, 0);
    project_capture(&a);
    ok = legacy_fun10(&wire, &a) && proj_import(&b, &wire, sizeof wire) &&
        b.motion.count == a.motion.count && b.motion.rsv[1] == 0xF1u;
    for (uint32_t i = 0; ok && i < a.motion.count; i++)
        ok &= !memcmp(&a.motion.event[i], &b.motion.event[i], sizeof(motion_event_t));
    bad += check("old Bonszai 111-value FUN10: engine lock and fine FX edges preserved together", ok);
#endif
    ok = up_store(0, "SOUND PORT") == 3;
    int32_t mo = t->p[P_MORPH], fb = t->p[P_PD_FB];
    t->p[P_MORPH] = t->p[P_PD_FB] = t->p[P_E7] = 0; up_load(0);
    ok &= t->p[P_MORPH] == mo && t->p[P_PD_FB] == fb && t->p[P_E7] == 93;
    bad += check("user preset: new controls independent of FOLD", ok);
    ok = ml_learn(30, 0, P_MORPH) == 1u && ml_learn(31, 0, P_PD_FB) == 1u &&
        ml_id(ml_code(P_E7)) == P_E7;
    bad += check("MIDI Learn: new common IDs and old engine-relative codes", ok);
    t->p[P_M1SRC] = MS_MODW; t->p[P_M1DST] = MD_PD_FB; t->p[P_M1AMT] = 63;
    t->mw = 127; fb = t->p[P_PD_FB];
    ok = mod_begin(t) && t->p[P_PD_FB] == (FELUCCA_PHASE_FB ? 127 : 0);
    mod_end(t); ok &= t->p[P_PD_FB] == fb && t->p[P_E7] == 93;
    bad += check("MOD feedback: appended destination, restores base, FOLD independent", ok);
    {
        static const uint32_t ids[] = {
#if FELUCCA_MORPH
            P_MORPH,
#endif
#if FELUCCA_PHASE_FB
            P_PD_FB,
#endif
        };
        ok = 1;
        for (uint32_t i = 0; i < NELEM(ids); i++) {
            uint32_t id = ids[i];
            ui_power_on(); t = TSEL; set_engine_of(t, 2); t->engine = 2;
            motion_clear(t); t->p[id] = 17;
            ok &= motion_param(id) && motion_engine_id(id) && ev_id_ok(t, id);
            ok &= motion_set_event(t, 0, id, 25) == 0 && motion_set_lock(t, 1, id, 80) == 0;
            motion_set_enabled(t, 1); motion_begin();
            motion_step(t, 0, &motion, 1); ok &= t->p[id] == 25;
            motion_step(t, 1, &motion, 1); ok &= t->p[id] == 80;
            motion_step(t, 2, &motion, 1); ok &= t->p[id] == 25;
            motion_end(); ok &= t->p[id] == 17;
            project_capture(&a);
            ok &= proj_pack(&wire, &a) && proj_import(&b, &wire, sizeof wire) &&
                  b.motion.count == 2 && MOTION_ID(&b.motion.event[0]) == id &&
                  MOTION_ID(&b.motion.event[1]) == id && (b.motion.event[1].param & MOTION_LOCK);
            motion_clear(t); song.rec = 1; seq_start(); seq_tick(t, CTL);
            t->p[id] = 31; motion_capture(t, id, 31);
            ok &= motion_count(t) == 1 && motion.event[0].value == 31;
            seq_stop(); ok &= t->p[id] == 17;
            ok &= motion_set_event(t, 2, P_REV, 20) == 0;
            ok &= motion_sound_loaded(t, 2, 12) == 1 && motion_count(t) == 1 &&
                  MOTION_ID(&motion.event[0]) == P_REV;
            t->eng_req = 12; ok &= !ev_id_ok(t, id);
        }
        bad += check("MORPH/feedback: real recording, playback, locks, base restore, project and engine guard", ok);
    }
    printf("JIANT sound stage: %s\n", bad ? "FAILED" : "all checks passed");
    return bad != 0;
}
