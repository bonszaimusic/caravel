/* SPDX-License-Identifier: GPL-3.0-only; unchanged V9 renderer for oracle testing. */
static __attribute__((noinline)) void track_filter_v9_reference(track_t *t, int32_t *b, int32_t *d, uint32_t n)
{
    tfilt_t *s = &tf[(uint32_t)(t - trk) % NTRK];
    int32_t v = clamp(t->p[P_FILT], -64, 63), want = v < 0 ? 1 : v > 0 ? 2 : 0, tgt, tw, i, f;
    uint32_t hp;
    if (!s->mode) {
        if (!want) {
            t->tf_run = 0;
            return;                                     /* off: bit for bit */
        }
        memset(s, 0, sizeof *s);                        /* starting: open, the poles at the input (no transient), the wet share from 0 */
        s->mode = (uint8_t)want;
        s->cut = want == 1 ? PF_TOP : 0;
        if (want == 1) {
            s->z[0] = s->z[1] = clamp(b[0], -TF_IN, TF_IN) << 8;
            if (d)
                s->z[2] = s->z[3] = clamp(d[0], -TF_IN, TF_IN) << 8;
        } else {
            s->z[0] = clamp(b[0], -TF_IN, TF_IN) << 8;
            if (d)
                s->z[2] = clamp(d[0], -TF_IN, TF_IN) << 8;
        }
    }
    hp = s->mode == 2u;
    if (want == s->mode) {                              /* the knob's place: LP from the top down, HP from the bottom up */
        tgt = hp ? v * (PF_TOP - PF_LOW) / 63 : PF_TOP + v * ((PF_TOP - PF_LOW) / 64);
        tw = 32768;
    } else {                                            /* off or the other side: open the cutoff, then fade out */
        tgt = hp ? 0 : PF_TOP;
        tw = s->cut == tgt ? 0 : 32768;
    }
    s->cut += clamp(tgt - s->cut, -TF_STEP, TF_STEP);
    i = s->cut >> 8;
    f = s->cut & 255;
    s->g = (uint32_t)((int32_t)TF_G[i] + (((int32_t)TF_G[i < TF_ROWS ? i + 1 : TF_ROWS] - (int32_t)TF_G[i]) * f >> 8));   /* between two rows */
    for (uint32_t j = 0; j < n; j++) {
        int32_t x = b[j];
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        b[j] = pf_mix(x, tf_sample(x, s->z, s->g, hp), s->w);
        if (d) {
            x = d[j];
            d[j] = pf_mix(x, tf_sample(x, s->z + 2, s->g, hp), s->w);
        }
    }
    if (!s->w && !tw)                                   /* faded out: at rest (the other side starts with the next block) */
        memset(s, 0, sizeof *s);
    t->tf_run = s->mode != 0;
}
