/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hügelton Instruments contributors */
/* Real TIMER5 scheduling and TRS parser against a simulated RX DMA. A busy
 * audio ISR must not postpone input timestamping until its render completes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#ifndef FELUCCA_UART
#define FELUCCA_UART 1
#endif
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_uart.h"
static uint32_t rx_pending, rx_write, rx_polls;
static uint32_t host_rx_take(void) { uint32_t n = rx_pending; rx_pending = 0; rx_polls++; return n; }
#define fm1_uart1_rx_take() host_rx_take()
#ifdef TIMER5_SYNTH_TEST
#define UI_TEST_NO_MAIN 1
/* Keep the UI's small diagnostic stub separate from the real timer fields. */
#define felucca_dbg ui_debug_stub
#define fm1_delay_ms unused_timer_delay
#include "ui_test.c"
#undef fm1_delay_ms
#undef felucca_dbg
#else
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
#endif
static uint32_t host_us, scans, acks, usb_polls, uac_polls;
#ifndef TIMER5_SYNTH_TEST
static uint32_t fm1_ticks(void) { return host_us; }
#endif
static void fm1_timer5_ack(void) { acks++; }
static void fm1_input_tick(void) { scans++; }
static void host_usb_poll(void) { usb_polls++; }
#define usb_poll() host_usb_poll()
#undef FELUCCA_UAC
#define FELUCCA_UAC 1
static void uac_service(void) { uac_polls++; }
#define FM1_TICKS_PER_US 1u
static struct { uint32_t timer_irqs, nested, in_audio; } felucca_dbg;
static uint32_t t5_nested_ticks;
#include "../firmware/src/timer5.c"
#ifndef TIMER5_SYNTH_TEST
static int check(const char *name, int ok) { printf("%s: %s\n", name, ok ? "ok" : "FAIL"); return !ok; }
#endif
static void wire_byte(uint32_t b) {
    um_ring[rx_write++ % UM_RING] = (uint8_t)b;
    rx_pending++;
}
int main(void) {
    int bad = 0, clock_timing_ok = 1, cc_ok = 1, usb_deferred_ok = 1;
    uint32_t next_byte = 320u, next_clock = 20833u, byte_phase = 0, value = 0;
    uint32_t clocks_sent = 0, clocks_seen = 0, cc_seen = 0, cc_sent = 0, bytes_sent = 0;
    uint32_t clock_sent_ms[512];
#ifdef TIMER5_SYNTH_TEST
    int bpm_min = 240, bpm_max = 40;
    ui_power_on(); ml_clear_all(); song.g[G_CLOCK] = 2; song.g[G_ROUTE] = ROUTE_TRACK;
    song.mch[0] = 0; song.mch[1] = 1; song.mch[2] = 2; song.mch[3] = 3;
    uint32_t decay_b = DX_SOUND_FIRST + 16u * DV_SNARE + 5u + DXP_DECAY;
    set_engine_of(&trk[3], ENGI_DRUM); apply_preset_to(&trk[3], 4);
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = 64; trk[k].p[P_SDIV] = 2;
        for (uint32_t i = 0; i < NSTEP; i++) trk[k].step[i] = (step_t){{(uint8_t)(k == 3 ? 38 : 60 + k)}, 1, ST_NOTE, 0, 100};
    }
    bad += check("learned CC controls an ANALOG parameter", ml_learn(12, 0, P_E4) == 1u);
    bad += check("learned CC controls an exact DRUM-X A/B lane", ml_learn(13, 3, decay_b) == 1u);
    /* Exercise a full 64-entry map on every incoming CC. */
    for (uint32_t i = 0, cc = 2; i < ML_N - 2; cc++) {
        if (!ml_free_cc(cc) || cc == 12 || cc == 13) continue;
        uint32_t id = DX_SOUND_FIRST + i;
        if (id >= decay_b) id++;
        bad += check("fill remaining MIDI Learn slot", ml_learn(cc, 3, id) == 1u); i++;
    }
    events_block(CTL); um_byte(0xFA); events_block(CTL);
#endif
    for (host_us = 100; host_us <= 10000000u; host_us += 100) {
        while (next_byte <= host_us) {
            if (next_byte >= next_clock) {
                wire_byte(0xF8u);
                clock_sent_ms[clocks_sent++] = next_byte / 1000u;
                next_clock = (clocks_sent + 1u) * 125000u / 6u;
            } else if (byte_phase == 0) { wire_byte(0xB0u); byte_phase = 1; }
            else if (byte_phase == 1) {
#ifdef TIMER5_SYNTH_TEST
                wire_byte((value & 1u) ? 13u : 12u);
#else
                wire_byte(12u);
#endif
                byte_phase = 2;
            }
            else { wire_byte(value++ & 127u); byte_phase = 0; cc_sent++; }
            next_byte += 320u;
            bytes_sent++;
        }
        /* 4.5 ms renders alternate with 1.5 ms outside the render. */
        felucca_dbg.in_audio = host_us % 6000u < 4500u;
#ifdef TIMER5_SYNTH_TEST
        host_ticks = host_us;
#endif
        uint32_t usb_before = usb_polls;
        fm1_timer5_irq();
        if (felucca_dbg.in_audio && usb_polls != usb_before) usb_deferred_ok = 0;
        /* Audio consumes only after a render, as on hardware. */
        if (!felucca_dbg.in_audio) {
            uint32_t read = mi_r;
            while (read != mi_w) {
                uint32_t at = read++ % MQ, pkt = midi_in_q[at];
                if (((pkt >> 8) & 255u) == 0xF8u) {
                    uint32_t lag = midi_in_ms[at] - clock_sent_ms[clocks_seen++];
                    if (lag > 1u || midi_in_source[at] != 2u) clock_timing_ok = 0;
                } else {
                    uint32_t cc = 12u;
#ifdef TIMER5_SYNTH_TEST
                    cc += cc_seen & 1u;
#endif
                    if (((pkt >> 8) & 255u) != 0xB0u || ((pkt >> 16) & 255u) != cc ||
                        (pkt >> 24) != (cc_seen & 127u)) cc_ok = 0;
                    cc_seen++;
                }
            }
#ifdef TIMER5_SYNTH_TEST
            events_block(CTL);
            if (clocks_seen > 6) {
                if (song.g[G_BPM] < bpm_min) bpm_min = song.g[G_BPM];
                if (song.g[G_BPM] > bpm_max) bpm_max = song.g[G_BPM];
            }
#else
            mi_r = read;
#endif
        }
    }
    for (uint32_t i = 0; i < 60; i++) {
        host_us += 100; felucca_dbg.in_audio = 0;
#ifdef TIMER5_SYNTH_TEST
        host_ticks = host_us;
#endif
        fm1_timer5_irq();
    }
    uint32_t read = mi_r;
    while (read != mi_w) {
        uint32_t at = read++ % MQ, pkt = midi_in_q[at];
        if (((pkt >> 8) & 255u) == 0xF8u) {
            if (midi_in_ms[at] - clock_sent_ms[clocks_seen++] > 1u) clock_timing_ok = 0;
        } else {
            if ((pkt >> 24) != (cc_seen & 127u)) cc_ok = 0;
            cc_seen++;
        }
    }
#ifdef TIMER5_SYNTH_TEST
    events_block(CTL);
    bad += check("120 BPM stays within 119..121 with a full map and alternating learned CCs", bpm_min >= 119 && bpm_max <= 121 && song.playing);
    int phase_ok = 1;
    for (uint32_t k = 1; k < NTRK; k++)
        phase_ok &= trk[k].seq_idx == trk[0].seq_idx && trk[k].seq_pos == trk[0].seq_pos;
    bad += check("all four tracks retain exactly the same sequencer phase under CC load", phase_ok);
    bad += check("ANALOG and DRUM-X learned parameters receive their final CC values", trk[0].p[P_E4] == (int16_t)((cc_seen - 2u) & 127u) && dx_get(3, decay_b) == (int32_t)((cc_seen - 1u) & 127u));
    printf("120 BPM input: %d..%d BPM; sequence step %u, phase %u\n", bpm_min, bpm_max, trk[0].seq_idx, trk[0].seq_pos);
#else
    mi_r = read;
#endif
#if FELUCCA_UART
    bad += check("TRS clock timestamps stay within 1 ms under saturated CC and 4.5 ms audio renders", clock_timing_ok && clocks_seen == clocks_sent);
    bad += check("interleaved realtime preserves every CC, with no DMA/queue drops", cc_ok && cc_seen == cc_sent && !um.drops && !midi_in_overflow && um.bytes == bytes_sent);
    bad += check("TRS is polled at 2 kHz throughout nested audio", rx_polls >= 20000u && rx_polls <= 20020u);
#else
    bad += check("UART-disabled build never polls or queues TRS input", !rx_polls && !mi_w);
#endif
    bad += check("USB MIDI remains deferred during audio while USB audio is serviced", usb_deferred_ok && usb_polls && uac_polls);
    bad += check("panel scan and nested accounting remain active", scans == acks && scans == felucca_dbg.timer_irqs && felucca_dbg.nested > 70000u);
    printf("wire: %u CC messages, %u clock pulses, %u bytes; drops: %u\n", cc_seen, clocks_seen, bytes_sent, um.drops);
    return bad != 0;
}
