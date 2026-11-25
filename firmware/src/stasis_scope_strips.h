/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS Scope V6/V7 LCD strips: 25 horizontal rectangles of 4 rows (y68..y167) that cover EVERY
 * pixel of the disc r<=48 around (120,116) and overshoot it by at most r51.0, so a partial
 * update never reaches the orbit controls (their pixels start at about r53). One source of truth
 * for the painter and the standalone geometry test.
 */
#ifndef CARAVEL_STASIS_SCOPE_STRIPS_H
#define CARAVEL_STASIS_SCOPE_STRIPS_H
#include <stdint.h>
#define STVIZ_STRIPS 25u
#define STVIZ_STRIP_H 4u
#define STVIZ_STRIP_Y0 68u
static const uint8_t STVIZ_HALF[STVIZ_STRIPS]={16u,24u,30u,34u,38u,40u,43u,44u,46u,47u,47u,47u,48u,47u,47u,46u,45u,43u,41u,38u,35u,31u,26u,19u,0u};
#endif
