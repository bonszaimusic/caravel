/* SPDX-License-Identifier: GPL-3.0-only
 * Prototype only: four context-independent ACTION banks on HOME.
 * Physical FM-1 keys 22..26 (out of 0..26): BANK + A1..A4.
 * The renderer, UI and audio keyboard share these five bits; no MIDI remapping.
 */
#ifndef CARAVEL_ACTION_BANKS
#define CARAVEL_ACTION_BANKS 0
#endif
#if CARAVEL_ACTION_BANKS
#define ACT_KEY_FIRST 22u
#define ACT_KEY_MASK (31u << ACT_KEY_FIRST)
#define ACT_BANKS 4u
static volatile uint8_t action_enabled, action_bank, action_setup;
#if CARAVEL_STASIS_EXPAND
static uint8_t action_stasis_grain_page; /* 0 existing FX controls, 1 granular controls */
#endif
static uint8_t action_bank_down, action_bank_long;
static uint32_t action_bank_started;
/* RANDOM A4: tap cycles S1..S4; hold 500ms stores current live HOME state. */
static uint8_t action_snap_slot,action_snap_down,action_snap_long;
static uint32_t action_snap_started;
#define ACTION_LONG_MS 500u
static volatile uint32_t action_capture_mask, action_preheld;
static uint32_t action_home_tap_ms;
static const char *const action_bank_names[ACT_BANKS] = {
 "RANDOM", "STASIS", "PAIR", "LIVE"
};
static const char *const action_labels[ACT_BANKS][4] = {
 {"RANDOM","FREEZE","RETURN","SNAP"},
 {"HOLD","ADD","BLEND","RELEASE"},
 {"A 0%","MID 50%","B 100%","MOTION"},
 {"MUTE T1","MUTE T2","MUTE T3","MUTE T4"}
};
/* The UI renderer is compiled before action_control.c in the FM-1 amalgamation. */
static void action_setup_caption(uint32_t slot,char *name,char *value);
static void action_setup_turn(uint32_t slot,int32_t steps);
static int32_t action_setup_value(uint32_t slot);
#endif
