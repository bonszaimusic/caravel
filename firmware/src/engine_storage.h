/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FELUCCA_ENGINE_STORAGE_H
#define FELUCCA_ENGINE_STORAGE_H
/* Rows 14/15 of the old favorites record belong to MIDI Learn/preferences.
 * Independent append-only engine IDs 14..21 get 32 favorites each in PER7. */
#define ENGINE_FAV_FIRST 14u
#define ENGINE_FAV_N 8u
#define ENGINE_FAV_PRESETS 32u
#endif
