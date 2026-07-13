#ifndef USB_CDC_MIDI_DEFINED_H_
#define USB_CDC_MIDI_DEFINED_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool usb_cdc_midi_connected(void);
size_t usb_cdc_midi_write(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
