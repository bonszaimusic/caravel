<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Frozen speaker EQ stimuli

Two 2-second mono float32 little-endian signals at 44.1 kHz: PUNCH, then ROUND.
The samples are the unchanged input of `aac1bc2:tests/speaker_test.c:67` (the
retired DRUM voice renders before normalization or speaker EQ). The new basis
reads these samples to keep the original spectral/headroom assertions without
retaining or rebuilding the deleted synthesis sources.

Decompressed size: 705600 bytes. SHA-256: `9f063cf8d56c0dd378be351b813f478c18bfe877e3c535143b69c65c1fdff31d`.
Generator used the original `dv_default`, `dv_setup`, `dv_init`, `dv_metal_tune`,
`dv_trigger`, `dv_metal_run`, `dv_run`, with a hit every `FS/2` samples, in blocks
of `CTL`. Allocate the final partial block rounded up to CTL, then retain exactly
88200 floats per signal. This also avoids the old fixture's 56-sample final-block
overrun. The normalization and all EQ/limiter thresholds remain in speaker_test.c.

The source is GPL-3.0-only, Copyright (C) 2026 Leo Kuroshita (@kurogedelic),
Hügelton Instruments; see repository NOTICE and LICENSES.
