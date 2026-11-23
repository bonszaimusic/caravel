/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of firmware/src/usb_update.c: the Classic-protocol USB update command (F0 22 24 35 7F F7, sent by the
 * Firmware Hub and the frozen Classic installer) only ASKS. Nothing is started without OCT+ (usb_update_yes), the
 * dialog is taken back when unanswered, and a late OCT+ arms the unit for the host's next command. The UI
 * and the clock are stubs; usb.c is the real SysEx parser. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/src/usb.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_ms++; }

enum { CF_NONE, CF_USB_UPDATE = 15 };
static struct { uint8_t confirm, menu, layer, uboot, force; } ui;
static uint32_t flash_ok = 1, uxl_play, wakes;   /* (fm1_ms comes from usb.c) */
static char last_msg[40];
static int name_open_flag;
#define UXL_PLAY (uxl_play)
static int name_on(void) { return name_open_flag; }
static void scr_wake_now(void) { wakes++; }
static void ui_message(const char *m) { strncpy(last_msg, m, sizeof last_msg - 1); }
static void confirm_open(uint32_t kind, uint32_t trk) { (void)trk; ui.confirm = (uint8_t)kind; ui.force = 1; }
#include "../firmware/src/usb_update.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void feed(const uint8_t *p, uint32_t n) { while (n--) sysex_byte(*p++); }
static const uint8_t CLASSIC_UP[6] = {0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7};
static const uint8_t CLASSIC_REC[6] = {0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7};

/* what main.c does each loop pass */
static void loop_pass(void)
{
    if (usb.ota_classic_req) {
        usb.ota_classic_req = 0;
        usb_update_classic_request();
    }
    usb_update_service();
}
static void host_asks(void)
{
    feed(CLASSIC_UP, sizeof CLASSIC_UP);
    loop_pass();
}
/* OCT+ on the dialog, as ui_input.c does it */
static void oct_plus(void)
{
    uint32_t kind = ui.confirm;
    ui.confirm = 0;
    if (kind == CF_USB_UPDATE)
        usb_update_yes();
}
static void reset(void)
{
    memset(&usb, 0, sizeof usb);
    memset(&ui, 0, sizeof ui);
    memset(&usbupd, 0, sizeof usbupd);
    flash_ok = 1;
    uxl_play = 0;
    name_open_flag = 0;
    last_msg[0] = 0;
    fm1_ms += 1000000u;
}

int main(void)
{
    reset();
    feed(CLASSIC_UP, sizeof CLASSIC_UP);
    check("Classic upgrade command sets only ota_classic_req", usb.ota_classic_req && !usb.ota_req && !sx_ready);
    reset();
    feed(CLASSIC_REC, sizeof CLASSIC_REC);
    check("Classic recovery command is still ignored", !usb.ota_classic_req && !usb.ota_req && !usb.uboot_req);

    reset();
    host_asks();
    check("idle unit: the dialog opens", ui.confirm == CF_USB_UPDATE && wakes > 0);
    check("nothing starts without OCT+", !usb.ota_req);
    fm1_ms += 3000;
    oct_plus();
    check("OCT+ within the host's wait starts the update", usb.ota_req == 1 && !usbupd.armed_end);

    reset();
    host_asks();
    fm1_ms += 5000;
    ui.confirm = 0;                                     /* OCT-: the dialog is gone */
    check("OCT-: nothing starts", !usb.ota_req && !usbupd.armed_end);
    host_asks();
    check("OCT- does not stop the next question", ui.confirm == CF_USB_UPDATE);

    reset();
    host_asks();
    fm1_ms += 7000;
    host_asks();                                        /* the host retries: only the time moves */
    check("a second command while asking does not stack dialogs", ui.confirm == CF_USB_UPDATE && !usb.ota_req);
    fm1_ms += 7000;                                     /* 14 s after the first, 7 s after the latest */
    oct_plus();
    check("OCT+ counts from the latest command", usb.ota_req == 1);

    reset();
    host_asks();
    fm1_ms += 20000;                                    /* the host gave up long ago */
    oct_plus();
    check("late OCT+ does not start a session nobody serves", !usb.ota_req && usbupd.armed_end);
    check("late OCT+ tells the user to ask again", strcmp(last_msg, "PRESS INSTALL AGAIN") == 0);
    fm1_ms += 10000;
    host_asks();
    check("armed: the next command starts at once, no second dialog", usb.ota_req == 1 && !ui.confirm && !usbupd.armed_end);
    usb.ota_req = 0;
    host_asks();
    check("armed state is used up after one start", ui.confirm == CF_USB_UPDATE && !usb.ota_req);

    reset();
    host_asks();
    fm1_ms += 20000;
    oct_plus();
    fm1_ms += USBUPD_ARM_MS + 1000u;
    host_asks();
    check("armed state expires: the dialog asks again", ui.confirm == CF_USB_UPDATE && !usb.ota_req);

    reset();
    host_asks();
    fm1_ms += USBUPD_ASK_MS - 1u;
    loop_pass();
    check("dialog stays up almost a minute", ui.confirm == CF_USB_UPDATE);
    fm1_ms += 2;
    loop_pass();
    check("unanswered dialog is taken back, nothing starts", !ui.confirm && !usb.ota_req &&
          strcmp(last_msg, "USB UPDATE CANCELLED") == 0);

    reset();
    ui.menu = 1;
    host_asks();
    check("menu open: no dialog, a hint", !ui.confirm && !usb.ota_req && strcmp(last_msg, "LEAVE MENU FIRST") == 0);
    reset();
    name_open_flag = 1;
    host_asks();
    check("NAME screen open: no dialog", !ui.confirm && !usb.ota_req);
    reset();
    uxl_play = 1;
    host_asks();
    check("PLAY view: the dialog opens as in BUILD, nothing starts", ui.confirm == CF_USB_UPDATE && !usb.ota_req);
    fm1_ms += 2000;
    oct_plus();
    check("PLAY view: OCT+ starts the update", usb.ota_req == 1);
    reset();
    ui.confirm = 3;                                     /* another dialog (CLEAR TRACK ...) */
    host_asks();
    check("another dialog is never replaced", ui.confirm == 3 && !usb.ota_req &&
          strcmp(last_msg, "ANSWER DIALOG FIRST") == 0);
    reset();
    flash_ok = 0;
    host_asks();
    check("no flash: nothing is asked, nothing starts", !ui.confirm && !usb.ota_req);
    reset();
    flash_ok = 1;
    host_asks();
    flash_ok = 0;
    oct_plus();
    check("flash gone before OCT+: nothing starts", !usb.ota_req);

    reset();
    feed((const uint8_t *)"\xF0\x7D\x43\x41\x7F\x12\x02\xF7", 8);
    check("protocol 18 upgrade still starts directly (Caravel's own installer)", usb.ota_req && !usb.ota_classic_req);

    printf(fails ? "USB UPDATE TEST FAILED (%d)\n" : "usb_update: all ok\n", fails);
    return fails != 0;
}
