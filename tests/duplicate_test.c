/* SPDX-License-Identifier: GPL-3.0-only */
/* DUP x2 through real UI, motion, project and sequencer paths. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void dup_boot(uint32_t k, uint32_t len)
{
    ui_power_on();
    memset(seq_extend_end, 0, sizeof seq_extend_end);
    memset(seq_extend_target, 0, sizeof seq_extend_target);
    track_select(k);
    TSEL->p[P_SLEN] = (int16_t)len;
    for (uint32_t i = 0; i < NSTEP; i++) {
        step_t *s = &TSEL->step[i];
        step_clear(s);
        s->time = i % 3u == 1u ? ST_TIE : i % 3u == 2u ? ST_REST : ST_NOTE;
        s->n = s->time == ST_NOTE ? 4u : 0u;
        for (uint32_t j = 0; j < 4u; j++) s->note[j] = (uint8_t)(36u + i + j);
        s->vel = (uint8_t)(40u + i);
        s->flags |= SF_ACCENT | SF_SLIDE;
        step_set_chance(s, 50);
        step_set_ratchet(s, 3);
        step_set_nudge(s, -3);
    }
}
static int event_at(uint32_t place, uint32_t param, int32_t value)
{
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (e->place == place && e->param == param && e->value == value) return 1;
    }
    return 0;
}
/* Track replacement may regroup the shared pool. Within-track FX press order
 * must remain stable; unrelated tracks' position in the pool is immaterial. */
static int motion_same(const motion_store_t *a, const motion_store_t *b)
{
    if (a->count != b->count || a->on != b->on || memcmp(a->rsv, b->rsv, sizeof a->rsv)) return 0;
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t j = 0;
        for (uint32_t i = 0; i < a->count; i++) {
            if ((a->event[i].place >> 6) != k) continue;
            while (j < b->count && (b->event[j].place >> 6) != k) j++;
            if (j == b->count || memcmp(&a->event[i], &b->event[j++], sizeof a->event[i])) return 0;
        }
    }
    return 1;
}
int main(void)
{
    int bad = 0, ok = 1;
    step_t before[NSTEP];
    motion_store_t original;
    /* Every legal length and track: unaligned lengths, boundary 1/32, all bytes. */
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t len = 1; len <= 32u; len++) {
            dup_boot(k, len);
            track_t other = trk[(k + 1u) % NTRK];
            memcpy(before, TSEL->step, sizeof before);
            ok &= tl_do(TSEL, TL_DUP, 0) != 0 && TSEL->p[P_SLEN] == 2u * len;
            ok &= !memcmp(TSEL->step, before, len * sizeof before[0]);
            ok &= !memcmp(TSEL->step + len, before, len * sizeof before[0]);
            ok &= !memcmp(TSEL->step + 2u * len, before + 2u * len, (NSTEP - 2u * len) * sizeof before[0]);
            ok &= !memcmp(&other, &trk[(k + 1u) % NTRK], sizeof other);
        }
    bad += check("DUP: all 1..32 lengths on all tracks copy every step byte; others/tail stay", ok);
    dup_boot(0, 16);
    TSEL->eng_req = ENGI_DRUM;
    grid_hit(TSEL, 0, DV_KICK, 1); grid_acc(TSEL, 0, DV_KICK, 1);
    grid_hit(TSEL, 3, DV_SNARE, 1);
    memcpy(before, TSEL->step, sizeof before);
    bad += check("DUP: drum lanes and accents copy together", tl_do(TSEL, TL_DUP, 0) &&
        !memcmp(before, TSEL->step + 16, 16u * sizeof before[0]));

    dup_boot(2, 24);
    motion.on = 0xAu;
    motion.rsv[0] = 0x20u; /* global FX owner T3 */
    motion.count = 7;
    motion.event[0] = (motion_event_t){128, P_PAN, 30};
    motion.event[1] = (motion_event_t){131, P_PAN | MOTION_LOCK, -20};
    motion.event[2] = (motion_event_t){132, MOTION_FX_FIRST + PF_REPEAT, 20};
    motion.event[3] = (motion_event_t){132, (MOTION_FX_FIRST + PF_REPEAT) | MOTION_LOCK, (int8_t)210};
    motion.event[4] = (motion_event_t){160, P_PAN, 45}; /* overwritten destination */
    motion.event[5] = (motion_event_t){180, P_PAN, 40}; /* preserved tail */
    motion.event[6] = (motion_event_t){64, P_PAN, 50}; /* other track */
    motion_fx_meta(&motion);
    original = motion;
    memcpy(before, TSEL->step, sizeof before);
    bad += check("DUP: copy automation, locks and fine FX edges; replace stale destination", tl_do(TSEL, TL_DUP, 0) &&
        motion.count == 10 && event_at(152, P_PAN, 30) && event_at(155, P_PAN | MOTION_LOCK, -20) &&
        event_at(156, MOTION_FX_FIRST + PF_REPEAT, 20) &&
        event_at(156, (MOTION_FX_FIRST + PF_REPEAT) | MOTION_LOCK, (int8_t)210) && !event_at(160, P_PAN, 45));
    bad += check("DUP: retain tail, other track, automation switches and FX owner", event_at(180, P_PAN, 40) &&
        event_at(64, P_PAN, 50) && motion.on == 0xAu && motion_fx_track() == 2u && motion_valid(&motion));
    motion_store_t doubled = motion;
    undo_swap();
    bad += check("DUP: undo restores LEN, all steps and destination motion", TSEL->p[P_SLEN] == 24 &&
        !memcmp(before, TSEL->step, sizeof before) && motion_same(&original, &motion));
    undo_swap();
    bad += check("DUP: redo restores doubled pattern and events", TSEL->p[P_SLEN] == 48 &&
        !memcmp(TSEL->step, TSEL->step + 24, 24u * sizeof before[0]) && motion_same(&doubled, &motion));
    bad += check("DUP: save project", project_save(0) == 0);
    TSEL->p[P_SLEN] = 1; motion.count = 0;
    project_load(0);
    bad += check("DUP: project round trip keeps copies, timing and FX owner", trk[2].p[P_SLEN] == 48 &&
        motion_same(&doubled, &motion) && motion_fx_track() == 2u);

    dup_boot(2, 8);
    motion.on = 4;
    motion.rsv[0] = 0x20u;
    motion_set_event(TSEL, 0, P_PAN, 30);
    motion_set_lock(TSEL, 3, P_PAN, -20);
    motion_fx_put(TSEL, 1, PF_LPF, 20, 1);
    motion_fx_put(TSEL, 1, PF_LPF, 210, 0);
    tl_do(TSEL, TL_DUP, 0);
    seq_start(); seq_tick(TSEL, CTL);
    motion_step(TSEL, 8, &motion, 1);
    bad += check("DUP: copied sound automation executes", TSEL->p[P_PAN] == 30);
    motion_step(TSEL, 11, &motion, 1);
    bad += check("DUP: copied parameter lock executes", TSEL->p[P_PAN] == -20);
    motion_step(TSEL, 12, &motion, 1);
    bad += check("DUP: copied lock releases to the latest copied automation", TSEL->p[P_PAN] == 30);
    TSEL->seq_idx = 9; TSEL->seq_pos = 0;
    motion_fx_step(TSEL, &motion);
    uint32_t fx_length = step_samples(TSEL, div_samples(TSEL->p[P_SDIV]), 9);
    TSEL->seq_pos = (fx_length * 32u + 255u) / 256u;
    seq_tick(TSEL, 0);
    bad += check("DUP: copied FX ON replays at its fine phase on global owner", perf_motion[2] == PF_BIT(PF_LPF));
    TSEL->seq_pos = (fx_length * 220u + 255u) / 256u;
    seq_tick(TSEL, 0);
    bad += check("DUP: copied FX OFF releases at its fine phase", !perf_motion[2]);
    seq_stop();

    /* Exact-full pool succeeds if the replaced range supplies the space. */
    dup_boot(0, 16);
    motion.count = MOTION_MAX;
    for (uint32_t i = 0; i < MOTION_MAX; i++)
        motion.event[i] = (motion_event_t){(uint8_t)(i < 64u ? i : 64u + i - 64u), P_PAN, 20};
    bad += check("DUP: exactly 128 events allowed after replacing the destination", tl_do(TSEL, TL_DUP, 0) &&
        motion.count == MOTION_MAX && TSEL->p[P_SLEN] == 32);
    dup_boot(0, 16);
    motion.count = MOTION_MAX;
    for (uint32_t i = 0; i < MOTION_MAX; i++)
        motion.event[i] = (motion_event_t){(uint8_t)(i ? 64u + (i % 64u) : 0), i < 64u ? P_PAN : P_REV, 20};
    original = motion;
    memcpy(before, TSEL->step, sizeof before);
    tl_undo_take(&trk[1]);
    uint8_t undo_track = undo.trk;
    bad += check("DUP: overflow rejects atomically and keeps previous undo", !tl_do(TSEL, TL_DUP, 0) &&
        msg_is("AUTOMATION FULL") && TSEL->p[P_SLEN] == 16 && undo.trk == undo_track &&
        !memcmp(before, TSEL->step, sizeof before) && !memcmp(&original, &motion, sizeof motion));
    motion.count = MOTION_MAX - 1u;
    motion_fx_owner[PF_REPEAT] = 2;
    bad += check("DUP: another track's reserved FX release counts toward capacity", !tl_do(TSEL, TL_DUP, 0) &&
        msg_is("AUTOMATION FULL") && TSEL->p[P_SLEN] == 16);
    motion_fx_owner[PF_REPEAT] = 1;
    bad += check("DUP: own unfinished recorded hold refuses before mutation", !tl_do(TSEL, TL_DUP, 0) &&
        msg_is("RELEASE FX FIRST") && TSEL->p[P_SLEN] == 16);
    memset(motion_fx_owner, 0, sizeof motion_fx_owner);
    for (uint32_t len = NSTEP; len <= NSTEP; len++) {
        dup_boot(0, len);
        ok &= !tl_do(TSEL, TL_DUP, 0) && msg_is("64 STEPS: NO ROOM") && TSEL->p[P_SLEN] == len && !undo.trk;
    }
    bad += check("DUP: 64 steps refuses without new undo", ok);

    /* Actual key path: silent D5 on melodic and drum tracks, OCT- restoration. */
    dup_boot(0, 16);
    go_page(GR_ROLL); frame();
    usb.config = 1;
    uint32_t sent = mo_w;
    memcpy(before, TSEL->step, sizeof before);
    btn_down(B_SEQ); frames(500);
    key_down(white(TL_DOUBLE)); frame(); key_up(white(TL_DOUBLE)); frame();
    bad += check("DUP: SEQ + D5 doubles via UI, with track/length message and no MIDI note", TSEL->p[P_SLEN] == 32 &&
        msg_is("DOUBLED: 32 STEPS") && ui.layer == LAYER_SEQ && !gates() && sent == mo_w && lys.loaded);
    oct_back();
    bad += check("DUP: layer OCT- restores all original steps and length", TSEL->p[P_SLEN] == 16 &&
        !memcmp(before, TSEL->step, sizeof before));
    btn_up(B_SEQ); frame(); usb.config = 0;
    dup_boot(0, 16); chain.running = 1;
    tools_key(TL_DUP);
    bad += check("DUP: SONG retains existing STOP TO EDIT guard", msg_is("STOP TO EDIT") && TSEL->p[P_SLEN] == 16);
    chain.running = 0;

    /* Playback: finish current short pass, then enter the doubled pass. */
    dup_boot(1, 16);
    TSEL->p[P_SDIV] = 2; TSEL->p[P_SSWING] = 37;
    seq_start(); seq_tick(TSEL, 64);
    TSEL->seq_idx = 15;
    uint32_t length = step_samples(TSEL, div_samples(TSEL->p[P_SDIV]), 15);
    TSEL->seq_pos = length - 100;
    uint32_t at = TSEL->seq_pos;
    song.rec = 2;
    bad += check("DUP: live edit retains playback, record arm and cursor", tl_do(TSEL, TL_DUP, 0) &&
        song.playing && song.rec == 2 && TSEL->seq_idx == 15 && TSEL->seq_pos == at && seq_loop_len(TSEL) == 16);
    seq_tick(TSEL, 50);
    bad += check("DUP: current swung loop still finishes at its old end", TSEL->seq_idx == 15 && seq_loop_len(TSEL) == 16);
    seq_tick(TSEL, 64);
    bad += check("DUP: old boundary wraps to zero, preserves overshoot, activates new length", TSEL->seq_idx == 0 &&
        TSEL->seq_pos == 14 && seq_loop_len(TSEL) == 32 && !seq_extend_end[1]);
    TSEL->seq_idx = 15; TSEL->seq_pos = length - 10;
    seq_tick(TSEL, 16);
    bad += check("DUP: next pass reaches copied second half instead of wrapping", TSEL->seq_idx == 16 && TSEL->seq_pos == 6);
    seq_stop();

    dup_boot(0, 8);
    TSEL->p[P_SQNT] = 0;
    step_set_ratchet(&TSEL->step[0], 1);
    step_set_chance(&TSEL->step[0], 100);
    seq_start(); seq_tick(TSEL, CTL);
    TSEL->seq_idx = 7;
    uint32_t nd_period = div_samples(TSEL->p[P_SDIV]);
    uint32_t nd_length = step_samples(TSEL, nd_period, 7);
    TSEL->seq_pos = 0;
    seq_nd_start(TSEL, nd_period, 8);
    tl_do(TSEL, TL_DUP, 0);
    TSEL->seq_pos = nd_length - 32;
    seq_tick(TSEL, 16);
    bad += check("DUP: early nudge across old boundary still targets step zero", TSEL->nd_fired == 1 && TSEL->seq_idx == 7);
    seq_tick(TSEL, 32);
    bad += check("DUP: nudged wrap activates doubled length without refiring step zero", TSEL->seq_idx == 0 &&
        TSEL->seq_pos == 16 && TSEL->nd_fired == 0 && seq_loop_len(TSEL) == 16);
    seq_stop();

    dup_boot(0, 16); seq_start(); seq_tick(TSEL, 64);
    tl_do(TSEL, TL_DUP, 0); tl_do(TSEL, TL_DUP, 0);
    bad += check("DUP: rapid two presses grow 16 to 64 but finish old 16-step pass", TSEL->p[P_SLEN] == 64 && seq_loop_len(TSEL) == 16);
    undo_swap();
    bad += check("DUP: live undo restores 32 and cancels transition", TSEL->p[P_SLEN] == 32 && !seq_extend_end[0]);
    tl_do(TSEL, TL_DUP, 0);
    TSEL->p[P_SLEN] = 12;
    bad += check("DUP: manual LEN edit cancels transition", seq_loop_len(TSEL) == 12 && !seq_extend_end[0]);
    TSEL->p[P_SLEN] = 16;
    tl_do(TSEL, TL_DUP, 0); seq_stop(); seq_start();
    bad += check("DUP: stop/start keeps doubled data and starts with new length", TSEL->p[P_SLEN] == 32 && seq_loop_len(TSEL) == 32);
    seq_stop();
    dup_boot(0, 8); seq_start(); seq_tick(TSEL, CTL); tl_do(TSEL, TL_DUP, 0);
    load_begin(TSEL, UNDO_PAT); load_end(TSEL);
    bad += check("DUP: phrase-load path cancels transition even at the same LEN", !seq_extend_end[0] && seq_loop_len(TSEL) == 16);
    seq_stop();

    dup_boot(0, 16);
    bad += check("DUP: save launch target", project_save(1) == 0);
    seq_start(); seq_tick(TSEL, 64); tl_do(TSEL, TL_DUP, 0);
    bad += check("DUP: queue live part during transition", project_pattern_queue(1) == 0);
    TSEL->seq_idx = 15;
    TSEL->seq_pos = step_samples(TSEL, div_samples(TSEL->p[P_SDIV]), 15) - 32;
    project_pattern_tick(64);
    bad += check("DUP: queued part uses old boundary and clears transition on all tracks", !chain.live_pending &&
        chain.live_applied && TSEL->p[P_SLEN] == 16 && !seq_extend_end[0] && !seq_extend_end[1]);
    seq_stop();
    /* 1.6 overlap: clipped DOUBLE and HALF retain raw FX phase and conditions. */
    dup_boot(0, 40);
    step_set_chance_v(&TSEL->step[2], 100u + COND_FILL);
    motion_fx_put(TSEL, 2, PF_LPF, 93, 1);
    motion_fx_put(TSEL, 3, PF_LPF, 201, 0);
    motion_fx_put(TSEL, 30, PF_HPF, 7, 1);
    ok = tl_do(TSEL, TL_DOUBLE, 0) && TSEL->p[P_SLEN] == 64 && motion.count == 5;
    ok &= step_cond(&TSEL->step[42]) == COND_FILL && event_at(42, motion.event[0].param, motion.event[0].value);
    bad += check("DOUBLE: clipped 40 -> 64 copies conditions and fine FX edges only where they fit", ok);
    ok = tl_do(TSEL, TL_HALF, 0) && TSEL->p[P_SLEN] == 32 && motion.count == 3 && motion_valid(&motion);
    bad += check("HALF: trims copied FX, retaining source edge phases and valid schema", ok);
    printf("DUP x2: %s\n", bad ? "FAILED" : "all checks passed");
    return bad != 0;
}
