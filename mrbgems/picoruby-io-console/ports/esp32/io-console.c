#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "driver/uart_vfs.h"
#if defined(CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG) || \
    defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
#include "driver/usb_serial_jtag_vfs.h"
#endif

/*-------------------------------------
 *
 *  IO::Console
 *
 *------------------------------------*/

static bool raw_mode = false;
static bool raw_mode_saved = false;
static bool echo_mode = true;
static bool echo_mode_saved = true;

bool
io_raw_q(void)
{
  return raw_mode;
}

void
io_raw_bang(bool nonblock)
{
  (void)nonblock;
  raw_mode_saved = raw_mode;
  raw_mode = true;
  /* Disable line ending conversion on both TX and RX so binary frames are not corrupted */
  uart_vfs_dev_port_set_tx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_LF);
  uart_vfs_dev_port_set_rx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_LF);
#if defined(CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG) || \
    defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
  usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
  usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
#endif
}

void
io_cooked_bang(void)
{
  raw_mode_saved = raw_mode;
  raw_mode = false;
  /* Restore line ending conversion for normal terminal I/O */
  uart_vfs_dev_port_set_tx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
  uart_vfs_dev_port_set_rx_line_endings(
    CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
#if defined(CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG) || \
    defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
  usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
  usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
#endif
}

void
io_echo_eq(bool flag)
{
  echo_mode_saved = echo_mode;
  echo_mode = flag;
}

bool
io_echo_q(void)
{
  return echo_mode;
}

void
io__restore_termios(void)
{
  raw_mode = raw_mode_saved;
  echo_mode = echo_mode_saved;
  if (!raw_mode) {
    uart_vfs_dev_port_set_tx_line_endings(
      CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    uart_vfs_dev_port_set_rx_line_endings(
      CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
#if defined(CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG) || \
    defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
#endif
  }
}
