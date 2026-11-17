/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Complete musical archive: no raw addresses are accepted. Small objects are staged
 * in main-loop RAM and fully validated before the existing A/B commit path writes.
 * Sample restore uses SMP_BEGIN/WRITE/END and its existing CRC/header-last commit.
 * A disconnected sample restore can lose that sample; the exported file is retained.
 * Id 8 (the FM6 patch bank of 1.0..1.0.2) is listed empty since 1.0.3; a PUT of it from an older archive moves its
 * patches into the user presets restored before it (ids 6, 7), as the first boot after the update does (up_fm6.c).
 * Id 9 is the user presets' FM6 patches (up_fm6.c), appended in 1.0.3.
 */
static const uint8_t ED_BK_IDS[] = {0,1,2,3,4,5,6,7};
#define ED_BK_N ((uint32_t)sizeof ED_BK_IDS)
#define ED_BK_MAX ((uint32_t)sizeof proj_wire_u.raw)
#define ED_BK_RAW (proj_wire_u.raw)  /* reuse the existing serialized main-loop scratch */
#if FELUCCA_PERF_HW
/* A stable working snapshot while slot GETs reuse hp_io / proj_wire. */
static hp_store_t ed_bk_performance __attribute__((section(".pool"),aligned(4)));
#endif
static persist_t ed_bk_settings __attribute__((section(".pool")));
static uint8_t ed_bk_valid, ed_bk_put, ed_bk_id, ed_bk_gen;
static uint32_t ed_bk_len, ed_bk_crc, ed_bk_pos, ed_bk_ms, ed_bk_usb;
static void ed_bk_u32(uint32_t n) { for (uint32_t i = 0; i < 5u; i++) ed_b((n >> (i * 7u)) & 127u); }
static uint32_t ed_bk_r32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28;
}
static void ed_bk_pack(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, mask = 0;
        for (uint32_t i = 0; i < k; i++) mask |= (uint32_t)(p[i] >> 7) << i;
        ed_b(mask);
        for (uint32_t i = 0; i < k; i++) ed_b(p[i]);
        p += k; n -= k;
    }
}
/* FORMAT 2: a user preset bank is backed up in its stored form (engine numbers 0..5, as in flash), though RAM keeps the
 * running ones: the byte at pos of the stored form, computed on the way out (upreset.c up_bank_map is the same change) */
static uint8_t ed_bk_bank_byte(const uint8_t *p, uint32_t pos)
{
    if (pos >= 8u && (pos - 8u) % sizeof(up_rec_t) == 2u && p[pos - 2u] == UP_USED) return (uint8_t)eng_store(p[pos]);
    return p[pos];
}
static uint32_t ed_bk_bank_crc(const uint8_t *p, uint32_t len)
{
    uint32_t c = 0xFFFFFFFFu, i;
    for (i = 0; i < len; i++) { uint8_t b = ed_bk_bank_byte(p, i); c = st_crc32_add(c, &b, 1u); }
    return ~c;
}
static int ed_bk_is_bank(uint32_t id) { return id == 6u || id == 7u; }
static const uint8_t *ed_bk_object(uint32_t id, uint32_t *len)
{
    *len = 0;
    if (id == 0u) {
#if FELUCCA_PERF_HW
        *len=HP_CAPTURE_BYTES;return (const uint8_t*)&ed_bk_performance;
#else
        *len = sizeof(project_store_t); return ED_BK_RAW;
#endif
    }
    if (id == 1u) { *len = sizeof ed_bk_settings; return (const uint8_t *)&ed_bk_settings; }
    if (id >= 2u && id <= 5u) {
#if FELUCCA_PERF_HW
        const hp_store_t*p=hp_slot(id-2u);if(p){*len=p->size;return (const uint8_t*)p;}
        return ED_BK_RAW; /* no native-only archive in the complete project format */
#endif
        if (project_used(id - 2u)) *len = sizeof proj_slot[0];
        return (const uint8_t *)&proj_slot[id - 2u];
    }
    if (id == 6u || id == 7u) {
        if (up_bank[id - 6u].magic == UP_BANK_MAGIC) *len = sizeof up_bank[0];
        return (const uint8_t *)&up_bank[id - 6u];
    }
    return 0;
}
static uint32_t ed_bk_capture(void)
{
    if (ed_flash_stop()) return 3;
#if FELUCCA_PERF_HW
    hp_store_t*p=&ed_bk_performance;
    if(!hp_capture(p))return 2;
    p->crc=hp_hash((const uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);
#else
    project_capture(&proj_scratch);
    if (!proj_pack((project_store_t *)ED_BK_RAW, &proj_scratch)) return 2;
#endif
    ed_bk_gen = ++proj_wire_gen;
#if FELUCCA_FLASH
    ed_bk_settings = persist_saved;                 /* fields absent from this build survive */
#else
    memset(&ed_bk_settings, 0, sizeof ed_bk_settings);
#endif
    settings_export(&ed_bk_settings);
    ed_bk_put = 0; ed_bk_valid = 1; ed_bk_usb = usb.resets;
    return 0;
}
static int ed_bk_panel_valid(const panel_t *p)
{
    uint32_t b = 0, e = 0;
    if (p->magic != PANEL_MAGIC) return 0;
    for (uint32_t i = 0; i < NB; i++) {
        if (p->btn[i] >= NB || (b & (1u << p->btn[i]))) return 0;
        b |= 1u << p->btn[i];
    }
    for (uint32_t i = 0; i < NE; i++) {
        if (p->enc[i] >= NE || (e & (1u << p->enc[i])) || (p->dir[i] != 1 && p->dir[i] != -1)) return 0;
        e |= 1u << p->enc[i];
    }
    return 1;
}
static uint32_t ed_bk_commit(void)
{
    uint8_t *raw = ED_BK_RAW;
    uint32_t obj;
    if (ed_bk_pos != ed_bk_len || st_crc32(raw, ed_bk_len) != ed_bk_crc) return 2;
    if (ed_bk_id == 0u || (ed_bk_id >= 2u && ed_bk_id <= 5u)) {
#if FELUCCA_PERF_HW
        if(ed_bk_len==sizeof(hp_store_t)||ed_bk_len==HP_V2_SIZE){
            if(hp_io.size!=ed_bk_len)return 2;
            if(!hp_capsule_ok(&hp_io))return 2;
            if(ed_bk_id==0u)return hp_restore(&hp_io)?0u:2u;
            if(!proj_import(&proj_scratch,&hp_io.native,sizeof hp_io.native))return 2;
            if(!hp_project_import(ed_bk_id-2u,&hp_io))return 4;
            if(proj_cur==ed_bk_id-2u)proj_cur=PROJ_NO_SLOT;
            sync_reload=1;ui.force=1;return 0;
        }
#endif
        if (ed_bk_len) return 2; /* a complete HPM2 capsule is mandatory */
        if (ed_bk_len && !proj_import(&proj_scratch, raw, (int)ed_bk_len)) return 2;
        if (ed_bk_len) {                            /* an older format becomes FUN10 inside its ranges */
            proj_bound(&proj_scratch);
            if (!proj_pack((project_store_t *)raw, &proj_scratch)) return 2;
            ed_bk_len = sizeof(project_store_t);
        }
        if (ed_bk_id == 0u) {
            if (!ed_bk_len) return 2;
            uint32_t rc=project_restore_runtime(&proj_scratch) ? 2u : 0u;
#if FELUCCA_HOME_PERF
            if(!rc)hp_init(); /* Native archives contain no performance extension. */
#endif
            return rc;
        }
        if (project_compat_save_blocked(ed_bk_id - 2u)) return 4;
        obj = OBJ_PROJECT0 + ed_bk_id - 2u;
    } else if (ed_bk_id == 1u) {
        const persist_t *p = (const persist_t *)raw;
        if(ed_bk_len!=sizeof *p||p->magic!=PERSIST_MAGIC)return 2;
        if (!((ed_bk_len == sizeof *p && p->magic == PERSIST_MAGIC) ||
              (ed_bk_len == PERSIST_PREENGINE_SIZE && p->magic == 0x50455236u) ||
#if FELUCCA_DRUMX
              (ed_bk_len == PERSIST_PREVOICE_SIZE && p->magic == 0x50455235u) ||
#endif
              (ed_bk_len == PERSIST_BASE_SIZE && p->magic == 0x50455234u)) || !palette_stored_ok(p->palette) ||
            p->lowcut > 3u || !leds_stored_ok(p->zoom) || !hold_stored_ok(p->bold) || p->favorites.filter > 1u || !ed_bk_panel_valid(&p->panel)) return 2;
        obj = OBJ_SETTINGS;
    } else if (ed_bk_id == 6u || ed_bk_id == 7u) {
        const up_bank_t *p = (const up_bank_t *)raw;
        if (ed_bk_len && (ed_bk_len != sizeof *p || p->magic != UP_BANK_MAGIC ||
            p->rsize != sizeof(up_rec_t) || p->nslot != UP_PER_BANK)) return 2;
        if(ed_bk_len)for(uint32_t i=0;i<UP_PER_BANK;i++) {
            up_rec_t r=p->r[i];if(!r.used)continue;
            r.engine=(uint8_t)eng_unstore(r.engine);
            if(r.used!=UP_USED||(r.ver!=4u&&r.ver!=8u)||r.np!=P_COUNT||!eng_ok(r.engine)||!up_valid(&r))return 2;
        }
        /* Unknown record versions remain inert bytes only in legacy builds. */
        obj = OBJ_UPRESET0 + ed_bk_id - 6u;

    } else return 1;
#if FELUCCA_FLASH
    if (!flash_ok || st_save(obj, raw, ed_bk_len)) return 4;
#else
    (void)obj;
#endif
    if (ed_bk_id >= 2u && ed_bk_id <= 5u) {
#if FELUCCA_HOME_PERF
        if(hp_forget(ed_bk_id-2u))return 4; /* Do not resurrect an older capsule over the imported native slot. */
#endif
        memset(&proj_slot[ed_bk_id - 2u], 0, sizeof proj_slot[0]);
        if (proj_cur == ed_bk_id - 2u)
            proj_cur = PROJ_NO_SLOT;                     /* (another project there now: the music keeps its name) */
        if (ed_bk_len) memcpy(&proj_slot[ed_bk_id - 2u], raw, ed_bk_len);
    } else if (ed_bk_id == 1u) {
        memset(&ed_bk_settings, 0, sizeof ed_bk_settings);
        memcpy(&ed_bk_settings, raw, ed_bk_len);
        settings_import(&ed_bk_settings, ed_bk_len);
        panel_init(); settings_init(); palette_apply();
#if FELUCCA_PERF_HW
        hp_default=hp_pref_value(); /* Restored start template must not be overwritten by the live preference. */
#endif
#if FELUCCA_FLASH
        persist_saved = ed_bk_settings; persist_pending = 0;
#endif
    } else {
        uint32_t b = ed_bk_id - 6u;
        memset(&up_bank[b], 0, sizeof up_bank[b]);
        if (ed_bk_len) memcpy(&up_bank[b], raw, ed_bk_len);
        up_bank_check(b, (int)ed_bk_len); up_gen++;
    }
    sync_reload = 1; ui.force = 1;
    return 0;
}
/* a project object's length: FUN10 (today's, 1.2), FUN9 (1.1), FUN8 (before the DRUM lane levels, 1.0.x), FUN7 */
static int proj_store_len(uint32_t len)
{
#if CARAVEL_FORMAT2 && FELUCCA_PERF_HW
    return len == HP_V2_SIZE;
#else
    return
#if FELUCCA_PERF_HW
        len==sizeof(hp_store_t)||len==HP_V2_SIZE||
#endif
        len == sizeof(project_store_t)
        ;
#endif
}
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    /* the arguments first: a malformed request never stops the transport */
    if (n < 2u || a[0] > 3u || a[1] > (CARAVEL_FORMAT2 ? 7u : 9u)) return 1;
    if (a[0] == 0u) {
        if (n != 12u || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if (len > ED_BK_MAX || (a[1] == 0u && !proj_store_len(len)) ||
            (a[1] == 1u && len != sizeof(persist_t) && len != PERSIST_PREENGINE_SIZE && len != PERSIST_PREVOICE_SIZE && len != PERSIST_BASE_SIZE) ||
            (a[1] >= 2u && a[1] <= 5u && len && !proj_store_len(len)) ||
            ((a[1] == 6u || a[1] == 7u) && len && len != sizeof(up_bank_t)) ||
            0) return 1;
        if (ed_flash_stop()) return 3;
        ed_bk_valid = 0; ed_bk_put = 1; ed_bk_id = a[1]; ed_bk_len = len; ed_bk_gen = ++proj_wire_gen;
        ed_bk_crc = ed_bk_r32(a + 7); ed_bk_pos = 0;
        ed_bk_usb = usb.resets; ed_bk_ms = fm1_ms;
        return 0;
    }
    if (!ed_bk_put || ed_bk_id != a[1] || ed_bk_usb != usb.resets || fm1_ms - ed_bk_ms > 15000u ||
        ed_bk_gen != proj_wire_gen) {                 /* (a project save / load reused the staging RAM) */
        ed_bk_put = 0; return 5;
    }
    ed_bk_ms = fm1_ms;
    if (a[0] == 3u) { ed_bk_put = 0; return n == 2u ? 0u : 1u; }
    if (a[0] == 2u) {
        if (n != 2u) return 1;
        if (ed_flash_stop()) return 3;
        uint32_t rc = ed_bk_commit(); ed_bk_put = 0; return rc;
    }
    if (n < 9u || a[6] > 15u || ed_bk_r32(a + 2) != ed_bk_pos) return 1;
    uint32_t count = ed_unpack7(a + 7, n - 7u, ed_smp_buf, 256u);
    if (!count || count > ed_bk_len - ed_bk_pos) return 1;
    if (ed_flash_stop()) return 3;
    memcpy(ED_BK_RAW + ed_bk_pos, ed_smp_buf, count); ed_bk_pos += count;
    return 0;
}
static int ed_backup_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_BACKUP_LIST) {
        uint32_t rc = n ? 1u : ed_bk_capture();
        ed_b(CARAVEL_FORMAT2 ? 2u : 1u); ed_b(rc); ed_b(rc ? 0u : ED_BK_N);
        if (!rc) for (uint32_t i = 0; i < ED_BK_N; i++) {
            uint32_t len;
            const uint8_t *p = ed_bk_object(ED_BK_IDS[i], &len);
            ed_b(ED_BK_IDS[i]); ed_bk_u32(len); ed_bk_u32(ed_bk_is_bank(ED_BK_IDS[i]) && len ? ed_bk_bank_crc(p, len) : st_crc32(p, len));
            fm1_wdt_feed();
        }
        return 1;
    }
    if (cmd == ED_BACKUP_GET) {
        uint32_t len = 0, off = n >= 6u ? ed_bk_r32(a + 1) : 0u;
        uint32_t count = n == 8u ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u;
        const uint8_t *p = n ? ed_bk_object(a[0], &len) : 0;
        uint32_t rc = (!ed_bk_valid || ed_bk_usb != usb.resets
#if !FELUCCA_PERF_HW
                       || (n && !a[0] && ed_bk_gen != proj_wire_gen)
#endif
                       ) ? 5u : transport_busy() ? 3u :
            n != 8u || a[5] > 15u || !p || !count || count > 256u || off > len || count > len - off ? 1u : 0u;
        ed_b(n ? a[0] : 127u); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc && ed_bk_is_bank(a[0]) && len) {         /* (FORMAT 2: the stored form of a bank) */
            uint8_t tmp[256]; uint32_t j;
            for (j = 0; j < count; j++) tmp[j] = ed_bk_bank_byte(p, off + j);
            ed_bk_pack(tmp, count);
        } else if (!rc) ed_bk_pack(p + off, count);
        return 1;
    }
    if (cmd == ED_BACKUP_PUT) {
        uint32_t rc = ed_bk_write(a, n);
        ed_b(n ? a[0] : 127u); ed_b(n >= 2u ? a[1] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
