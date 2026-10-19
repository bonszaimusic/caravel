/* SPDX-License-Identifier: GPL-3.0-only */
/* Native DRONE contract tests. No golden files and no device CPU claims.
 * Fresh means a power-on pool owner, not merely a cleared voice_t. */
#ifdef C1_HOST
#include "c1_host.h"
#include <sys/wait.h>
#include <unistd.h>
#endif
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>

/* Legacy gestures may update appended ownership bookkeeping, never the DSP prefix. */
_Static_assert(offsetof(drone_part_t, held) == 104, "legacy DSP prefix");
#define DR_LEGACY_BYTES offsetof(drone_part_t, held)
static volatile int64_t dr_sink;
static void dr_setup(void)
{
    ui_power_on();
    host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    for (uint32_t p = 0; p < NPART; p++) {
        host_preset(&trk[p], ENGI_DRONE, p % ENG_DRONE.npresets);
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
    }
}
static vmod_t dr_mod(uint32_t note)
{
    return (vmod_t){.inc = pitch_inc(note * 16), .pitch16 = note * 16,
        .amp0 = 32767, .amp1 = 32767, .shape = 64 * 256};
}
static void dr_cold(track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    eng_mem_owner[p] = 0;
    /* Claim via the host path before any note_on or pool access. */
    host_preset(t, ENGI_DRONE, 0);
    memset(t->v, 0, sizeof t->v);
}
static void dr_block(track_t *t, int32_t *out, const vmod_t *m)
{
    memset(out, 0, CTL * sizeof *out);
    ENG_DRONE.render(t, &t->v[0], out, CTL, m);
}
static int dr_controls(void)
{
    int bad = 0; dr_setup(); track_t *t = &trk[0];
    bad += check("DRONE appended at ID16, NENGINES17, one voice and claimed pool",
        ENGI_DRONE == 16 && NENGINES == 17 && ENGINES[16] == &ENG_DRONE &&
        ENG_DRONE.poly == 1 && trk_nvoice(t) == 1 && eng_ok(16) &&
        ENG_DRONE.mem == sizeof(drone_part_t) && ENG_DRONE.mem <= ENG_MEM_PART &&
        eng_mem_owner[0] == 17);
    int ok = 1;
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t id = P_E0 + k;
        ok &= ml_learn(20 + k, 0, id) == 1;
        midi_learned(20 + k, 127); ok &= t->p[id] == (id == P_E3 ? 2 : track_desc(t, id)->max);
        midi_learned(20 + k, 0); ok &= t->p[id] == track_desc(t, id)->min;
        midi_learned(20 + k, 64);
        ok &= t->p[id] >= track_desc(t, id)->min && t->p[id] <= (id == P_E3 ? 2 : track_desc(t, id)->max);
        ok &= motion_param(id) && ev_id_ok(t, id);
    }
    bad += check("all eight controls use actual MIDI Learn CC dispatch (including TOPO)", ok);
    ok = 1;
    for (uint32_t k = 0; k < ENG_DRONE.npresets; k++) {
        host_preset(t, ENGI_DRONE, k);
        motion_clear(t); motion_set_lock(t, 3, P_E2, 117); motion_set_enabled(t, 1);
        project_t q, restored; project_store_t wire;
        project_capture(&q);
        int packed = proj_pack(&wire, &q) && proj_unpack(&restored, wire.raw, sizeof wire);
        ok &= packed;
        if (!packed) continue;
        host_preset(t, 0, 0); project_restore_runtime(&restored);
        ok &= t->eng_req == ENGI_DRONE && t->preset == k;
        for (uint32_t j = 0; j < 8; j++) ok &= t->p[P_E0+j] == DRONE_PRESETS[k].e[j];
        motion_begin(); motion_step(t, 3, &motion, 1);
        ok &= t->p[P_E2] == 117;
        motion_end(); ok &= t->p[P_E2] == DRONE_PRESETS[k].e[2];
    }
    bad += check("every factory project roundtrip retains all eight values and executes lock", ok);
    ok = 1;
    for (uint32_t k = 0; k < 8; k++) {
        host_preset(t, ENGI_DRONE, 0);
        t->p[P_E0+k] = track_desc(t, P_E0+k)->max;
        int16_t expected[8]; memcpy(expected, &t->p[P_E0], sizeof expected);
        ok &= up_store(2, "DRONE USR") == 3;
        host_preset(t, 0, 0); up_load(2);
        ok &= t->eng_req == ENGI_DRONE && !memcmp(expected, &t->p[P_E0], sizeof expected);
    }
    bad += check("user preset roundtrips every drone control and engine ID16", ok);
    ok = 1;
    for (uint32_t k = 0; k < 8; k++) {
        host_preset(t, ENGI_DRONE, 0); uint32_t id = P_E0+k;
        int16_t base = t->p[id], target = track_desc(t,id)->max;
        motion_clear(t); motion_set_lock(t, 0, id, target); motion_set_enabled(t, 1);
        motion_begin(); motion_step(t, 0, &motion, 1);
        ok &= t->p[id] == target; motion_end(); ok &= t->p[id] == base;
    }
    bad += check("all eight locks apply and motion_end restores their base values", ok);
    dr_setup();
    /* Sentinel all legacy storage, including Learn and preference bytes. */
    favorites.factory[15][10] = 1; favorites.factory[15][11] = 3;
    uint8_t prefs[sizeof favorites]; memcpy(prefs, &favorites, sizeof prefs);
    ok = favorite_set(ENGI_HADES, 0, 1);
    for (uint32_t k = 0; k < ENG_DRONE.npresets; k++)
        ok &= favorite_set(ENGI_DRONE, k, 1) && favorite_has(ENGI_DRONE, k);
    ok &= favorite_set(ENGI_DRONE, 1, 0) && !favorite_has(ENGI_DRONE, 1);
    ok &= favorite_has(ENGI_HADES, 0) && !memcmp(prefs, &favorites, sizeof prefs);
    persist_t saved = {0}; settings_export(&saved);
    memset(engine_favorites, 0, sizeof engine_favorites);
    ok &= settings_import(&saved, sizeof saved) == 1;
    for (uint32_t k = 0; k < ENG_DRONE.npresets; k++) ok &= !!favorite_has(ENGI_DRONE,k) == (k != 1);
    ok &= favorite_has(ENGI_HADES,0) && !memcmp(prefs, &favorites, sizeof prefs);
    bad += check("engine16 favorites persist without modifying legacy preferences/Learn", ok);
    return bad;
}
static int dr_corners(void)
{
    static const uint8_t notes[] = {0,24,48,84,108,127};
    int ok = 1; int64_t peak = 0; uint32_t cases = 0; dr_setup(); track_t *t = &trk[0];
    for (uint32_t topo = 0; topo < 3; topo++)
    for (uint32_t ni = 0; ni < NELEM(notes); ni++)
    for (uint32_t bits = 0; bits < 128; bits++) {
        dr_cold(t);
        for (uint32_t k = 0, bit = 0; k < 8; k++) {
            if (k == 3) t->p[P_E0+k] = topo;
            else t->p[P_E0+k] = (bits & (1u << bit++)) ? 127 : 0;
        }
        /* Canaries outside advertised memory, inside the allocated pool. */
        memset((uint8_t *)ENG_MEM(0) + ENG_DRONE.mem, 0xA7, ENG_MEM_PART - ENG_DRONE.mem);
        ENG_DRONE.note_on(t, &t->v[0]); vmod_t m = dr_mod(notes[ni]);
        for (uint32_t b = 0; b < 80; b++) {
            int32_t out[CTL];
            m.cutoff = b & 1 ? 127*256 : -127*256;
            m.shape = b & 1 ? 127*256 : 0;
            dr_block(t,out,&m);
            for (uint32_t i = 0; i < CTL; i++) {
                int64_t a = llabs((int64_t)out[i]); if (a > peak) peak = a;
                ok &= a < 200000;
            }
            drone_part_t *s = drone_state(t);
            for (uint32_t k = 0; k < 4; k++)
                ok &= llabs((int64_t)s->prev[k]) <= 32768 && s->inc[k] >= 0 && s->inc[k] <= 0x60000000;
        }
        for (uint32_t k = ENG_DRONE.mem; k < ENG_MEM_PART; k++) ok &= ((uint8_t *)ENG_MEM(0))[k] == 0xA7;
        cases++;
    }
    printf("DRONE corners: %u cases x %u samples; peak %lld, pool bytes %u\n",
        cases,80u*CTL,(long long)peak,ENG_DRONE.mem);
    return check("3 topologies x 6 notes x 128 endpoints, modulation, bounds/canaries",ok && peak > 100);
}
static int dr_state_checks(void)
{
    enum { BLOCKS = 100 };
    static int32_t ref[BLOCKS*CTL];
    dr_setup(); track_t *a = &trk[0], *b = &trk[1]; vmod_t m = dr_mod(48), other = dr_mod(84);
    ENG_DRONE.note_on(a,&a->v[0]);
    for (uint32_t i=0;i<BLOCKS;i++) dr_block(a,ref+i*CTL,&m);
    drone_part_t expected = *drone_state(a);
    dr_cold(a); ENG_DRONE.note_on(a,&a->v[0]);
    int same=1, nonzero=0;
    for (uint32_t i=0;i<BLOCKS;i++) {
        int32_t out[CTL]; dr_block(a,out,&m);
        same &= !memcmp(out,ref+i*CTL,sizeof out);
        for (uint32_t j=0;j<CTL;j++) nonzero |= out[j]!=0;
    }
    int bad=check("fresh claimed renders deterministic and nonzero",same && nonzero);
    dr_cold(a); ENG_DRONE.note_on(a,&a->v[0]); ENG_DRONE.note_on(b,&b->v[0]);
    same=1;
    for (uint32_t i=0;i<BLOCKS;i++) {
        int32_t out[CTL], extra[CTL];
        b->p[P_E3]=i%3; b->p[P_E2]=i%128;
        dr_block(b,extra,&other); dr_block(a,out,&m);
        same &= !memcmp(out,ref+i*CTL,sizeof out);
    }
    bad += check("rendering/editing another part leaves first part bit-identical",same &&
        !memcmp(&expected,drone_state(a),sizeof expected));
    /* A callback, even AMP=0, must preserve the original DSP and next audio. */
    drone_part_t warm=*drone_state(a);
    voice_start(a,&a->v[0],48,100,0);
    same=!memcmp(&warm,drone_state(a),DR_LEGACY_BYTES);
    a->v[0].env_out=0;
    voice_start(a,&a->v[0],48,100,0);
    same &= !memcmp(&warm,drone_state(a),DR_LEGACY_BYTES);
    int32_t before[CTL], after[CTL]; dr_block(a,before,&m);
    *drone_state(a)=warm; ENG_DRONE.note_on(a,&a->v[0]); dr_block(a,after,&m);
    bad += check("warm/AMP-zero retrigger preserves legacy104 DSP and next output exactly",same &&
        !memcmp(before,after,sizeof before));
    vmod_t silent=m; silent.amp0=silent.amp1=0; same=1;
    for (uint32_t j=0;j<200;j++) {
        dr_block(a,after,&silent);
        for (uint32_t i=0;i<CTL;i++) same &= after[i]==0;
    }
    bad += check("amp0=amp1=0 produces exact silence with a warm oscillating network",same);
    warm=*drone_state(a); after[0]=12345;
    ENG_DRONE.render(a,&a->v[0],after,0,&m);
    bad += check("zero-length render leaves state and output untouched",after[0]==12345 &&
        !memcmp(&warm,drone_state(a),sizeof warm));
    /* Constant amplitude makes sample-wise comparison independent of amp_at's block ramp. */
    dr_block(a,before,&m); *drone_state(a)=warm; memset(after,0,sizeof after);
    for (uint32_t i=0;i<CTL;i++) ENG_DRONE.render(a,&a->v[0],after+i,1,&m);
    bad += check("constant controls render identically in block and single-sample chunks",
        !memcmp(before,after,sizeof before));
    return bad;
}
static double dr_bin(const int32_t *s,uint32_t n,double hz)
{
    double re=0,im=0;
    for (uint32_t i=0;i<n;i++) {
        double ph=6.283185307179586*hz*i/FS;
        re+=s[i]*cos(ph); im+=s[i]*sin(ph);
    }
    return hypot(re,im)/n;
}
static int dr_pitch(void)
{
    static int32_t sig[FS]; static const uint8_t notes[]={24,48,69,84,108};
    int ok=1; dr_setup(); track_t *t=&trk[0];
    for (uint32_t k=0;k<NELEM(notes);k++) {
        dr_cold(t);
        for (uint32_t j=0;j<7;j++) t->p[P_E0+j]=0;
        t->p[P_E7]=127; vmod_t m=dr_mod(notes[k]); ENG_DRONE.note_on(t,&t->v[0]);
        for (uint32_t b=0;b<FS/CTL;b++) { int32_t out[CTL]; dr_block(t,out,&m); }
        for (uint32_t i=0;i<FS;) {
            uint32_t n=FS-i<CTL?FS-i:CTL;
            memset(sig+i,0,n*sizeof *sig); ENG_DRONE.render(t,&t->v[0],sig+i,n,&m); i+=n;
        }
        double hz=(double)m.inc*FS/4294967296.0;
        double main=dr_bin(sig,FS,hz), half=dr_bin(sig,FS,hz/2), lo=dr_bin(sig,FS,hz*.985),hi=dr_bin(sig,FS,hz*1.015);
        double nominal=440*pow(2,((int)notes[k]-69)/12.0);
        printf("DRONE pitch note %u: %.4f Hz (nominal %.4f), bin %.1f; half %.1f, -/+1.5%% %.1f/%.1f\n",
            notes[k],hz,nominal,main,half,lo,hi);
        ok &= fabs(1200*log2(hz/nominal))<2 && main>100 && half<main*.12 && lo<main*.8 && hi<main*.8;
    }
    return check("uncoupled unison, detune/drift0: nonzero fundamental at host tuning",ok);
}
static uint32_t dr_active(track_t *t)
{
    uint32_t n=0; for(uint32_t i=0;i<NVOICE;i++) n+=!!t->v[i].active; return n;
}
static int dr_lifecycle(void)
{
    int bad=0;
    for(uint32_t mode=V_POLY;mode<=V_UNISON;mode++) {
        dr_setup(); track_t *t=&trk[0];
        t->p[P_VOICE]=mode; t->p[P_ATK]=t->p[P_DEC]=t->p[P_REL]=0; t->p[P_SUS]=127;
        int ok=1, sounded=0; int32_t out[2*CTL];
        for(uint32_t k=0;k<40;k++) {
            uint32_t note=36+k%24;
            trk_note_on(t,note,100);
            for(uint32_t b=0;b<8;b++) {
                mix_block(out,CTL); ok &= dr_active(t)<=1;
                for(uint32_t i=0;i<2*CTL;i++) sounded |= out[i]!=0;
            }
            trk_note_off(t,note);
        }
        for(uint32_t b=0;b<2u*FS/CTL;b++) mix_block(out,CTL);
        ok &= !dr_active(t) && !t->nmono;
        for(uint32_t i=0;i<2*CTL;i++) ok &= out[i]==0;
        char label[100]; snprintf(label,sizeof label,"voice mode %u: real note on/off, single network, release, no hanging notes",mode);
        bad+=check(label,ok && sounded);
        trk_note_on(t,60,100);
        for(uint32_t b=0;b<100;b++) mix_block(out,CTL);
        /* Exercise the real host allocator with overlapping notes, not just
         * the note_on callback: steals and MONO/LEGATO fallback stay warm. */
        drone_part_t warm=*drone_state(t);
        trk_note_on(t,67,110);
        ok=!memcmp(&warm,drone_state(t),DR_LEGACY_BYTES) && dr_active(t)==1;
        for(uint32_t b=0;b<100;b++) mix_block(out,CTL);
        warm=*drone_state(t); t->v[0].env_out=0;
        trk_note_on(t,67,100);
        ok &= !memcmp(&warm,drone_state(t),DR_LEGACY_BYTES) && dr_active(t)==1;
        trk_note_off(t,67); trk_note_off(t,60);
        for(uint32_t b=0;b<2u*FS/CTL;b++) mix_block(out,CTL);
        ok &= !dr_active(t) && !t->nmono;
        snprintf(label,sizeof label,"voice mode %u: overlapping/repeated notes and AMP-zero retrigger stay warm",mode);
        bad+=check(label,ok);
        trk_note_on(t,64,100);
        for(uint32_t b=0;b<100;b++) mix_block(out,CTL);
        panic_req |= 1u;
        for(uint32_t b=0;b<2u*FS/CTL;b++) mix_block(out,CTL);
        ok=!dr_active(t) && !t->nmono && !t->nheld && !t->xp_n;
        for(uint32_t i=0;i<2*CTL;i++) ok &= out[i]==0;
        snprintf(label,sizeof label,"voice mode %u: host panic clears gates/held notes and reaches exact silence",mode);
        bad+=check(label,ok);
    }
    return bad;
}
static int dr_live(void)
{
    dr_setup(); track_t *t=&trk[0]; vmod_t m=dr_mod(48); ENG_DRONE.note_on(t,&t->v[0]);
    int ok=1; int64_t peak=0,step=0; int32_t last=0;
    for(uint32_t b=0;b<4u*FS/CTL;b++) {
        if(b%17==0) {
            for(uint32_t j=0;j<8;j++) t->p[P_E0+j]=(b/17+j)&1?127:0;
            t->p[P_E3]=(b/17)%3;
            m=dr_mod((b/17)&1?84:24);
            m.cutoff=b&1?127*256:-127*256; m.shape=b&1?127*256:0;
        }
        int32_t out[CTL]; dr_block(t,out,&m);
        for(uint32_t i=0;i<CTL;i++) {
            int64_t a=llabs((int64_t)out[i]),d=llabs((int64_t)out[i]-last);
            if(a>peak) peak=a; if(d>step) step=d; last=out[i]; ok &= a<200000;
        }
        drone_part_t *s=drone_state(t);
        ok &= s->ctrl[DR_TOPO]>=0 && s->ctrl[DR_TOPO]<=65536;
    }
    printf("DRONE live jumps: peak %lld; largest sample step %lld (diagnostic, not a click/listening verdict)\n",(long long)peak,(long long)step);
    return check("held-network runtime topology/all-control/pitch/modulation changes stay bounded",ok && peak>100);
}
static void dr_bench(void)
{
    dr_setup(); vmod_t m[NPART]; const uint32_t blocks=4096;
    for(uint32_t p=0;p<NPART;p++) {
        for(uint32_t k=0;k<8;k++) trk[p].p[P_E0+k]=k==3?2:127;
        m[p]=dr_mod(36+12*p); ENG_DRONE.note_on(&trk[p],&trk[p].v[0]);
    }
    clock_t start=clock();
    for(uint32_t b=0;b<blocks;b++) for(uint32_t p=0;p<NPART;p++) {
        int32_t out[CTL]; dr_block(&trk[p],out,&m[p]); dr_sink+=out[b%CTL];
    }
    double sec=(double)(clock()-start)/CLOCKS_PER_SEC;
    printf("DRONE native benchmark: %u parts / one network each, %.1f ns/frame total, %.2fx audio duration.\n"
           "  Native process CPU time, direct DSP, no FX: NOT device CPU utilization or CPU2 validation.\n",
        NPART,sec*1e9/(blocks*CTL),sec/((double)blocks*CTL/FS));
}
static int dr_demos(void)
{
    int bad=0;
    for(uint32_t k=0;k<ENG_DRONE.npresets;k++) {
        dr_setup(); track_t *t=&trk[0]; host_preset(t,ENGI_DRONE,k);
        t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;
        char path[128]; snprintf(path,sizeof path,"build/drone_demo/%u.wav",k);
        FILE *f=fopen(path,"wb"); if(!f) { perror(path); bad++; continue; }
        const uint32_t frames=28u*FS; wav_hdr(f,frames); trk_note_on(t,48,100);
        for(uint32_t pos=0;pos<frames;) {
            uint32_t n=frames-pos<CTL?frames-pos:CTL;
            /* Hold 20 seconds, release eight. First 8 s preserve the preset;
             * then sweep coupling and change topology on the held network. */
            if(pos>=8u*FS && pos<20u*FS) {
                t->p[P_E2]=(pos-8u*FS)*127u/(12u*FS);
                t->p[P_E3]=(pos/(4u*FS))%3;
            }
            if(pos>=20u*FS && pos-n<20u*FS) trk_note_off(t,48);
            int32_t out[2*CTL]; mix_block(out,n);
            for(uint32_t i=0;i<n;i++) wav_put(f,out[2*i],out[2*i+1]);
            pos+=n;
        }
        int failed=ferror(f); failed |= fclose(f)!=0; bad+=!!failed;
        printf("DRONE demo %s: %s, 20 s hold + 8 s release, native stereo\n",path,DRONE_PRESETS[k].name);
    }
    return bad;
}

#ifdef C1_HOST
typedef struct {
    uint64_t hash;
    int32_t peak[NPART];
    uint32_t taken;
} dr_parity_t;

static dr_parity_t dr_parity_render(uint32_t scene, int dual, const char *mask)
{
    static const uint8_t mixed[NPART] = {ENGI_DRONE, ENGI_HADES, ENGI_SEM, ENGI_FM6};
    dr_parity_t r = {.hash = 0xCBF29CE484222325ull};
    dr_setup();
    c1h_off = !dual;
    setenv("C1_MASK", mask, 1);
    for (uint32_t p=0; p<NPART; p++) {
        track_t *t=&trk[p];
        uint32_t e=scene==2 ? mixed[p] : ENGI_DRONE;
        host_preset(t,e,(p+(scene==1 ? 4 : 0))%ENGINES[e]->npresets);
        t->p[P_VOICE]=V_POLY;
        t->p[P_ATK]=20; t->p[P_DEC]=20; t->p[P_SUS]=110; t->p[P_REL]=30;
        t->p[P_DIST]=0;
        t->p[P_CHOR]=12+3*p; t->p[P_DLY]=10+4*p; t->p[P_REV]=18+3*p;
        t->p[P_ITYPE]=p+1; t->p[P_IMIX]=50;
        t->p[P_LWAVE]=4; t->p[P_LRATE]=90+5*p;
        t->p[P_M1SRC]=MS_SH; t->p[P_M1DST]=MD_CUT; t->p[P_M1AMT]=12;
        t->p[P_M2SRC]=MS_LFO; t->p[P_M2DST]=MD_RATE; t->p[P_M2AMT]=10;
        trk_note_on(t,48+5*p,100);
    }
    for (uint32_t b=0; b<3u*FS/CTL; b++) {
        for (uint32_t p=0; p<NPART; p++) {
            track_t *t=&trk[p];
            if (t->engine==ENGI_DRONE && b%97==0) {
                t->p[P_E3]=(b/97+p)%3;
                t->p[P_E2]=(b/97*19+p*31)%128;
                t->p[P_E5]=(b/97*13+p*17)%128;
                t->p[P_E7]=(b/97*23+p*11)%128;
            }
            if (b==FS/CTL || b==FS/CTL+100) trk_note_on(t,60+5*p,110);
            if (b==2u*FS/CTL) {
                trk_note_off(t,48+5*p); trk_note_off(t,60+5*p);
            }
        }
        int32_t out[2*CTL]; mix_block(out,CTL);
        for (uint32_t p=0; p<NPART; p++) if(trk[p].peak>r.peak[p]) r.peak[p]=trk[p].peak;
        for (uint32_t i=0; i<2*CTL; i++) for (uint32_t k=0; k<4; k++) {
            r.hash ^= ((uint32_t)out[i]>>(k*8))&255u;
            r.hash *= 0x100000001B3ull;
        }
    }
    r.taken=c1h_taken;
    return r;
}
static int dr_parity_fork(uint32_t scene, int dual, const char *mask, dr_parity_t *r)
{
    int fd[2],status=0;
    if(pipe(fd)) return 0;
    fflush(NULL);                     /* sanitizer child shutdown must not replay parent output */
    pid_t pid=fork();
    if(pid<0) { close(fd[0]); close(fd[1]); return 0; }
    if(!pid) {
        close(fd[0]);
        dr_parity_t result=dr_parity_render(scene,dual,mask);
        int ok=write(fd[1],&result,sizeof result)==sizeof result;
        close(fd[1]); _exit(ok ? 0 : 2);
    }
    close(fd[1]);
    int ok=read(fd[0],r,sizeof *r)==sizeof *r;
    close(fd[0]);
    ok &= waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0;
    return ok;
}
static int dr_cpu2(void)
{
    static const char *const names[]={"4 DRONE presets 0..3", "4 DRONE presets 4,0,1,2", "DRONE HADES SEM FM6"};
    static const char *const masks[]={"rot","auto","1","2","3","4","5","6","7","8","9","10","11","12","13","14"};
    int bad=0;
    /* Run before any parent-side DSP or threads: each fork has boot-fresh FX,
     * RNG and part state, exactly as the existing dualcore_test harness does. */
    for(uint32_t scene=0;scene<NELEM(names);scene++) {
        dr_parity_t ref={0};
        int ok=dr_parity_fork(scene,0,"rot",&ref);
        for(uint32_t p=0;p<NPART;p++) ok &= ref.peak[p]>200;
        if(!ok) {
            printf("DRONE CPU2 %s: reference failed or silent (%d %d %d %d)\n",names[scene],ref.peak[0],ref.peak[1],ref.peak[2],ref.peak[3]);
            bad++; continue;
        }
        for(uint32_t m=0;m<NELEM(masks);m++) {
            dr_parity_t got={0};
            int match=dr_parity_fork(scene,1,masks[m],&got) && got.hash==ref.hash &&
                !memcmp(got.peak,ref.peak,sizeof ref.peak) && got.taken>0;
            if(!match) printf("DRONE CPU2 %s split %s: hash %016llx expected %016llx, thread jobs %u FAIL\n",
                names[scene],masks[m],(unsigned long long)got.hash,(unsigned long long)ref.hash,got.taken);
            ok &= match;
        }
        printf("DRONE CPU2 %s: %u splits, nonzero parts, output/peaks bit-identical: %s\n",names[scene],
            (unsigned)NELEM(masks),ok?"PASS":"FAIL");
        bad+=!ok;
    }
    return !!bad;
}
#endif

int main(int argc,char **argv)
{
#ifdef C1_HOST
    return dr_cpu2();
#else
    int bad=dr_controls()+dr_corners()+dr_state_checks()+dr_pitch()+dr_lifecycle()+dr_live();
    dr_bench();
    if(argc>1 && !strcmp(argv[1],"--demo")) bad+=dr_demos();
    printf("DRONE ENGINE: %s (%d failed checks)\n",bad?"FAIL":"PASS",bad);
    return !!bad;
#endif
}
