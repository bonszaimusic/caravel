/* SPDX-License-Identifier: GPL-3.0-only */
enum { ED_LEARN2_GET=85,ED_LEARN2_SET };
static void ed_learn2_list(void)
{
    uint32_t i;
    ed_b(ML_N);
    for (i = 0; i < ML_N; i++) {
        uint32_t e = ml_get(i), id = ml_id(e >> 9);
        int used = (e >> 9) && (id < P_COUNT || virtual_target(id));
        ed_b(used); ed_b(used ? e & 127u : 0); ed_b(used ? (e >> 7) & 3u : 0);
        ed_b(used ? id & 127u : 0); ed_b(used ? id >> 7 : 0);
    }
}
static int ed_learn2_handle(uint32_t cmd,const uint8_t*a,uint32_t n){
 uint32_t id,rc=1;
    if (cmd == ED_LEARN2_GET) { ed_learn2_list(); return 1; }
    if (cmd == ED_LEARN2_SET) {
        if (n == 5u && a[0] == 0u) {
            id = a[3] | (uint32_t)a[4] << 7;
            rc = ml_learn(a[1], a[2], id); rc = rc == 1u ? 0u : rc ? 1u : 5u;
        } else if (n == 4u && a[0] == 3u && a[1] < NTRK) {
            id = a[2] | (uint32_t)a[3] << 7;
            if (id < P_COUNT || virtual_target(id)) { ui.ml.on = 1; ml_knob(&trk[a[1]], id); rc = 0; }
        } else if (n == 2u && a[0] == 1u) { ml_drop(a[1], NTRK, P_COUNT); rc = 0; }
        else if (n == 1u && a[0] == 2u) { ml_clear_all(); rc = 0; }
        ed_b(rc ? rc : ed_ui_save()); ed_learn2_list(); ui.force = 1; return 1;
    }
 return 0;
}
