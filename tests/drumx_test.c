/* SPDX-License-Identifier: GPL-3.0-only */
/* Real UI, persistence, MIDI and audio paths, including fixed A/B lane targets. */
#include <assert.h>
#define DRUMX_TEST_ONLY 1
#include "ui_test.c"
#undef main
int main(void)
{
    uint8_t pk[128], oldpk[128];
    project_t p, q;
    project_store_t wire;
    ui_power_on(); song.sel = 0;
    set_engine(ENGI_DRUM); apply_preset(4);
    assert(dx_track(TSEL));
    uint32_t decay_b = DX_SOUND_FIRST + 16u * DV_SNARE + 5u + DXP_DECAY;
    uint32_t pitch_a = DX_SOUND_FIRST + 16u * DV_KICK + DXP_PITCH;
    dx_set(0, decay_b, 117); dx_set(0, pitch_a, 12);
    dx_set(0, DX_FIRST + DX_FB, 7);
    dx_pack(0, pk); memcpy(oldpk, pk, 128);
    for (uint32_t i = 0; i < 128u; i++) assert(pk[i] < 128u);
    dx_reset(0); assert(dx_unpack(0, pk));
    assert(dx_get(0, decay_b) == 117 && dx_get(0, DX_FIRST + DX_FB) == 7);
    pk[0] = 0; assert(!dx_unpack(0, pk)); memcpy(pk, oldpk, 128);
    project_capture(&p); assert(proj_pack(&wire, &p));
    assert(*(uint32_t *)wire.raw == PROJ_MAGIC_DX);
    assert(proj_import(&q, wire.raw, sizeof wire));
    dx_reset(0); project_restore_runtime(&q);
    assert(dx_track(TSEL) && dx_get(0, decay_b) == 117);
    assert(up_store(31, "DX TEST") == 3);
    dx_set(0, decay_b, 3); assert(up_load(31) == 0 && dx_get(0, decay_b) == 117);
    ml_clear_all(); assert(ml_learn(20, 0, decay_b) == 1);
    dx_ui_lane = DV_KICK; dx_ui_side = 0; song.sel = 2;
    assert(midi_learned(20, 127));
    assert(dx_get(0, decay_b) == 127 && dx_get(0, pitch_a) == 12);
    dx_set(0, decay_b, 19);
    trk[0].p[P_E0] = DK_STD;
    assert(midi_learned(20, 100) && dx_get(0, decay_b) == 19);
    trk[0].p[P_E0] = DK_DX;
    persist_t saved;
    memset(&saved, 0, sizeof saved); settings_export(&saved);
    memset(ml_extra, 0, sizeof ml_extra);
    assert(settings_import(&saved, sizeof saved));
    assert(ml_cc_of(0, decay_b) == 21);
    for (uint32_t i = 0, cc = 2; i < ML_N; cc++) {
        if (!ml_free_cc(cc) || cc == 20u) continue;
        if (i == ML_N - 1u) break;
        uint32_t id = DX_SOUND_FIRST + i;
        if (id >= decay_b) id++;
        assert(ml_learn(cc, 0, id) == 1); i++;
    }
    assert(ml_count() == ML_N);
    ml_clear_all();
    assert(ml_learn(20, 0, P_E3) == 1);
    memset(&saved, 0, sizeof saved); settings_export(&saved);
    saved.magic = 0x50455234u;
    memset(ml_extra, 0xFF, sizeof ml_extra); memset(ml_virtual, 0xFF, sizeof ml_virtual);
    assert(settings_import(&saved, PERSIST_BASE_SIZE) == 2);
    assert(ml_count() == 1 && ml_cc_of(0, P_E3) == 21);
    for (uint32_t k=0;k<NTRK;k++) { set_engine_of(&trk[k], ENGI_DRUM); apply_preset_to(&trk[k],4); dx_set(k,pitch_a,10+k); }
    project_capture(&p); assert(proj_pack(&wire,&p)); assert(proj_import(&q,wire.raw,sizeof wire));
    for(uint32_t k=0;k<NTRK;k++)dx_reset(k);
    project_restore_runtime(&q);
    for(uint32_t k=0;k<NTRK;k++)assert(dx_get(k,pitch_a)==(int32_t)(10+k));
    uint32_t sig=autosave_sig(); dx_set(0,decay_b,19); assert(sig!=autosave_sig());
    song.sel = 0;
    assert(ev_id_ok(&trk[0], P_MORPH));
    motion_clear(&trk[0]); trk[0].p[P_MORPH] = 17;
    assert(motion_set_event(&trk[0],0,P_MORPH,25)==0);
    assert(motion_set_lock(&trk[0],1,P_MORPH,80)==0);
    motion_set_enabled(&trk[0],1); motion_begin();
    motion_step(&trk[0],0,&motion,1); assert(trk[0].p[P_MORPH]==25);
    motion_step(&trk[0],1,&motion,1); assert(trk[0].p[P_MORPH]==80);
    motion_step(&trk[0],2,&motion,1); assert(trk[0].p[P_MORPH]==25);
    motion_end(); assert(trk[0].p[P_MORPH]==17);
    motion_clear(&trk[0]); song.rec=1; seq_start(); seq_tick(&trk[0],CTL);
    trk[0].p[P_MORPH]=31; motion_capture(&trk[0],P_MORPH,31);
    assert(motion_count(&trk[0])==1 && motion.event[0].value==31);
    seq_stop(); assert(trk[0].p[P_MORPH]==17);
    motion_clear(&trk[0]);
    assert(motion_set_event(&trk[0],0,P_MORPH,127)==0);
    assert(motion_set_event(&trk[0],1,P_E6,100)==0);
    project_capture(&p); assert(proj_pack(&wire,&p)); assert(proj_import(&q,wire.raw,sizeof wire));
    memset(&motion,0,sizeof motion); project_restore_runtime(&q);
    assert(motion.count==2 && trk[0].p[P_E0]==DK_DX);
    /* A KIT automation event is transient: save the base kit's payload, keep autosave stable. */
    motion_clear(&trk[0]); trk[0].p[P_E0] = DK_DX;
    assert(motion_set_event(&trk[0],0,P_E0,DK_STD)==0);
    motion_begin(); sig=autosave_sig(); motion_step(&trk[0],0,&motion,1);
    assert(trk[0].p[P_E0]==DK_STD && sig==autosave_sig());
    project_capture(&p); assert(proj_pack(&wire,&p) && proj_import(&q,wire.raw,sizeof wire));
    dx_config_t c; assert(q.t[0].p[P_E0]==DK_DX && dx_decode(&c,q.fm6[0]) && dx_get_config(&c,pitch_a)==10);
    motion_end();
    motion_clear(&trk[0]); trk[0].p[P_E0] = DK_STD;
    assert(motion_set_event(&trk[0],0,P_E0,DK_DX)==0);
    motion_begin(); sig=autosave_sig(); motion_step(&trk[0],0,&motion,1);
    assert(trk[0].p[P_E0]==DK_DX && sig==autosave_sig());
    fm6_pack(fm6_patch[0],oldpk); project_capture(&p);
    assert(p.t[0].p[P_E0]==DK_STD && !memcmp(oldpk,p.fm6[0],128));
    assert(proj_pack(&wire,&p) && proj_import(&q,wire.raw,sizeof wire));
    motion_end();
    ui_power_on();
    assert(dx_get(0,decay_b)==DX_KIT_DEF[DV_SNARE].b[DXP_DECAY]);
    song.sel=0; set_engine(ENGI_DRUM); apply_preset(4);
    song.sel = 0;
    song.master_q12 = 4096; events_block(CTL);
    int32_t out[2 * CTL]; int64_t energy = 0;
    for (uint32_t l = 0; l < 8u; l++) {
        energy = 0;
        trk_note_on(TSEL, DRUM_LANE_NOTE[l], 100);
        for (uint32_t b = 0; b < 80u; b++) {
            mix_block(out, CTL);
            for (uint32_t i = 0; i < 2u * CTL; i++) { assert(out[i] >= -32768 && out[i] <= 32767); energy += (int64_t)out[i] * out[i]; }
        }
        assert(energy > 0);
    }
    for (uint32_t i = DX_FIRST; i < DX_END; i++) if (dx_target(i)) {
        const param_desc_t *d = dx_desc(i);
        dx_set(0, i, d->max); assert(dx_get(0, i) == d->max);
        dx_set(0, i, d->min); assert(dx_get(0, i) == d->min);
    }
    puts("DRUM-X: kit, A/B payload, project/preset, fixed MIDI targets, persistence, 64 learns and audio passed");
    return 0;
}
