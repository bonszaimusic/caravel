# Post-CAPTURE Quantize (private prototype)

For live note loops in **PLAY**: hold **REC**, turn **K2 QUANT**. The existing **K1 LENGTH** remains unchanged. The screen displays the owner track as `T1`–`T4`; when PAIR is active, use the same owner as CAPTURE.

`QUANT`: **OFF, 1/16, 1/8, 1/4, 1/2, 1/1**. The clock uses four beats per bar. **1/1 is one whole note / one bar in 4/4.** A note-on is moved to the nearest grid point (halfway rounds forward); its note-off moves by exactly the same amount. Note, velocity, original length, loop length and raw capture timestamps remain unchanged.

- **OFF** plays the original, unquantized loop exactly; switching grids *always* starts from that original loop.
- **While playing**, an accepted new grid is prepared incrementally and committed on the next **eligible** loop boundary. The screen shows `NEXT LOOP` while preparing. If other tracks occupy the single scratch worker or the loop is extremely short, preparation can miss the first boundary and commit at a later one.
- **While paused**, the prepared grid is adopted without changing the paused phase. Resume keeps the existing CAPTURE resume semantics.
- **Overdub** is still recorded raw. The current quantized loop keeps its previous event count until a replacement is ready; then the combined original recording is quantized for the next loop. Live playing is never quantized.
- **No loop = no QUANT setting**. CLEAR/WIPE returns QUANT to OFF. QUANT is runtime-only, not saved in projects.
- **Same-pitch collisions**: if two occurrences of the same raw pitch would overlap after rounding, or the source has an unpairable/zero-length note-on/off, reject the *whole* grid. `QUANT COLLISION: OFF` indicates fallback to raw playback at the next boundary. Different pitches may begin on the same grid point. At equal timestamps, releases are sorted before new note-ons.
- **Transition safety**: when changing an active loop, any notes owned by the previous playback schedule are explicitly released once at the switch to prevent stuck notes; a note crossing that exact switching boundary may therefore be shortened for that one transition. Subsequent cycles retain recorded durations. No audio is retriggered by changing K2 alone.

### Implementation limits

The audio owner alone prepares the transformed list: at most two input events handled per 128-frame block, with bounded matching/sorting loops of 128 events maximum. No UI mutation of active notes, no new project format, no audio DSP changes. The transformed playback lists plus shared staging list use **2,560 bytes** of event storage; staging match bookkeeping adds 144 bytes plus small control state. The original CAPTURE ring and loop remain intact.

Target memory, CPU spike and full firmware build validation are **required** before flashing. Focused native regression tests are in `tests/capture_post_quant_test.c`, invoked by `sh tests/capture_post_quant.sh` after creating the existing `build/gen` prerequisites. Tests in this source draft are not a substitute for a JieLi target build or hardware audition.
