/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share one budget of NVOICE sounding voices (voice.c).
 * FORMAT 2 retains ANALOG, PHYS, SEM, HADES, DRONE and ENSEMBLE.
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>
#include "voice_policy.h"

/* Hot DSP may run from the existing executable RAM window on the FM-1.
 * Keep host/WASM placement unchanged; no DSP arithmetic depends on it.
 * The build places these roots after LLVM inlining and bridges far calls. */
#ifndef CARAVEL_AUDIO_RAM
#define CARAVEL_AUDIO_RAM 0
#endif
#if CARAVEL_AUDIO_RAM && defined(__PI32V2__)
#define CARAVEL_AUDIO_HOT __attribute__((noinline))
#else
#define CARAVEL_AUDIO_HOT
#endif

/* Product identity. Caravel is a fork of Felucca 1.6.1 (NOTICE, LICENSING.md); these are display strings only. Identifiers
 * (FELUCCA_*, felucca.c), package markers and the Felucca-Engines names retain their origin. FORMAT 2
 * project/preset data is deliberately incompatible with older firmware. "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments:
 * they appear only as the origin of the code. */
#ifndef BRAND_NAME
#define BRAND_NAME "Caravel"
#define BRAND_UPPER "CARAVEL"
#define BRAND_BASE "Felucca 1.6.1"
#define BRAND_MAKER "bonszaimusic"
#endif

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* voices per part, and the budget shared by all parts */
#define NPART 4                  /* synth parts: tracks 1..4 */
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#include "lfo2_types.h"
#define NSTEP 64
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
#ifndef FELUCCA_SLICE
#define FELUCCA_SLICE 0          /* the SLICE engine (eng_slice.c), engine 13; FELUCCA_SLICE=0 builds without it */
#endif
/* Experimental lean reverb: omit ROOM/SPRING DSP, retain saved IDs
 * 0 and 1 as HALL aliases. Default 0 preserves stock Caravel behavior. */
/* HOME ACTION/RANDOM prototypes are opt-in. Published firmware is unchanged. */
#ifndef CARAVEL_ACTION_BANKS
#define CARAVEL_ACTION_BANKS 0
#endif
#ifndef FELUCCA_RANDOM_MORPH
#define FELUCCA_RANDOM_MORPH 0
#endif
#ifndef CARAVEL_UX_EVOLVE
#define CARAVEL_UX_EVOLVE 0
#endif
/* E85: FORMAT 2 and its six-engine profile are the new basis. */
#ifndef CARAVEL_SLIM
#define CARAVEL_SLIM 1
#endif
/* FORMAT 2 (docs/FORMAT_2_ENTWURF): one new revision of everything stored. Needs CARAVEL_SLIM (six engines, stored as 0..5).
 * Every commit record carries the tag ST_FMT2 (storage.c); an object without it is OLD DATA: it is never loaded,
 * never overwritten without the owner's say (the first-start notice, ui_f2.c), only listed and erased. Projects are "FUNW". */
#ifndef CARAVEL_FORMAT2
#define CARAVEL_FORMAT2 1
#endif
#if !CARAVEL_FORMAT2 || !CARAVEL_SLIM
#error "Caravel 1.0 requires FORMAT 2 and the six-engine profile"
#endif
/* Developer-only ELASTIC P1. Production/release profiles remain unchanged. */
#ifndef CARAVEL_ELASTIC_P1
#define CARAVEL_ELASTIC_P1 0
#endif
/* P1.2 four-visible-cell pilot; keep existing P1.1 unchanged by default. */
#ifndef CARAVEL_ELASTIC_PLAYABLE
#define CARAVEL_ELASTIC_PLAYABLE 0
#endif
#if CARAVEL_ELASTIC_PLAYABLE && !CARAVEL_ELASTIC_P1
#error "ELASTIC playable pilot requires CARAVEL_ELASTIC_P1=1"
#endif
#if CARAVEL_ELASTIC_P1 && (!CARAVEL_ACTION_BANKS || !FELUCCA_HOME_PERF)
#error "ELASTIC P1 prototype needs HOME performance and ACTION banks"
#endif
#ifndef CARAVEL_REVERB_MINIMAL
#define CARAVEL_REVERB_MINIMAL 0
#endif
/* Opt-in granular STASIS experiment: default release and project formats are unchanged. */
#ifndef CARAVEL_STASIS_EXPAND
#define CARAVEL_STASIS_EXPAND 0
#endif
/* Private stage-1 probe. OFF preprocesses to the original STASIS paths.
 * ON bypasses the retired ADD/BLEND bank and uses the full PCM source ring. */
#ifndef CARAVEL_STASIS2
#define CARAVEL_STASIS2 0
#endif
#if CARAVEL_STASIS2 && (!CARAVEL_REVERB_MINIMAL || !CARAVEL_STASIS_EXPAND)
#error "STASIS2 REGION requires minimal reverb and expanded capture"
#endif
/* 1.0 AFTERBURNER, a separate post-reverb tail. RC build escape hatch:
 * -DCARAVEL_AFTERBURNER=0 removes all buffers, DSP and editing UI. */
#ifndef CARAVEL_AFTERBURNER
#define CARAVEL_AFTERBURNER 1
#endif
#if CARAVEL_AFTERBURNER
static struct {
    int16_t on, time, amt, damp; /* ON/OFF; RT60 10..60s; wet amount; high-frequency loss */
} afterburner = {0, 30, 32, 70}; /* OFF preserves the sound of existing projects */
#endif
/* GRAV was rejected by owner listening. Never accidentally rebuild its
 * discarded DSP with a stale local test-firmware flag. */
#if defined(CARAVEL_GRAVITY_LITE) && CARAVEL_GRAVITY_LITE
#error "GRAV has been removed; omit -DCARAVEL_GRAVITY_LITE=1"
#endif
#if CARAVEL_STASIS_EXPAND && !CARAVEL_REVERB_MINIMAL
#error "Expanded STASIS requires the experimental minimal reverb implementation"
#endif
#if CARAVEL_STASIS_EXPAND
/* Grain window/quality/scan/reverse remain RAM-only for this experimental
 * pilot; TONE below is project-persistent through the FUNW FX extension.
 * int16_t matches the firmware's page_desc / param_turn ABI exactly:
 * FX > GRAIN and ACTION > STASIS edit the same fields, never shadow copies.
 * Existing STR/MOTN/LAYER continue to live only in song.g. */
/* STASIS grain sizes. Flag off: 0..2 = 93/186/372 ms (4096<<g output samples), default 186 ms.
 * STASIS2: four steps 0..3 = 23/46/93/186 ms (1024<<g), default 186 ms (the 372 ms step is gone, a very short grain
 * is worth more than a very long one; measured: no extra cost per sample); the stored value is the 2 bit GRAIN field
 * of the A6 project extension (unreleased, so its meaning may change: index 2 was 186 ms, now 93 ms). */
#if CARAVEL_STASIS2
#define ST_GRAIN_BASE 1024u
#define ST_GRAIN_MAX 3
#define ST_GRAIN_DEFAULT 3u
#define ST_GRAIN_ENV_SHIFT 6u
#else
#define ST_GRAIN_BASE 4096u
#define ST_GRAIN_MAX 2
#define ST_GRAIN_DEFAULT 1u
#define ST_GRAIN_ENV_SHIFT 4u
#endif
static struct {
 int16_t grain;    /* 0=4096, 1=8192, 2=16384 output samples */
 int16_t quality;  /* 0=44.1k, 1=22.05k, 2=11.025k capture */
 int16_t scan;     /* legacy MOD destination retained; STASIS2 names it POS */
 int16_t reverse;  /* 0 forward, 1 reverse per grain */
#if CARAVEL_STASIS2
 int16_t length;   /* source REGION LENGTH, 0..127, runtime-only */
#endif
} st_expand = {.grain=ST_GRAIN_DEFAULT,.quality=0u,.scan=0u,.reverse=0u
#if CARAVEL_STASIS2
 ,.length=64u
#endif
};
#if CARAVEL_STASIS2
/* STASIS hall controls, runtime only (fx.c): FEED = grains into the hall (TEXTURE K4), HALL = the hall's return level
 * (TANK K4). 127 = 100 % = the sound as before; the project has no free bit, so a load starts at 100 %. */
static volatile uint8_t st_hall_feed = 64u, st_hall_ret = 64u;
#endif
/* Global STASIS TONE (PLAY > SPACE > STASIS K4): 64 preserves the
 * approved STASIS sound. Independent of track FILTER / HALL DAMP.
 * Persisted with AFTER in the fixed-length FORMAT-2 FUNW FX extension. */
static int16_t stasis_tone = 64;
static int32_t stasis_tone_sm = 64 * 256; /* click-free control-block slew */
#endif
#ifndef FELUCCA_FM4
#define FELUCCA_FM4 0            /* the DIGITAL engine (eng_digital.c, four-operator FM): kept in the tree, not built
                                  * by default; replaced by FM6, its sounds convert (fm4_convert.c) */
#endif
#ifndef FELUCCA_CPU2
#define FELUCCA_CPU2 1           /* the second core (src/felucca.c): 1 = built in, MENU > 2ND CORE ON lets it render
                                  * some parts; 0 = none of it */
#endif
#define C1_NP (FELUCCA_CPU2 ? NPART : 1)   /* the copies of the state a part's render writes that parts shared before the
                                            * second core (mod.c, voice.c SPREAD, eng_formant.c): one each with two cores,
                                            * one for all with one, as up to 1.6 */
#define C1_PI(t) ((uint32_t)((t) - trk) % C1_NP)   /* a part's copy */
/* Scratch a part's render writes and reads again inside ONE mix_part_inner call (fx.c: dx_sides, pd_scratch, spread_buf, mod_w) is
 * written before it is read in every call and not read in the next one, so it needs one copy per core that renders at the same time,
 * not one per part: C1_NS copies, picked by C1_CI(t), the core that renders part t this block (track_t.c1i, set by fx.c mix_block). */
#if FELUCCA_CPU2
#define C1_NS 2
#define C1_CI(t) ((uint32_t)(t)->c1i)
#else
#define C1_NS 1
#define C1_CI(t) 0u
#endif
#define ENGI_SEM 14u             /* append-only; 13 stays reserved when SLICE is disabled */
#define ENGI_HADES 15u           /* appended; stable across optional builds */
#define ENGI_DRONE 16u           /* experimental coupled drone; append-only */
#define ENGI_ENSEMBLE 17u        /* strings / choir / dark texture with a built-in ensemble (eng_ensemble.c); append-only */
#define NENGINES 18u
#define ENGI_FM6 12u
#define ENGI_DIGITAL 1u          /* reserved without FELUCCA_FM4: never selectable (eng_ok), its sounds load as FM6 */
#if CARAVEL_SLIM                                       /* E27, E29: six engines stay: ANALOG PHYS SEM HADES DRONE ENSEMBLE */
#define NENG_SHOWN 6
#define ENG_FALLBACK 0u                                 /* a removed engine's sounds play as ANALOG */
#else
#define NENG_SHOWN (NENGINES - !FELUCCA_FM4 - !FELUCCA_SLICE)
#define ENG_FALLBACK ENGI_FM6                           /* (DIGITAL without FELUCCA_FM4: its sounds play as FM6) */
#endif   /* the engines one can pick: PRESETS, the EDIT layer, the editor,
                                                * in the display order of engines.c ENGINE_ORDER */
#define UP_SLOTS 32u             /* user presets (upreset.c) */

/* ------------------------------------------------------- parameters --- */
/* the engine interface (Felucca-Engines, MIT: engines/, a git submodule): param_desc_t and F_*, voice_t,
 * vmod_t, preset_t with FX() / PAT(), engine_t, NELEM */
#include "../engines/src/felucca_engine.h"

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ED_FX,     /* P_ED_FX: no sound parameter any more (nothing reads it); Caravel keeps the
                                                * track's LATCH in it (P_LATCH below) */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH,      /* SLICER insert (slicer.c) */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 4 slots of SRC, DST, AMT; */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* new common parameters go just before P_E0 (user presets */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* and projects map by count) */
    P_M4SRC, P_M4DST, P_M4AMT,
    P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL, P_FM1_LEVEL,
    P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL, P_FM2_LEVEL,
    P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL, P_FM3_LEVEL,
    P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL, P_FM4_LEVEL,
    P_CHRD, P_VOIC,                            /* chord keys (chord.c): one key plays a chord; its voicing */
    P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7,   /* DRUM lane levels (eng_drum.c, #97): KICK .. BELL */
    P_LSYNC, P_LTRIG, P_LPOL,                  /* 1.2 (#154): LFO SYNC (OFF, a note value), TRIG (NOTE, FREE), POL (BI, UNI) */
    P_SQNT,                                    /* 1.2 (#162): QUANTIZE, the steps' recorded / nudged timing (ON: none) */
    P_SPRD,                                    /* (#148): SPREAD, the voices of a POLY / UNISON track around PAN (fx.c) */
    P_ITYPE, P_IA, P_IB, P_IC, P_IMIX,         /* 1.5 (#78, #177): the INSERT, its TYPE, three values and MIX (fx.c) */
    P_FTYPE,                                   /* 1.5 (#104): ANALOG's filter TYPE, LP (as before) / BP / HP (eng_analog.c) */
    P_ESYNC,                                   /* 1.5 (#175): ENV SYNC: ATK DEC REL as note values of the tempo (voice.c) */
    P_MORPH, P_PD_FB,                         /* Custom sound slots; old common IDs and FM values retained. */
    P_FILT,                                    /* Caravel 0.3.0: the track's FILTER, one bipolar knob (LP / OFF / HP; fx.c track_filter);
                                                * the last common id there is room for: P_COUNT 114 = MOTION_FX_FIRST, 1 spare byte left in FUN10 */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};

/* Caravel: LATCH (seq.c, CHORD page): 0 OFF, 1 NEW, 2 STACK; any other stored value counts as OFF. It lives in the
 * retired P_ED_FX slot (stored by every format and the editor protocol already, so no layout change) */
#define P_LATCH P_ED_FX

/* Caravel CHORD SOURCE / EXT (the settings; chord.c's CHORD EXT resolver and seq.c's hooks read them, once per event: seq.c ext_cfg): the track's CHORD SOURCE, EXT and
 * OCT live in the retired DIGITAL operator-4 envelope ids ATK, DEC, REL (76, 77, 79: the three operator values whose
 * default is 0, so a fresh FORMAT 2 project starts with the function off). Pre-1.0 backups are not imported. Without
 * FELUCCA_FM4 only: with it these ids are real operator values, so the aliases are not defined and any use is a
 * compile error, not a silent clash (FM4=1 is a host-test / manual build, not shipped).
 *   CSRC  stored 0 OFF, 1 HARM, 2..5 T1..T4 (N_CSRC names them);
 *         a stored value outside the range counts as OFF (params.c param_fit), never as the maximum
 *   CEXT  stored 0 = 7+9 (the default), 1 = 7, 2 = 7+9+13; shown 7 / 7+9 / 7+9+13 (chordext_disp, chordext_stored)
 *   COCT  -2 .. +2 octaves, 0 the default
 * param_kept (a sound load leaves them), motion_param excludes them, fm4_apply / fm4_load_preset / up_load keep them
 * across a DIGITAL conversion. CHORDSET_ID(i): i is one of the three (0 with FM4) */
#if !FELUCCA_FM4
#define P_CSRC P_FM4_ATK
#define P_CEXT P_FM4_DEC
#define P_COCT P_FM4_REL
_Static_assert(P_FM4_ATK == 76 && P_FM4_DEC == 77 && P_FM4_SUS == 78 && P_FM4_REL == 79 && P_FM4_LEVEL == 80 &&
               P_CHRD == 81 && P_CSRC == 76 && P_CEXT == 77 && P_COCT == 79,
               "CHORD SOURCE / EXT / OCT are the retired ids 76, 77, 79");
#define CHORDSET_ID(i) ((i) == P_CSRC || (i) == P_CEXT || (i) == P_COCT)
/* STASIS2: the track's DRY, how much of its own sound is heard beside the sends (PLAY > SPACE > TANK K3), lives in the retired
 * DIGITAL operator-4 LEVEL id 80: inert without FELUCCA_FM4, default 127 = 100 %, so a project that never touched it is
 * byte-identical and the sound is unchanged. It is a channel strip value like LEVEL and PAN (ui.c param_kept). */
#if CARAVEL_STASIS2
#define P_DRY P_FM4_LEVEL
_Static_assert(P_FM4_LEVEL == 80, "DRY is the retired id 80");
#define DRY_ID(i) ((i) == P_DRY)
/* Independent STASIS ring send (0..127 = 0..100%), stored in retired DIGITAL operator-4 SUS id 78.
 * Legacy FOLLOW values are migrated only by the project wire reader. */
#define P_SSEND P_FM4_SUS
_Static_assert(P_FM4_SUS == 78, "the STASIS send is the retired id 78");
#endif
/* EXT's stored order (0 = 7+9, 1 = 7, 2 = 7+9+13) <-> the order the UI shows (0 = 7, 1 = 7+9, 2 = 7+9+13): the swap of
 * 0 and 1. A stored value outside 0..2 reads as the default 7+9 (shown 1); a shown index outside 0..2 stores 7+9 (0) */
static inline uint32_t chordext_disp(uint32_t stored) { return stored == 1u ? 0u : stored == 2u ? 2u : 1u; }
static inline uint32_t chordext_stored(uint32_t disp) { return disp == 0u ? 1u : disp == 2u ? 2u : 0u; }
#else
#define CHORDSET_ID(i) 0
#endif
#ifndef DRY_ID
#define DRY_ID(i) 0
#endif
#if CARAVEL_STASIS2 && FELUCCA_FM4
#error "STASIS2 DRY lives in the retired DIGITAL id 80 and needs FELUCCA_FM4=0"
#endif
/* Clear ephemeral live-source collection and follower memory when an accepted project
 * replaces the tracks. HARM is resolved directly from the bus; nothing new is stored. */
static void chordext_reset_runtime(void);

/* Caravel CHORD EXT status for the UI (chord.c fills it; read-only, safe from the main loop). state: what K4
 * shows; why: the second text line; src: the stored SOURCE; the source chord (root + mask for chord_name) when one is
 * known (AMBIG keeps the old one); pc[]: the pool's pitch classes in priority order 7, 9, 13 (npc of want available);
 * mono: the follower plays one note at a time (MONO/LEGATO/UNISON, shown "1 OF n"). EXT_INFO_HOOK(f, o), if a test
 * defines it, may fill o and return 1 to override (host UI renders). */
enum { EXT_ST_OFF, EXT_ST_NA, EXT_ST_WAIT, EXT_ST_WAITBUS, EXT_ST_AMBIG, EXT_ST_EMPTY, EXT_ST_OK };
enum { EXT_WHY_NONE, EXT_WHY_SELF, EXT_WHY_SRCEXT, EXT_WHY_KIT, EXT_WHY_PAIR, EXT_WHY_PLAY, EXT_WHY_BUSOFF, EXT_WHY_KEPT, EXT_WHY_INSRC };
typedef struct {
    uint8_t state, why, src, have_chord, root, npc, want, mono;
    uint16_t mask;
    uint8_t pc[3];
} ext_info_t;
static void ext_info(uint32_t f, ext_info_t *o);

#define FELUCCA_DRUMX 1
#define DX_KIT_ID 12u
#define ML_EXTRA_N 48u
static uint32_t ml_extra[ML_EXTRA_N];
static uint16_t ml_virtual[16];
#if CARAVEL_FORMAT2
static uint8_t ml_base[32]; /* independent of favorites and removed Drum engine */
#endif

#define FELUCCA_MORPH 1
#define FELUCCA_PHASE_FB 1

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_SYNC, G_ROUTE, G_INFO,   /* G_ROUTE: MIDI IN, 0 CH1-4 (channels 1..4 -> parts 1..4, 5..16 ignored), 1 SEL;
                                        * 1.2: 2..4 CH5-8 CH9-12 CH13-16 (seq.c midi_base); 1.5.5: 5 TRACK (song.mch) */
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_ENGSEL, G_ENGGO,          /* no page: the editor switches the engine with a SET of G_ENGSEL; G_ENGGO is
                                 * unused (ids are fixed by the formats and the protocol) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING, 2 HALL (former default), 3 STASIS (1.0 new-project default) (fx.c). Was
                                 * G_DRCH, the GM drum part's MIDI channel (inert since 1.0, never read); projects
                                 * of formats before FUN7 load it as ROOM (project.c proj_rtype_room) */
    G_DRLVL, G_DRREV,           /* inert (label "-", on no page): the GM drum part they set is gone; kept
                                 * because the ids and G_COUNT are fixed by the formats and the protocol (only
                                 * the import of an old project reads them: proj_drums_to_part) */
    G_COUNT
};

#if CARAVEL_REVERB_MINIMAL
/* Reuse three retired, previously inert globals; project/protocol IDs stay fixed. */
#define G_STMODE G_SYNC
#define G_STAMT G_INFO
#define G_STTIME G_ENGGO
#endif

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
/* PHYS MODEL DRUM (MODEL 4, before 1.0) -> the DRUM engine, its E values in place: {MODEL, TUNE, TONE, DECY,
 * SNAP, ACC, KICK 0..127, PERC 0..127} -> {KIT, TUNE, TONE, DECY, SNAP, ACC, KICK 0..1, DRV 0}. 1 = it was one
 * (its engine is ENGI_DRUM now); projects (project.c) and user presets (upreset.c) */
static int drum_from_phys(uint32_t engine, int16_t *e)
{
    if (engine != ENGI_PHYS || e[0] != 4)
        return 0;
    e[0] = (int16_t)((e[7] < 0 ? 0 : e[7] > 127 ? 127 : e[7]) >> 5);   /* PERC -> KIT: STD HAND CYM H+CYM */
    e[6] = (int16_t)(e[6] >= 64);                                       /* KICK: PUNCH, ROUND */
    e[7] = 0;
    return 1;
}

/* SAMPLE SET 4 was PERC, the GM-mapped drum kit (tools/gen_samples.py); retired after 1.0.2: its index stays
 * (SET / GRAIN SRC 4 is an alias of PIANO, USR1..3 keep 5..7). A sound that selected it is the DRUM engine with
 * its default kit (eng_drum.c DRUM_PRESETS[0]: the same GM key map, so its patterns still play as drums): its E
 * values become the kit's, the rest of the sound (mix, sends, matrix, ..) stays. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c proj_perc), user presets (upreset.c up_migrate), factory preset 4 and
 * favourites (ui.c, settings_persist.c). Idempotent */
#define ENGI_SAMPLE 4u
#define SMP_SET_PERC 4u
#define DRUM_KIT_E {0, 64, 70, 64, 64, 100, 0, 0}   /* {KIT STD, TUNE, TONE, DECY, SNAP, ACC, KICK PUNCH, DRV} */
static int drum_from_perc(uint32_t engine, int16_t *e)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    uint32_t i;
    if (engine != ENGI_SAMPLE || e[0] != (int16_t)SMP_SET_PERC)
        return 0;
    for (i = 0; i < 8u; i++)
        e[i] = KIT[i];
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
/* voice_t, vmod_t, preset_t, engine_t: felucca_engine.h (above) */

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
#define NLANE 8                  /* drum lanes of a step (the DRUM engine's: eng_drum.c DRUM_LANE_NOTE) */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide; drum hits */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t hit;                 /* bit l: lane l hits (its GM note, on any engine): the DRUM grid */
    uint8_t acc;                 /* bit l: that hit is accented (velocity 127) */
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent; 1.5.5: 102..112 a condition */
} step_t;
typedef struct {                 /* a step as formats 1..4 (projects to FUN4) stored it: no hits */
    uint8_t note[4];
    uint8_t n, time, flags, vel;
} step8_t;

/* Probability keeps zero-initialized and legacy patterns at 100%. */
/* 1.5.5 (#209): CONDITIONS, in the chance byte's free values (no format change): COND_FIRST + c - 1 is condition c
 * (1..NCOND): a:b (1:2 2:2 1:3 2:3 3:3 1:4 2:4 3:4 4:4) plays on loop a of every b of its track (seq.c seq_cond), FILL
 * while FILL is held (GLO + E4), !FILL while not. A step has a chance or a condition (one byte): a condition plays at
 * 100 %. Firmware before 1.5.5 refuses a project holding one (its proj_unpack wants 0..101): the slot reads empty there.
 * The knobs and the lists show the two as one value (step_chance_v): 0..100 %, then 101..100 + NCOND the conditions */
#define COND_FIRST 102u
#define NCOND 11u
#define COND_FILL 10u                    /* FILL, NOT FILL: the last two */
#define COND_NFILL 11u
#define CHANCE_V_MAX (100u + NCOND)
static uint32_t step_cond(const step_t *s)   /* its condition 1..NCOND, 0 none */
{
    return s->probability >= COND_FIRST && s->probability < COND_FIRST + NCOND ? s->probability - COND_FIRST + 1u : 0u;
}
static uint32_t step_chance(const step_t *s)
{
    return !s->probability ? 100u : s->probability <= 100u ? s->probability : s->probability == 101u ? 0u : 100u;
}
static void step_set_chance(step_t *s, uint32_t chance) { s->probability = (uint8_t)(chance >= 100u ? 0u : chance ? chance : 101u); }
static uint32_t step_chance_v(const step_t *s) { uint32_t c = step_cond(s); return c ? 100u + c : step_chance(s); }
static void step_set_chance_v(step_t *s, uint32_t v)
{
    if (v > 100u)
        s->probability = (uint8_t)(COND_FIRST - 101u + (v < CHANCE_V_MAX ? v : CHANCE_V_MAX));
    else
        step_set_chance(s, v);
}
/* RATCH: a NOTE step plays its notes and hits this many times (1..4), in equal parts of the step; every older
 * pattern holds 0 there, x1 */
static uint32_t step_ratchet(const step_t *s) { return ((s->flags & SF_RATCH) >> SF_RATCH_SH) + 1u; }
static void step_set_ratchet(step_t *s, uint32_t hits)
{
    s->flags = (uint8_t)((s->flags & ~SF_RATCH) | ((hits < 1u ? 0u : hits > 4u ? 3u : hits - 1u) << SF_RATCH_SH));
}
/* NUDGE (1.2, #162, D98): a step's timing, in 1/NUDGE_DIV of the track's DIV, -8..+7 (half a step early .. 7/16 late);
 * live recording keeps it, SEQ > AUTOMATION's NUDGE rows and STEP (steps held + PRESETS) set it, the track's QUANTIZE
 * OFF (P_SQNT, AUTOMATION's QUANTIZE row) plays it (seq.c seq_tick).
 * Four bits of the flags no other field uses: 4 (bit 3 of the nudge, its sign) and 32 64 128 (bits 0..2). 0 = on the
 * step: every older pattern. A user preset's pattern keeps none (upreset.c up_pat_norm) */
#define NUDGE_DIV 16
#define SF_NUDGE (4u | 0xE0u)
static int32_t step_nudge(const step_t *s)
{
    uint32_t u = (uint32_t)(s->flags >> 5) | (uint32_t)(s->flags & 4u) << 1;
    return (int32_t)(u ^ 8u) - 8;
}
static void step_set_nudge(step_t *s, int32_t v)
{
    uint32_t u = (uint32_t)(v < -8 ? -8 : v > 7 ? 7 : v) & 15u;
    s->flags = (uint8_t)((s->flags & ~SF_NUDGE) | (u & 7u) << 5 | (u >> 1 & 4u));
}
#define MOTION_MAX 128u          /* 1.2 (FUN10): 64 before */
/* Four tracks x64 steps fit one byte. Values retain their signed parameter range (-64..127: an int8_t, 1.2).
 * param: the P_* id (< 128: P_COUNT is at most 127, project.c), bit 7 (MOTION_LOCK, 1.1) a parameter lock: the value
 * sounds on that step only and goes back after it (motion.c motion_step); without it an automation event, the
 * value holds until another one changes it. One record per (track, step, id), of either kind */
#define MOTION_LOCK 0x80u
#define MOTION_ID(e) ((uint32_t)(e)->param & 0x7Fu)
/* Bonszai: fourteen master FX edges use the otherwise unused ids 114..127.
 * place keeps track/step; value is an unsigned 1/256-step phase, bit 7 of
 * param means release (not a lock). No larger RAM or project event pool. */
#define MOTION_FX_FIRST 114u
#define MOTION_IS_FX(e) (MOTION_ID(e) >= MOTION_FX_FIRST)
_Static_assert(P_COUNT <= MOTION_FX_FIRST, "FX event ids must not overlap sound parameters");
typedef struct { uint8_t place, param; int8_t value; } motion_event_t;
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
_Static_assert(sizeof(motion_store_t) == 388u, "motion disk layout (FUN10)");

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    uint8_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    voice_t v[NVOICE];
    /* LFO */
    uint32_t lfo_ph;
    uint16_t mo_cur, mo_prev, fb_cur, fb_prev; /* Q8: slewed at most 3 steps per audio block. */
    int32_t lfo_val;             /* Q15 */
    int32_t lfo_fade;            /* Q15 ramp after note-on */
    uint32_t lfo_rnd;
    int16_t lfo_prev;            /* the S&H value of the cycle before (mod.c SLEW glides from it to lfo_rnd's) */
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t latched;             /* HOLD: keep notes after release */
    /* arp runtime */
    uint32_t arp_pos;            /* q8 samples into the current arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp pitch, including MIDI 0; arp_gate bit 0 owns it */
    uint8_t arp_walk;            /* WALK: the place in the note list it stands on */
    uint8_t arp_ch[3];           /* CHORD: the other pitches; arp_gate bits 1..3 own them */
    uint8_t arp_gate;            /* sounding ARP owners, independent of pitch (uses prior alignment padding) */
    uint32_t arp_off;            /* q8 sample time of its note-off */
    /* sequencer */
    step_t step[NSTEP];
    uint32_t seq_pos;            /* q8 samples into the current step */
    uint16_t seq_idx;
    uint8_t seq_notes[4 + NLANE];   /* sounding seq notes (the step's notes, then its hits) */
    uint8_t seq_n;
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[NLANE];   /* do not trigger them again there (they sound already) */
    /* NUDGE (QUANTIZE OFF, seq.c seq_tick): a step plays at its start + its nudge. Latched as a step starts: */
    uint32_t nd_late;            /* the step playing now plays at this seq_pos + 1 (0: it played) */
    uint32_t nd_early;           /* the next step plays early, at this seq_pos + 1 of this one (0: on its start) */
    uint8_t nd_fired;            /* that step + 1 once it played early (its start then plays nothing), 0 = none */
    /* live recording of held notes (seq.c rec_hold): the steps they are held into become TIEs */
    uint8_t rh_n, rh_note[4];    /* recorded notes still held, 0 = none */
    uint8_t rh_start;            /* the step they were recorded into */
    uint8_t rh_ties;             /* TIE steps written after it */
    uint8_t rh_last;             /* the last of them; rh_bak: what it held (an early release puts it back) */
    step_t rh_bak;
    /* mono */
    uint8_t mono_stack[8];
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state (fx.c) */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST / INSERT tail) */
    uint8_t ins_run;             /* the INSERT still sounds (fx.c track_insert: its wet share above 0) */
    uint8_t tf_run;              /* the track's FILTER still runs (fx.c track_filter: a mode set, or fading out) */
    uint8_t cond_loop;           /* 1.5.5 (#209): the loop of its LEN playing (0..COND_LOOPS - 1, seq.c seq_tick) */
#if FELUCCA_CPU2
    /* the second core: a part's render state, its own, so the two cores' parts share nothing (fx.c mix_block); in
     * the pad that keeps sizeof(track_t) a multiple of 16 (0.3.0's tf_run and the 114th parameter took bytes of the pad) */
    uint8_t lfo_wrap;            /* the LFO's phase wrapped this block: its S&H is drawn after the parts (voice.c) */
    uint32_t esync_k[3];         /* ATK's step, DEC's and REL's coefficient (Q16) this block (voice.c esync_coef) */
    uint8_t spread_on;           /* SPREAD this block (fx.c mix_part, voice.c track_render) */
    uint8_t c1i;                 /* the core that renders the part this block (0 this one, 1 the second: fx.c mix_block), which copy of the scratch C1_NS it uses */
#else
    uint8_t rsv_sz[1];          /* (sizeof(track_t) a multiple of 16: below) */
#endif
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    uint8_t sp_alt;              /* SPREAD: the side the next voice starts on (alternating, voice.c voice_start) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
} track_t;
/* the audio ISR indexes trk[] with a multiply by a small immediate and a shift while the size is a multiple of 16; 1.5's
 * new track parameters (the INSERT, the filter TYPE, ENV SYNC) and state would make it no multiple (the constant
 * loaded first: +3 % on the ISR's static cost, target_budget.py): one pad, rsv_sz, keeps it */
_Static_assert(sizeof(track_t) % 16u == 0, "track_t: a multiple of 16 bytes (a cheap trk[] index on the target)");

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays; 3: SONG, A..D are its (seq.c song_key) */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
    uint8_t mch[NTRK];           /* 1.5.5 (#202): MIDI IN TRACK (G_ROUTE ROUTE_TRACK), each track's channel as stored: 0..15
                                  * the channel's distance up from the track's own (track i: channel (i + v) % 16, so 0, the
                                  * zeroed default, is T1..T4 on channels 1..4), MCH_OMNI (any channel no track has), MCH_OFF
                                  * (none); mch_get / mch_set. The project's, in FUN10's spare bytes (project.c PROJ_MIDI_OFF);
                                  * MENU > MIDI > T1..T4 CHANNEL */
} song_t;
#define ROUTE_SEL 1
#define ROUTE_TRACK 5                /* G_ROUTE: TRACK, each track on its own channel (song.mch) */
#define MCH_OMNI 16u
#define MCH_OFF 17u
#define MCH_N 18u                    /* channels 1..16, OMNI, OFF */

static track_t trk[NTRK];        /* the instrument: four parts */
static song_t song;
#if FELUCCA_PAIR_MORPH
#include "../../experiments/engine-pair-morph/pair_state.h"
#endif

#define TSEL (&trk[song.sel])
#if FELUCCA_HOME_PERF
#include "../../experiments/home-perf/home_state.h"
#endif
    /* the selected track */
/* MIDI IN TRACK: track i's channel 0..15, MCH_OMNI or MCH_OFF (song.mch), and set */
static inline uint32_t mch_get(uint32_t i)
{
    uint32_t v = song.mch[i % NTRK];
    return v < 16u ? (v + i) & 15u : v < MCH_N ? v : (i & 15u);
}
static inline void mch_set(uint32_t i, uint32_t ch) { song.mch[i % NTRK] = (uint8_t)(ch < 16u ? (ch - i) & 15u : ch < MCH_N ? ch : 0u); }

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) and the SLICER (slicer.c sl_enter) both time steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));



#if FELUCCA_PERF_HW
#include "../../experiments/home-perf/perf_diag.h"
#endif
