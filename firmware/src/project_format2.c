/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* FORMAT 2 native codec. Stored engine IDs are 0..5. No legacy conversion.
 * Wire layout and validation: tests/format2_project_test.c. */
#define PROJ_MAGIC_WIRE 0x46554E57u
#define PROJ_MAGIC_V11 PROJ_MAGIC_WIRE
#define PROJ_MAGIC PROJ_MAGIC_WIRE
#define PROJ_PHYS 2u
#define PROJ_NAME_LEN 12u
#define PROJ_HARMONY_SIZE 24u
#define PROJ_STEP_SIZE 7u
#define PROJ_LFO2_SIZE (NTRK * L2_N)
#define PROJ_STORE_SIZE ((68u + NTRK*(P_COUNT+2u+NSTEP*PROJ_STEP_SIZE) + PROJ_HARMONY_SIZE + sizeof(motion_store_t) + PROJ_LFO2_SIZE + 3u + PROJ_NAME_LEN + 4u + 3u) & ~3u)
#define PROJ_NAME_OFF (PROJ_STORE_SIZE - 4u - PROJ_NAME_LEN)
#define PROJ_MIDI_OFF (PROJ_NAME_OFF - 3u)
#define PROJ_LFO2_OFF (PROJ_MIDI_OFF - PROJ_LFO2_SIZE)
/* FUNW backward-readable extension: two formerly-zero header bytes (b[65] marker, b[67] ON+TIME) and 21 bits (AMT, DAMP, TONE)
 * in the unused top six bits of the four HARMONY DURATION bytes (they hold 0..3; the gap between motion and LFO2 is only one
 * byte, so it cannot hold them). No marker means historical defaults and untouched harmony bytes, so every 1.0 project loads
 * exactly as before. Old firmware rejects *new* nondefault data (marker, or DURATION > 3); the project size, object addresses
 * and checksum are unchanged. */
#define PROJ_FX_EXT_MARK 0xA5u
#define PROJ_HARM_DUR_OFF 4u
static void proj_fx_hi_pack(uint8_t *hb, const uint8_t fx[5])
{
    uint32_t v = (uint32_t)fx[2] | (uint32_t)fx[3] << 7 | (uint32_t)fx[4] << 14, i;
    for (i = 0; i < 4u; i++) hb[PROJ_HARM_DUR_OFF + i] = (uint8_t)(hb[PROJ_HARM_DUR_OFF + i] | ((v >> (6u * i)) & 63u) << 2);
}
static void proj_fx_hi_unpack(uint8_t *hb, uint8_t fx[5])   /* reads the bits and clears them from the copy */
{
    uint32_t v = 0, i;
    for (i = 0; i < 4u; i++) { v |= (uint32_t)(hb[PROJ_HARM_DUR_OFF + i] >> 2) << (6u * i); hb[PROJ_HARM_DUR_OFF + i] &= 3u; }
    fx[2] = (uint8_t)(v & 127u); fx[3] = (uint8_t)(v >> 7 & 127u); fx[4] = (uint8_t)(v >> 14 & 127u);
}
typedef struct {                               /* one track */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel;                               /* the selected track */
    uint8_t parts;                             /* NPART: always four melodic tracks */
    uint8_t phys;                              /* PROJ_PHYS: fixed model ordering */
    uint8_t rsv;
    proj_trk_t t[NTRK];
    motion_store_t motion;
    lfo2_config_t lfo2[NTRK];
    uint8_t mch[NTRK];                         /* 1.5.5: MIDI IN TRACK's channels, as song.mch (PROJ_MIDI_OFF) */
    uint8_t af_on, af_time, af_amt, af_damp, st_tone; /* optional project FX extension; never stored as session prefs */
    char name[PROJ_NAME_LEN];                  /* the project's name: upper-case ASCII 32..126, 0-padded; "" = none */
    uint32_t sum;
} project_t;
typedef union { uint32_t align; uint8_t raw[PROJ_STORE_SIZE]; } project_store_t;
project_store_t proj_slot[4] __attribute__((section(".noinit")));
typedef struct {
    uint8_t root[4], dur[4], on, link_src[NTRK], link_retrig[NTRK], follow[NTRK];
    uint8_t key_mode, key_root, key_scale;
    uint8_t local_shape[NTRK]; /* remembered KEY chord shapes, four nibbles in wire */
    uint32_t bar, base;
} harmony_saved_t;
static harmony_saved_t proj_harmony;
static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC_WIRE &&
    q->size == sizeof *q && q->sum == proj_sum(q); }
static uint32_t proj_name_get(char *d, const uint8_t *s)   /* its length */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME_LEN && s[i]; i++) {
        if (s[i] < 32u || s[i] > 126u) { i = 0; break; }
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32u : s[i]);
    }
    d[i] = 0;
    return i;
}
static uint32_t proj_route_fallback(const uint8_t *mch)
{
    uint32_t k;
    for (k = 1; k < NTRK; k++)
        if (mch[k] != mch[0])
            return 0;
    return mch[0] % 4u || mch[0] >= 16u ? 0u : mch[0] ? mch[0] / 4u + 1u : 0u;
}
static void proj_midi_pack(uint8_t *b, const project_t *q)
{
    uint32_t k, v = 0;
    for (k = 0; k < NTRK; k++)
        v |= (uint32_t)(q->mch[k] < MCH_N ? q->mch[k] : 0u) << (5u * k);
    if (q->g[G_ROUTE] == ROUTE_TRACK) {
        int16_t r = (int16_t)proj_route_fallback(q->mch);
        memcpy(b + 8 + 2u * G_ROUTE, &r, 2);
        v |= 1u << 23;
    }
    b[PROJ_MIDI_OFF] = (uint8_t)v; b[PROJ_MIDI_OFF + 1u] = (uint8_t)(v >> 8); b[PROJ_MIDI_OFF + 2u] = (uint8_t)(v >> 16);
}
static void proj_midi_unpack(project_t *q, const uint8_t *b)
{
    uint32_t k, v = b[PROJ_MIDI_OFF] | (uint32_t)b[PROJ_MIDI_OFF + 1u] << 8 | (uint32_t)b[PROJ_MIDI_OFF + 2u] << 16, f;
    for (k = 0; k < NTRK; k++) {
        f = (v >> (5u * k)) & 31u;
        q->mch[k] = (uint8_t)(f < MCH_N ? f : 0u);
    }
    if (v >> 23)
        q->g[G_ROUTE] = ROUTE_TRACK;
}
static void proj_harmony_off(void)
{
    memset(&proj_harmony, 0, sizeof proj_harmony);
    memset(proj_harmony.follow, 1, sizeof proj_harmony.follow);
}
static int proj_engines_ok(const project_t *q) {
    for(uint32_t t=0;t<NTRK;t++) if(!eng_ok(q->t[t].engine) || q->t[t].preset>=ENGINES[q->t[t].engine]->npresets) return 0;
    return 1;
}
static int f2_harmony_valid(const harmony_saved_t *h) {
    if(h->on>1u)return 0;
    for(uint32_t t=0;t<NTRK;t++)if(h->root[t]>11u||h->dur[t]>3u||h->link_src[t]>4u||h->link_retrig[t]>3u||h->follow[t]>1u)return 0;
    if(h->key_mode>1u)return 0;
    if(h->key_mode && (h->key_root>11u || h->key_scale>=sizeof SCALE_MASK/sizeof SCALE_MASK[0]))return 0;
    for(uint32_t i=0;i<NTRK;i++)if(h->local_shape[i]>CH_POW)return 0;
    return 1;
}
static void f2_harmony_pack(uint8_t *b,const harmony_saved_t *h) {
    memset(b,0,PROJ_HARMONY_SIZE);
    memcpy(b,h->root,4);memcpy(b+4,h->dur,4);b[8]=h->on;
    memcpy(b+9,h->link_src,4);memcpy(b+13,h->link_retrig,4);memcpy(b+17,h->follow,4);
    if(h->key_mode){
        /* Three reserved bytes, no larger project:
         * low nibble = ROOT+1 (0 means legacy); high nibble = SCALE;
         * next four nibbles retain the per-track local CHORD shape. */
        b[21]=(uint8_t)((h->key_scale<<4)|(h->key_root+1u));
        b[22]=(uint8_t)(h->local_shape[0]|(h->local_shape[1]<<4));
        b[23]=(uint8_t)(h->local_shape[2]|(h->local_shape[3]<<4));
    }
}
static int f2_harmony_unpack(harmony_saved_t *h,const uint8_t *b) {
    memset(h,0,sizeof *h);memcpy(h->root,b,4);memcpy(h->dur,b+4,4);h->on=b[8];
    memcpy(h->link_src,b+9,4);memcpy(h->link_retrig,b+13,4);memcpy(h->follow,b+17,4);
    /* 0/0/0 retains the old T1-linked sound. New ROOT+1 is in 1..12. */
    if(!b[21]){
        if(b[22]||b[23])return 0;
    }else{
        uint8_t nr=b[21]&15u;
        if(!nr||nr>12u)return 0;
        h->key_mode=1u;h->key_root=(uint8_t)(nr-1u);h->key_scale=(uint8_t)(b[21]>>4);
        h->local_shape[0]=b[22]&15u;h->local_shape[1]=b[22]>>4;
        h->local_shape[2]=b[23]&15u;h->local_shape[3]=b[23]>>4;
    }
    return f2_harmony_valid(h);
}
/* Compact extension must leave all future/no-effect legacy saves
 * byte-identical. A zero-initialized project_t from older host tests is
 * treated as absent (legacy defaults). */
static void proj_fx_values(const project_t *q, uint8_t v[5])
{
    v[0] = q->af_on; v[1] = q->af_time; v[2] = q->af_amt;
    v[3] = q->af_damp; v[4] = q->st_tone;
    if (!v[0] && !v[1] && !v[2] && !v[3] && !v[4]) {
        v[1] = 30u; v[2] = 32u; v[3] = 70u; v[4] = 64u;
    }
}
static int proj_fx_valid(const uint8_t v[5])
{
    return v[0] <= 1u && v[1] >= 10u && v[1] <= 60u &&
           v[2] <= 127u && v[3] <= 127u && v[4] <= 127u;
}
static void proj_fx_defaults(project_t *q)
{
    q->af_on = 0; q->af_time = 30; q->af_amt = 32;
    q->af_damp = 70; q->st_tone = 64;
}
static int proj_pack(project_store_t *out, const project_t *q)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i; uint32_t magic = PROJ_MAGIC_WIRE, size = PROJ_STORE_SIZE, sum;
    uint8_t fx[5];
    proj_fx_values(q, fx);
    if (!proj_fx_valid(fx)) return 0;
    if (!proj_engines_ok(q) || !motion_valid(&q->motion) || !f2_harmony_valid(&proj_harmony) || P_COUNT > 127u) return 0;
    for (t = 0; t < NTRK; t++) {
        if (!lfo2_valid(&q->lfo2[t])) return 0;
        (void)lfo2_nonzero(&q->lfo2[t]);           /* FUN11 always carries the LFO 2 block */
    }
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, sizeof q->g); b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = P_COUNT;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < P_COUNT; i++) {
            if (q->t[t].p[i] < -64 || q->t[t].p[i] > 127) return 0;
            b[pos++] = (uint8_t)(q->t[t].p[i] + 64);
        }
        b[pos++] = (uint8_t)eng_store(q->t[t].engine); b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u;          /* RATCH: bit 7 of the velocity and chance bytes (see the top) */
            uint32_t nd = (uint32_t)step_nudge(s) & 15u;   /* NUDGE (FUN10): bit j in bit 7 of note j */
            if (s->n > 4u || s->time > ST_REST || (s->flags & ~(3u | SF_RATCH | SF_NUDGE)) || s->probability >= COND_FIRST + NCOND)
                return 0;
            for (uint32_t j = 0; j < 4u; j++)           /* what proj_unpack checks, so a saved project always loads: */
                b[pos++] = (uint8_t)((s->note[j] > 127u ? 127u : s->note[j]) | (nd >> j & 1u) << 7);   /* notes and
                                                         * velocity 0..127, accents */
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);   /* only on hits */
            b[pos++] = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7);
            b[pos++] = (uint8_t)(s->probability | (r >> 1) << 7);
        }
    }
    if (pos + PROJ_HARMONY_SIZE + sizeof q->motion > PROJ_LFO2_OFF) return 0;
    f2_harmony_pack(b + pos, &proj_harmony);
    if (fx[0] || fx[1] != 30u || fx[2] != 32u || fx[3] != 70u || fx[4] != 64u) proj_fx_hi_pack(b + pos, fx);
    pos += PROJ_HARMONY_SIZE;
    memcpy(b + pos, &q->motion, sizeof q->motion);
    if (fx[0] || fx[1] != 30u || fx[2] != 32u || fx[3] != 70u || fx[4] != 64u) {
        b[65] = PROJ_FX_EXT_MARK;
        b[67] = (uint8_t)(fx[1] | (fx[0] << 7)); /* ON + TIME 10..60 */
    }
    proj_midi_pack(b, q);
    memcpy(b + PROJ_LFO2_OFF, q->lfo2, PROJ_LFO2_SIZE);
    {   /* the name (0-padded; stops at the first 0) */
        char n[PROJ_NAME_LEN + 1u];
        memcpy(b + PROJ_NAME_OFF, n, proj_name_get(n, (const uint8_t *)q->name));
    }
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
    return 1;
}
static int proj_unpack(project_t *q, const uint8_t *b, uint32_t st)
{
    uint32_t pos = 68u, t, i, magic, size, sum, np;
    uint8_t fx[5] = {0,30,32,70,64}, hb[PROJ_HARMONY_SIZE];
    int ext;
    harmony_saved_t h;
    if(st != PROJ_STORE_SIZE) return 0;
    np = b[66];
    uint32_t name_off = PROJ_NAME_OFF;
    memcpy(&magic,b,4);memcpy(&size,b+4,4);memcpy(&sum,b+st-4u,4);
    ext = b[65] == PROJ_FX_EXT_MARK;
    if(magic!=PROJ_MAGIC_WIRE||size!=st||np!=P_COUNT||
       (b[65] && !ext) || (!ext && b[67]) ||
       b[62]>=NTRK||b[63]!=NPART||b[64]!=PROJ_PHYS||sum!=proj_hash(b,st-4u)||
       68u+NTRK*(np+2u+NSTEP*PROJ_STEP_SIZE)+PROJ_HARMONY_SIZE+sizeof q->motion>PROJ_LFO2_OFF)return 0;
    memcpy(hb,b+68u+NTRK*(np+2u+NSTEP*PROJ_STEP_SIZE),PROJ_HARMONY_SIZE);
    if (ext) {
        fx[0] = b[67] >> 7; fx[1] = b[67] & 127u;
        proj_fx_hi_unpack(hb, fx);
        if (!proj_fx_valid(fx)) return 0;
    }
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC_V11; q->size = sizeof *q;
    proj_fx_defaults(q);
    q->af_on = fx[0]; q->af_time = fx[1]; q->af_amt = fx[2];
    q->af_damp = fx[3]; q->st_tone = fx[4];
    memcpy(q->g, b + 8, sizeof q->g); q->sel = b[62]; q->parts = b[63]; q->phys = b[64];
    for (t = 0; t < NTRK; t++) {
        int16_t values[P_COUNT], def[P_COUNT];
        for (i = 0; i < np; i++) { if (b[pos] > 191u) return 0; values[i] = (int16_t)b[pos++] - 64; }
        q->t[t].engine = (uint8_t)eng_unstore(b[pos++]); q->t[t].preset = b[pos++];
        if (q->t[t].engine >= NENGINES) return 0;     /* (FORMAT 2: eng_unstore gives 0xFF for a number past the six) */
        for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(q->t[t].engine, i)->def;
        params_by_count(q->t[t].p, values, np, def);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i]; uint32_t meta, nd = 0;
            for (uint32_t j = 0; j < 4u; j++) {         /* (FUN10: bit 7 of note j is bit j of the nudge) */
                nd |= (uint32_t)(b[pos] >> 7) << j;
                s->note[j] = b[pos++] & 127u;
            }
            meta = b[pos++];
            s->n = meta & 7u; s->time = (meta >> 3) & 3u; s->flags = (meta >> 5) & 3u;
            s->vel = b[pos] & 127u; meta |= (b[pos++] & 128u) << 1;      /* (RATCH bit 0 at bit 8 of meta) */
            s->probability = b[pos] & 127u; meta |= (b[pos++] & 128u) << 2;   /* (bit 1 at bit 9) */
            if ((meta & 255u) > 127u || s->n > 4u || s->time > ST_REST || s->probability >= COND_FIRST + NCOND) return 0;
            step_set_ratchet(s, (meta >> 8) + 1u);
            step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
        }
    }
    if(!f2_harmony_unpack(&h,hb))return 0;
    pos+=PROJ_HARMONY_SIZE;
    memcpy(&q->motion,b+pos,sizeof q->motion);
    if(!motion_valid(&q->motion)||!proj_engines_ok(q))return 0;
    motion_drop_chordset(&q->motion);
    for(i=pos+sizeof q->motion;i<PROJ_LFO2_OFF;i++)if(b[i])return 0;
    proj_midi_unpack(q,b);
    memcpy(q->lfo2, b + PROJ_LFO2_OFF, PROJ_LFO2_SIZE);
    {
        char n[PROJ_NAME_LEN + 1u];
        memcpy(q->name, n, proj_name_get(n, b + name_off));
    }
    for (t = 0; t < NTRK; t++) if (!lfo2_valid(&q->lfo2[t])) return 0;
    q->sum = proj_sum(q); proj_harmony = h;
    return 1;
}
static int proj_import(project_t *q,const void *b,int n) {
    return b && n==(int)PROJ_STORE_SIZE && proj_unpack(q,b,PROJ_STORE_SIZE);
}
_Static_assert(PROJ_STORE_SIZE <= 3840u,"FORMAT 2 native project fits storage");
_Static_assert(sizeof(project_store_t)==PROJ_STORE_SIZE,"FORMAT 2 wire size includes alignment");
_Static_assert(PROJ_LFO2_OFF >= 68u + NTRK * (P_COUNT+2u+NSTEP*PROJ_STEP_SIZE) +
               PROJ_HARMONY_SIZE + sizeof(motion_store_t),
               "Project body overlaps LFO2");

_Static_assert(sizeof(lfo2_config_t)==L2_N,"LFO2 explicit byte layout");
