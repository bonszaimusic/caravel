/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone test; no generated firmware or device required.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/stasis_ui_new.h"
int main(void)
{
    static const uint32_t t[] = {0, 199, 200, 399, 400, 599, 600, 799, 800, 1000};
    static const uint8_t step[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
    uint32_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        assert(stasis_new_progress_step(1000u, 1000u + t[i], 1u) == step[i]);
        assert(stasis_new_progress_step(0xffffffe0u,
                                       (uint32_t)(0xffffffe0u + t[i]), 1u) == step[i]);
        assert(stasis_new_progress_step(1000u, 1000u + t[i], 0u) == 0u);
    }
    puts("STASIS2 NEW SOURCE hold progress: 30 assertions PASS (incl. timer wrap)");
    return 0;
}
