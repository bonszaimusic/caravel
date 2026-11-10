/* SPDX-License-Identifier: GPL-3.0-only
 * HOME ACTION banks on fixed physical keyboard positions.
 * Physical keys: C#5/D#5 BANK back/forward, D5/E5/F5/G5 A1..A4.
 * The renderer, UI and audio keyboard share these six bits; no MIDI remapping.
 */
#ifndef CARAVEL_ACTION_BANKS
#define CARAVEL_ACTION_BANKS 0
#endif
#if CARAVEL_ACTION_BANKS
#define ACT_KEY_FIRST 20u
#define ACT_BANK_PREV 20u
#define ACT_BANK_NEXT 22u
static const uint8_t action_keys[4]={21u,23u,24u,26u};
#define ACT_KEY_MASK ((1u<<20)|(1u<<21)|(1u<<22)|(1u<<23)|(1u<<24)|(1u<<26))
#define ACT_BANKS 6u
#define ACT_BANK_HARMONY 5u
#define ACT_BANK_JOURNEY 4u   /* numbered after LIVE so the other banks keep their numbers; BANK steps RANDOM, JOURNEY, HARM, STASIS, PAIR, LIVE */
static volatile uint8_t action_enabled, action_bank, action_setup;
#if CARAVEL_STASIS_EXPAND
static uint8_t action_stasis_grain_page; /* 0 existing FX controls, 1 granular controls */
#endif
static uint8_t action_bank_down[2], action_bank_long[2];
static uint32_t action_bank_started[2];
/* RANDOM A4: tap cycles S1..S4; hold 500ms stores current live HOME state. */
static uint8_t action_snap_slot,action_snap_down,action_snap_long;
static uint32_t action_snap_started;
#define ACTION_LONG_MS 500u
static volatile uint32_t action_capture_mask, action_preheld;
static uint32_t action_home_tap_ms;
static const char *const action_bank_names[ACT_BANKS] = {
 "RANDOM", "STASIS", "PAIR", "LIVE", "JOURNEY", "HARM"
};
static const char *const action_labels[ACT_BANKS][4] = {
 {"RANDOM","FREEZE","RETURN","SNAP"},
 {"HOLD","ADD","BLEND","RELEASE"},
 {"A 0%","MID 50%","B 100%","MOTION"},
 {"MUTE T1","MUTE T2","MUTE T3","MUTE T4"},
 {"PLAY","DIR","NEXT","WAIT"},
 {"HOLD","PREV","NEXT","RESET"}
};
/* The UI renderer is compiled before action_control.c in the FM-1 amalgamation. */
static void action_setup_caption(uint32_t slot,char *name,char *value);
static void action_setup_turn(uint32_t slot,int32_t steps);
static int32_t action_setup_value(uint32_t slot);
#endif
