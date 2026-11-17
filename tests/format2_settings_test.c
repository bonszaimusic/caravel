/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
#include <stdint.h>
#define CARAVEL_FORMAT2 1
static uint8_t ml_base[32];
#include "settings_harness.h"
int main(void) {
    persist_t p={0},q;
    p.magic=0x50455237u;p.panel=PANEL_DEFAULT;
    p.favorites.factory[14][0]=74;p.favorites.factory[14][1]=12;
    p.favorites.factory[0][0]=255;p.favorites.user=7;p.engine_favorites[0]=3;
    p.favorites.factory[15][13]=7;
    assert(settings_import(&p,PERSIST_OLD7_SIZE));
    assert(ml_base[0]==74 && ml_base[1]==12 && p.learn_base[0]==74);
    assert(!favorites.factory[0][0] && !favorites.user && !engine_favorites[0]);
    assert(favorites.factory[15][13]==7);
    assert(!favorites.factory[14][0]);
    q=p;settings_export(&q);assert(q.magic==PERSIST_MAGIC);
    memset(ml_base,0,sizeof ml_base);assert(settings_import(&q,sizeof q)==1);
    assert(ml_base[0]==74 && ml_base[1]==12);
    q.magic=0x50455236u;assert(!settings_import(&q,PERSIST_PREENGINE_SIZE));
    puts("FORMAT 2 settings: explicit Learn round-trip, PER7 preference import and one-time favorites reset passed");
}
