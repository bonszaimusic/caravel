/* SPDX-License-Identifier: GPL-3.0-only
 * End-to-end channel input and external-clock tests using the real queues,
 * UART parser, ownership model, sequencer, voice renderer and UI source. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void midi_test_reset(void)
{
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners); memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target); memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um); memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = 0; midi_in_overflow = 0; midi_beat_samples = 0;
    fm1_in.notes = kb_prev = 0; fm1_ms = 0; song.sel = 0;
    memset(song.mch, 0, sizeof song.mch); song.g[G_ROUTE] = 0; midi_seen = 0; memset(midi_lit_ms, 0, sizeof midi_lit_ms);
    events_block(CTL);
}
static void queued(uint32_t st, uint32_t d1, uint32_t d2, uint32_t source)
{
    midi_enqueue((st >= 0xF8u ? 0xFu : st >> 4) | st << 8 | d1 << 16 | d2 << 24, source);
    events_block(CTL);
}
static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static int controls_test(void)
{
    int bad = 0; midi_test_reset(); track_t *t = &trk[0];
    queued(0xE0, 127, 127, 1);
    bad += check("USB maximum bend is exactly the default +2 semitones", midi_bend_target[0] == 512);
    queued(0xE0, 0, 0, 1);
    bad += check("USB minimum bend is exactly the default -2 semitones", midi_bend_target[0] == -512);
    queued(0xE0, 0, 64, 1);
    bad += check("bend centre is zero", midi_bend_target[0] == 0);
    queued(0xB0, 101, 0, 1); queued(0xB0, 100, 0, 1); queued(0xB0, 6, 12, 1); queued(0xB0, 38, 50, 1);
    queued(0xE0, 127, 127, 1);
    bad += check("RPN0 supports semitone and cent bend sensitivity", midi_ch[0].semis == 12 && midi_ch[0].cents == 50 && midi_bend_target[0] == 3200);
    queued(0xB0, 99, 0, 1); queued(0xB0, 6, 24, 1);
    bad += check("selecting NRPN cancels RPN data entry", midi_ch[0].semis == 12);
    queued(0x90, 60, 100, 1); int32_t out[CTL]; int32_t before = midi_bend_q8[0];
    track_render(t, out, CTL);
    bad += check("bend is smoothed toward the target in the real voice renderer", midi_bend_q8[0] > before && midi_bend_q8[0] < midi_bend_target[0]);
    for (uint32_t i = 0; i < 64; i++) track_render(t, out, CTL);
    bad += check("bend smoothing reaches the exact target without changing saved pitch", midi_bend_q8[0] == 3200 && t->p[P_TRANS] == 0 && t->v[0].note == 60);
    queued(0xB0, 121, 0, 1);
    bad += check("Reset Controllers centres bend and preserves RPN sensitivity", !midi_bend_target[0] && midi_ch[0].semis == 12 && gate_note(t, 60));
    queued(0xB0, 1, 87, 1); queued(0xB0, 11, 23, 1); queued(0xD0, 90, 0, 1);
    bad += check("existing modwheel/expression/aftertouch stay routed to the track", t->mw == 87 && t->ex_off == 104 && t->at == 90);
    queued(0xE3, 127, 127, 1);
    bad += check("drum track ignores pitch bend", !midi_bend_target[3]);
    return bad;
}
static int sustain_test(void)
{
    int bad = 0; midi_test_reset(); track_t *t = &trk[0];
    queued(0x90, 60, 100, 1); queued(0xB0, 64, 127, 1); queued(0x80, 60, 0, 1);
    bad += check("pedal holds a released synth note and its owner", gate_note(t, 60) && midi_notes[0][60] == (1u | MIDI_PEDAL_NOTE));
    queued(0xB0, 64, 0, 1);
    bad += check("pedal release releases its held note", !gate_note(t, 60) && !midi_notes[0][60] && !midi_owners[0]);
    queued(0x90, 62, 100, 1); queued(0xB0, 64, 127, 1); queued(0xB0, 123, 0, 1);
    bad += check("All Notes Off honours sustain rather than hard-killing", gate_note(t, 62) && (midi_notes[0][62] & MIDI_PEDAL_NOTE));
    queued(0xB0, 121, 0, 1);
    bad += check("Reset Controllers releases pedal-held notes", !gate_note(t, 62) && !midi_notes[0][62]);
    queued(0x93, 36, 100, 1); queued(0xB3, 64, 127, 1); queued(0x83, 36, 0, 1);
    bad += check("drum note-offs do not accumulate pedal-held owners", !midi_notes[3][36] && !midi_owners[3]);
    queued(0x90, 67, 100, 1); queued(0xB0, 64, 127, 1); queued(0x80, 67, 0, 1); queued(0xB0, 120, 0, 1);
    bad += check("All Sound Off ignores pedal and discards all track ownership", !midi_notes[0][67] && !midi_owners[0] && !t->nheld && !gate_note(t, 67));
    return bad;
}
static int ownership_test(void)
{
    int bad = 0; midi_test_reset(); song.g[G_ROUTE] = 1; track_t *t = &trk[0];
    queued(0x94, 60, 100, 1); queued(0x95, 60, 100, 2);
    bad += check("USB/TRS channels can share one sounding note with independent owners", midi_owners[0] == 2u && gate_note(t, 60));
    queued(0x84, 60, 0, 1);
    bad += check("one channel's note-off preserves another channel's hold", midi_owners[0] == 1u && gate_note(t, 60));
    queued(0x85, 60, 0, 2);
    bad += check("last channel owner releases the shared note", !midi_owners[0] && !gate_note(t, 60));
    fm1_in.notes = 1u; events_block(CTL); uint32_t local = kb_note[0];
    queued(0x94, local, 100, 1); queued(0x84, local, 0, 1);
    bad += check("MIDI release cannot cut a still-held local keyboard note", gate_note(t, local));
    fm1_in.notes = 0; events_block(CTL);
    bad += check("local release after MIDI release ends the shared note", !gate_note(t, local));
    queued(0x94, 65, 100, 1); song.sel = 1; queued(0x84, 65, 0, 1);
    bad += check("note-off follows its note-on across selected-track changes", !gate_note(&trk[0], 65) && !midi_notes[4][65]);
    song.sel = 0; queued(0x94, 67, 100, 1); queued(0xB4, 64, 127, 1); queued(0x84, 67, 0, 1);
    panic_req |= 1u; events_block(CTL);
    queued(0x90, 67, 100, 1); queued(0xB4, 64, 0, 1);
    bad += check("preset panic forgets old pedal owners so later pedal-up preserves new notes", !midi_notes[4][67] && gate_note(&trk[0], 67));
    for (uint32_t i = 0; i <= MQ; i++) midi_enqueue(0x09u | 0x90u << 8 | 70u << 16 | 100u << 24, 1);
    events_block(CTL);
    bad += check("queue overflow recovers ownership and releases stuck notes", !midi_in_overflow && mi_r == mi_w && !midi_owners[0] && !gate_note(&trk[0], 67));
    return bad;
}
static void clock_setup(uint32_t mode)
{
    midi_test_reset(); song.g[G_CLOCK] = (int16_t)mode;
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2;
    for (uint32_t i = 0; i < 16; i++) trk[0].step[i] = (step_t){{(uint8_t)(60 + i)}, 1, ST_NOTE, 0, 100};
    events_block(CTL);
}
static void clock_packet(uint32_t source, uint32_t status, uint32_t ms)
{
    fm1_ms = ms;
    if (source == 2u) { um_byte(status); events_block(CTL); }
    else queued(status, 0, 0, source);
}
static void clock_to(uint32_t source, uint32_t start, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) clock_packet(source, 0xF8, start + (i * 125u) / 6u);
}
static int clock_test(uint32_t source)
{
    int bad = 0; clock_setup(source);
    clock_packet(source == 1u ? 2u : 1u, 0xFA, 1);
    bad += check(source == 1 ? "USB mode ignores TRS Start" : "TRS mode ignores USB Start", !song.playing);
    clock_packet(source, 0xFA, 2);
    bad += check(source == 1 ? "USB Start begins at step zero" : "TRS Start begins at step zero", song.playing && trk[0].seq_idx == 0u);
    clock_to(source, 2, 7);
    bad += check(source == 1 ? "USB six pulses advance exactly one 16th step" : "TRS six pulses advance exactly one 16th step", trk[0].seq_idx == 1u && song.g[G_BPM] == 120);
    fm1_ms = 132; events_block(CTL); uint16_t idx = trk[0].seq_idx; uint32_t pos = trk[0].seq_pos;
    clock_packet(source, 0xFC, 133);
    bad += check("external Stop preserves step position and releases sequence notes", !song.playing && trk[0].seq_idx == idx && trk[0].seq_pos == pos && !trk[0].seq_n);
    clock_packet(source, 0xFB, 150);
    bad += check("external Continue resumes the existing step position", song.playing && trk[0].seq_idx == idx && trk[0].seq_pos == pos);
    clock_packet(source, 0xF8, 151); fm1_ms = 672; events_block(CTL);
    bad += check("lost clock times out and releases transport notes", !song.playing && !trk[0].seq_n);
    clock_packet(source, 0xFA, 700); clock_to(source, 700, 7);
    bad += check("Start after timeout restarts cleanly at the master's phase", song.playing && trk[0].seq_idx == 1u);
    /* Original base values remain after MIDI Stop even while motion sounds. */
    trk[0].p[P_REV] = 23; motion_clear(&trk[0]); motion_set_event(&trk[0], 0, P_REV, 100);
    clock_packet(source, 0xFA, 1000); clock_packet(source, 0xF8, 1000);
    bad += check("external-clock playback applies recorded motion", trk[0].p[P_REV] == 100);
    clock_packet(source, 0xFC, 1010);
    bad += check("external Stop restores the musical parameter base", trk[0].p[P_REV] == 23);
    return bad;
}
static int clock_arp_and_boundaries(void)
{
    int bad = 0; clock_setup(1); trk[0].p[P_AMODE] = 1; trk[0].p[P_ARATE] = 2;
    queued(0x90, 60, 100, 1); queued(0x90, 64, 100, 1);
    clock_packet(1, 0xFA, 10); clock_to(1, 10, 1); uint32_t before = trk[0].arp_idx;
    clock_to(2, 20, 7);
    bad += check("wrong-source clocks cannot advance the ARP", trk[0].arp_idx == before);
    clock_to(1, 10, 7);
    bad += check("selected-source MIDI clock advances the ARP", trk[0].arp_idx > before);
    clock_setup(2); fm1_ms = 100; um_byte(0x90); um_byte(60); um_byte(0xFA); um_byte(100); events_block(CTL);
    bad += check("TRS realtime interleaving preserves the incomplete note message", gate_note(&trk[0], 60) && song.playing);
    /* Millisecond counter wraps naturally during external-clock operation. */
    clock_setup(1); clock_packet(1, 0xFA, 0xFFFFFFF0u); clock_packet(1, 0xF8, 0xFFFFFFF0u);
    clock_packet(1, 0xF8, 5u); fm1_ms = 10; events_block(CTL);
    bad += check("external clock timestamps survive uint32 millisecond wrap", song.playing && midi_clock.last_ms == 5u);
    clock_setup(1); transport_req = 1; events_block(CTL); fm1_ms = 501; events_block(CTL);
    bad += check("local PLAY waiting for missing external clock also times out", !song.playing);
    return bad;
}
/* ARP HOLD latched on an external clock: a Stop, a lost clock or a tap while stopped must not leave the arp note
 * sounding; Continue plays the latched chord again; the internal clock's free-running arp is unchanged by STOP */
static void arp_latch(uint32_t mode)
{
    midi_test_reset(); song.g[G_CLOCK] = (int16_t)mode; events_block(CTL);
    trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = 1; trk[0].p[P_ARATE] = 2; trk[0].p[P_AGATE] = 120; trk[0].p[P_AOCT] = 2;
    events_block(CTL);
}
static uint32_t arp_ext_run(uint32_t ms)       /* Start, tap 60 (latched), clocks until the arp plays 72 */
{
    uint32_t i;
    clock_packet(1, 0xFA, ms); queued(0x90, 60, 100, 1); queued(0x80, 60, 0, 1);
    for (i = 0; i < 400 && trk[0].arp_note != 72; i++) clock_packet(1, 0xF8, ms + i * 21u);
    return ms + i * 21u;
}
static void idle_ms(uint32_t ms)               /* about ms of audio blocks, no MIDI */
{
    uint32_t i, ms0 = fm1_ms;
    for (i = 0; i < ms * 23u / 16u; i++) { fm1_ms = ms0 + i * 16u / 23u; events_block(CTL); }
}
static int arp_ext_stop_test(void)
{
    int bad = 0; uint32_t ms, i, off;
    arp_latch(1); ms = arp_ext_run(10);
    bad += check("ARP HOLD on the external clock plays the upper octave", trk[0].arp_note == 72 && gate_note(&trk[0], 72));
    clock_packet(1, 0xFC, ms + 5); idle_ms(2000);
    bad += check("external Stop ends the sounding ARP note", !trk[0].arp_note && !gate_note(&trk[0], 72) && !gate_note(&trk[0], 60));
    bad += check("external Stop keeps the latched HOLD chord", trk[0].nheld == 1u && trk[0].held[0] == 60u);
    ms = fm1_ms; clock_packet(1, 0xFB, ms);
    for (i = 1; i < 40 && !trk[0].arp_note; i++) clock_packet(1, 0xF8, ms + i * 21u);
    bad += check("Continue plays the latched chord again", trk[0].arp_note != 0);
    arp_latch(1); arp_ext_run(10); idle_ms(2000);
    bad += check("a lost external clock stops the transport and ends the ARP note", !song.playing && !trk[0].arp_note && !gate_note(&trk[0], 72));
    arp_latch(1); queued(0x90, 64, 100, 1); queued(0x80, 64, 0, 1);
    bad += check("a tap while the external transport is stopped sounds", trk[0].arp_note == 64);
    idle_ms(2000);
    bad += check("its ARP gate still ends in real time", !trk[0].arp_note && !gate_note(&trk[0], 64));
    arp_latch(0); transport_req = 1; events_block(CTL); queued(0x90, 60, 100, 1); queued(0x80, 60, 0, 1);
    for (i = 0; i < 4000 && trk[0].arp_note != 72; i++) events_block(CTL);
    off = trk[0].arp_off;
    transport_req = 2; events_block(CTL);
    bad += check("internal clock: STOP leaves the free-running ARP note to its gate", trk[0].arp_note == 72 && gate_note(&trk[0], 72) && trk[0].arp_off == off - CTL);
    for (i = 0; i < 4000 && (!trk[0].arp_note || trk[0].arp_note == 72); i++) events_block(CTL);
    bad += check("internal clock: the ARP keeps stepping after STOP", trk[0].arp_note && trk[0].arp_note != 72 && trk[0].nheld == 1u);
    return bad;
}
/* USB-MIDI back-pressure: full 64-byte bulk packets (16 events) faster than the audio ISR drains the ring are
 * left in the endpoint (NAK) instead of overflowing it, which would panic every sounding note */
static int usb_burst_test(void)
{
    int bad = 0; uint8_t pkt[64]; uint32_t e, k, taken = 0;
    midi_test_reset();
    queued(0x90, 48, 100, 1);
    for (e = 0; e < 16; e++) {                       /* CC7 = 100 on 16 channels */
        pkt[4 * e] = 0x0B; pkt[4 * e + 1] = (uint8_t)(0xB0 | e); pkt[4 * e + 2] = 7; pkt[4 * e + 3] = 100;
    }
    for (k = 0; k < 5; k++)                          /* 5 polls at 2 kHz before the next half drains */
        taken += (uint32_t)ep1_take(pkt, sizeof pkt);
    bad += check("a USB burst fills the MIDI ring without overflowing it", taken == 3u && !midi_in_overflow && mi_w - mi_r == 48u);
    bad += check("the packet that does not fit is refused (left for the next poll)", !ep1_take(pkt, sizeof pkt) && mi_w - mi_r == 48u);
    for (e = 0; e < 8; e++)                          /* TRS MIDI meanwhile: its 8 slots are free */
        midi_in_event(0x0Bu | 0xB1u << 8 | 1u << 16 | e << 24);
    bad += check("TRS MIDI into a ring full of USB still fits (no overflow)", !midi_in_overflow && mi_w - mi_r == 56u);
    events_block(CTL);
    bad += check("the ring drains with no panic: the held note keeps sounding", gate_note(&trk[0], 48) && mi_r == mi_w);
    bad += check("the refused packet is taken once the ring has room", ep1_take(pkt, sizeof pkt) && mi_w - mi_r == 16u);
    events_block(CTL);
    bad += check("the held note survives all 88 events", gate_note(&trk[0], 48) && !midi_in_overflow);
    for (k = 0; k < 3; k++) ep1_take(pkt, sizeof pkt);
    midi_in_event(0x09u | 0x90u << 8 | 50u << 16 | 100u << 24);   /* one more event: 49 used, 15 free */
    bad += check("free space below one packet refuses a whole packet", !ep1_take(pkt, sizeof pkt) && !midi_in_overflow);
    events_block(CTL);
    return bad;
}
/* MENU > MIDI > MIDI IN (ROUT; up to 1.1.5 GLO > SYSTEM, #68): CH1-4 listens to channels 1..4 only, channels 5..16 are free for other instruments
 * (notes, bend, CCs, aftertouch, panic and reset all ignored, from USB and TRS); SEL plays the selected track
 * from every channel; a switch to CH1-4 lets go of what channels 5..16 held */
static int any_gate(void)
{
    for (uint32_t i = 0; i < NTRK * NVOICE; i++) if (trk[i / NVOICE].v[i % NVOICE].active && trk[i / NVOICE].v[i % NVOICE].gate) return 1;
    return 0;
}
static int route_test(void)
{
    int bad = 0; uint32_t ch, src, owned = 0;
    midi_test_reset(); song.sel = 2;
    bad += check("ROUT defaults to CH1-4", song.g[G_ROUTE] == 0);
    for (src = 1; src <= 2u; src++)
        for (ch = 4; ch < 16u; ch++) queued(0x90 | ch, 60 + ch, 100, src);
    for (ch = 4; ch < 16u; ch++) owned |= midi_notes[ch][60 + ch];
    bad += check("CH1-4: note-ons on channels 5..16 (USB and TRS) play nothing", !any_gate() && !owned && !midi_owners[2] && !midi_hint);
    queued(0x91, 62, 100, 1); queued(0x93, 40, 100, 2);
    bad += check("CH1-4: channels 2 and 4 still play parts 2 and 4", gate_note(&trk[1], 62) && midi_notes[1][62] == 2u && midi_notes[3][40] == 4u);
    queued(0xE4, 127, 127, 1); queued(0xEF, 0, 0, 2); queued(0xE9, 127, 127, 1);
    bad += check("CH1-4: pitch bend on channels 5, 10, 16 bends no part", !midi_bend_target[0] && !midi_bend_target[1] && !midi_bend_target[2] && !midi_ch[4].bend && !midi_ch[15].bend);
    queued(0xB4, 1, 99, 1); queued(0xB9, 11, 10, 2); queued(0xDF, 77, 0, 1);
    bad += check("CH1-4: CC1 / CC11 and aftertouch on channels 5..16 reach no MOD source", !trk[2].mw && !trk[2].ex_off && !trk[2].at && !trk[0].mw && !trk[0].at);
    queued(0xB4, 101, 0, 1); queued(0xB4, 100, 0, 1); queued(0xB4, 6, 24, 1);
    bad += check("CH1-4: RPN on channel 5 changes nothing", midi_ch[4].semis != 24u);
    queued(0xB4, 64, 127, 1); queued(0x81, 62, 0, 1);
    bad += check("CH1-4: a channel 5 sustain pedal does not hold channel 2's note", !gate_note(&trk[1], 62) && !midi_notes[1][62]);
    queued(0x91, 62, 100, 1); queued(0xB1, 1, 50, 1);
    queued(0xB4, 120, 0, 1); queued(0xB9, 123, 0, 2); queued(0xBF, 121, 0, 1);
    bad += check("CH1-4: CC120 / CC123 / CC121 on channels 5..16 leave parts 1..4 sounding", gate_note(&trk[1], 62) && gate_note(&trk[3], 40) && midi_owners[1] == 1u && trk[1].mw == 50);
    queued(0xB1, 123, 0, 1); queued(0x83, 40, 0, 2);
    bad += check("CH1-4: panic on channel 2 still works", !gate_note(&trk[1], 62) && !midi_owners[1] && !midi_owners[3]);

    midi_test_reset(); song.sel = 2; song.g[G_ROUTE] = 1; trk[2].p[P_VOICE] = V_POLY; trk[2].p[P_SUS] = 127; events_block(CTL);
    queued(0x90, 60, 100, 1); queued(0x94, 62, 100, 2); queued(0x99, 64, 100, 1); queued(0x9F, 65, 100, 2);
    bad += check("SEL: channels 1, 5, 10, 16 all play the selected track", gate_note(&trk[2], 60) && gate_note(&trk[2], 62) && gate_note(&trk[2], 64) && gate_note(&trk[2], 65) && !gate_note(&trk[0], 60) && midi_owners[2] == 4u);
    queued(0xEF, 127, 127, 1); queued(0xB9, 1, 66, 2); queued(0xD4, 33, 0, 1);
    bad += check("SEL: bend, CC1 and aftertouch from channels 5..16 reach the selected track", midi_bend_target[2] == 512 && trk[2].mw == 66 && trk[2].at == 33);
    queued(0xB4, 64, 127, 1); queued(0x84, 62, 0, 2);
    bad += check("SEL: channel 5 pedal holds its note", gate_note(&trk[2], 62) && (midi_notes[4][62] & MIDI_PEDAL_NOTE));
    song.g[G_ROUTE] = 0; events_block(CTL);
    bad += check("SEL -> CH1-4 releases channels 5..16's notes (pedal-held too), keeps channel 1's",
                 !gate_note(&trk[2], 62) && !gate_note(&trk[2], 64) && !gate_note(&trk[2], 65) && gate_note(&trk[2], 60) &&
                 !midi_notes[4][62] && !midi_notes[9][64] && !midi_notes[15][65] && midi_owners[2] == 1u && !midi_ch[4].pedal);
    bad += check(".. and resets their bend: each part follows its own channel 1..4", !midi_bend_target[2] && !midi_ch[15].bend);
    queued(0x80, 60, 0, 1);
    bad += check("channel 1's note-off still releases its note after the switch", !any_gate() && !midi_owners[2]);
    return bad;
}
/* 1.2 (Discussion #143): ROUT CH5-8, CH9-12, CH13-16: tracks 1..4 on those four channels (notes, bend, CCs, the CC map,
 * panic), every other channel ignored, as CH1-4 does with 1..4; a switch between blocks lets go of the old block's
 * notes; the clock (realtime) is not a channel's: it runs whatever ROUT says; the keys send on the block's channel */
static uint32_t out_last(void) { return mo_w != mo_r ? midi_out_q[(mo_w - 1u) % MQ] : 0u; }
static int route_block_test(void)
{
    static const char *const NAME[6] = {"CH1-4", "SEL", "CH5-8", "CH9-12", "CH13-16", "TRACK"};
    int bad = 0, ok = 1; uint32_t r, ch, i;
    ok = GP[G_ROUTE].max == 5 && GP[G_ROUTE].def == 0 && ROUTE_TRACK == 5;
    for (r = 0; r < 6u; r++) ok &= str_eq(GP[G_ROUTE].names[r], NAME[r]);
    bad += check("ROUT: CH1-4 SEL (stored 0 1, as before), CH5-8 CH9-12 CH13-16 appended, 1.5.5 TRACK", ok);
    for (r = 2; r <= 4u; r++) {
        uint32_t b = (r - 1u) * 4u;
        midi_test_reset(); song.sel = 2; song.g[G_ROUTE] = (int16_t)r;
        trk[3].eng_req = trk[3].engine = 0u;               /* (a synth on track 4: its notes held as gates) */
        events_block(CTL);
        ok = midi_base() == (int32_t)b;
        for (ch = 0; ch < 16u; ch++) {                     /* a note on every channel: only the block's play */
            uint32_t n = 40u + ch;
            queued(0x90 | ch, n, 100, 1 + (ch & 1u));
            for (i = 0; i < NTRK; i++)
                ok &= gate_note(&trk[i], n) == (ch >= b && ch < b + 4u && i == ch - b);
        }
        queued(0xE0 | (b + 1u), 127, 127, 1); queued(0xE0 | (b ? 0u : 4u), 127, 127, 1);
        ok &= midi_bend_target[1] == 512 && !midi_bend_target[0] && !midi_bend_target[2];
        queued(0xB0 | (b + 2u), 7, 5, 1); queued(0xB0 | (b + 2u) % 16u, 1, 44, 1);
        queued(0xB0 | (b + 4u) % 16u, 7, 9, 1);           /* (the next block's first channel: ignored) */
        ok &= trk[2].p[P_LEVEL] == 5 && trk[2].mw == 44 && trk[0].p[P_LEVEL] != 9 && trk[3].p[P_LEVEL] != 9;
        queued(0xB0 | (b + 4u) % 16u, 123, 0, 1); queued(0xB0 | (b + 4u) % 16u, 120, 0, 1);
        ok &= gate_note(&trk[0], 40u + b) && gate_note(&trk[3], 43u + b);
        queued(0xB0 | (b + 3u), 123, 0, 1);
        ok &= !gate_note(&trk[3], 43u + b) && gate_note(&trk[0], 40u + b);
        bad += check(r == 2u ? "ROUT CH5-8: channels 5..8 play tracks 1..4 (notes, bend, CC map, CC1, panic); the rest ignored" :
                     r == 3u ? "ROUT CH9-12: channels 9..12 play tracks 1..4; the rest ignored" :
                               "ROUT CH13-16: channels 13..16 play tracks 1..4; the rest ignored", ok);
    }
    /* a switch between blocks: the old block's notes let go (pedal-held too), its bend reset */
    midi_test_reset(); song.g[G_ROUTE] = 2; events_block(CTL);
    queued(0x94, 60, 100, 1); queued(0xB5, 64, 127, 1); queued(0x95, 62, 100, 1); queued(0x85, 62, 0, 1); queued(0xE4, 127, 127, 1);
    ok = gate_note(&trk[0], 60) && gate_note(&trk[1], 62) && midi_bend_target[0] == 512;
    song.g[G_ROUTE] = 3; events_block(CTL);
    ok &= !gate_note(&trk[0], 60) && !gate_note(&trk[1], 62) && !midi_owners[0] && !midi_owners[1] && !midi_ch[4].bend &&
          !midi_ch[5].pedal && !midi_bend_target[0];
    queued(0x98, 64, 100, 1);
    ok &= gate_note(&trk[0], 64);
    song.g[G_ROUTE] = 1; events_block(CTL);             /* -> SEL: nothing let go (every channel heard) */
    ok &= gate_note(&trk[0], 64);
    queued(0x88, 64, 0, 1);
    ok &= !any_gate();
    bad += check("ROUT CH5-8 -> CH9-12 lets go of channels 5..8 (pedal-held too, bend reset); -> SEL keeps notes", ok);
    /* the clock is not routed: an external clock runs with ROUT CH13-16 as with CH1-4 */
    clock_setup(1); song.g[G_ROUTE] = 4; events_block(CTL);
    clock_packet(1, 0xFA, 2); clock_to(1, 2, 7);
    ok = song.playing && trk[0].seq_idx == 1u;
    clock_packet(1, 0xFC, 133);
    ok &= !song.playing;
    song.g[G_CLOCK] = 0; events_block(CTL);
    bad += check("ROUT CH13-16: the USB clock (Start, pulses, Stop) runs the transport as ever", ok);
    /* MIDI out: the keys send on the selected track's channel of the block (SEL, CH1-4: 1..4) */
    usb.config = 1;
    ok = 1;
    for (r = 0; r <= 4u; r++) {
        uint32_t want = (r >= 2u ? (r - 1u) * 4u : 0u) + 2u;
        midi_test_reset(); song.g[G_ROUTE] = (int16_t)r; song.sel = 2; mo_r = mo_w; events_block(CTL);
        fm1_in.notes = 1u << 7; events_block(CTL);
        ok &= ((out_last() >> 8) & 0xFFu) == (0x90u | want);
        fm1_in.notes = 0; events_block(CTL);
        ok &= ((out_last() >> 8) & 0xFFu) == (0x80u | want);
    }
    usb.config = 0; mo_r = mo_w;
    song.g[G_ROUTE] = 0; song.sel = 0; events_block(CTL);
    bad += check("MIDI out: the keys of track 3 send on channel 3 (CH1-4, SEL), 7 (CH5-8), 11 (CH9-12), 15 (CH13-16)", ok);
    return bad;
}
/* 1.5.5 (#202, #143): MIDI IN TRACK, each track its own channel (song.mch: MENU > MIDI > T1..T4 CHANNEL): notes, bend, CC1,
 * aftertouch, the CC map, the panic CCs, Program Change and MIDI out all follow it; OMNI takes what no track has, OFF
 * nothing; two tracks on one channel: the first; a channel change lets go of what the old channel held; the defaults
 * (channels 1..4) are CH1-4 exactly */
static void tch(uint32_t k, uint32_t ch) { menu_put(MI_TCH1 + k, ch); events_block(CTL); }
static void pc(uint32_t ch, uint32_t prog) { queued(0xC0u | ch, prog, 0, 1); }
static int route_track_test(void)
{
    int bad = 0, ok; uint32_t ch, i, r;
    midi_test_reset();
    ok = mch_get(0) == 0u && mch_get(1) == 1u && mch_get(2) == 2u && mch_get(3) == 3u && !song.mch[0] && !song.mch[3];
    ok &= menu_n(MI_TCH1) == 18u && str_eq(menu_vname(MI_TCH1, 0), "1") && str_eq(menu_vname(MI_TCH1, 15), "16") &&
          str_eq(menu_vname(MI_TCH1, MCH_OMNI), "OMNI") && str_eq(menu_vname(MI_TCH1, MCH_OFF), "OFF") &&
          str_eq(MI_NAME[MI_TCH1], "T1 CHANNEL") && str_eq(MI_NAME[MI_TCH4], "T4 CHANNEL") && MI_TAB[MI_TCH3] == MTAB_MIDI &&
          menu_get(MI_TCH2) == 1u && MI_PROJECT(MI_TCH1) && MI_PROJECT(MI_TCH4) && !MI_PROJECT(MI_LCLEAR);
    bad += check("TRACK: T1..T4 CHANNEL (MENU > MIDI) 1..16 OMNI OFF, channels 1..4 by default (song.mch all 0)", ok);
    /* the defaults: TRACK plays as CH1-4, note for note */
    for (r = 0; r < 2u; r++) {
        midi_test_reset(); song.sel = 2; song.g[G_ROUTE] = r ? ROUTE_TRACK : 0; trk[3].eng_req = trk[3].engine = 0u;
        events_block(CTL);
        ok = 1;
        for (ch = 0; ch < 16u; ch++) {
            queued(0x90 | ch, 40u + ch, 100, 1);
            for (i = 0; i < NTRK; i++)
                ok &= gate_note(&trk[i], 40u + ch) == (ch < 4u && i == ch);
            ok &= midi_slot(ch) == (ch < 4u ? (int32_t)ch : -1);
        }
        bad += check(r ? "TRACK, the default channels: channels 1..4 play tracks 1..4, 5..16 nothing (as CH1-4)" :
                         "CH1-4 (for the comparison): channels 1..4 play tracks 1..4, 5..16 nothing", ok);
    }
    /* drums on 10, bass on 2, OMNI, OFF */
    midi_test_reset(); song.sel = 3; song.g[G_ROUTE] = ROUTE_TRACK; trk[0].p[P_VOICE] = trk[2].p[P_VOICE] = V_POLY;
    events_block(CTL);
    tch(0, 9); tch(1, 1); tch(2, MCH_OMNI); tch(3, MCH_OFF);
    ok = mch_get(0) == 9u && mch_get(1) == 1u && mch_get(2) == MCH_OMNI && mch_get(3) == MCH_OFF &&
         song.mch[0] == 9u && song.mch[1] == 0u && song.mch[2] == MCH_OMNI;
    ok &= midi_slot(9) == 0 && midi_slot(1) == 1 && midi_slot(0) == 2 && midi_slot(4) == 2 && midi_slot(15) == 2;
    queued(0x99, 50, 100, 1); queued(0x91, 51, 100, 2); queued(0x94, 52, 100, 1); queued(0x90, 53, 100, 2);
    ok &= gate_note(&trk[0], 50) && gate_note(&trk[1], 51) && gate_note(&trk[2], 52) && gate_note(&trk[2], 53) &&
          !gate_note(&trk[3], 50) && !gate_note(&trk[3], 52) && midi_notes[9][50] == 1u && midi_notes[4][52] == 3u;
    bad += check("TRACK: T1 on 10, T2 on 2, T3 OMNI (every other channel), T4 OFF (none; the selected track too)", ok);
    queued(0xE9, 127, 127, 1); queued(0xB9, 1, 77, 1); queued(0xD9, 66, 0, 1); queued(0xB9, 7, 0, 1);
    ok = midi_bend_target[0] == 512 && !midi_bend_target[1] && !midi_bend_target[3] && trk[0].mw == 77 && trk[0].at == 66 &&
         trk[0].p[P_LEVEL] == 0 && trk[3].p[P_LEVEL] != 0;
    queued(0xB1, 7, 127, 1);
    ok &= trk[1].p[P_LEVEL] == TP[P_LEVEL].max;
    queued(0xB9, 123, 0, 1);
    ok &= !gate_note(&trk[0], 50) && gate_note(&trk[1], 51) && gate_note(&trk[2], 52);
    bad += check("TRACK: bend, CC1, aftertouch, the CC map (CC7) and All Notes Off on channel 10 reach T1 only", ok);
    /* MIDI LEARN: a learned CC sets its own track, from any heard channel; a channel no track hears stays ignored */
    {
        uint32_t e = 20u | 2u << 7 | (P_PAN + 1u) << 9;
        uint8_t keep[2] = {ml_tab[0], ml_tab[1]};
        ml_tab[0] = (uint8_t)e; ml_tab[1] = (uint8_t)(e >> 8);
        trk[2].p[P_PAN] = 0;
        queued(0xB9, 20, 127, 1);
        ok = trk[2].p[P_PAN] == TP[P_PAN].max && trk[0].p[P_PAN] == 0;
        tch(2, 5);                                         /* (T3 off OMNI: channel 16 heard by no track) */
        queued(0xBF, 20, 0, 1);
        ok &= trk[2].p[P_PAN] == TP[P_PAN].max;
        queued(0xB5, 20, 0, 1);
        ok &= trk[2].p[P_PAN] == TP[P_PAN].min;
        ml_tab[0] = keep[0]; ml_tab[1] = keep[1];
        bad += check("TRACK: a learned CC (T3 PAN) from channel 10 sets T3; from a channel no track hears, nothing", ok);
    }
    /* two tracks on one channel: the first */
    tch(1, 9);
    queued(0x99, 55, 100, 1);
    ok = gate_note(&trk[0], 55) && !gate_note(&trk[1], 55) && midi_slot(9) == 0;
    bad += check("TRACK: T1 and T2 both on 10: the first (T1) plays it", ok);
    /* a channel change lets go of the old channel's notes (pedal-held too) and its bend */
    midi_test_reset(); song.g[G_ROUTE] = ROUTE_TRACK; trk[0].p[P_VOICE] = V_POLY; events_block(CTL);
    tch(0, 9);
    queued(0x99, 60, 100, 1); queued(0xB9, 64, 127, 1); queued(0x99, 62, 100, 1); queued(0x89, 62, 0, 1);
    queued(0xE9, 127, 127, 1);
    ok = gate_note(&trk[0], 60) && gate_note(&trk[0], 62) && midi_bend_target[0] == 512;
    tch(0, 10);
    ok &= !gate_note(&trk[0], 60) && !gate_note(&trk[0], 62) && !midi_owners[0] && !midi_ch[9].pedal && !midi_ch[9].bend &&
          !midi_bend_target[0];
    queued(0x9A, 64, 100, 1); queued(0x99, 65, 100, 1);
    ok &= gate_note(&trk[0], 64) && !gate_note(&trk[0], 65);
    queued(0x8A, 64, 0, 1);
    ok &= !any_gate();
    bad += check("TRACK: T1 10 -> 11 lets go of channel 10 (pedal-held too, bend reset); 11 plays, 10 is ignored", ok);
    /* TRACK <-> a block */
    midi_test_reset(); song.g[G_ROUTE] = ROUTE_TRACK; events_block(CTL);
    tch(1, 10);
    queued(0x9A, 60, 100, 1);
    song.g[G_ROUTE] = 2; events_block(CTL);                /* -> CH5-8: channel 7 is T3's there */
    ok = !gate_note(&trk[1], 60) && !midi_owners[1];
    queued(0x96, 62, 100, 1);
    ok &= gate_note(&trk[2], 62);
    song.g[G_ROUTE] = ROUTE_TRACK; events_block(CTL);
    ok &= !gate_note(&trk[2], 62);
    bad += check("TRACK -> CH5-8 -> TRACK: each switch lets go of what the new routing does not hear", ok);
    /* Program Change follows it (the main loop loads it) */
    midi_test_reset(); song.g[G_ROUTE] = ROUTE_TRACK; events_block(CTL);
    set_engine_of(&trk[0], 0); set_engine_of(&trk[1], 0); set_engine_of(&trk[2], 0);
    tch(0, 9); tch(2, MCH_OFF);
    pc(9, 3); pc(2, 4); pc(0, 5);
    ok = ui.mpc[0] == (0x80u | 3u) && !ui.mpc[1] && !ui.mpc[2] && !ui.mpc[3];
    frame();
    ok &= trk[0].preset == 3 && trk[2].preset == 0;
    bad += check("TRACK: a PC on channel 10 loads T1's; on 3 (T3 OFF) and 1 (no track) nothing", ok);
    /* MIDI out: the keys of a track send on its channel (OMNI, OFF: its CH1-4 one) */
    usb.config = 1;
    ok = 1;
    for (r = 0; r < 3u; r++) {
        uint32_t set = r == 0u ? 11u : r == 1u ? MCH_OMNI : MCH_OFF, want = r ? 2u : 11u;
        midi_test_reset(); song.g[G_ROUTE] = ROUTE_TRACK; song.sel = 2; events_block(CTL);
        tch(2, set); mo_r = mo_w;
        fm1_in.notes = 1u << 7; events_block(CTL);
        ok &= ((out_last() >> 8) & 0xFFu) == (0x90u | want);
        fm1_in.notes = 0; events_block(CTL);
        ok &= ((out_last() >> 8) & 0xFFu) == (0x80u | want);
    }
    usb.config = 0; mo_r = mo_w;
    bad += check("TRACK: MIDI out of track 3's keys on its channel 12; OMNI / OFF on channel 3", ok);
    midi_test_reset();
    return bad;
}

/* 1.5.5 (#208): MIDI IN notes light their track's cushion (always); MENU > MIDI > AUTO SELECT ON selects the track they
 * play, held off while the selected track has notes (MF_HOLD_MS) and after a switch; OFF by default; not with SEL */
static int follow_test(void)
{
    int bad = 0, ok;
    midi_test_reset(); fm1_ms = 1000; frame();
    ok = menu_get(MI_MFOLLOW) == 0u && !(ui_rec_prefs & 0x80u) && str_eq(MI_NAME[MI_MFOLLOW], "AUTO SELECT") &&
         MI_TAB[MI_MFOLLOW] == MTAB_MIDI && !MI_PROJECT(MI_MFOLLOW) && PREF_MIDI_FOLLOW == (0x80u << 8);
    queued(0x91, 60, 100, 1);
    ok &= midi_seen == 2u;
    frame();
    ok &= !midi_seen && song.sel == 0u && midi_lit(1) && !midi_lit(0) && msg_is("MIDI IN -> T2");
    frames(160);
    ok &= !midi_lit(1);
    bad += check("AUTO SELECT OFF (the default): a note into T2 lights its cushion ~90 ms, says MIDI IN -> T2, T1 stays", ok);

    menu_put(MI_MFOLLOW, 1);
    ok = (ui_rec_prefs & 0x80u) && menu_get(MI_MFOLLOW) == 1u && click_mode == CLICK_OFF;   /* (its own bit) */
    queued(0x81, 60, 0, 1); queued(0x92, 61, 100, 1); frame();
    ok &= song.sel == 2u && midi_lit(2) && !msg_is("MIDI IN -> T3");
    queued(0x91, 62, 100, 1); frame();
    ok &= song.sel == 2u;                                /* (T3 just played: held off) */
    frames(300);
    queued(0x91, 63, 100, 1); frame();
    ok &= song.sel == 1u;
    bad += check("AUTO SELECT ON: a note into T3 selects it; T2 right after does not (hold-off), 300 ms later it does", ok);

    frames(300);                                         /* a chord across T1 and T4, T2 selected and quiet */
    queued(0x93, 36, 100, 1); queued(0x90, 64, 100, 1); frame();
    ok = song.sel == 0u && midi_lit(0) && midi_lit(3);
    queued(0x93, 38, 100, 1); queued(0x90, 65, 100, 1); frame();
    frames(48);
    queued(0x93, 40, 100, 1); queued(0x90, 67, 100, 1); frame();
    ok &= song.sel == 0u;
    frames(300);                                         /* T1 (selected) and T3 in one frame: the selection stays */
    queued(0x90, 69, 100, 1); queued(0x92, 69, 100, 1); frame();
    ok &= song.sel == 0u;
    bad += check("  a chord across T1 and T4: the lower selected once, no flicker; the selected track with another: stays", ok);

    frames(300);
    song.g[G_ROUTE] = ROUTE_SEL; events_block(CTL);
    queued(0x92, 50, 100, 1); frame();
    ok = song.sel == 0u && midi_lit(0);
    song.g[G_ROUTE] = ROUTE_TRACK; events_block(CTL);
    tch(3, 9);
    frames(300);
    queued(0x99, 36, 100, 1); frame();
    ok &= song.sel == 3u;
    frames(300);
    hold(B_HOME);                                        /* (the menu up: no switch) */
    queued(0x90, 48, 100, 1); frame();
    ok &= ui.menu && song.sel == 3u;
    press(B_HOME);
    menu_put(MI_MFOLLOW, 0);
    bad += check("  SEL: every note is the selected track's (no switch); TRACK: T4 on 10 selected; not with the menu up", ok);

    menu_put(MI_MFOLLOW, 1);                             /* a note in the menu or NAME: lit there, then gone */
    frames(300);
    hold(B_HOME);
    queued(0x90, 50, 100, 1); frame();
    ok = ui.menu && !midi_seen && midi_lit(0) && song.sel == 3u;
    frames(300);
    press(B_HOME);
    frames(48);
    ok &= !ui.menu && song.sel == 3u;
    name_open(NK_USER_SAVE, 5);
    queued(0x91, 50, 100, 1); frame();
    ok &= name_on() && !midi_seen && midi_lit(1) && song.sel == 3u;
    frames(300);
    name_close();
    frames(48);
    ok &= song.sel == 3u;
    menu_put(MI_MFOLLOW, 0);
    bad += check("  a note in the menu or NAME: its light there, the flag taken; no AUTO SELECT from it later", ok);

    {   /* a step held for a lock (the DRUM grid on SEQ > STEP): the selection stays until it is let go */
        track_t *t4 = &trk[3];
        uint32_t k4;
        menu_put(MI_MFOLLOW, 1);
        song.sel = 3;
        track_defaults_steps(t4);
        t4->p[P_SLEN] = 16;
        go_home();
        open_family(FAM_SEQ);
        frames(300);
        k4 = key_at(0, 4);
        fm1_in.notes |= 1u << k4; host_notes |= 1u << k4; frame();
        ok = grid_on() && lock_held() == (1u << 4);
        queued(0x90, 60, 100, 1); frame();               /* T1's channel */
        ok &= song.sel == 3u && lock_held() == (1u << 4) && midi_lit(0);
        turn(EN_K1, 3);
        ok &= motion_lock_count(t4) == 1u && !motion_lock_count(&trk[0]);
        fm1_in.notes &= ~(1u << k4); frame();
        frames(48);
        ok &= song.sel == 3u;                            /* (the note during the lock: gone) */
        frames(300);
        queued(0x90, 62, 100, 1); frame();
        ok &= song.sel == 0u;
        menu_put(MI_MFOLLOW, 0);
        bad += check("  a step held for a lock (DRUM grid): no switch while held (the lock on its own track), the next note after", ok);
    }
    midi_test_reset();
    return bad;
}

/* #103: the standard CC map, on the track the channel plays, as a knob turn (and as AUTOMATION records one) */
static int cc_map_test(void)
{
    static const struct { uint8_t cc; uint8_t id; } COMMON[] = {
        {5, P_GLIDE}, {7, P_LEVEL}, {72, P_REL}, {73, P_ATK}, {75, P_DEC}, {91, P_REV}, {93, P_CHOR}, {94, P_DLY},
    };
    int bad = 0, ok = 1; uint32_t i, e; int16_t before[P_COUNT];
    midi_test_reset();
    bad += check("power-on parts: ANALOG, FM6, LOFI, DRUM", trk[0].eng_req == 0u && trk[1].eng_req == ENGI_FM6 &&
                 trk[2].eng_req == 3u && trk[3].eng_req == ENGI_DRUM);
    for (i = 0; i < NELEM(COMMON); i++) {
        queued(0xB0, COMMON[i].cc, 127, 1);
        ok &= trk[0].p[COMMON[i].id] == TP[COMMON[i].id].max;
        queued(0xB0, COMMON[i].cc, 0, 1);
        ok &= trk[0].p[COMMON[i].id] == TP[COMMON[i].id].min;
        queued(0xB0, COMMON[i].cc, 37, 1);
        ok &= trk[0].p[COMMON[i].id] == 37;
    }
    bad += check("CC5 7 72 73 75 91 93 94: GLIDE LEVEL REL ATK DEC REV CHO DLY, 0..127 over their range", ok);
    queued(0xB0, 10, 0, 1); ok = trk[0].p[P_PAN] == -64;
    queued(0xB0, 10, 127, 1); ok &= trk[0].p[P_PAN] == 63;
    queued(0xB0, 10, 64, 1); ok &= trk[0].p[P_PAN] == 0;
    bad += check("CC10 PAN: 0 left, 64 centre, 127 right", ok);
    queued(0xB0, 74, 127, 1); queued(0xB0, 71, 0, 1);
    bad += check("CC74 / CC71 on ANALOG: CUT / RES", trk[0].p[P_E4] == 127 && trk[0].p[P_E5] == 0);
    queued(0xB1, 74, 64, 1); queued(0xB1, 74, 127, 1);
    bad += check("CC74 on FM6 (channel 2): MLVL, bipolar (127: +63)", trk[1].p[P_E2] == 63);
    memcpy(before, trk[1].p, sizeof before); queued(0xB1, 71, 99, 1);
    bad += check("CC71 on FM6 (no resonance): ignored", !memcmp(before, trk[1].p, sizeof before));
    queued(0xB3, 74, 10, 1);
    bad += check("CC74 on DRUM (channel 4): TONE", trk[3].p[P_E2] == 10);
    for (e = 0, ok = 1; e < NENGINES; e++) {   /* the table names a continuous EDIT value of every engine */
        uint32_t m = MIDI_CC_ENG[e], k;
        for (k = 0; k < 2u; k++) {
            uint32_t n = (m >> (k ? 0 : 4)) & 15u;
            const param_desc_t *d = n ? &ENGINES[e]->edit[n - 1u] : 0;
            ok &= n <= 8u && (!d || (d->fmt != F_ENUM && d->max - d->min >= 24));
        }
    }
    bad += check("every engine's CC74 / CC71 target is a continuous EDIT value (or none)", ok);
    trk[2].eng_req = trk[2].engine = 7u;               /* WHEEL: no brightness, no resonance */
    memcpy(before, trk[2].p, sizeof before); queued(0xB2, 74, 3, 1); queued(0xB2, 71, 3, 1);
    bad += check("CC74 / CC71 on WHEEL: ignored", !memcmp(before, trk[2].p, sizeof before));
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 121, 0, 1);
    bad += check("CC121 resets no mapped parameter (RP-015)", !memcmp(before, trk[0].p, sizeof before));
    queued(0xB0, 20, 99, 1); queued(0xB0, 76, 99, 1); queued(0xB0, 95, 99, 1);
    bad += check("CCs outside the map change nothing", !memcmp(before, trk[0].p, sizeof before));
    memcpy(before, trk[2].p, sizeof before);
    queued(0xB4, 7, 5, 1); queued(0xB9, 74, 5, 2); queued(0xBF, 91, 5, 1);
    ok = !memcmp(before, trk[2].p, sizeof before) && trk[0].p[P_LEVEL] != 5 && trk[3].p[P_LEVEL] != 5;
    bad += check("ROUT CH1-4: mapped CCs on channels 5..16 reach no part", ok);
    song.g[G_ROUTE] = 1; song.sel = 2; events_block(CTL);
    queued(0xB9, 7, 5, 2);
    bad += check("ROUT SEL: channel 10's CC7 sets the selected track's LEVEL", trk[2].p[P_LEVEL] == 5 && trk[0].p[P_LEVEL] != 5);
    song.g[G_ROUTE] = 0; song.sel = 0; events_block(CTL);

    /* AUTOMATION: the selected track armed and playing records the CC as a knob move; else it is the new base */
    midi_test_reset();
    song.rec = 1; seq_start(); events_block(CTL);
    queued(0xB0, 74, 20, 1);
    bad += check("armed and playing: CC74 records a motion event of CUT on the selected track", motion.count == 1u &&
                 motion.event[0].param == P_E4 && motion.event[0].value == 20 && (motion.event[0].place >> 6) == 0u);
    queued(0xB1, 74, 20, 1);
    bad += check("  another track (channel 2, not selected): not recorded", motion.count == 1u && trk[1].p[P_E2] != 0);
    seq_stop(); events_block(CTL);
    bad += check("  stopped: the patch's own CUT comes back", trk[0].p[P_E4] == ANALOG_PRESETS[trk[0].preset].e[4]);
    song.rec = 0;
    queued(0xB0, 74, 77, 1);
    bad += check("not armed: the CC is the new value, no event", trk[0].p[P_E4] == 77 && motion.count == 1u);
    return bad;
}
/* 1.5 (Discussion #170): MIDI LEARN. GLO + D4 on, a knob picks, a CC is learned; the map in favorites.factory[14] */
static int ml_entry(uint32_t cc, uint32_t k, uint32_t id) { return ml_cc_of(k, id) == cc + 1u; }
static int learn_test(void)
{
    int bad = 0, ok;
    uint32_t i;
    int16_t before[P_COUNT], cut, lvl;
    midi_test_reset();
    bad += check("MIDI LEARN: nothing learned at power-on (every older setting: all 0)", !ui.ml.on && !ml_count() &&
                 !ml_tab[0] && !memcmp(ml_tab, ml_tab + 1, 31));
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 20, 99, 1);
    bad += check("  an unlearned CC changes nothing (as before)", !memcmp(before, trk[0].p, sizeof before));

    /* on: GLO + D4 */
    lay_combo(B_GLO, white(5)); key_up(white(5)); frame();
    ok = ui.ml.on && msg_is("MIDI LEARN ON") && ui.layer == LAYER_GLO;
    ok &= ((leds_at(0) | leds_at(250)) >> white(5)) & 1u;          /* (lit: on) */
    btn_up(B_GLO); frame();
    bad += check("GLO + D4: MIDI LEARN on (the D4 cell / LED lit), the map closes with GLO", ok && ui.ml.on && !ui.layer);
    queued(0xB0, 20, 99, 1); frame();
    bad += check("  no parameter picked yet: a CC learns nothing, sets nothing", !ml_count() &&
                 !memcmp(before, trk[0].p, sizeof before));

    /* a knob picks (as it edits), a CC is learned */
    frames(300);                                                     /* (#39: the knobs quiet after a layer) */
    go_title("EDIT 2");
    cut = trk[0].p[P_E4];
    turn(EN_K1, -3);
    ok = ui.ml.pick && ui.ml.trk == 0u && ui.ml.id == P_E4 && trk[0].p[P_E4] == cut - 3 && ml_arm;
    { char b[24]; ml_line(b); ok &= str_eq(b, "T1 CUT: SEND A CC"); }
    queued(0xB0, 1, 99, 1); queued(0xB0, 64, 127, 1); queued(0xB0, 64, 0, 1); queued(0xB0, 0, 5, 1);
    frame();
    ok &= !ml_count() && ui.ml.pick;                                   /* (CC1, CC64, CC0: never learned) */
    queued(0xB0, 20, 100, 1); frame();
    ok &= ml_count() == 1u && ml_entry(20, 0, P_E4) && msg_is("CC20 -> T1 CUT") && !ui.ml.pick && !ml_arm;
    ok &= trk[0].p[P_E4] == cut - 3;                                /* (the learning CC sets nothing itself) */
    { char b[24]; ml_line(b); ok &= str_eq(b, "LEARN: TURN A KNOB"); }
    bad += check("  EDIT 2 KNOB 1 picks T1 CUT; CC1 / CC64 / CC0 are passed over; CC20 is learned", ok);
    queued(0xB0, 20, 127, 1);
    ok = trk[0].p[P_E4] == 127;
    queued(0xB2, 20, 0, 1);                                          /* (channel 3: track 3's, still T1 CUT) */
    ok &= trk[0].p[P_E4] == 0;
    turn(EN_K1, 1);
    { char b[24]; ml_line(b); ok &= str_eq(b, "T1 CUT = CC20"); }
    bad += check("  CC20 sets T1 CUT on any channel ROUT hears; the knob again shows its CC", ok);
    song.g[G_ROUTE] = 0; queued(0xB9, 20, 64, 1);
    bad += check("  ROUT CH1-4: CC20 on channel 10 is ignored as before", trk[0].p[P_E4] == 1);

    /* the standard map: a learned CC no longer reaches it; GLO's knobs pick a track's LEVEL */
    btn_down(B_GLO); frame();
    turn(EN_K2, 2);
    ok = ui.ml.pick && ui.ml.trk == 1u && ui.ml.id == P_LEVEL;
    btn_up(B_GLO); frames(300);
    queued(0xB0, 74, 10, 1); frame();
    ok &= ml_entry(74, 1, P_LEVEL) && msg_is("CC74 -> T2 LVL");
    cut = trk[0].p[P_E4];
    queued(0xB0, 74, 127, 1);
    ok &= trk[1].p[P_LEVEL] == TP[P_LEVEL].max && trk[0].p[P_E4] == cut;
    bad += check("  GLO + KNOB 2 picks T2 LEVEL; CC74 learned: T2 LEVEL, no more T1 CUT (the standard map)", ok);

    /* one CC one parameter, a parameter one CC */
    go_title("EDIT 2");
    turn(EN_K1, 1);
    queued(0xB0, 21, 0, 1); frame();
    ok = ml_count() == 2u && ml_entry(21, 0, P_E4) && !ml_entry(20, 0, P_E4);
    queued(0xB0, 20, 99, 1);
    ok &= trk[0].p[P_E4] != 99;
    turn(EN_K2, 1);                                                  /* T1 RES */
    queued(0xB0, 21, 0, 1); frame();
    ok &= ml_count() == 2u && ml_entry(21, 0, P_E5) && !ml_cc_of(0, P_E4);
    bad += check("  learning again replaces: CUT on CC21 drops CC20; CC21 on RES drops CUT's", ok);

    /* OCT- clears the picked parameter's CC; no octave */
    turn(EN_K2, -1);
    press(B_OCTDN);
    ok = !ml_cc_of(0, P_E5) && msg_is("T1 RES CC CLEARED") && song.octave == 0 && ui.ml.on;
    press(B_OCTDN);
    ok &= msg_is("NO CC TO CLEAR") && song.octave == 0;
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 21, 50, 1);
    ok &= !memcmp(before, trk[0].p, sizeof before) && ml_count() == 1u;
    bad += check("  OCT- clears the picked parameter's CC (OCT- again: none), no octave", ok);

    /* review 1.5: OCT- on one parameter keeps CC16 learned elsewhere (16 = ML_N was the "no CC" mark) */
    ml_learn(16, 2, P_LEVEL);
    ml_learn(30, 0, P_E5);
    turn(EN_K2, -1);                                                   /* T1 RES picked again */
    press(B_OCTDN);                                                    /* T1 RES picked: drops CC30 only */
    ok = ml_entry(16, 2, P_LEVEL) && !ml_cc_of(0, P_E5) && msg_is("T1 RES CC CLEARED");
    press(B_OCTDN);
    ok &= ml_entry(16, 2, P_LEVEL) && msg_is("NO CC TO CLEAR");
    ml_drop(16, NTRK, P_COUNT);
    bad += check("  OCT- keeps CC16 learned on another parameter", ok);

    /* full: ML_N; a CC never learned */
    uint32_t uint_cc;
    for (i = 0, uint_cc = 2; i < ML_N - 1u; uint_cc++) {
        if (!ml_free_cc(uint_cc) || uint_cc == 74u || uint_cc == 90u) continue;
        ml_learn(uint_cc, 2, P_ATK + i++);
    }
    turn(EN_K1, 1);
    queued(0xB0, 90, 5, 1); frame();
    ok = msg_is("LEARN FULL") && ml_count() == ML_N && !ml_cc_of(0, P_E4);
    ok &= ml_learn(64, 0, P_E4) == 2u && ml_learn(1, 0, P_E4) == 2u && ml_learn(123, 0, P_E4) == 2u;
    bad += check("  64 learned: the 65th says LEARN FULL, nothing replaced; CC1 / 64 / 123 are never learned", ok);

    /* OCT+: done; a CC is no longer taken */
    press(B_OCTUP);
    ok = !ui.ml.on && msg_is("LEARN DONE") && song.octave == 0 && !ml_arm;
    queued(0xB0, 74, 0, 1);
    ok &= trk[1].p[P_LEVEL] == TP[P_LEVEL].min;
    press(B_OCTUP);
    ok &= song.octave == 1;
    song.octave = 0;
    bad += check("OCT+: LEARN DONE (the map stays, OCT± shift the octave again)", ok);
    lay_combo(B_GLO, white(5)); key_up(white(5)); btn_up(B_GLO); frame();
    lay_combo(B_GLO, white(5)); key_up(white(5)); btn_up(B_GLO); frame();
    bad += check("GLO + D4 twice: on, then LEARN DONE", !ui.ml.on && msg_is("LEARN DONE"));

    /* MENU > MIDI > LEARN CLEAR (1.5): OCT+ clears every learned CC */
    hold(B_HOME);
    turn(EN_ALGO, 1); turn(EN_ALGO, 1); turn(EN_ALGO, 1); for (i = MI_MIDIIN; i < MI_LCLEAR; i++) turn(EN_PRESET, 1);   /* (1.5.5: past T1..T4 CHANNEL) */
    ok = ui.menu == 1 && ui.menu_sel == MI_LCLEAR && menu_tab() == MTAB_MIDI && str_eq(MI_NAME[MI_LCLEAR], "LEARN CLEAR") &&
         ml_count() == ML_N && oct_leds() == OCT_BREATH && !menu_valued(MI_LCLEAR) && menu_valued(MI_MIDIIN);
    turn(EN_K1, 3); press(B_OCTDN);
    ok &= ml_count() == ML_N && ui.menu == 1;                           /* (knobs, OCT-: nothing) */
    press(B_OCTUP);
    ok &= !ml_count() && msg_is("LEARN CLEARED") && ui.menu == 1 && song.octave == 0 && oct_leds() == 0u;
    memcpy(before, trk[1].p, sizeof before);
    press(B_HOME);
    queued(0xB0, 74, 3, 1);
    ok &= !memcmp(before, trk[1].p, sizeof before) && trk[0].p[P_E4] == 3;   /* (CC74: the standard map again) */
    hold(B_HOME); press(B_OCTUP);
    ok &= msg_is("NOTHING LEARNED") && ui.menu_sel == MI_LCLEAR;
    press(B_HOME);
    bad += check("MENU > MIDI > LEARN CLEAR: OCT+ clears every CC (LEARN CLEARED; none: NOTHING LEARNED)", ok);

    /* the stored codes with 1.5's parameters (the INSERT 96..100, TYPE 101, ESYNC 102, P_E0 103): a common parameter
     * id + 1, below ML_E0; an engine one ML_E0 + k whatever P_E0 is; every id back as it went in */
    ok = P_ESYNC == 102u && P_MORPH == 103u && P_PD_FB == 104u && P_E0 == 105u && P_COUNT == 113u && P_E0 < ML_E0;
    for (i = 0; i < P_COUNT; i++)
        ok &= ml_id(ml_code(i)) == i && (i < P_E0 ? ml_code(i) == i + 1u && ml_code(i) < ML_E0 : ml_code(i) == ML_E0 + i - P_E0);
    ok &= ml_id(0) == P_COUNT && ml_id(P_E0 + 1u) == P_COUNT && ml_id(ML_E0 - 1u) == P_COUNT;   /* (no such parameter) */
    memset(favorites.factory[14], 0, 32);
    ok &= ml_learn(30, 1, P_IMIX) == 1u && ml_learn(31, 2, P_ESYNC) == 1u && ml_learn(33, 3, P_E0 + 3u) == 1u;
    ok &= ml_get(0) == (30u | 1u << 7 | (P_IMIX + 1u) << 9) && ml_get(1) == (31u | 2u << 7 | (P_ESYNC + 1u) << 9) &&
          ml_get(2) == (33u | 3u << 7 | (ML_E0 + 3u) << 9);
    queued(0xB0, 30, 0, 1);
    queued(0xB0, 31, 127, 1);
    queued(0xB0, 33, 127, 1);
    ok &= trk[1].p[P_IMIX] == 0 && trk[2].p[P_ESYNC] == 1 && trk[3].p[P_E0 + 3u] == param_desc_of(eng_idx(trk[3].eng_req), P_E0 + 3u)->max;
    bad += check("MIDI LEARN codes with 1.5's P_E0 103: common ids + 1 (INSERT MIX, ESYNC), engine ML_E0 + k; all ids round trip", ok);
    memset(favorites.factory[14], 0, 32);
    return bad;
}
/* 1.5.1 (#179): MIDI Program Change. The audio side only notes it (ui.mpc); the main loop loads it, as PRESETS
 * numbers the engine's sounds (aliases skipped), DRUM's kits (retired variants skipped), FM6's presets then its
 * voice bank; past the last: ignored */
static uint32_t pc_bank_k;                               /* the bank voice pc_bank_read was last asked for */
static int pc_bank_read(uint32_t k, uint8_t *pk)
{
    pc_bank_k = k;
    memcpy(pk, FM6_FACTORY[(k + 2u) % FM6_NFACTORY], FM6_PACKED);
    return 0;
}
static int pc_test(void)
{
    int bad = 0, ok;
    uint32_t e, n, k;
    int32_t last;
    track_t *t = &trk[0], was, got;
    midi_test_reset();
    set_engine_of(t, 0);                                 /* ANALOG */
    t->p[P_LEVEL] = 90; t->p[P_AMODE] = 2; t->p[P_SLCR] = SL_GATE;
    t->p[P_ITYPE] = 1; t->p[P_FTYPE] = 1; t->p[P_ESYNC] = 1; t->p[P_E4] = 11;
    my_steps(t);
    was = *t;
    pc(0, 3);
    ok = !memcmp(was.p, t->p, sizeof was.p) && t->preset == was.preset && ui.mpc[0] == (0x80u | 3u) && !ui.mpc[1];
    bad += check("PC on ch 1: the audio side only notes it (track 1, program 3), nothing loaded there", ok);
    frame();
    got = *t;
    bad += check("  the main loop loads track 1's preset 3", t->preset == 3 && t->eng_req == 0 && !ui.mpc[0]);
    *t = was;
    apply_preset_to(t, 3);
    ok = !memcmp(got.p, t->p, sizeof got.p) && !memcmp(got.step, was.step, sizeof got.step);
    ok &= got.p[P_LEVEL] == 90 && got.p[P_AMODE] == 2 && got.p[P_SLCR] == SL_GATE;
    ok &= got.p[P_ITYPE] == 1 && got.p[P_FTYPE] == TP[P_FTYPE].def && got.p[P_ESYNC] == TP[P_ESYNC].def;
    bad += check("  as PRESETS loads it: steps, LEVEL, ARP, SLICER and the INSERT kept; TYPE, ESYNC the preset's", ok);

    /* a run of them: the last wins, one undo */
    midi_test_reset();
    set_engine_of(t, 0);
    undo.keep = 0;                                       /* (the PCs' own undo copy) */
    was = *t;
    pc(0, 2); pc(0, 5);
    frame();
    ok = t->preset == 5;
    pc(0, 1); frame();
    ok &= t->preset == 1;
    hold(B_SAVE);
    ok &= same_sound(t, &was) && msg_is("UNDO/REDO T1");
    bad += check("  two before a frame: the last loads; a run of PCs is one undo (SAVE held: the sound before)", ok);

    /* the editor: a RELOAD for the selected track only */
    set_engine_of(&trk[1], 0);
    sync_reload = 0;
    pc(1, 2); frame();
    ok = trk[1].preset == 2 && !sync_reload;
    pc(0, 2); frame();
    ok &= sync_reload;
    bad += check("  a RELOAD to the editor for the selected track's load, none for another's", ok);

    /* every engine: program n = the n-th sound PRESETS shows, then nothing */
    ok = 1;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e) || e == ENGI_DRUM || !preset_shown(e))
            continue;
        set_engine_of(t, e);
        for (n = 0, last = -1; n < preset_shown(e); n++) {
            pc(0, n); frame();
            ok &= t->eng_req == e && (int32_t)t->preset > last && preset_orig(ENGINES[e], t->preset) == t->preset;
            last = t->preset;
        }
        was = *t;
        pc(0, n); frame(); pc(0, 127); frame();
        ok &= !memcmp(was.p, t->p, sizeof was.p) && t->preset == was.preset && t->eng_req == e;
    }
    bad += check("  every engine: 0.. its presets in PRESETS' order (no alias twice); past the last ignored", ok);
    set_engine_of(t, ENGI_SAMPLE);
    pc(0, 1); frame();
    ok = str_eq(ENGINES[ENGI_SAMPLE]->presets[t->preset].name, "FLUTE");
    pc(0, 4); frame();
    ok &= t->eng_req == ENGI_SAMPLE && str_eq(ENGINES[ENGI_SAMPLE]->presets[t->preset].name, "FLUTE");
    bad += check("  SAMPLE: PC 1 is FLUTE (not PIANO's alias); PC 4 (once PERC) does not turn the track into DRUM", ok);

    /* DRUM: the kits as KIT steps through them */
    {
        const param_desc_t *d = param_desc_of(ENGI_DRUM, P_E0);
        track_t *dr = &trk[3];
        set_engine_of(dr, ENGI_DRUM);
        dr->p[P_E0] = 1;                                 /* (HAND, retired: plays 66) */
        ok = 1;
        for (n = 0, last = -1;; n++) {
            was = *dr;
            pc(3, n); frame();
            if (!memcmp(was.p, dr->p, sizeof was.p))
                break;
            ok &= dr->p[P_E0] > last && enum_orig(d, dr->p[P_E0]) == dr->p[P_E0] && dr->preset == was.preset;
            for (k = 0; k < P_COUNT; k++)
                ok &= k == P_E0 || dr->p[k] == was.p[k];
            last = dr->p[P_E0];
        }
        ok &= n == 10u && last == d->max;                 /* (1.5.6, #216: DEEP TIGHT PERC 6..8) */
        pc(3, DK_COUNT + 2u); frame();
        ok &= dr->p[P_E0] == last;
        bad += check("  DRUM: original programs 0..8 unchanged, program 9 DRUM-X, only KIT; 10.. ignored", ok);
    }

    /* FM6: its presets, then the bank's voices */
    {
        track_t *f = &trk[1];
        uint8_t v[FP_SIZE + 1u];
        set_engine_of(f, ENGI_FM6);
        pc(1, 4); frame();
        ok = f->preset == 4 && f->p[P_E7] == 4 && fm6_slot[1] == 4u && f->p[P_E6] == 30;   /* PAD: F5, DTUN 30 */
        bad += check("  FM6: 0..7 its presets (PC 4: PAD, SLOT F5, its DTUN)", ok);
        pc(1, 8); frame();
        ok = f->p[P_E7] == 4 && fm6_slot[1] == 4u;
        fm6_bank_used = 1u | 1u << 5;
        fm6_bank_read = pc_bank_read;
        pc(1, 8); frame();
        fm6_unpack(FM6_FACTORY[2], v);
        ok &= f->p[P_E7] == (int16_t)(FM6_OWN + 1u) && fm6_slot[1] == FM6_OWN + 1u && pc_bank_k == 0u &&
              !memcmp(v, fm6_patch[1], FP_SIZE) && f->p[P_E6] == 30 && f->preset == 4;
        pc(1, 9); frame();
        ok &= f->p[P_E7] == (int16_t)(FM6_OWN + 1u);
        pc(1, 13); frame();
        ok &= f->p[P_E7] == (int16_t)(FM6_OWN + 6u) && pc_bank_k == 5u;
        pc(1, 8u + FM6_NBANK); frame();
        ok &= f->p[P_E7] == (int16_t)(FM6_OWN + 6u);
        pc(1, 0); frame();
        ok &= f->p[P_E7] == 0 && fm6_slot[1] == 0u && f->p[P_E6] == 0;
        fm6_bank_used = 0;
        fm6_bank_read = 0;
        bad += check("  FM6: 8.. the voice bank B1..B32 through SLOT (the rest stays), an empty or no bank ignored", ok);
    }

    /* ROUT */
    midi_test_reset();
    set_engine_of(&trk[0], 0); set_engine_of(&trk[1], 0); set_engine_of(&trk[2], 0);
    pc(4, 3); pc(9, 3);
    ok = !ui.mpc[0] && !ui.mpc[1] && !ui.mpc[2] && !ui.mpc[3];
    frame();
    ok &= trk[0].preset == 0;
    bad += check("  ROUT CH1-4: a PC on channels 5..16 is ignored", ok);
    song.g[G_ROUTE] = 1;
    song.sel = 2;
    pc(9, 3); frame();
    ok = trk[2].preset == 3 && trk[0].preset == 0;
    song.g[G_ROUTE] = 2;
    song.sel = 0;
    events_block(CTL);
    pc(5, 1); pc(0, 2); frame();
    ok &= trk[1].preset == 1 && trk[0].preset == 0;
    song.g[G_ROUTE] = 0;
    events_block(CTL);
    bad += check("  ROUT SEL: any channel the selected track; CH5-8: channel 6 track 2, channel 1 ignored", ok);

    /* TRS: one data byte, running status */
    um_byte(0xC1); um_byte(2); events_block(CTL);
    ok = ui.mpc[1] == (0x80u | 2u);
    frame();
    ok &= trk[1].preset == 2;
    um_byte(3); events_block(CTL); frame();
    ok &= trk[1].preset == 3;
    bad += check("  TRS MIDI IN: a PC (one data byte) and the next under running status", ok);

    /* MIDI LEARN waiting for a CC: a PC is no CC */
    ml_arm = 1;
    pc(0, 2);
    ok = !ml_heard && ui.mpc[0] == (0x80u | 2u);
    ml_arm = 0;
    frame();
    ok &= trk[0].preset == 2;
    bad += check("  MIDI LEARN waiting: a PC is not taken for a CC, it loads", ok);
    return bad;
}
int main(void)
{
    int bad = controls_test() + sustain_test() + ownership_test() + clock_test(1) + clock_test(2) + clock_arp_and_boundaries() +
              arp_ext_stop_test() + usb_burst_test() + route_test() + route_block_test() + route_track_test() + follow_test() + cc_map_test() + learn_test() + pc_test();
    printf("%s\n", bad ? "MIDI CONTROL/CLOCK TEST FAILED" : "MIDI control/clock integration tests passed"); return bad != 0;
}
