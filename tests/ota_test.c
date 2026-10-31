/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/ota.c: a fake M-UPGRADE serves a real .fwsc over
 * the SysEx protocol (pack7, cmd 0x30 reads, 0xE0000000 -> "success") and a
 * simulated NOR plays the flash.
 *   cc -o ota_test tests/ota_test.c && ./ota_test FILE.fwsc
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)   /* (the sources' .pool sections: no Mach-O section names) */

#define FELUCCA_ID "FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000];
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024];                 /* one pending host frame (7-bit bytes) */
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, ident_replies, mute_after = 0xFFFFFFFFu, bad_erase, erases;
static int committed;
static uint8_t committed_parm[112];
static int32_t last_code;
static uint8_t *corrupt_at;              /* flip a served byte (loader body) */

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}

static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static void host_reply(const uint8_t *msg, uint32_t n)       /* queue msg as a device-bound frame */
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

/* ---- hooks ---- */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) { ident_replies++; return 0; }
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg cmd %02x len %u\n", u[2], d); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (++requests > mute_after)
        return 0;                                         /* host gone */
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0;
    m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && corrupt_at >= logical + addr && corrupt_at < logical + addr + len)
            m[14 + (corrupt_at - (logical + addr))] ^= 0x01;
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    host_reply(m, 15 + len);
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len;
    return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms += 1; }
static int ota_erase(uint32_t off)
{
    if (off < 0xE0000u || off >= 0xE5000u || (off & 0xFFFu)) { bad_erase++; return -8; }
    erases++;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < 0xE0000u || off + n > 0xE5000u) { bad_erase++; return -8; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm) { committed = 1; memcpy(committed_parm, parm, 112); }

/* the "official writer" (the one that rewrites the package's whole flash.bin, boot area included) is recognised by its
 * CRCs; the host test has no M-VAVE binary, so the package's own loader stands in for it when these are set (#4) */
static uint32_t off_hcrc, off_dcrc, off_len;
#define OTA_LDR_HCRC off_hcrc
#define OTA_LDR_DCRC off_dcrc
#define OTA_LDR_LEN off_len
#include "../firmware/src/ota.c"

static uint32_t ota_body_off(void)                  /* ota.bin data + 0x20 in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, logical, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 100) return ota_rd32(e + 8) + 0x20u;
    }
    return 0;
}

/* rewrite flash.bin's size in the logical image's UFW entry list (the entries' and the header's CRCs too): a package
 * of the same chip whose app area is another board's size */
static void set_fl_size(uint8_t *lg, uint32_t size)
{
    uint8_t h[0x40 + 11 * 0x50];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0)
            ota_wr32(e + 12, size);
        ota_jl_enc(e, 0x50);
    }
    ota_wr16(h + 2, ota_crc16(h + 0x40, n * 0x50u, 0));   /* (over the entries as stored) */
    ota_wr16(h, ota_crc16(h + 2, 0x3E, 0));
    ota_jl_enc(h, 0x40);
    memcpy(lg, h, 0x40 + n * 0x50u);
}

static uint32_t entry_off(uint32_t type)             /* data offset of a UFW entry in the logical image, 0 if absent */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, logical, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == type) return ota_rd32(e + 8);
    }
    return 0;
}

/* rewrite the package's boot area as the ROM reads it: decode, let mut() change the container (16-byte header,
 * payload), then seal every CRC again, so that only what mut() changed can be wrong. mut == NULL: the same bytes */
static void boot_edit(uint32_t at, void (*mut)(uint8_t *hdr, uint8_t *pl))
{
    uint8_t *f = logical + at, hd[32], rec[32], c[0x4000];
    uint32_t len;
    memcpy(hd, f, 32);
    memcpy(rec, f + 32, 32);
    ota_jl_enc(hd, 32);
    ota_jl_enc(rec, 32);
    len = ota_rd32(rec + 8);
    memcpy(c, f + 0xA0, len);
    ota_jl_enc(c, 16);
    ota_jl_enc_k(c + 16, len - 16, 0xFFFFu);
    if (mut)
        mut(c, c + 16);
    ota_wr16(c + 12, ota_crc16(c + 16, len - 16, 0));
    ota_wr16(c + 14, ota_crc16(c, 14, 0));
    ota_wr16(rec + 2, ota_crc16(c, len, 0));
    ota_wr16(rec, ota_crc16(rec + 2, 30, 0));
    ota_wr16(hd, ota_crc16(hd + 2, 30, 0));
    ota_jl_enc(hd, 32);
    ota_jl_enc(rec, 32);
    ota_jl_enc(c, 16);
    ota_jl_enc_k(c + 16, len - 16, 0xFFFFu);
    memcpy(f, hd, 32);
    memcpy(f + 32, rec, 32);
    memcpy(f + 0xA0, c, len);
}
/* the package's boot area is the SDK's uboot.boot in the clear (a Caravel package never writes its head); the flash
 * image of an official package holds it as burned: the container's header and payload each ciphered from 0xFFFF */
static void boot_burn(uint32_t at)
{
    uint8_t rec[32];
    uint32_t len;
    memcpy(rec, logical + at + 32, 32);
    ota_jl_enc(rec, 32);
    len = ota_rd32(rec + 8);
    ota_jl_enc(logical + at + 0xA0, 16);
    ota_jl_enc_k(logical + at + 0xA0 + 16, len - 16, 0xFFFFu);
}
static void mut_addr(uint8_t *h, uint8_t *pl) { (void)pl; ota_wr32(h + 4, 0x01C02100u); }
static void mut_banks(uint8_t *h, uint8_t *pl) { (void)pl; ota_wr16(h, 2); }
static void mut_pos(uint8_t *h, uint8_t *pl) { (void)pl; ota_wr32(h + 8, 32); }
static void mut_fm1(uint8_t *h, uint8_t *pl) { (void)h; memcpy(pl + 0x40, "AC79BLID", 8); memcpy(pl + 0x48, "fm1", 4); }
static void mut_other(uint8_t *h, uint8_t *pl) { (void)h; memcpy(pl + 0x40, "AC79BLID", 8); memcpy(pl + 0x48, "fm2", 4); }

static int check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = 0xE0000; i < 0xE5000; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + 0x400, 0x4000);                 /* device flash head = package flash.bin head */
    rx_full = 0; requests = 0; ident_replies = 0; committed = 0; last_code = 0;
    mute_after = 0xFFFFFFFFu; corrupt_at = NULL; bad_erase = 0; erases = 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw;
    size_t n, i;
    int bad = 0, rc;
    if (argc < 2) { printf("usage: ota_test FILE.fwsc\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x4400) { printf("%s: too short for a package\n", argv[1]); return 2; }
    logical = malloc(n);
    for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);   /* drop the marker bytes */
    memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
    logical_len = n - 20;

    /* handshake outside a session */
    reset_dev();
    {
        uint8_t m[7] = {0, 0x59, 0x11, 0, 0, 0, 0xFF};
        host_reply(m, 7);
        ota_service();
        bad += check("handshake answered", ident_replies == 1);
        bad += check("identity frame is " FELUCCA_ID,
                     ota_msg[2] == 0x11 && !memcmp(ota_msg + 6, FELUCCA_ID, sizeof FELUCCA_ID - 1));
    }

    /* full session */
    reset_dev();
    rc = ota_session();
    printf("  %u requests\n", requests);
    bad += check("session commits (flash.bin exactly 0x93000)", rc == 0 && committed);
    bad += check("headers + loader + confirm, no head reads", requests >= 10 && requests <= 30);
    {
        uint8_t *h = nor + 0xE0000, *p = committed_parm;
        bad += check("staged body == the package's loader", !memcmp(nor + 0xE0020, logical + ota_body_off(), 64));
        bad += check("LOADER.BIN head CRC",
                     (h[0] | h[1] << 8) == ota_crc16(h + 2, 30, 0) && !memcmp(h + 16, "LOADER.BIN", 10));
        bad += check("record at 0xE4F00 == RAM record", !memcmp(nor + 0xE4F00, p, 112));
        bad += check("record CRC / type / magic / ota_addr",
                     (p[0] | p[1] << 8) == ota_crc16(p + 2, 78, 0) && (p[2] | p[3] << 8) == 0x5A0D &&
                     (p[6] | p[7] << 8) == 0x5441 && ota_rd32(p + 72) == 0xE0000);
        bad += check("nothing written outside 0xE0000..0xE4FFF", bad_erase == 0);
    }
    /* boot cleanup erases it */
    bad += check("boot cleanup erases the area", ota_boot_cleanup() == 1 && area_erased());
    bad += check("second boot: nothing to do", ota_boot_cleanup() == 0);

    /* 1.6.1: another board's package (flash.bin not this board's app area size): refused in step 1, nothing erased */
    {
        uint8_t keep[0x40 + 11 * 0x50];
        memcpy(keep, logical, sizeof keep);
        reset_dev();
        set_fl_size(logical, 0x9D000u);
        rc = ota_session();
        bad += check("another board's size (0x9D000): -15, no commit, no erase", rc == -15 && !committed && erases == 0 &&
                     area_erased());
        memcpy(logical, keep, sizeof keep);
    }
    /* host disappears in the middle */
    reset_dev();
    mute_after = 4;
    rc = ota_session();
    bad += check("host gone -> no commit, area clean", rc < 0 && !committed && area_erased());

    /* host never confirms */
    reset_dev();
    mute_after = 0xFFFFFFFFu;
    {
        uint32_t total;
        rc = ota_session();
        total = requests;                                  /* requests of a full run incl. confirm */
        reset_dev();
        mute_after = total - 1;                            /* answer everything but the confirmation */
        rc = ota_session();
        bad += check("no 'success' -> -13, no commit, area clean", rc == -13 && !committed && area_erased());
    }

    /* #4: the official writer rewrites the head, and the package's must be a bootable one: it need not equal the device's */
    {
        uint32_t fl = entry_off(0), ob = entry_off(100);
        uint8_t keep[0x4400], dev_head[0x4000];
        if (!fl || !ob || fl + 0x4000u > sizeof keep * 4 || fl + sizeof keep > logical_len) {
            printf("no flash.bin / ota.bin entry in %s\n", argv[1]);
            return 2;
        }
        boot_burn(fl);
        memcpy(keep, logical + fl, 0x4000);
        {
            uint8_t b[512];
            memcpy(b, logical + ob, 512);
            off_hcrc = ota_rd16(b);
            off_dcrc = ota_rd16(b + 2);
            off_len = ota_rd32(b + 8);
        }
        printf("official-writer path (the package's loader stands in)\n");

        reset_dev();
        bad += check("boot area as packaged: valid", (rc = ota_session()) == 0 && committed);
        boot_edit(fl, NULL);
        reset_dev();
        bad += check("re-sealed unchanged: still valid", ota_session() == 0 && committed);
        memcpy(logical + fl, keep, 0x4000);

        reset_dev();                                         /* device head: nothing like the package's */
        memset(nor, 0xA5, 0x4000);
        memcpy(dev_head, nor, 0x4000);
        rc = ota_session();
        bad += check("device head differs completely: accepted", rc == 0 && committed && erases > 0);
        bad += check("... and the head is not read or written", !memcmp(nor, dev_head, 0x4000));
        reset_dev();
        for (i = 0; i < 0x4000; i++) nor[i] = (uint8_t)(nor[i] ^ (i * 131u + 7u));
        bad += check("device head scrambled: accepted", ota_session() == 0 && committed);
        reset_dev();
        memset(nor, 0xFF, 0x4000);
        bad += check("device head blank: accepted", ota_session() == 0 && committed);

        /* a boot area the ROM would not boot: refused in step 2, before any erase */
        {
            static const struct { const char *what; uint32_t at; } flip[] = {
                {"flash header byte", 5}, {"boot record CRC", 32}, {"boot record data CRC", 34},
                {"boot record offset (0xA0)", 36}, {"boot record length", 40}, {"container header", 0xA0 + 4},
                {"container payload CRC (header)", 0xA0 + 12}, {"payload first byte", 0xA0 + 16}, {"payload middle", 0xA0 + 300},
                {"payload last byte", 0},
            };
            uint32_t len;
            uint8_t rec[32];
            memcpy(rec, logical + fl + 32, 32);
            ota_jl_enc(rec, 32);
            len = ota_rd32(rec + 8);
            for (i = 0; i < sizeof flip / sizeof flip[0]; i++) {
                uint32_t at = flip[i].at ? flip[i].at : 0xA0 + len - 1;
                char what[96];
                logical[fl + at] ^= 0x04;
                reset_dev();
                rc = ota_session();
                snprintf(what, sizeof what, "%s damaged: -5, nothing erased", flip[i].what);
                bad += check(what, rc == -5 && !committed && erases == 0 && area_erased() && bad_erase == 0);
                logical[fl + at] ^= 0x04;
            }
        }
        {
            static const struct { const char *what; void (*mut)(uint8_t *, uint8_t *); int rc; } m[] = {
                {"load address 0x01C02100 (CRCs right)", mut_addr, -5}, {"two banks (CRCs right)", mut_banks, -5},
                {"payload not at +16 (CRCs right)", mut_pos, -5}, {"names the FM-1 (AC79BLID fm1)", mut_fm1, 0},
                {"names another board (AC79BLID fm2)", mut_other, -5},
            };
            for (i = 0; i < sizeof m / sizeof m[0]; i++) {
                char what[96];
                boot_edit(fl, m[i].mut);
                reset_dev();
                rc = ota_session();
                snprintf(what, sizeof what, "%s: %s", m[i].what, m[i].rc ? "refused, nothing erased" : "accepted");
                bad += check(what, m[i].rc ? (rc == m[i].rc && !committed && erases == 0 && area_erased())
                                           : (rc == 0 && committed));
                memcpy(logical + fl, keep, 0x4000);
            }
        }
        {                                                    /* a flash.bin too short for a boot area is another board's: -15 first */
            uint8_t ent[0x40 + 11 * 0x50];
            memcpy(ent, logical, sizeof ent);
            reset_dev();
            set_fl_size(logical, 0x3000u);
            bad += check("flash.bin of 0x3000: -15, nothing erased", ota_session() == -15 && erases == 0);
            memcpy(logical, ent, sizeof ent);
        }
        {                                                    /* the host goes away, or never confirms, on this path too */
            uint32_t total;
            reset_dev();
            rc = ota_session();
            total = requests;
            for (i = 3; i < 8; i++) {                        /* mid-UFW header, loader header, boot area reads */
                char what[96];
                reset_dev();
                mute_after = (uint32_t)i;
                rc = ota_session();
                snprintf(what, sizeof what, "host gone after %u requests: no commit, nothing staged", (unsigned)i);
                bad += check(what, rc < 0 && !committed && erases == 0 && area_erased());
            }
            reset_dev();
            mute_after = total - 1;
            rc = ota_session();
            bad += check("no 'success': -13, no commit, area clean", rc == -13 && !committed && area_erased());
        }
        reset_dev();                                         /* and the package is whole again */
        bad += check("restored package: valid again", ota_session() == 0 && committed);
    }
    printf("%s\n", bad ? "OTA TEST FAILED" : "ota test passed");
    return bad != 0;
}
