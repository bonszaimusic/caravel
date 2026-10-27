/* SPDX-License-Identifier: GPL-3.0-only */
/* Real UI, native/combined project and NOR paths, in both storage backends. */
#define FELUCCA_HOME_PERF 1
#if FELUCCA_PERF_HW
#define WEB_FLASH_TEST 1
#endif
#define PAIR_TEST_MAIN pair_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
static project_store_t original, disk;
static uint8_t before_flash[sizeof nor];
static void tap_compat(uint32_t b)
{
    web_buttons(1u << b); run_ms(70);
    web_buttons(0); run_ms(160);
}
static void make_original(uint32_t reverb, int slice)
{
    project_capture(&proj_scratch);
    proj_scratch.g[G_RTYPE] = (int16_t)reverb;
    if (slice) {
        proj_scratch.t[0].engine = 13;
        proj_scratch.t[0].p[P_E0] = 4;
        proj_scratch.t[0].p[P_E1] = 5;
        proj_scratch.t[0].p[P_E2] = -24;
        proj_scratch.t[0].p[P_E4] = 2;
    }
    CHECK(proj_pack(&original, &proj_scratch));
}
static void original_kept(void)
{
    CHECK(!memcmp(&original, &proj_slot[0], sizeof original));
    CHECK(st_load(OBJ_PROJECT0, &disk, sizeof disk) == sizeof disk);
    CHECK(!memcmp(&original, &disk, sizeof original));
}
int main(void)
{
    web_nor_erase(); web_boot(); run_ms(1200);
    CHECK(song.g[G_RTYPE] == 2);
    make_original(0, 1);
    CHECK(proj_import(&proj_scratch, &original, sizeof original));
    CHECK(project_compat_flags(&proj_scratch) == (2u | (!FELUCCA_SLICE ? 1u : 0u)));
    proj_bound(&proj_scratch);
#if !FELUCCA_SLICE
    CHECK(proj_scratch.t[0].p[P_E0] == 4 && proj_scratch.t[0].p[P_E2] == -24);
#endif
    CHECK(!st_save(OBJ_PROJECT0, &original, sizeof original));
    proj_slot[0] = original;
    uint32_t sig = autosave_sig(); hp_model_t home_before = hp;
    project_load(0);
    CHECK(ui.confirm == CF_COMPAT_PROJECT && project_compat_state.hold_autosave);
    CHECK(autosave_sig() == sig && !memcmp(&hp, &home_before, sizeof hp));
    char a[24], b[24]; confirm_text(a, b);
    CHECK(str_eq(b, "ROOM > HALL"));
    draw_confirm();
    if (getenv("COMPAT_SCREENSHOT")) {
        FILE *f = fopen("build/compat-dialog.ppm", "wb"); CHECK(f);
        fprintf(f, "P6\n240 240\n255\n");
        for (uint32_t i = 0; i < 240u * 240u; i++) {
            uint16_t v = (uint16_t)((web_fb[i] >> 8) | (web_fb[i] << 8));
            uint8_t rgb[3] = {(v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31};
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
    tap_compat(B_OCTDN);
    CHECK(!ui.confirm && autosave_sig() == sig && !project_compat_state.hold_autosave);
    original_kept();
    project_load(0); CHECK(ui.confirm == CF_COMPAT_PROJECT);
    web_buttons(1u << B_HOME); run_ms(900); web_buttons(0); run_ms(160);
    CHECK(ui.menu && !ui.confirm && !project_compat_state.hold_autosave);
    tap_compat(B_HOME); CHECK(!ui.menu);
    memcpy(before_flash, nor, sizeof nor);
    trk[1].p[P_FILT] = -13; run_ms(21000);
    CHECK(memcmp(before_flash, nor, sizeof nor)); /* normal session autosave still works */
    original_kept();
    project_load(0); tap_compat(B_OCTUP);
    CHECK(!ui.confirm && project_compat_state.trial && project_compat_state.hold_autosave && (project_compat_state.protected_slots & 1u));
    CHECK(song.g[G_RTYPE] == 2);
    CHECK(trk[0].eng_req == (FELUCCA_SLICE ? 13 : 0));
    original_kept();
    CHECK(project_save_as(0, "BAD") != 0);
    CHECK(hp_project_save(0, "BAD") != 0);
    CHECK(project_erase(0) != 0);
    memcpy(before_flash, nor, sizeof nor);
    trk[1].p[P_FILT] = -23; run_ms(21000);
    CHECK(!memcmp(before_flash, nor, sizeof nor));
    CHECK(!project_save_as(1, "TEST COPY"));
    CHECK(!project_compat_state.trial && !project_compat_state.hold_autosave);
    original_kept();
    CHECK(proj_import(&proj_scratch, &proj_slot[1], sizeof proj_slot[1]));
    CHECK(!project_compat_flags(&proj_scratch));
    project_load(1); CHECK(!ui.confirm && song.g[G_RTYPE] == 2);
#if FELUCCA_PERF_HW
    /* A failed explicit copy remains retryable even while autosave is held. */
    project_load(0); tap_compat(B_OCTUP);
    web_flash_fail_after = 0;
    CHECK(hp_project_save(1, "RETRY COPY") != 0);
    CHECK(project_compat_state.hold_autosave && (hp_pending_mask & 2u));
    web_flash_fail_after = -1; fm1_ms += 1100;
    hp_autosave_poll(); CHECK(!(hp_pending_mask & 2u));
    original_kept();
    CHECK(!hp_project_save(1, "RETRY COPY"));
#endif
    /* Combined HOME/PAIR capsule must pass the same dialog before hp_restore. */
    const hp_store_t *saved = hp_slot(1); CHECK(saved);
    memcpy(&hp_io, saved, sizeof hp_io);
    hp_io.native = original;
    hp_io.crc = hp_hash((const uint8_t *)&hp_io + 32, sizeof hp_io - 32);
    hp_io.reserved[0] = hp_hash(&hp_io, 20);
#if FELUCCA_PERF_HW
    CHECK(hp_project_import(2, &hp_io));
#else
    hp_commit(2, &hp_io); proj_slot[2] = original;
#endif
    sig = autosave_sig(); CHECK(hp_project_load(2));
    CHECK(ui.confirm == CF_COMPAT_PROJECT && autosave_sig() == sig);
    tap_compat(B_OCTUP);
    CHECK(project_compat_state.trial && (project_compat_state.protected_slots & 4u));
    CHECK(hp_project_save(2, 0) != 0);
    CHECK(!hp_project_save(3, "CAPSULE COPY"));
    CHECK(!project_compat_state.trial && !project_compat_state.hold_autosave);
    /* A direct session/USB restore has no durable source: refuse safely. */
    CHECK(proj_import(&proj_scratch, &original, sizeof original));
    sig = autosave_sig(); CHECK(project_restore_runtime(&proj_scratch));
    CHECK(autosave_sig() == sig && !project_compat_state.approved);
    /* RESTORE LAST also asks, preserving the original autosave across cancel. */
#if FELUCCA_PERF_HW
    CHECK(hp_hw_commit(4, 0, 0, 1));
#else
    memset(nor + HP_EXT + 4u * 2u * HP_COPY, 255, 2u * HP_COPY);
#endif
    CHECK(!st_save(OBJ_AUTOSAVE, &original, sizeof original));
    CHECK(!autosave_boot(1)); CHECK(ui.confirm == CF_COMPAT_PROJECT);
    tap_compat(B_OCTDN); memcpy(before_flash, nor, sizeof nor);
    trk[1].p[P_FILT] = -33; run_ms(21000);
    CHECK(!memcmp(before_flash, nor, sizeof nor));
    CHECK(!autosave_boot(1)); tap_compat(B_OCTUP);
    CHECK(project_compat_state.trial && project_compat_state.protected_slots == 15u);
    CHECK(st_load(OBJ_AUTOSAVE, &disk, sizeof disk) == sizeof disk);
    CHECK(!memcmp(&original, &disk, sizeof original));
    /* A source changed while its dialog is open must be re-read and rechecked. */
    project_load(0); CHECK(ui.confirm == CF_COMPAT_PROJECT);
    CHECK(proj_import(&proj_scratch, &original, sizeof original));
    proj_scratch.t[0].engine = 0; proj_scratch.g[G_RTYPE] = 1;
    CHECK(proj_pack(&proj_slot[0], &proj_scratch));
    sig = autosave_sig(); tap_compat(B_OCTUP);
    CHECK(ui.confirm == CF_COMPAT_PROJECT && project_compat_state.warning == 4u);
    CHECK(autosave_sig() == sig && !project_compat_state.approved);
    tap_compat(B_OCTDN); proj_slot[0] = original;
    /* A combined autosave must not fall through to the native fallback. */
    saved = hp_slot(3); CHECK(saved);
    memcpy(&hp_io, saved, sizeof hp_io); hp_io.native = original;
    hp_io.model.macro[0][0].base = 73;
    hp_io.crc = hp_hash((const uint8_t *)&hp_io + 32, sizeof hp_io - 32);
    hp_io.reserved[0] = hp_hash(&hp_io, 20);
#if FELUCCA_PERF_HW
    uint8_t meta[HP_HW_META]; hp_meta_pack(meta, &hp_io.model, &hp_io.pair);
    CHECK(hp_hw_commit(4, meta, &original, 0));
#else
    hp_commit(4, &hp_io);
#endif
    CHECK(!autosave_boot(1)); CHECK(ui.confirm == CF_COMPAT_PROJECT);
    tap_compat(B_OCTUP); CHECK(project_compat_state.trial && hp.macro[0][0].base == 73);
    /* SPRING, HALL, STASIS: exact warning conditions. */
    CHECK(proj_import(&proj_scratch, &original, sizeof original));
    proj_scratch.t[0].engine = 0;
    proj_scratch.g[G_RTYPE] = 1; CHECK(project_compat_flags(&proj_scratch) == 4);
    proj_scratch.g[G_RTYPE] = 2; CHECK(!project_compat_flags(&proj_scratch));
    proj_scratch.g[G_RTYPE] = 3; CHECK(!project_compat_flags(&proj_scratch));
    printf("project_compat PASS: %u checks (hardware backend %u, SLICE %u)\n", checks, FELUCCA_PERF_HW, FELUCCA_SLICE);
    return 0;
}
