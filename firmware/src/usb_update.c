/* SPDX-License-Identifier: GPL-3.0-only */
/* The Classic-protocol USB update (FELUCCA_OTA): the Firmware Hub and the frozen Classic installer send
 * F0 22 24 35 7F F7. Caravel 1.0 and 1.1 dropped that command so that no Classic client could replace a 1.x
 * firmware unasked (a 1.x project does not open in an older version, and the Classic installer would put an older
 * package on the unit). It is accepted again, but it only ASKS: the unit shows the OCT- / OCT+ dialog, and
 * nothing is staged, erased or written without OCT+ (in BUILD and in PLAY; not over MENU, NAME or another dialog). The update itself is the unchanged ota_session(); Caravel's
 * own installer (protocol 18) keeps its direct path.
 *
 * The Hub gives up about 10 s after its command (2 s wait, then 8 s without a read request). OCT+ within
 * USBUPD_WAIT_MS of the last command starts the update at once. A later OCT+ arms the unit for USBUPD_ARM_MS:
 * the host's next command (Install again) then starts it without asking twice. An unanswered dialog
 * is taken back after USBUPD_ASK_MS. */
#if FELUCCA_OTA
#define USBUPD_WAIT_MS 8000u
#define USBUPD_ARM_MS 60000u
#define USBUPD_ASK_MS 60000u
static struct {
    uint32_t req_t, ask_end, armed_end;
} usbupd;

static int usbupd_before(uint32_t end, uint32_t now) { return (int32_t)(end - now) > 0; }

static void usb_update_classic_request(void)
{
    uint32_t now = fm1_ms;
    if (usbupd.armed_end && usbupd_before(usbupd.armed_end, now)) {   /* OCT+ was pressed a moment ago */
        usbupd.armed_end = 0;
        if (flash_ok)
            usb.ota_req = 1;
        return;
    }
    usbupd.armed_end = 0;
    usbupd.req_t = now;
    if (ui.confirm == CF_USB_UPDATE)                    /* the dialog is up: only the time of the latest command */
        return;
    if (!flash_ok)
        return;                                         /* (no flash: ota_session() is never entered either) */
    scr_wake_now();
    if (ui.confirm) {
        ui_message("ANSWER DIALOG FIRST");
    } else if (ui.menu || name_on() || ui.layer || ui.uboot) {
        ui_message("LEAVE MENU FIRST");
    } else {                                            /* BUILD and PLAY alike: confirm_open's dialog draws over both */
        confirm_open(CF_USB_UPDATE, 0);
        usbupd.ask_end = now + USBUPD_ASK_MS;
    }
}

/* OCT+ on the dialog (ui_input.c) */
static void usb_update_yes(void)
{
    uint32_t now = fm1_ms;
    if (!flash_ok)
        return;
    if (now - usbupd.req_t <= USBUPD_WAIT_MS) {
        usb.ota_req = 1;                                /* the host is still waiting: ota_session() from the main loop */
    } else {
        usbupd.armed_end = now + USBUPD_ARM_MS;         /* the host has given up: it must ask again */
        ui_message("PRESS INSTALL AGAIN");
    }
}

static void usb_update_service(void)
{
    if (ui.confirm == CF_USB_UPDATE && !usbupd_before(usbupd.ask_end, fm1_ms)) {
        ui.confirm = 0;
        ui.force = 1;
        ui_message("USB UPDATE CANCELLED");
    }
}
#endif
