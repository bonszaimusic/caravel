/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent modulation, real UI/Learn and unchanged-size sound storage. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void l2slot(track_t *t, uint32_t source, uint32_t dest, int amount)
{
    t->p[P_M1SRC] = source; t->p[P_M1DST] = dest; t->p[P_M1AMT] = amount;
}
static void lfo_page_order(void)
{
    static const char *const titles[] = {"LFO", "LFO SYNC", "LFO2", "LFO2 SYNC", "LFO DEST", "MOD"};
    uint32_t n = sizeof titles / sizeof titles[0];
    ui_power_on();
    press(B_LFO);
    for (uint32_t i = 0; i < n; i++) {
        assert(str_eq(cur_page()->title, titles[i]));
        press(B_LFO);
    }
    assert(str_eq(cur_page()->title, titles[0]));
    for (uint32_t i = 0; i < n; i++) {
        assert(str_eq(cur_page()->title, titles[i]));
        page_turn(1);
    }
    assert(str_eq(cur_page()->title, titles[0]));
    for (uint32_t i = n; i > 0; i--) {
        page_turn(-1);
        assert(str_eq(cur_page()->title, titles[i - 1u]));
    }
    for (uint32_t i = 0; i < n; i++) {
        go_title(titles[i]);
        page_turn(1);
        const page_t *remembered = cur_page();
        press(B_ENV);
        press(B_LFO);
        assert(cur_page() == remembered);
    }
}

static void mod_selection_order(void)
{
    static const uint8_t sources[] = {MS_OFF, MS_LFO, MS_SH, MS_SLEW, MS_LFO2, MS_SH2, MS_SLEW2,
                                     MS_ENV, MS_VEL, MS_KEY, MS_RAND, MS_MODW, MS_AT, MS_EXPR};
    static const uint8_t destinations[] = {MD_OFF, MD_PITCH, MD_CUT, MD_SHP, MD_AMP, MD_PAN,
        MD_MORPH, MD_PD_FB, MD_RATE, MD_VIB, MD_DEPTH, MD_E1, MD_E1+1, MD_E1+2, MD_E1+3,
        MD_E1+4, MD_E1+5, MD_E1+6, MD_E1+7, MD_VEL, MD_TIME, MD_DIST, MD_CHO, MD_DLY, MD_REV};
    assert(sizeof sources == MS_N && sizeof destinations == MD_N);
    ui_power_on();
    go_title("MOD");
    for (uint32_t slot = 0; slot < NMSLOT; slot++) {
        mod_ui_slot = slot;
        for (uint32_t col = 1; col <= 2; col++) {
            const uint8_t *order = col == 1 ? sources : destinations;
            uint32_t n = col == 1 ? MS_N : MD_N, seen = 0;
            uint32_t id = P_M1SRC + 3u * slot + col - 1u;
            TSEL->p[id] = order[0];
            for (uint32_t i = 0; i < n; i++) {
                assert(TSEL->p[id] == order[i]);
                assert(!(seen & (1u << order[i])));
                seen |= 1u << order[i];
                assert(enum_rank(&TP[id], order[i]) == (int32_t)i);
                edit_param(col, 1);
            }
            assert(TSEL->p[id] == order[n - 1u]);
            for (uint32_t i = n; i > 0; i--) {
                assert(TSEL->p[id] == order[i - 1u]);
                edit_param(col, -1);
            }
            assert(TSEL->p[id] == order[0]);
        }
    }
}

static void mod_source_equivalence(void)
{
    static const uint32_t randoms[] = {0, 1, 0x80000000u, 0xFFFF0000u, 0xFFFFFFFFu};
    static const uint32_t phases[] = {0, 1, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    static const int32_t values[] = {-32768, -1, 0, 1, 32767};
    static const int32_t fades[] = {0, 1, 16384, 32767};
    ui_power_on();
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        int32_t legacy[MS_LFO2];
        t->m_env = 12345; t->m_vel = 73; t->m_key = 48; t->m_rnd = -23456;
        t->mw = 91; t->at = 45; t->ex_off = 16;
        t->lfo_rnd = 0xFEDC1234u; t->lfo_prev = -12345; t->lfo_ph = 0x87654321u;
        for (uint32_t s = MS_LFO; s < MS_LFO2; s++) legacy[s] = mod_tsrc(t, s, -16384);
        for (uint32_t r = 0; r < NELEM(randoms); r++)
            for (uint32_t p = 0; p < NELEM(phases); p++)
                for (uint32_t v = 0; v < NELEM(values); v++)
                    for (uint32_t prev = 0; prev < NELEM(values); prev++)
                        for (uint32_t f = 0; f < NELEM(fades); f++) {
                            lfo2_state_t *l2 = &lfo2_state[k];
                            l2->rnd = randoms[r]; l2->ph = phases[p]; l2->val = values[v];
                            l2->prev = values[prev]; l2->fade = fades[f];
                            uint8_t before[sizeof *l2]; memcpy(before, l2, sizeof before);
                            /* Reference equations from Custom 22, including the unseeded RNG case. */
                            int32_t sh = l2->rnd ? (int32_t)(l2->rnd >> 16) - 32768 : 0;
                            int32_t slew = l2->prev + (((sh - l2->prev) * (int32_t)(l2->ph >> 17)) >> 15);
                            assert(mod_tsrc(t, MS_LFO2, -16384) == mulq15(l2->val, l2->fade));
                            assert(mod_tsrc(t, MS_SH2, -16384) == sh);
                            assert(mod_tsrc(t, MS_SLEW2, -16384) == slew);
                            for (uint32_t s = MS_LFO; s < MS_LFO2; s++)
                                assert(mod_tsrc(t, s, -16384) == legacy[s]);
                            assert(!memcmp(before, l2, sizeof before));
                        }
    }
}

int main(void)
{
    lfo_page_order();
    mod_selection_order();
    mod_source_equivalence();
    ui_power_on();
    assert(P_COUNT == 114 && MS_LFO2 == 11 && MS_SH2 == 12 && MS_SLEW2 == 13);
    track_t *t = &trk[0];
    lfo2_set(0, L2_FIRST + L2_RATE, 40);
    lfo2_set(1, L2_FIRST + L2_RATE, 100);
    t->p[P_LRATE] = 70;
    uint32_t ph = t->lfo_ph;
    for (int i = 0; i < 100; i++) { track_lfo2_tick(t); track_lfo2_tick(&trk[1]); }
    assert(t->lfo_ph == ph);
    assert(lfo2_state[0].ph == (uint32_t)(100u * LFO_INC[40]));
    assert(lfo2_state[1].ph == (uint32_t)(100u * LFO_INC[100]));
    lfo2_set(0, L2_FIRST + L2_SYNC, 5); song.g[G_BPM] = 120;
    assert(lfo2_sync_inc(5) == lfo_sync_inc(&(track_t){.p={[P_LSYNC]=5}}));
    lfo2_set(0, L2_FIRST + L2_PHASE, 37);
    lfo2_trigger(t, 0); assert(lfo2_state[0].ph == (37u << 25));
    lfo2_set(0, L2_FIRST + L2_TRIG, 1);
    lfo2_state[0].ph = 123; lfo2_trigger(t, 0); assert(lfo2_state[0].ph == 123);
    lfo2_trigger(t, 1); assert(lfo2_state[0].ph == (37u << 25));
    lfo2_state[0].val = 0; lfo2_state[0].fade = 32767;
    l2slot(t, MS_LFO2, MD_AMP, 63);
    lfo2_set(0, L2_FIRST + L2_POL, 0); mod_begin(t); int32_t bi = MOD(t).gain; mod_end(t);
    lfo2_set(0, L2_FIRST + L2_POL, 1); mod_begin(t); int32_t uni = MOD(t).gain; mod_end(t);
    assert(uni < 1000 && bi > 16000);
    lfo2_state[0].rnd = 0xFFFF0000u; lfo2_state[0].prev = -32768; lfo2_state[0].ph = 0x80000000u;
    assert(mod_tsrc(t, MS_SH2, 0) == 32767);
    assert(mod_tsrc(t, MS_SLEW2, 0) == -1);
    l2slot(t, MS_LFO2, MD_CUT, 20);
    lfo2_state[0].val = 16384; mod_begin(t); assert(MOD(t).cut > 0); mod_end(t);
    /* UI target is independent of the track subsequently displayed. */
    go_title("LFO2"); frame();
    uint32_t rate = lfo2_get(0,L2_FIRST); turn(EN_K1, 1);
    assert(lfo2_get(0,L2_FIRST) > rate);
    ml_clear_all(); assert(ml_learn(20,0,L2_FIRST+L2_RATE)==1);
    song.sel=2; assert(midi_learned(20,127));
    assert(lfo2_get(0,L2_FIRST)==127 && lfo2_get(2,L2_FIRST)==0);
    persist_t settings; memset(&settings,0,sizeof settings); settings_export(&settings);
    memset(ml_virtual,0,sizeof ml_virtual); memset(ml_extra,0,sizeof ml_extra);
    assert(settings_import(&settings,sizeof settings)); assert(ml_cc_of(0,L2_FIRST)==21);
    /* Both patch payloads and all four LFO settings survive the fixed-size project. */
    song.sel=0; set_engine_of(&trk[1],ENGI_FM6); set_engine_of(&trk[2],ENGI_DRUM); apply_preset_to(&trk[2],4);
    set_engine_of(&trk[3],ENGI_SEM);
    for(uint32_t k=0;k<NTRK;k++) { lfo2_set(k,L2_FIRST,17+k); lfo2_set(k,L2_FIRST+L2_WAVE,k); }
    project_t p,q; project_store_t wire;
    project_capture(&p); assert(proj_pack(&wire,&p)); assert(*(uint32_t*)wire.raw==PROJ_MAGIC_L2);
    assert(sizeof wire==3840 && proj_import(&q,wire.raw,sizeof wire));
    assert(!memcmp(p.fm6,q.fm6,sizeof p.fm6) && !memcmp(p.lfo2,q.lfo2,sizeof p.lfo2));
    project_store_t broken=wire;
    broken.raw[PROJ_FM6_OFF+120] |= 128u;
    uint32_t sum=proj_hash(broken.raw,PROJ_STORE_SIZE-4u); memcpy(broken.raw+PROJ_STORE_SIZE-4u,&sum,4);
    assert(!proj_import(&q,broken.raw,sizeof broken));
    broken=wire; broken.raw[PROJ_FM6_OFF+8u+3u] |= 128u; /* WAVE 8 is invalid. */
    sum=proj_hash(broken.raw,PROJ_STORE_SIZE-4u); memcpy(broken.raw+PROJ_STORE_SIZE-4u,&sum,4);
    assert(!proj_import(&q,broken.raw,sizeof broken));
    assert(proj_import(&q,wire.raw,sizeof wire));
    for(uint32_t k=0;k<NTRK;k++) lfo2_load(k,0);
    assert(project_restore_runtime(&q)==0);
    for(uint32_t k=0;k<NTRK;k++) assert(lfo2_get(k,L2_FIRST)==(int32_t)(17+k));
    /* User presets: normal, FM6 and DX, with payload identity retained. */
    for(uint32_t k=0;k<3;k++) {
        song.sel=k; uint8_t pk[128],saved[128]; int payload=k==1||k==2;
        if(k==1) fm6_pack(fm6_patch[k],pk); else if(k==2) dx_pack(k,pk);
        assert(up_store(k,"LFO2 TEST")==3); assert(up_rec(k)->ver>=8 && up_valid(up_rec(k)));
        if(payload) assert(upf_get(k,saved)==0 && !memcmp(pk,saved,128));
        lfo2_set(k,L2_FIRST,100); assert(up_load(k)==0 && lfo2_get(k,L2_FIRST)==(int32_t)(17+k));
    }
    song.sel=0; undo.keep=0; lfo2_set(0,L2_FIRST,51); apply_preset(0);
    assert(lfo2_get(0,L2_FIRST)==0); undo_swap(); assert(lfo2_get(0,L2_FIRST)==51);
    uint32_t sig=autosave_sig(); lfo2_set(0,L2_FIRST,52); assert(sig!=autosave_sig());
    /* Legacy imports clear LFO2. Ordinary saves keep their original format. */
    for(uint32_t k=0;k<NTRK;k++) { set_engine_of(&trk[k],0); lfo2_load(k,0); }
    project_capture(&p); assert(proj_pack(&wire,&p)); assert(*(uint32_t*)wire.raw==PROJ_MAGIC);
    assert(proj_import(&q,wire.raw,sizeof wire)); for(uint32_t k=0;k<NTRK;k++) assert(!lfo2_nonzero(&q.lfo2[k]));
    /* Tagged preset transfer: a melodic frame and invalid data are handled atomically. */
    uint8_t args[400]; uint32_t n=0,slot=99; up_rec_t rec;
    args[n++]=20; args[n++]=0; args[n++]='L'; args[n++]=0;
    for(uint32_t i=0;i<P_COUNT;i++) { uint32_t v=8192+p.t[0].p[i]; args[n++]=v&127; args[n++]=v>>7; }
    memset(args+n,0,50); n+=50; args[n++]=0x32;
    for(uint32_t i=0;i<L2_N;i++) args[n++]=i==L2_RATE?42:0;
    assert(up_parse(args,n,&rec,&slot)==0 && rec.ver==8 && rec.packed[UP_L2_AT]==42);
    up_rec_t before=rec; args[n-1]=2;
    assert(up_parse(args,n,&rec,&slot)==1 && !memcmp(&rec,&before,sizeof rec));
    puts("independent LFO2 tests passed"); return 0;
}
