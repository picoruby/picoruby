#include "../../include/hal.h"
#include "../../include/machine.h"
#include "../../include/ringbuffer.h"
#include "../../../picoruby-io-console/include/io-console.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include "esp_sleep.h"
#include "esp_timer.h"
#include "hal/efuse_hal.h"
#include "rom/ets_sys.h"

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "hal/usb_serial_jtag_ll.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#endif

#define ESP32_MSEC_PER_TICK       (10)
#define ESP32_TIMER_UNIT_PER_SEC  (1000000)

#ifdef MRBC_NO_TIMER
  #error "MRBC_NO_TIMER is not supported"
#endif

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
#endif

static esp_timer_handle_t periodic_timer;
volatile int sigint_status = 0; /* MACHINE_SIG_NONE */

/*-------------------------------------
 *
 * stdin RingBuffer
 *
 *------------------------------------*/

#ifndef PICORB_STDIN_BUFFER_SIZE
#define PICORB_STDIN_BUFFER_SIZE 1024
#endif

static uint8_t stdin_buf_mem[sizeof(RingBuffer) + PICORB_STDIN_BUFFER_SIZE]
  __attribute__((aligned(4)));
static RingBuffer *stdin_rb = (RingBuffer *)stdin_buf_mem;

bool
picorb_hal_stdin_push(uint8_t ch)
{
  if (!io_raw_q()) {
    if (ch == 3) {
      sigint_status = MACHINE_SIGINT_RECEIVED;
      return true;
    }
    if (ch == 26) {
      sigint_status = MACHINE_SIGTSTP_RECEIVED;
      return true;
    }
  }
  return RingBuffer_push(stdin_rb, ch);
}

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG

static TaskHandle_t stdin_task_handle = NULL;

static void IRAM_ATTR
usb_serial_jtag_rx_isr(void *arg)
{
  (void)arg;
  usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT);
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(stdin_task_handle, &woken);
  portYIELD_FROM_ISR(woken);
}

static void
stdin_reader_task(void *arg)
{
  (void)arg;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    uint8_t buf[64];
    uint32_t len = usb_serial_jtag_ll_read_rxfifo(buf, sizeof(buf));
    for (uint32_t i = 0; i < len; i++) {
      if (!picorb_hal_stdin_push(buf[i])) break;
    }
  }
}

#else /* !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG */

static void
stdin_reader_task(void *arg)
{
  (void)arg;
  for (;;) {
    int c;
    while ((c = fgetc(stdin)) != EOF) {
      if (!picorb_hal_stdin_push((uint8_t)c)) break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

#endif /* CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG */

static void
alarm_handler(void *arg)
{
#if defined(PICORB_VM_MRUBYC)
  picorb_tick();
#else
  picorb_tick(mrb_);
#endif
}

void
#if defined(PICORB_VM_MRUBY)
picorb_hal_init(mrb_state *mrb)
#elif defined(PICORB_VM_MRUBYC)
picorb_hal_init(void)
#endif
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = (mrb_state *)mrb;
#endif
  esp_timer_create_args_t timer_create_args;
  timer_create_args.callback = &alarm_handler;
  timer_create_args.arg = NULL;
  timer_create_args.dispatch_method = ESP_TIMER_TASK;
  timer_create_args.name = "mrbc_tick_timer";

  esp_timer_create(&timer_create_args, &periodic_timer);
#if defined(PICORB_VM_MRUBY)
  esp_timer_start_periodic(periodic_timer, MRB_TICK_UNIT * 1000);
#else
  esp_timer_start_periodic(periodic_timer, MRBC_TICK_UNIT * 1000);
#endif

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  Machine_delay_ms(3000);
#endif

  RingBuffer_init(stdin_rb, PICORB_STDIN_BUFFER_SIZE);
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  {
    TaskHandle_t h;
    xTaskCreate(stdin_reader_task, "stdin_reader", 2048, NULL, tskIDLE_PRIORITY + 2, &h);
    stdin_task_handle = h;
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT);
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT);
    esp_intr_alloc(ETS_USB_SERIAL_JTAG_INTR_SOURCE, ESP_INTR_FLAG_IRAM,
                   usb_serial_jtag_rx_isr, NULL, NULL);
  }
#else
  xTaskCreate(stdin_reader_task, "stdin_reader", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
#endif
}

#if defined(PICORB_VM_MRUBY)
void
picorb_hal_final(mrb_state *mrb)
{
  (void)mrb;
}
#endif

void
picorb_hal_enable_irq(void)
{
  portENABLE_INTERRUPTS();
}

void
picorb_hal_disable_irq(void)
{
  portDISABLE_INTERRUPTS();
}

void
#if defined(PICORB_VM_MRUBYC)
picorb_hal_idle_cpu()
#elif defined(PICORB_VM_MRUBY)
picorb_hal_idle_cpu(mrb_state *mrb)
#endif
{
  vTaskDelay(1);
}

#if defined(PICORB_VM_MRUBY)
void
picorb_hal_sleep_us(mrb_state *mrb, mrb_int usec)
{
  (void)mrb;
  ets_delay_us((uint32_t)usec);
}
#endif

int
picorb_hal_write(int fd, const void *buf, int nbytes)
{
  FILE *stream = (fd == 1) ? stdout : stderr;
  for (int i = 0 ; i < nbytes ; i++) {
    fputc(((char*)buf)[i], stream);
  }
  fflush(stream);
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  usb_serial_jtag_ll_txfifo_flush();
#endif
  return nbytes;
}

int picorb_hal_flush(int fd) {
  FILE *stream = (fd == 1) ? stdout : stderr;
  int ret = fflush(stream);
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  usb_serial_jtag_ll_txfifo_flush();
#endif
  return ret;
}


int
picorb_hal_read_available(void)
{
  return (RingBuffer_data_size(stdin_rb) > 0) ? 1 : 0;
}

int
picorb_hal_getchar(void)
{
  if (sigint_status == MACHINE_SIGINT_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 3;
  }
  if (sigint_status == MACHINE_SIGTSTP_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 26;
  }

  uint8_t ch;
  if (RingBuffer_pop(stdin_rb, &ch)) {
    return (int)ch;
  }
  return HAL_GETCHAR_NODATA;
}

void
picorb_hal_abort(const char *s)
{
  if(s) {
    picorb_hal_write(1, s, strlen(s));
  }

  abort();
}


/*-------------------------------------
 *
 * USB
 *
 *------------------------------------*/

void
Machine_tud_task(void)
{
  /* Not required for ESP32 */
}

bool
Machine_tud_mounted_q(void)
{
  /* Not required for ESP32 */
  return 0;
}


/*-------------------------------------
 *
 * RTC
 *
 *------------------------------------*/

/*
 * deep_sleep doesn't work yet
 */
void
Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high)
{
}

void
Machine_sleep(uint32_t seconds)
{
  esp_sleep_enable_timer_wakeup(seconds * ESP32_TIMER_UNIT_PER_SEC);
  esp_light_sleep_start();
}

void
Machine_delay_ms(uint32_t ms)
{
  vTaskDelay(ms / ESP32_MSEC_PER_TICK);
}

/*
 * busy_wait_ms doesn't work yet
 */
void
Machine_busy_wait_ms(uint32_t ms)
{
  ets_delay_us(1000 * ms);
}

void
Machine_busy_wait_us(uint32_t us)
{
  ets_delay_us(us);
}

bool
Machine_get_unique_id(char *id_str)
{
  uint8_t mac[6];
  efuse_hal_get_mac(mac);
  sprintf(id_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

uint32_t
Machine_stack_usage(void)
{
  // Not implemented
  return 0;
}

bool
Machine_set_hwclock(const struct timespec *ts)
{
  clock_settime(CLOCK_REALTIME, ts);
  return true;
}

bool
Machine_get_hwclock(struct timespec *ts)
{
  clock_gettime(CLOCK_REALTIME, ts);
  return true;
}

void
Machine_exit(int status)
{
  (void)status; // no-op
}

uint64_t
Machine_uptime_us(void)
{
  return (uint64_t)esp_timer_get_time();
}
