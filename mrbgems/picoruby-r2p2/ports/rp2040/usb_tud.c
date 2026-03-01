#include <pico/stdlib.h>
#include <stdio.h>
#include "picoruby/debug.h"

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void
tud_mount_cb(void)
{
  printf("USB mounted!\n");
}

// Invoked when device is unmounted
void
tud_umount_cb(void)
{
  printf("USB unmounted!\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void
tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  printf("USB suspended!\n");
}

// Invoked when usb bus is resumed
void
tud_resume_cb(void)
{
  printf("USB resumed!\n");
}

