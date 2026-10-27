/* SPDX-License-Identifier: GPL-3.0-only
 * Real DSP + panel/storage harness, with either hardware or browser storage. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN layer_pair_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"

static int16_t old_bank[ST_RING_LEN], expected_bank[ST_RING_LEN];
static uint32_t sample_clock;
static uint64_t layer_render(uint32_t frames, uint32_t input)
{
    int32_t rev[CTL], zero[CTL] = {0}, out[CTL];
    uint64_t energy = 0;
    while (frames) {
        uint32_t n = frames < CTL ? frames : CTL;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t p = sample_clock++;
            rev[i] = input == 1 ? osc_sine(p * 25000000u) * 3 :
                     input == 2 ? osc_sine(p * 37000000u) * 3 : input == 3 ? 131068 : 0;
            out[i] = 0;
        }
        fx_buses(zero, zero, rev, out, n);
        for (uint32_t i = 0; i < n; ++i) {
            int32_t l = out[i] + rev_side[i], r = out[i] - rev_side[i];
            CHECK(l > -1000000 && l < 1000000 && r > -1000000 && r < 1000000);
            energy += (uint64_t)abs(l) + (uint64_t)abs(r);
        }
        frames -= n;
    }
    return energy;
}
static void layer_init(void)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx); memset(&hl, 0, sizeof hl); memset(&st, 0, sizeof st);
    memset(dly_buf, 0, sizeof dly_buf); stasis_layer_reset(); rev_clear(); sample_clock = 0;
    song.g[G_RTYPE] = 3; song.g[G_RSIZE] = 95; song.g[G_DRREV] = 75;
    song.g[G_STMODE] = 0; song.g[G_STAMT] = 32; song.g[G_STTIME] = 1;
    CHECK(!stasis_layer_command(0)); /* A LIVE texture must not start an overdub. */
    layer_render(ST_RING_LEN + ST_GRAIN_LEN, 1);
    song.g[G_DRLVL] = 1;
    layer_render(ST_GRAIN_LEN, 0);
    CHECK(st.auto_hold && st.valid == ST_RING_LEN && stasis_layer_can_start());
    memcpy(old_bank, dly_buf, sizeof old_bank);
}
static void prepare_incoming(uint32_t mode, uint32_t input)
{
    song.g[G_STMODE] = (int16_t)mode;
    layer_render(ST_RING_LEN, input);
    CHECK(st_layer.valid == ST_RING_LEN && st_layer.phase == STL_IDLE);
    CHECK(!memcmp(old_bank, dly_buf, sizeof old_bank));
    CHECK(stasis_layer_command(0));
    layer_render(1, input);
    CHECK(st_layer.phase == STL_FADE && st_layer.left);
}
static void finish_and_check(uint32_t input)
{
    uint32_t delta = st_layer.delta, anchor = st.w;
    int32_t target = st_layer.target;
    for (uint32_t p = 0; p < ST_RING_LEN; ++p)
        expected_bank[p] = (int16_t)((old_bank[p] * (32768 - target) +
            dly_buf[ST_RING_LEN + ((p + delta) & ST_RING_MASK)] * target) >> 15u);
    layer_render(st_layer.left, input);
    CHECK(st_layer.phase == STL_MERGE && st_layer.mix == target && !st_layer.merge);
    CHECK(!memcmp(old_bank, dly_buf, sizeof old_bank));
    for (uint32_t p = 0; p < ST_RING_LEN; ++p) CHECK(stasis_read(p) == expected_bank[p]);
    /* Incremental copy must be transparent to every playing grain address. */
    for (uint32_t block = 0; block < ST_RING_LEN / CTL; ++block) {
        layer_render(CTL, 0);
        for (uint32_t k = 0; k < 17; ++k) {
            uint32_t p = (block * 757u + k * 1901u) & ST_RING_MASK;
            CHECK(stasis_read(p) == expected_bank[p]);
        }
    }
    CHECK(!st_layer.phase && !st_layer.mix && st.w == anchor && song.g[G_DRLVL] == 1);
    CHECK(!memcmp(expected_bank, dly_buf, sizeof expected_bank));
}
static void panel_tap(uint32_t b)
{
    web_buttons(1u << b); run_ms(70); web_buttons(0); run_ms(160);
}
static void layer_screenshot(void)
{
    const char *path = getenv("LAYER_SCREENSHOT");
    if (!path) return;
    ui.force = 1; ui_draw();
    FILE *f = fopen(path, "wb"); CHECK(f);
    fprintf(f, "P6\n240 240\n255\n");
    for (uint32_t i = 0; i < 240u * 240u; ++i) {
        uint16_t v = (uint16_t)((web_fb[i] >> 8) | (web_fb[i] << 8));
        uint8_t rgb[3] = {(v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
int main(void)
{
    web_nor_erase(); web_boot(); run_ms(1200);
    layer_init();
    CHECK(stasis_layer_command(0)); layer_render(FS * 3u, 0);
    CHECK(st_layer.phase == STL_WAIT && !st_layer.mix && !memcmp(old_bank, dly_buf, sizeof old_bank));
    CHECK(stasis_layer_command(1)); layer_render(CTL, 0);
    CHECK(!st_layer.phase && !memcmp(old_bank, dly_buf, sizeof old_bank));
    puts("stasis-layer: silence waits without erasing; CANCEL keeps original bytes");

    layer_init(); prepare_incoming(0, 2); finish_and_check(2);
    puts("stasis-layer: ADD retains exact old/new weights; merge is sample-identical");
    for (uint32_t i = 0; i < 8; ++i) {
        memcpy(old_bank, dly_buf, sizeof old_bank);
        song.g[G_STAMT] = 64;
        prepare_incoming(0, i & 1u ? 3 : 2); finish_and_check(0);
    }
    puts("stasis-layer: repeated ADD with hot inputs remains bounded");

    layer_init(); song.g[G_STAMT] = 0; prepare_incoming(1, 2);
    CHECK(st_layer.target == 32768 && st_layer.left == FS - 1u);
    finish_and_check(2);
    puts("stasis-layer: BLEND follows TIME, ignores ADD amount, then holds the new source");

    layer_init(); song.g[G_STTIME] = 3; prepare_incoming(1, 2);
    layer_render(FS / 3u, 2); CHECK(st_layer.mix > 0 && st_layer.mix < 32768);
    CHECK(stasis_layer_command(1)); layer_render(FS / 5u, 0);
    CHECK(!st_layer.phase && !st_layer.mix && !memcmp(old_bank, dly_buf, sizeof old_bank));
    puts("stasis-layer: cancelling a blend fades back and preserves the original exactly");

    layer_init(); prepare_incoming(1, 2); layer_render(FS / 4u, 2);
    stasis_transport_stop(); CHECK(st.stopping && !st.auto_hold && !st_layer.request);
    uint64_t early = layer_render(FS, 0), late = layer_render(FS, 0);
    CHECK(!st_layer.phase && early > late && !song.g[G_DRLVL]);
    stasis_transport_start(); layer_render(CTL, 0);
    CHECK(!st_layer.mix && !st_layer.phase && st.valid < ST_MIN_VALID);
    puts("stasis-layer: STOP fades the audible mixture; PLAY cannot resurrect an operation");

    layer_init(); prepare_incoming(1, 2); layer_render(FS / 4u, 2);
    song.g[G_DRLVL] = 0; layer_render(FS * 2u, 0);
    CHECK(!st.releasing && !st.auto_hold && !st_layer.phase && !st_layer.mix);
    puts("stasis-layer: HOLD to LIVE completes the existing release envelope");

    layer_init();
    uint32_t page = NPAGES;
    for (uint32_t i = 0; i < NPAGES; ++i) if (PAGES[i].graph == GR_STLAYER) page = i;
    CHECK(page < NPAGES && page_visible(page));
    song.g[G_RTYPE] = 2; CHECK(!page_visible(page)); song.g[G_RTYPE] = 3;
    ui.home = 0; ui.page = (uint8_t)page; page_entered();
    layer_screenshot();
    CHECK(ui.act == 4 && act_cols() == 8 && act_ready());
    edit_param(0, 1); edit_param(1, 1); edit_param(2, 1); edit_param(3, 1);
    CHECK(song.g[G_STMODE] == 1 && !st_layer.request && !st_layer.phase);
    panel_tap(B_OCTUP); CHECK(st_layer.phase == STL_WAIT && stasis_layer_can_cancel());
    panel_tap(B_OCTUP); CHECK(!st_layer.phase && !st_layer.request);
    CHECK(!memcmp(old_bank, dly_buf, sizeof old_bank));
    ui.force = 1; ui_draw();
    puts("stasis-layer: real panel navigation cannot trigger recording; OCT+ starts/cancels");

    static project_store_t packed;
    song.g[G_STMODE] = 1; song.g[G_STAMT] = 73; song.g[G_STTIME] = 12;
    project_capture(&proj_scratch); CHECK(proj_pack(&packed, &proj_scratch));
    CHECK(proj_import(&proj_scratch, &packed, sizeof packed)); proj_bound(&proj_scratch);
    CHECK(proj_scratch.g[G_STMODE] == 1 && proj_scratch.g[G_STAMT] == 73 && proj_scratch.g[G_STTIME] == 12);
    CHECK(G_COUNT == 27 && ST_RING_LEN * 2u == DLY_LEN && sizeof dly_buf == 131072u);
    printf("stasis-layer: PASS %u checks; storage=%u, extra state=%zu bytes, no added audio buffers\n",
           checks, FELUCCA_PERF_HW, sizeof st_layer);
    return 0;
}
