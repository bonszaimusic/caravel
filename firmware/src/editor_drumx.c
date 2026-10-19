/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-X descriptors/values and extended MIDI Learn. IDs are unsigned 14-bit values. */
enum { ED_DX_DESC = 82, ED_DX_GET, ED_DX_SET, ED_LEARN2_GET, ED_LEARN2_SET, ED_DX_USER_PUT };
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
static int ed_drumx_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i, k, id, rc = 1;
    if (cmd < ED_DX_DESC || cmd > ED_DX_USER_PUT) return 0;
    if (cmd == ED_DX_USER_PUT) {
        dx_config_t c = {0}; uint8_t pk[128]; uint32_t off = 1;
        if (n == 271u && a[0] < UP_SLOTS && upf_dx(a[0])) {
            rc = 0;
            for (i = DX_FIRST; i < DX_END; i++) if (dx_target(i)) {
                const param_desc_t *d = dx_desc(i); int32_t v = ed_rv(a + off); off += 2;
                if (v < d->min || v > d->max) { rc = 1; break; }
                dx_set_config(&c, i, v);
            }
            if (!rc) {
                if (ed_flash_stop()) rc = 2;
                else { dx_pack_config(&c, pk); upf_set(a[0], pk); rc = upf_save() == 2 ? 2u : 0u; }
            }
        }
        ed_b(n ? a[0] : 127); ed_b(rc); return 1;
    }
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
    if (cmd == ED_DX_DESC) {
        if (n != 2u || !dx_target(id = a[0] | (uint32_t)a[1] << 7)) { ed_b(1); return 1; }
        const param_desc_t *d = dx_desc(id);
        ed_b(0); ed_b(id & 127u); ed_b(id >> 7); ed_str(d->label, 8);
        ed_b(d->fmt); ed_v(d->min); ed_v(d->max);
        ed_b(d->fmt == F_ENUM ? d->max + 1u : 0);
        if (d->fmt == F_ENUM) for (k = 0; k <= (uint32_t)d->max; k++) ed_str(d->names[k], 8);
        return 1;
    }
    if (cmd == ED_DX_GET && n == 2u && a[1] == 1u) {
        uint8_t pk[128]; dx_config_t c;
        int active = a[0] < UP_SLOTS && upf_dx(a[0]) && !upf_get(a[0], pk) && dx_decode(&c, pk);
        ed_b(a[0]); ed_b(active);
        for (i = DX_FIRST; i < DX_END; i++) if (dx_target(i)) ed_v(active ? dx_get_config(&c, i) : 0);
        return 1;
    }
    if (n < 1u || a[0] >= NTRK) { ed_b(1); return 1; }
    k = a[0];
    if (cmd == ED_DX_SET) {
        if (n == 5u && dx_track(&trk[k])) {
            id = a[1] | (uint32_t)a[2] << 7;
            if (dx_target(id)) { dx_set(k, id, ed_rv(a + 3)); rc = 0; }
        }
        ed_b(rc);
    }
    ed_b(k); ed_b(dx_track(&trk[k]));
    for (i = DX_FIRST; i < DX_END; i++) if (dx_target(i)) ed_v(dx_get(k, i));
    ui.force = 1;
    return 1;
}

