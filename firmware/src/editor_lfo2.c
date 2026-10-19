/* SPDX-License-Identifier: GPL-3.0-only */
enum { ED_L2_DESC = 88, ED_L2_GET, ED_L2_SET };
static int ed_lfo2_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t k, id, rc = 1;
    lfo2_config_t c = {{0}};
    if (cmd < ED_L2_DESC || cmd > ED_L2_SET) return 0;
    if (cmd == ED_L2_DESC) {
        const param_desc_t *d;
        if (n != 2u || !(d = lfo2_desc(id = a[0] | (uint32_t)a[1] << 7))) { ed_b(1); return 1; }
        ed_b(0); ed_b(id & 127u); ed_b(id >> 7); ed_str(d->label, 8);
        ed_b(d->fmt); ed_v(d->min); ed_v(d->max);
        ed_b(d->fmt == F_ENUM ? d->max + 1u : 0);
        if (d->fmt == F_ENUM) for (k = 0; k <= (uint32_t)d->max; k++) ed_str(d->names[k], 8);
        return 1;
    }
    if (cmd == ED_L2_GET && n == 2u && a[1] == 1u) {
        k = a[0]; ed_b(k); ed_b(up_used(k));
        if (up_used(k) && up_rec(k)->ver >= 8u) memcpy(c.p, up_rec(k)->packed + UP_L2_AT, L2_N);
        for (id = 0; id < L2_N; id++) ed_v(c.p[id]);
        return 1;
    }
    if (n < 1u || a[0] >= NTRK) { ed_b(1); return 1; }
    k = a[0];
    if (cmd == ED_L2_SET) {
        if (n == 5u && lfo2_target(id = a[1] | (uint32_t)a[2] << 7)) {
            lfo2_set(k, id, ed_rv(a + 3)); load_extend(&trk[k]); rc = 0;
        }
        ed_b(rc);
    } else if (n != 1u) { ed_b(1); return 1; }
    ed_b(k); ed_b(1);
    for (id = 0; id < L2_N; id++) ed_v(lfo2_config[k].p[id]);
    ui.force = 1;
    return 1;
}
