/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions adapted from JIANT (juanjiant-bit, GPL-3.0-only); changes since the fork: Copyright (C) 2026 bonszaimusic
 * DRUM-X port from juanjiant-bit/JIANT; Custom track-local control and storage. */
#define DX_FIRST 256u
#define DX_SOUND_FIRST 264u
#define DX_SOUND_STRIDE 16u
#define DX_END (DX_SOUND_FIRST + 8u * DX_SOUND_STRIDE)
enum { DX_BP, DX_FB, DX_IMAGE, DX_RATE, DX_DEPTH, DX_SHAPE, DX_RAND };
typedef struct {
    dx_lane_t lane[8];
    uint8_t tone, layer[8], send[8], mot[4], image;
} dx_config_t;
_Static_assert(sizeof(dx_config_t) == 110u, "DX kit: 110 bytes in a 128-byte 7-bit sound payload");
static dx_config_t dx_config[NTRK];
static uint8_t dx_ready[NTRK];
static uint32_t dx_phase[NTRK], dx_rng[NTRK];
static int32_t dx_motion_value[NTRK];
static int dx_track(const track_t *t) { return t->eng_req == ENGI_DRUM && t->p[P_E0] == DX_KIT_ID; }
static int dx_sounding(const track_t *t) { return t->engine == ENGI_DRUM && t->p[P_E0] == DX_KIT_ID; }
static void dx_reset(uint32_t k)
{
    if (k >= NTRK) return;
    memset(&dx_config[k], 0, sizeof dx_config[k]);
    memcpy(dx_config[k].lane, DX_KIT_DEF, sizeof DX_KIT_DEF);
    memcpy(dx_config[k].layer, DX_LAY_DEF, sizeof DX_LAY_DEF);
    dx_ready[k] = 1;
    dx_phase[k] = 0;
    dx_rng[k] = 0x9E3779B9u + k;
}
static dx_config_t *dx_of(uint32_t k)
{
    if (k >= NTRK) return 0;
    if (!dx_ready[k]) dx_reset(k);
    return &dx_config[k];
}
static int dx_target(uint32_t id) { return (id >= DX_FIRST && id < DX_FIRST + 7u) || (id >= DX_SOUND_FIRST && id < DX_END); }
static const char *const DX_WAVE_NAMES[] = {"SIN", "FM", "MTL", "BEL"};
static const char *const DX_MODE_NAMES[] = {
    "SIN LP", "SIN LP+", "SIN BP", "SIN BP+", "SIN HP", "SIN HP+",
    "FM LP", "FM LP+", "FM BP", "FM BP+", "FM HP", "FM HP+",
    "MTL LP", "MTL LP+", "MTL BP", "MTL BP+", "MTL HP", "MTL HP+",
    "BEL LP", "BEL LP+", "BEL BP", "BEL BP+", "BEL HP", "BEL HP+"
};
static const char *const DX_PM_NAMES[] = {"DECAY", "LONG", "NOISE", "SINE"};
static const char *const DX_RATE_NAMES[] = {"OFF", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
static const char *const DX_SHAPE_NAMES[] = {"SINE", "TRI", "SAW", "RAMP", "S&H"};
static const param_desc_t DX_MORPH_DESC = {"MORPH", F_PCT, 0, 127, 0, 0, 0};
static const param_desc_t DX_GLOBAL_DESC[7] = {
    {"BP", F_INT, 0, 15, 0, 0, 0}, {"FEEDBK", F_INT, 0, 15, 0, 0, 0},
    {"IMAGE", F_PCT, 0, 127, 0, 0, 0}, {"RATE", F_ENUM, 0, 8, 0, DX_RATE_NAMES, 0},
    {"DEPTH", F_PCT, 0, 127, 0, 0, 0}, {"SHAPE", F_ENUM, 0, 4, 0, DX_SHAPE_NAMES, 0},
    {"RAND", F_PCT, 0, 127, 0, 0, 0}
};
static const char *const DX_SEND_NAMES[49] = {"OFF", "CHO 6%", "CHO 12%", "CHO 18%", "CHO 25%", "CHO 31%", "CHO 37%", "CHO 43%", "CHO 50%", "CHO 56%", "CHO 62%", "CHO 68%", "CHO 75%", "CHO 81%", "CHO 87%", "CHO 93%", "CHO 100%", "DLY 6%", "DLY 12%", "DLY 18%", "DLY 25%", "DLY 31%", "DLY 37%", "DLY 43%", "DLY 50%", "DLY 56%", "DLY 62%", "DLY 68%", "DLY 75%", "DLY 81%", "DLY 87%", "DLY 93%", "DLY 100%", "REV 6%", "REV 12%", "REV 18%", "REV 25%", "REV 31%", "REV 37%", "REV 43%", "REV 50%", "REV 56%", "REV 62%", "REV 68%", "REV 75%", "REV 81%", "REV 87%", "REV 93%", "REV 100%"};
static const param_desc_t DX_SOUND_DESC[16] = {
    {"PITCH", F_INT, 0, 127, 0, 0, 0}, {"PMOD", F_PCT, 0, 127, 0, 0, 0},
    {"DECAY", F_PCT, 0, 127, 0, 0, 0}, {"NOISE", F_PCT, 0, 127, 0, 0, 0}, {"COLOR", F_PCT, 0, 127, 0, 0, 0},
    {"PITCH", F_INT, 0, 127, 0, 0, 0}, {"PMOD", F_PCT, 0, 127, 0, 0, 0},
    {"DECAY", F_PCT, 0, 127, 0, 0, 0}, {"NOISE", F_PCT, 0, 127, 0, 0, 0}, {"COLOR", F_PCT, 0, 127, 0, 0, 0},
    {"MODE", F_ENUM, 0, 23, 0, DX_MODE_NAMES, 0}, {"PM.TYPE", F_ENUM, 0, 3, 0, DX_PM_NAMES, 0},
    {"DRIVE", F_ONOFF, 0, 1, 0, 0, 0}, {"LAYER", F_INT, 0, 15, 0, 0, 0},
    {"FEEDBK", F_INT, 0, 15, 0, 0, 0}, {"SEND", F_ENUM, 0, 48, 0, DX_SEND_NAMES, 0}
};
static const param_desc_t *dx_desc(uint32_t id)
{
    if (!dx_target(id)) return 0;
    return id < DX_SOUND_FIRST ? &DX_GLOBAL_DESC[id - DX_FIRST] : &DX_SOUND_DESC[(id - DX_SOUND_FIRST) % 16u];
}
static uint32_t dx_mode_ix(uint32_t m) { return (m & 3u) * 6u + ((m >> 2) & 3u) * 2u + ((m >> 4) & 1u); }
static uint32_t dx_mode_of(uint32_t x) { return x / 6u | (x % 6u / 2u) << 2 | (x & 1u) << 4; }
static int32_t dx_get_config(const dx_config_t *c, uint32_t id)
{
    uint32_t l, f, s;
    if (!c || !dx_target(id)) return 0;
    if (id < DX_SOUND_FIRST) {
        f = id - DX_FIRST;
        return f == DX_BP ? c->tone & 15u : f == DX_FB ? c->tone >> 4 : f == DX_IMAGE ? c->image : c->mot[f - DX_RATE];
    }
    l = (id - DX_SOUND_FIRST) / 16u; f = (id - DX_SOUND_FIRST) % 16u;
    if (f < 10u) return f < 5u ? c->lane[l].a[f] : c->lane[l].b[f - 5u];
    if (f == 10u) return (int32_t)dx_mode_ix(c->lane[l].mode);
    if (f == 11u) return (c->lane[l].mode >> 5) & 3u;
    if (f == 12u) return c->lane[l].mode >> 7;
    if (f == 13u) return c->layer[l] & 15u;
    if (f == 14u) return c->layer[l] >> 4;
    s = c->send[l];
    return s >> 6 ? ((s >> 6) - 1u) * 16u + ((s & 63u) + 1u) / 4u : 0;
}
static int32_t dx_get(uint32_t k, uint32_t id) { return dx_get_config(dx_of(k), id); }
static void dx_set_config(dx_config_t *c, uint32_t id, int32_t value)
{
    const param_desc_t *d = dx_desc(id);
    uint32_t l, f, v;
    if (!c || !d) return;
    v = (uint32_t)clamp(value, d->min, d->max);
    if (id < DX_SOUND_FIRST) {
        f = id - DX_FIRST;
        if (f == DX_BP) c->tone = (uint8_t)((c->tone & 0xF0u) | v);
        else if (f == DX_FB) c->tone = (uint8_t)((c->tone & 15u) | v << 4);
        else if (f == DX_IMAGE) c->image = (uint8_t)v;
        else c->mot[f - DX_RATE] = (uint8_t)v;
        return;
    }
    l = (id - DX_SOUND_FIRST) / 16u; f = (id - DX_SOUND_FIRST) % 16u;
    if (f < 5u) c->lane[l].a[f] = (uint8_t)v;
    else if (f < 10u) c->lane[l].b[f - 5u] = (uint8_t)v;
    else if (f == 10u) c->lane[l].mode = (uint8_t)((c->lane[l].mode & 0xE0u) | dx_mode_of(v));
    else if (f == 11u) c->lane[l].mode = (uint8_t)((c->lane[l].mode & ~0x60u) | v << 5);
    else if (f == 12u) c->lane[l].mode = (uint8_t)((c->lane[l].mode & 0x7Fu) | v << 7);
    else if (f == 13u) c->layer[l] = (uint8_t)((c->layer[l] & 0xF0u) | v);
    else if (f == 14u) c->layer[l] = (uint8_t)((c->layer[l] & 15u) | v << 4);
    else c->send[l] = v ? (uint8_t)(((v - 1u) / 16u + 1u) << 6 | ((v - 1u) % 16u + 1u) * 4u - 1u) : 0;
}
/* DX-only payload in the engine-specific sound slot: 110 raw bytes -> 126 7-bit bytes.
 * Projects and presets retain their sizes; no FM6 parameter or patch on an FM6 track is reused. */
static void dx_set(uint32_t k, uint32_t id, int32_t value) { dx_set_config(dx_of(k), id, value); }
static void dx_pack_config(const dx_config_t *c, uint8_t *out)
{
    const uint8_t *raw = (const uint8_t *)c;
    uint32_t i, acc = 0, bits = 0, o = 2;
    memset(out, 0, 128);
    out[0] = 'D'; out[1] = 'X';
    for (i = 0; i < sizeof(dx_config_t); i++) {
        acc |= (uint32_t)raw[i] << bits; bits += 8;
        while (bits >= 7u) { out[o++] = (uint8_t)(acc & 127u); acc >>= 7; bits -= 7; }
    }
    if (bits) out[o] = (uint8_t)acc;
}
static void dx_pack(uint32_t k, uint8_t *out) { dx_pack_config(dx_of(k), out); }
static int dx_decode(dx_config_t *out, const uint8_t *in)
{
    dx_config_t c;
    uint8_t *raw = (uint8_t *)&c;
    uint32_t i, acc = 0, bits = 0, o = 2;
    if (in[0] != 'D' || in[1] != 'X') return 0;
    for (i = 0; i < sizeof c; i++) {
        while (bits < 8u) { if (o >= 128u || in[o] > 127u) return 0; acc |= (uint32_t)in[o++] << bits; bits += 7; }
        raw[i] = (uint8_t)acc; acc >>= 8; bits -= 8;
    }
    if (!dx_kit_ok(c.lane) || c.image > 127u || c.mot[0] > 8u || c.mot[1] > 127u || c.mot[2] > 4u || c.mot[3] > 127u) return 0;
    if (acc) return 0;
    for (i = 0; i < 8u; i++) if (c.send[i] && (!(c.send[i] >> 6) || (c.send[i] & 3u) != 3u)) return 0;
    *out = c;
    return 1;
}
static int dx_unpack(uint32_t k, const uint8_t *in)
{
    dx_config_t c;
    if (k >= NTRK || !dx_decode(&c, in)) return 0;
    dx_config[k] = c; dx_ready[k] = 1; return 1;
}
static const param_desc_t *drumx_desc(const track_t *t, uint32_t e)
{
    static const param_desc_t noise = {"NOISE", F_PCT, 0, 127, 64, 0, 0};
    static const param_desc_t fm = {"FM ENV", F_PCT, 0, 127, 0, 0, 0};
    static const param_desc_t fold = {"FOLD", F_PCT, 0, 127, 0, 0, 0};
    static const char *const kick_names[] = {"PUNCH", "ROUND"};
    static const param_desc_t kick = {"KICK", F_ENUM, 0, 1, 0, kick_names, 0};
    if (!dx_track(t)) return e == 6u ? &kick : 0;
    return e == 4u ? &noise : e == 5u ? &fm : e == 6u ? &fold : 0;
}
typedef struct { int32_t sl[CTL], sr[CTL], send[3][CTL]; uint8_t on; } dx_side_t;
static dx_side_t dx_sides[C1_NP];
#define DX_SIDE(t) dx_sides[C1_PI(t)]
static void dx_side_begin(track_t *t, uint32_t n)
{
    uint32_t i, l, k = (uint32_t)(t - trk);
    dx_config_t *c;
    dx_side_t *side = &DX_SIDE(t);
    side->on = 0;
    if (!dx_track(t) || k >= NTRK) return;
    c = dx_of(k);
    if (c->image) side->on = 1;
    for (l = 0; l < 8u; l++) if (c->send[l] >> 6) side->on = 1;
    if (side->on) for (i = 0; i < n; i++) {
        side->sl[i] = side->sr[i] = 0;
        for (l = 0; l < 3u; l++) side->send[l][i] = 0;
    }
    if (c->mot[0]) {
        uint32_t rate = c->mot[0];
        uint32_t inc = (uint32_t)(((uint64_t)(uint32_t)clamp(song.g[G_BPM], 1, 999) * n << 32) / (60u * 44100u * 16u));
        uint32_t old = dx_phase[k], ph;
        int32_t v;
        dx_phase[k] += inc << (rate - 1u); ph = dx_phase[k];
        if (ph < old) dx_rng[k] = dx_rng[k] * 1664525u + 1013904223u;
        v = c->mot[2] == 0 ? osc_sine(ph) : c->mot[2] == 1 ? osc_tri(ph) : c->mot[2] == 2 ? (int32_t)(ph >> 16) - 32768 : c->mot[2] == 3 ? 32767 - (int32_t)(ph >> 16) : (int32_t)(dx_rng[k] >> 16) - 32768;
        dx_motion_value[k] = (v * (int32_t)c->mot[1]) >> 16;
    } else dx_motion_value[k] = 0;
}

static void dx_short_name(char *b, uint32_t id)
{
    static const char *const lanes[] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    static const char *const fields[] = {"PIT", "PMD", "DCY", "NOI", "COL", "PIT", "PMD", "DCY", "NOI", "COL", "MOD", "PMT", "DRV", "LAY", "FB", "SND"};
    if (id < DX_SOUND_FIRST) { str_cpy(b, dx_desc(id)->label, 9); return; }
    uint32_t n = id - DX_SOUND_FIRST, f = n % 16u;
    str_cpy(b, lanes[n / 16u], 9);
    b[2] = f < 10u ? (f < 5u ? 'A' : 'B') : '.';
    b[3] = '.'; str_cpy(b + 4, fields[f], 5);
}
