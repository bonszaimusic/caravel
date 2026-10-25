/* SPDX-License-Identifier: GPL-3.0-only */
/* Included after the legacy voice implementation. All calls below are in one control boundary. */
#include "pair_motion.c"
static const uint8_t PAIR_SHARED[] = {P_LEVEL, P_PAN, P_MUTE, P_LATCH, P_AMODE, P_ARATE, P_AHOLD,
    P_AOCT, P_AGATE, P_ASWING, P_APROB, P_AORDER, P_CHRD, P_VOIC, P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SGATE, P_SSWING, P_VOICE, P_PRIO, P_GLIDE, P_GLMODE};
static int pair_engine_ok(uint32_t e)
{
    /* Standard pitched-note lifecycle, including own-envelope and sampled engines.
     * Kit lanes and custom key maps require an explicit shared-note policy. */
    return eng_ok(e) && !ENGINES[e]->oneshot && !ENGINES[e]->keys &&
           (!ENGINES[e]->ownenv || ENGINES[e]->done);
}
static const char *pair_track_reason(const track_t *t)
{
    if (!pair_engine_ok(t->engine)) return "ENGINE NOTE MAP";
    if ((uint32_t)t->p[P_VOICE] > V_UNISON) return "VOICE MODE";
    if (t->p[P_SPRD]) return "SPREAD OFF";
    if (t->xf_on || t->eng_req != t->engine) return "ENGINE CHANGING";
    return 0;
}
static int pair_supported(const track_t *t) { return !pair_track_reason(t); }
static const char *pair_last_reason = "PAIR OFF";
static void pair_key_remove(uint32_t note)
{
    uint32_t n=0;
    for(uint32_t i=0;i<ep.key_count;i++) if(ep.keys[i]!=note) ep.keys[n++]=ep.keys[i];
    ep.key_count=n;
}
static uint32_t pair_key_pick(void)
{
    uint32_t note=ep.keys[ep.key_count-1u], prio=ep.mode==V_POLY?0:trk[ep.a].p[P_PRIO];
    for(uint32_t i=0;i<ep.key_count;i++) {
        if(prio==1 && ep.keys[i]<note) note=ep.keys[i];
        if(prio==2 && ep.keys[i]>note) note=ep.keys[i];
    }
    return note;
}
/* A CELL engine represents the complete gesture in a single DSP host voice. */
static uint32_t pair_cells_mask(void)
{
    return ((trk[ep.a].engine==ENGI_DRONE && drone_cells(&trk[ep.a])) ? 1u : 0u) |
           ((trk[ep.b].engine==ENGI_DRONE && drone_cells(&trk[ep.b])) ? 2u : 0u);
}
static void pair_budget(void)
{
    ep.slots=ep.live_slots=4;
    ep.cells=ep.mode==V_POLY ? pair_cells_mask() : 0;
    uint32_t ca=(ep.cells&1) ? 4u : trk_nvoice(&trk[ep.a]);
    uint32_t cb=(ep.cells&2) ? 4u : trk_nvoice(&trk[ep.b]);
    if(ca<ep.slots) ep.slots=ca;
    if(cb<ep.slots) ep.slots=cb;
    if(ep.mode==V_MONO || ep.mode==V_LEGATO) ep.slots=1;
    ep.live_slots=ep.slots;
    ep.budget=ep.mode==V_POLY ? ep.slots : 1;
}
/* Preserve the legacy protection of each other part's bass/mono voice. */
static voice_t *pair_other_victim(void)
{
    voice_t *best=0;uint32_t category=4;
    for(uint32_t k=0;k<NPART;k++) {
        if(k==ep.a || k==ep.b) continue;
        track_t *t=&trk[k];uint32_t mode=trk_vmode(t), low=mode==V_POLY?lowest_held(t):NVOICE;
        for(uint32_t i=0;i<NVOICE;i++) {
            voice_t *v=&t->v[i];if(!v->active || v->stage==4) continue;
            uint32_t c=!v->gate?1:(mode!=V_POLY && i>0)?2:(mode==V_POLY && i!=low)?3:4;
            if(c<category || (c==category && c<4 && (!best || v->age<best->age))) { best=v;category=c; }
        }
    }
    return best;
}
static int pair_room(uint32_t need)
{
    while(voices_busy()+need>NVOICE) {
        voice_t *v=pair_other_victim();if(!v) return 0;voice_kill(v);
    }
    return 1;
}
static const char *pair_enable_reason(uint32_t a, uint32_t b)
{
    if (ep.on) return "PAIR ALREADY ON";
    if (a >= NTRK || b >= NTRK || a == b) return "CHOOSE PARTNER";
    if (voice_policy & VOICE_POLICY_FIXED) return "USE AUTO VOICES";
    const char *r = pair_track_reason(&trk[a]);
    if (!r) r = pair_track_reason(&trk[b]);
    if (r) return r;
    if (song.playing) return "STOP TRANSPORT";
    for (uint32_t k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        if (t->nheld || t->nmono || t->xp_n) return "RELEASE NOTES";
        for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active) return "WAIT FOR TAILS";
    }
    return 0;
}
static void pair_disable(void)
{
    uint32_t irq=perf_irq_save();
    if (!ep.on) {perf_irq_restore(irq);return;}
    pair_cleanup_pending |= (1u << ep.a) | (1u << ep.b);
    trk_all_off_raw(&trk[ep.a]);
    trk_all_off_raw(&trk[ep.b]);
    ep.dispatch=1;
    for(uint32_t i=0;i<NVOICE;i++) {
        if(trk[ep.a].v[i].active) voice_kill(&trk[ep.a].v[i]);
        if(trk[ep.b].v[i].active) voice_kill(&trk[ep.b].v[i]);
    }
    ep.dispatch=0;ep.fade_block=2;
    for(uint32_t m=0;m<2;m++) {
        int16_t last=ep.weight[m][CTL-1];
        for(uint32_t i=0;i<CTL;i++)ep.weight[m][i]=last;
    }
    ep.endpoint=ep.weight[0][CTL-1]==32767 && ep.weight[1][CTL-1]==0 ? 1 :
                ep.weight[0][CTL-1]==0 && ep.weight[1][CTL-1]==32767 ? 2 : 0;
    ep.on = 0;
    ep.key_count=0;
    pair_last_reason = "PAIR OFF";
    memset(ep.used, 0, sizeof ep.used);
    perf_irq_restore(irq);
}
static int pair_enable(uint32_t a, uint32_t b)
{
    /* No hidden migration of occupied sequence/latch/voice state. UI must stop/clear first. */
    uint32_t irq=perf_irq_save();
    const char *reason = pair_enable_reason(a, b);
    if (reason) { pair_last_reason = reason;perf_irq_restore(irq);return 0; }
    pair_last_reason = "PAIR OFF";
    memset(&ep, 0, sizeof ep);
    ep.a = a; ep.b = b; ep.on = 1;
    ep.mode=trk[a].p[P_VOICE];
    trk[b].p[P_VOICE]=ep.mode;
    pair_budget();
    ep.base = ep.effective = 16384;
#if FELUCCA_HOME_PERF
 hp.pair_base=ep.base;hp.pair_motion=0;hp.pair_valid=1;
#endif
    ep.level = trk[a].p[P_LEVEL]; ep.pan = trk[a].p[P_PAN]; ep.mute = trk[a].p[P_MUTE];
    for (uint32_t i = 0; i < NSTEP; i++) ep.lane[i] = -1;
    for (uint32_t i = 0; i < NELEM(PAIR_SHARED); i++) {
        ep.shared[i] = trk[a].p[PAIR_SHARED[i]];
        trk[b].p[PAIR_SHARED[i]] = ep.shared[i];
    }
    int ok=ep.budget != 0;
    if(!ok)ep.on=0;
    perf_irq_restore(irq);
    return ok;
}
static void pair_validate(void)
{
    if (ep.on) {
        const char *reason = voice_policy & VOICE_POLICY_FIXED ? "USE AUTO VOICES" : pair_track_reason(&trk[ep.a]);
        if (!reason) reason = pair_track_reason(&trk[ep.b]);
        if (reason) { pair_disable(); pair_last_reason = reason; }
    }
    if (!ep.on) return;
    /* Editing either member updates the common performance values. A wins simultaneous writes. */
    for (uint32_t i = 0; i < NELEM(PAIR_SHARED); i++) {
        uint32_t id = PAIR_SHARED[i];
        int16_t v = trk[ep.a].p[id] != ep.shared[i] ? trk[ep.a].p[id] : trk[ep.b].p[id];
        ep.shared[i] = trk[ep.a].p[id] = trk[ep.b].p[id] = v;
    }
    if(ep.mode!=trk[ep.a].p[P_VOICE] ||
       ep.cells!=(trk[ep.a].p[P_VOICE]==V_POLY ? pair_cells_mask() : 0)) {
        /* A live mode edit releases the old gesture before changing the slot layout. */
        trk_all_off_raw(&trk[ep.a]);trk_all_off_raw(&trk[ep.b]);ep.key_count=0;
        ep.dispatch=1;
        for(uint32_t i=0;i<NVOICE;i++) {voice_kill(&trk[ep.a].v[i]);voice_kill(&trk[ep.b].v[i]);}
        ep.dispatch=0;memset(ep.used,0,sizeof ep.used);
        ep.mode=trk[ep.a].p[P_VOICE];pair_budget();
    }
    ep.level = trk[ep.a].p[P_LEVEL]; ep.pan = trk[ep.a].p[P_PAN]; ep.mute = trk[ep.a].p[P_MUTE];
}
static void pair_stolen(voice_t *v)
{
    if (!ep.on || ep.dispatch) return;
    if(ep.cells) {
        for(uint32_t m=0;m<2;m++) {
            track_t *t=&trk[m?ep.b:ep.a];
            if(ep.cells & (1u<<m)) {
                if(v!=&t->v[0]) continue;
                /* Losing the sole network retires the entire physical gesture. */
                ep.key_count=0;
                for(uint32_t k=0;k<2;k++) {
                    track_t *member=&trk[k?ep.b:ep.a];trk_all_off_raw(member);
                    for(uint32_t i=0;i<NVOICE;i++) {
                        member->v[i].gate=0;
                        member->v[i].stage=member->v[i].active?4:0;
                    }
                }
                memset(ep.used,0,sizeof ep.used);return;
            }
            for(uint32_t i=0;i<ep.slots;i++) if(v==&t->v[i]) {
                /* DSP slot ownership is separate from the network's held keys. */
                ep.used[i]=0;return;
            }
        }
        return;
    }
    /* A legacy allocator / overload fade taking either half retires its partner in the same call. */
    for (uint32_t s = 0; s < ep.slots; s++)
        if (v == &trk[ep.a].v[s] || v == &trk[ep.b].v[s]) {
            if(ep.mode!=V_POLY) {
                ep.key_count=0;
                for(uint32_t m=0;m<2;m++) {
                    track_t *t=&trk[m?ep.b:ep.a];
                    trk_all_off_raw(t);
                    for(uint32_t i=0;i<ep.slots;i++) {t->v[i].gate=0;t->v[i].stage=t->v[i].active?4:0;}
                }
                memset(ep.used,0,sizeof ep.used);return;
            }
            pair_key_remove(ep.note[s]);
            voice_t *other = v == &trk[ep.a].v[s] ? &trk[ep.b].v[s] : &trk[ep.a].v[s];
            other->gate = 0; other->stage = other->active ? 4 : 0;
            ep.used[s] = 0;
            return;
        }
}
static void pair_cells_on(uint32_t note, uint32_t vel)
{
    uint32_t s=ep.budget, oldest=0;
    for(uint32_t i=0;i<ep.budget;i++) {
        if(ep.used[i] && ep.note[i]==note) {s=i;break;}
        int free=!ep.used[i];
        for(uint32_t m=0;m<2;m++) if(!(ep.cells&(1u<<m))) {
            voice_t *v=&trk[m?ep.b:ep.a].v[i];
            if(!v->active || v->stage==4) free=1;
        }
        if(s==ep.budget && free) s=i;
        if(ep.age[i]<ep.age[oldest]) oldest=i;
    }
    if(s==ep.budget) s=oldest;
    uint32_t need=0;
    for(uint32_t m=0;m<2;m++) {
        voice_t *v=&trk[m?ep.b:ep.a].v[(ep.cells&(1u<<m))?0:s];
        need+=!v->active || v->stage==4;
    }
    if(!pair_room(need)) {pair_key_remove(note);return;}
    ep.used[s]=1;ep.note[s]=note;ep.age[s]=++ep.clock;
    for(uint32_t m=0;m<2;m++) {
        track_t *member=&trk[m?ep.b:ep.a];
        ep_alloc_track=member;ep_alloc=&member->v[(ep.cells&(1u<<m))?0:s];
        trk_note_on_raw(member,note,vel);
    }
    ep_alloc=0;ep_alloc_track=0;
}
static void trk_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    if (!pair_member(t)) { trk_note_on_raw(t, note, vel); return; }
    pair_validate();
    if (!ep.on || note > 127 || !vel || ep.mute) return;
    pair_key_remove(note);ep.keys[ep.key_count++]=note;
    if(ep.cells) {pair_cells_on(note,vel);return;}
    if(ep.mode!=V_POLY) {
        if(ep.mode==V_UNISON) {
            uint32_t protected=0;
            for(uint32_t k=0;k<NPART;k++) if(k!=ep.a && k!=ep.b) {
                track_t *other=&trk[k];
                for(uint32_t i=0;i<NVOICE;i++) if(other->v[i].active && other->v[i].gate && other->v[i].stage!=4) {
                    if((trk_vmode(other)==V_POLY && i==lowest_held(other)) || (trk_vmode(other)!=V_POLY && i==0)) protected++;
                }
            }
            ep.live_slots=ep.slots;
            if(ep.live_slots>(NVOICE-protected)/2) ep.live_slots=(NVOICE-protected)/2;
        }
        uint32_t need=0;
        for(uint32_t m=0;m<2;m++) for(uint32_t i=0;i<ep.live_slots;i++) {
            voice_t *v=&trk[m?ep.b:ep.a].v[i];need+=!v->active || v->stage==4;
        }
        if(!pair_room(need)) {pair_key_remove(note);return;}
        uint32_t wanted=pair_key_pick();
        for(uint32_t m=0;m<2;m++) {
            track_t *member=&trk[m?ep.b:ep.a];
            /* Feed the existing MONO/LEG/UNISON engine path the shared priority winner.
             * Its private 8-key fallback stack is replaced by the pair's full note order. */
            member->nmono=wanted!=note;member->mono_stack[0]=wanted;
            trk_note_on_raw(member,note,vel);
        }
        return;
    }
    uint32_t s = ep.budget, oldest = 0;
    for (uint32_t i = 0; i < ep.budget; i++) {
        if (ep.used[i] && ep.note[i] == note) { s = i; break; }
        if (s == ep.budget && !trk[ep.a].v[i].active && !trk[ep.b].v[i].active) s = i;
        if (ep.age[i] < ep.age[oldest]) oldest = i;
    }
    if (s == ep.budget) s = oldest;
    /* Retire both old slots before reserving two new slots. Never allocate one engine and fail the other. */
    ep.dispatch=1;voice_kill(&trk[ep.a].v[s]); voice_kill(&trk[ep.b].v[s]);ep.dispatch=0;
    if(!pair_room(2)) {pair_key_remove(note);return;}
    ep.used[s] = 1; ep.note[s] = note; ep.age[s] = ++ep.clock;
    for (uint32_t i = 0; i < 2; i++) {
        track_t *member = &trk[i ? ep.b : ep.a];
        int16_t mute = member->p[P_MUTE]; member->p[P_MUTE] = 0;
        ep_alloc_track = member; ep_alloc = &member->v[s];
        trk_note_on_raw(member, note, vel);
        member->p[P_MUTE] = mute;
    }
    ep_alloc = 0; ep_alloc_track = 0;
}
static void trk_note_off(track_t *t, uint32_t note)
{
    if (!pair_member(t)) { trk_note_off_raw(t, note); return; }
    pair_validate();
    if(!pair_member(t)) {trk_note_off_raw(t,note);return;}
    pair_key_remove(note);
    if(ep.cells) {
        for(uint32_t i=0;i<ep.slots;i++) if(ep.used[i] && ep.note[i]==note) ep.used[i]=0;
        trk_note_off_raw(&trk[ep.a],note);trk_note_off_raw(&trk[ep.b],note);return;
    }
    if(ep.mode!=V_POLY) {
        uint32_t next=ep.key_count?pair_key_pick():0;
        for(uint32_t m=0;m<2;m++) {
            track_t *member=&trk[m?ep.b:ep.a];
            member->nmono=ep.key_count!=0;member->mono_stack[0]=next;
            trk_note_off_raw(member,note);
            if(ep.key_count && !member->v[0].gate) mono_play(member,next,member->v[0].vel,0,member->p[P_GLIDE]!=0);
        }
        return;
    }
    trk_note_off_raw(&trk[ep.a], note);
    trk_note_off_raw(&trk[ep.b], note);

}
static void trk_all_off(track_t *t)
{
    if (!pair_member(t)) { trk_all_off_raw(t); return; }
    trk_all_off_raw(&trk[ep.a]); trk_all_off_raw(&trk[ep.b]);
    ep.key_count=0;
    memset(ep.used, 0, sizeof ep.used);
}
/* Exact endpoint identities: still render both engines and run their envelopes.
 * Only the subsequent output scaling is skipped. No new sample approximation. */
static inline __attribute__((always_inline)) void pair_apply_gain(track_t *t, int32_t *b, uint32_t n)
{
    if(ep.mute || (ep.endpoint && ep.endpoint!=(t==&trk[ep.b]?2:1))) {
        for(uint32_t i=0;i<n;i++) b[i]=0;
    } else if(!ep.endpoint) {
        const int16_t *w=ep.weight[t==&trk[ep.b]];
        for(uint32_t i=0;i<n;i++) b[i]=(int32_t)((int64_t)b[i]*w[i]/32767);
    }
}
static void pair_mix_begin(uint32_t n)
{
    if (!ep.on) { if(ep.fade_block) ep.fade_block--; return; }
    int32_t target = ep.base;
    uint32_t step = trk[ep.a].seq_idx % NSTEP;
    if (song.playing && ep.lane[step] >= 0) target = ep.lane[step];
    /* Like mod_begin: previous block LFO values. Every pair destination slot is read once here. */
    for (uint32_t m = 0; m < 2; m++) {
        track_t *t = &trk[m ? ep.b : ep.a];
        int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
        int32_t depth = mod_depth(t, lfo);
        if (depth < 32767) lfo = mulq15(lfo, depth);
        for (uint32_t k = 0; k < NMSLOT; k++) {
            uint32_t src = t->p[P_M1SRC + k * 3];
            if (t->p[P_M1DST + k * 3] == MD_PAIR && (src == MS_LFO || src == MS_LFO2))
                target += mod_tsrc(t, src, lfo) * t->p[P_M1AMT + k * 3] / 64;
        }
    }
    target = clamp(target, 0, 32767);
    /* The first slew step reaches this endpoint and every following sample
     * stays there. The optional curve also maps both endpoints identically. */
    ep.endpoint=(target==0 && ep.effective<=75) ? 1 :
                (target==32767 && ep.effective>=32767-75) ? 2 : 0;
    for (uint32_t i = 0; i < n; i++) {
        /* Bounded 10 ms linear slew, independent of render block size; linear unity-sum crossfade. */
        int32_t d = target - ep.effective;
        ep.effective += clamp(d, -75, 75);
        ep.weight[0][i] = 32767 - ep.effective;
        ep.weight[1][i] = ep.effective;
        if (ep.motion || ep.motion_mix) pair_motion_weights(i);
    }
}
