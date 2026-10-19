/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Track-local DRUM-X integration; original Felucca drum loops remain separate. */
static void drumx_note_on(track_t *t, voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t lane = drum_lane(v->note), i = (uint32_t)(v - t->v), k = (uint32_t)(t - trk);
    dx_config_t *c;
    if (!K || i >= NVOICE || k >= NTRK) return;
    c = dx_of(k);
    L = &K[(uint32_t)v->s[0] & 7u];
    if (L->owner == i + 1u) L->owner = 0;
    L = &K[lane];
    L->owner = (uint8_t)(i + 1u); L->dx_active = 1;
    L->st = DRUM_GM[(v->note >= 35u && v->note <= 81u ? v->note : 36u + (v->note + 120u - 36u) % 12u) - 35u][1];
    v->s[0] = (int32_t)lane; v->env_out = v->vel * 258; v->env = 1 << 24;
    if (c->mot[3]) dx_rng[k] = dx_rng[k] * 1664525u + 1013904223u;
    v->s[7] = c->mot[3] ? (((int32_t)(dx_rng[k] >> 24) - 128) * (int32_t)c->mot[3]) >> 8 : 0;
    dx_trigger(&L->x);
    if (lane == DV_HATC) dx_choke(&K[DV_HATO].x);
}
static int32_t drumx_amp(track_t *t, voice_t *v)
{
    const drum_lane_t *L = drum_lane_of(t, v);
    if (!v->active || !L || !L->dx_active || (!L->x.live && !L->x.trig)) {
        v->active = v->gate = v->stage = 0; v->env = 0; return 0;
    }
    v->env = 1 << 24; return 32767;
}
static __attribute__((noinline)) void drumx_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    drum_lane_t *L = drum_lane_of(t, v);
    uint32_t k = (uint32_t)(t - trk), lane = (uint32_t)v->s[0] & 7u, i;
    dx_config_t *c;
    int32_t y[CTL], lv, g = 0, mk = 0;
    vmod_t amp;
    if (!L || !L->dx_active || k >= NTRK) return;
    c = dx_of(k);
    if (n > CTL) n = CTL;
    if (n < 2u) return;
    lv = clamp(t->p[P_LN0 + lane], 0, 127);
    amp = *m;
    if (lv < 127) {
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);
        amp.amp0 = (m->amp0 >> 4) * gl >> 11; amp.amp1 = (m->amp1 >> 4) * gl >> 11;
    }
    if (lane == DV_BELL) { amp.amp0 = (amp.amp0 * 20675) >> 15; amp.amp1 = (amp.amp1 * 20675) >> 15; }
    dx_run(&c->lane[lane], &L->x, t->p[P_MORPH] + dx_motion_value[k] + v->s[7],
        L->st * 16 + (t->p[P_E1] - 64) * 3, t->p[P_E3] - 64, t->p[P_E2] - 64,
        t->p[P_E4] - 64, t->p[P_E6], t->p[P_E5], y, n, lane, c->tone, c->layer[lane]);
    if ((drum_mute[k] >> lane) & 1u) return;
    if (t->p[P_E7] > 0) {
        g = 4096 + t->p[P_E7] * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    for (i = 0; i < n; i++) {
        int32_t s = y[i], o;
        if (g) s = (softclip((s * g) >> 12) * mk) >> 15;
        o = voice_amp(soft_knee(s + (s >> 2), 24000), &amp, i) << 1;
        out[i] += o;
        if (DX_SIDE(t).on) {
            uint32_t bus = c->send[lane] >> 6;
            int32_t pan = c->image ? (osc_sine((uint32_t)c->image * (lane + 1u) * 33554432u + lane * 0x2E000000u) * (int32_t)(c->image < 16u ? c->image * 4u : 64u)) >> 15 : 0;
            if (lane == DV_KICK) pan /= 8;
            if (pan > 0) DX_SIDE(t).sl[i] += (o * pan) >> 7;
            else if (pan < 0) DX_SIDE(t).sr[i] += (o * -pan) >> 7;
            if (bus) DX_SIDE(t).send[bus - 1u][i] += (o * (int32_t)(c->send[lane] & 63u)) >> 6;
        }
    }
}
