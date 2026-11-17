/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic
 * Existing learned CCs in PLAY versus the current BUILD-only learn entry. */
#include <stdint.h>
static void action_refresh(void);
static void action_dispatch(uint32_t);
#define FELUCCA_CPU2 0
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
#include "../experiments/action-banks/action_control.c"
int main(void)
{
    int bad=0;uint32_t before;
    ui_power_on();hp_init();memset(&uxl,0,sizeof uxl);
    song.g[G_ROUTE]=0;uxl.play=1;
    bad+=check("a previously learned CC can target a PLAY parameter",ml_learn(74,0,P_E4)==1);
    before=trk[0].p[P_E4];midi_event(0xB0,0,74,1);
    bad+=check("learned CC changes the actual PLAY parameter",trk[0].p[P_E4]!=before);
    ui.ly=LAYER_GLO;ui.ml.on=0;
    bad+=check("PLAY GLO+D4 learn entry is currently blocked",uxl_harm_keys(1u<<9)==0 && !ui.ml.on);
    ui.ly=0;ml_toggle();uxl.area=UXL_SOUND;uxl.grp[UXL_SOUND]=0;
    uxl_knob(0,1);
    bad+=check("PLAY arc does not arm a new MIDI learn target",!ui.ml.pick);
    return bad?1:0;
}
