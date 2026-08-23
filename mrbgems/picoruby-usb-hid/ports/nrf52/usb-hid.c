/*
 * nRF52 port: USB HID over Nordic's app_usbd, not TinyUSB.
 *
 * One HID interface carrying three report types, told apart by report ID,
 * where the RP2040 port gives keyboard, mouse and consumer an interface
 * each. That is a deliberate difference, not a shortcut: nRF52840's USBD
 * has eight IN endpoints and R2P2 already spends four of them on two CDC
 * ACM pairs and MSC. Three more interfaces would not fit alongside them.
 * A composite report descriptor costs one endpoint and is what a
 * multi-function HID device normally looks like anyway.
 *
 * Composition stays with the product: this file defines the class and its
 * descriptors, and the firmware appends it to its own USB instance
 * through picorb_usb_hid_class_inst(). The product also has to set
 * APP_USBD_HID_ENABLED and APP_USBD_HID_GENERIC_ENABLED in sdk_config.h
 * and build the SDK's HID sources -- there is no way for a gem port to do
 * either from here.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "app_usbd.h"
#include "app_usbd_hid_generic.h"
#include "app_usbd_hid_types.h"
#include "nrf_drv_usbd.h"

#include "../../include/usb-hid.h"

#define HID_REPORT_ID_KEYBOARD 1
#define HID_REPORT_ID_MOUSE    2
#define HID_REPORT_ID_CONSUMER 3

/* Report ID byte first, then the report itself. */
#define HID_KEYBOARD_REPORT_SIZE 9   /* id + modifier + reserved + 6 keys */
#define HID_MOUSE_REPORT_SIZE    5   /* id + buttons + x + y + wheel */
#define HID_CONSUMER_REPORT_SIZE 3   /* id + usage code, little endian */

#define HID_KEYCODE_MAX 6

/*
 * A composite descriptor: boot-compatible keyboard, a three-button mouse
 * with wheel, and a consumer control page for media keys. Written out
 * rather than assembled from the SDK's per-device macros because those
 * emit no report IDs, which is the whole point of sharing one interface.
 */
#define PICORB_HID_REPORT_DSC() {                                       \
  /* ---- Keyboard, report ID 1 ---------------------------------- */   \
  0x05, 0x01,        /* Usage Page (Generic Desktop)     */             \
  0x09, 0x06,        /* Usage (Keyboard)                 */             \
  0xA1, 0x01,        /* Collection (Application)         */             \
  0x85, HID_REPORT_ID_KEYBOARD,                                         \
  0x05, 0x07,        /*   Usage Page (Keyboard/Keypad)   */             \
  0x19, 0xE0,        /*   Usage Minimum (LeftControl)    */             \
  0x29, 0xE7,        /*   Usage Maximum (Right GUI)      */             \
  0x15, 0x00, 0x25, 0x01,                                               \
  0x75, 0x01, 0x95, 0x08,                                               \
  0x81, 0x02,        /*   Input (Data,Var,Abs) modifiers */             \
  0x95, 0x01, 0x75, 0x08,                                               \
  0x81, 0x03,        /*   Input (Cnst) reserved byte     */             \
  0x05, 0x08,        /*   Usage Page (LEDs)              */             \
  0x19, 0x01, 0x29, 0x05,                                               \
  0x95, 0x05, 0x75, 0x01,                                               \
  0x91, 0x02,        /*   Output (Data,Var,Abs) LEDs     */             \
  0x95, 0x01, 0x75, 0x03,                                               \
  0x91, 0x03,        /*   Output (Cnst) LED padding      */             \
  0x05, 0x07,        /*   Usage Page (Keyboard/Keypad)   */             \
  0x19, 0x00, 0x29, 0xFF,                                               \
  0x15, 0x00, 0x26, 0xFF, 0x00,                                         \
  0x95, HID_KEYCODE_MAX, 0x75, 0x08,                                    \
  0x81, 0x00,        /*   Input (Data,Arr) keycodes      */             \
  0xC0,              /* End Collection                   */             \
                                                                        \
  /* ---- Mouse, report ID 2 ------------------------------------- */   \
  0x05, 0x01, 0x09, 0x02,                                               \
  0xA1, 0x01,                                                           \
  0x85, HID_REPORT_ID_MOUSE,                                            \
  0x09, 0x01,        /*   Usage (Pointer)                */             \
  0xA1, 0x00,        /*   Collection (Physical)          */             \
  0x05, 0x09,        /*     Usage Page (Button)          */             \
  0x19, 0x01, 0x29, 0x05,                                               \
  0x15, 0x00, 0x25, 0x01,                                               \
  0x95, 0x05, 0x75, 0x01,                                               \
  0x81, 0x02,        /*     Input buttons                */             \
  0x95, 0x01, 0x75, 0x03,                                               \
  0x81, 0x03,        /*     Input padding                */             \
  0x05, 0x01,        /*     Usage Page (Generic Desktop) */             \
  0x09, 0x30, 0x09, 0x31,                                               \
  0x15, 0x81, 0x25, 0x7F,                                               \
  0x75, 0x08, 0x95, 0x02,                                               \
  0x81, 0x06,        /*     Input (Data,Var,Rel) X,Y     */             \
  0x09, 0x38,        /*     Usage (Wheel)                */             \
  0x95, 0x01,                                                           \
  0x81, 0x06,        /*     Input (Data,Var,Rel) wheel   */             \
  0xC0, 0xC0,                                                           \
                                                                        \
  /* ---- Consumer control, report ID 3 -------------------------- */   \
  0x05, 0x0C,        /* Usage Page (Consumer)            */             \
  0x09, 0x01,        /* Usage (Consumer Control)         */             \
  0xA1, 0x01,                                                           \
  0x85, HID_REPORT_ID_CONSUMER,                                         \
  0x15, 0x00, 0x26, 0xFF, 0x03,                                         \
  0x19, 0x00, 0x2A, 0xFF, 0x03,                                         \
  0x75, 0x10, 0x95, 0x01,                                               \
  0x81, 0x00,        /*   Input (Data,Arr) usage code    */             \
  0xC0,                                                                  \
}

APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(hid_desc, PICORB_HID_REPORT_DSC());
static const app_usbd_hid_subclass_desc_t *hid_subclass_descs[] = { &hid_desc };

static uint8_t led_state;
/* Nordic's HID class holds a single IN report in flight; a second offered
   before the first completes is refused. Track that rather than guess. */
static volatile bool report_in_flight;

static void hid_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                app_usbd_hid_user_event_t event);

/*
 * Endpoint 7 IN, the last one, so the CDC and MSC assignments the product
 * already made keep theirs. The OUT report arrives on the control pipe,
 * so no OUT endpoint is claimed.
 */
#define HID_ENDPOINT_LIST() (NRF_DRV_USBD_EPIN7)
#define HID_REPORT_IN_QUEUE_SIZE  1
#define HID_REPORT_OUT_MAXSIZE    2
#define HID_REPORT_FEATURE_MAXSIZE 0

APP_USBD_HID_GENERIC_GLOBAL_DEF(m_picorb_hid,
                                PICORB_USB_HID_INTERFACE,
                                hid_user_ev_handler,
                                HID_ENDPOINT_LIST(),
                                hid_subclass_descs,
                                HID_REPORT_IN_QUEUE_SIZE,
                                HID_REPORT_OUT_MAXSIZE,
                                HID_REPORT_FEATURE_MAXSIZE,
                                APP_USBD_HID_SUBCLASS_NONE,
                                APP_USBD_HID_PROTO_GENERIC);

/*
 * Defined after the class instance because it has to name it: the OUT
 * report is read from the instance, and the macro that creates the
 * instance has to name the handler.
 */
static void
hid_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                    app_usbd_hid_user_event_t event)
{
  (void)p_inst;
  switch (event) {
    case APP_USBD_HID_USER_EVT_OUT_REPORT_READY: {
      /* The keyboard LED report: report ID, then the LED bitmap. */
      size_t size = 0;
      const uint8_t *report =
        app_usbd_hid_generic_out_report_get(&m_picorb_hid, &size);
      if (report != NULL && size >= 2 && report[0] == HID_REPORT_ID_KEYBOARD) {
        led_state = report[1];
      }
      break;
    }
    case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
      report_in_flight = false;
      break;
    default:
      break;
  }
}

app_usbd_class_inst_t const *
picorb_usb_hid_class_inst(void)
{
  return app_usbd_hid_generic_class_inst_get(&m_picorb_hid);
}

static bool
send_report(const uint8_t *report, size_t size)
{
  if (app_usbd_hid_generic_in_report_set(&m_picorb_hid, report, size)
        != NRF_SUCCESS) {
    return false;
  }
  report_in_flight = true;
  return true;
}

/*
 * Readiness is per-endpoint, not per-report type: all three reports share
 * one interface and one IN endpoint, so a mouse move offered while a key
 * report is still in flight is refused. The RP2040 port can answer per
 * device because each has an interface of its own.
 */
static bool
hid_ready(void)
{
  return !report_in_flight;
}

bool
usb_hid_keyboard_ready(void)
{
  return hid_ready();
}

bool
usb_hid_keyboard_send(uint8_t modifier, const uint8_t *keycode, uint8_t keycode_count)
{
  uint8_t report[HID_KEYBOARD_REPORT_SIZE] = {
    HID_REPORT_ID_KEYBOARD, modifier, 0,
  };

  if (keycode != NULL && keycode_count > 0) {
    if (keycode_count > HID_KEYCODE_MAX) {
      keycode_count = HID_KEYCODE_MAX;   /* the array in the descriptor */
    }
    memcpy(&report[3], keycode, keycode_count);
  }
  return send_report(report, sizeof(report));
}

bool
usb_hid_keyboard_release_all(void)
{
  uint8_t report[HID_KEYBOARD_REPORT_SIZE] = { HID_REPORT_ID_KEYBOARD };

  return send_report(report, sizeof(report));
}

uint8_t
usb_hid_keyboard_get_led_state(void)
{
  return led_state;
}

bool
usb_hid_mouse_ready(void)
{
  return hid_ready();
}

bool
usb_hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons)
{
  uint8_t report[HID_MOUSE_REPORT_SIZE] = {
    HID_REPORT_ID_MOUSE, buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel,
  };

  return send_report(report, sizeof(report));
}

bool
usb_hid_consumer_ready(void)
{
  return hid_ready();
}

bool
usb_hid_consumer_send(uint16_t usage_code)
{
  uint8_t report[HID_CONSUMER_REPORT_SIZE] = {
    HID_REPORT_ID_CONSUMER,
    (uint8_t)(usage_code & 0xFF),
    (uint8_t)(usage_code >> 8),
  };

  return send_report(report, sizeof(report));
}
