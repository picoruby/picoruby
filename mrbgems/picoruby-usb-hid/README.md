
## nRF52 notes

- Built on Nordic's `app_usbd` HID class, not TinyUSB.
- Keyboard, mouse and consumer control share **one** HID interface and are
  told apart by report ID, where the RP2040 port gives each an interface of
  its own. nRF52840 has eight IN endpoints and R2P2 already spends four on
  two CDC ACM pairs and MSC, so three more would not fit.
- A consequence of sharing: `ready?` is per-endpoint. A mouse report offered
  while a key report is still in flight is refused, and the send returns
  false rather than queueing.
- The firmware has to append the class to its own USB instance. The port
  exposes it as `picorb_usb_hid_class_inst()`; the product must also set
  `APP_USBD_HID_ENABLED`, `APP_USBD_HID_GENERIC_ENABLED` and
  `APP_USBD_HID_REPORT_IDLE_TABLE_SIZE` in its `sdk_config.h` and build the
  SDK's HID sources. None of that can be done from a gem port.
- The interface number comes from `PICORB_USB_HID_INTERFACE`, which the
  product defines so the number does not collide with its own classes.
- IN endpoint 7 is claimed. The LED output report arrives on the control
  pipe, so no OUT endpoint is taken.
