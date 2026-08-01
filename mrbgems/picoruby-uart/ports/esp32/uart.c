#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

#include "../../include/uart.h"

#define PICORB_UART_ESP32_UART0 (UART_NUM_0)
#define PICORB_UART_ESP32_UART1 (UART_NUM_1)
#ifdef UART_NUM_2
#define PICORB_UART_ESP32_UART2 (UART_NUM_2)
#endif
#define RECEIVE_BUFF_SIZE         (128)
#define QUEUE_LENGTH              (20)
#define STACK_SIZE                (4096)
#define PRIORITY                  (12)

typedef struct uart_context {
  int unit_num;
  QueueHandle_t queue;
  bool producer_started;
} uart_context_t;
static uart_context_t contexts[UART_NUM_MAX];

static void uart_event_task(void* pvParameters)
{
  uart_context_t* context = (uart_context_t*)pvParameters;
  uart_event_t event;
  uint8_t buff[RECEIVE_BUFF_SIZE];

  for(;;) {
    if(xQueueReceive(context->queue, (void*)&event, (TickType_t)portMAX_DELAY)) {
      switch (event.type) {
      case UART_DATA: {
        /* event.size may exceed the staging buffer, so take it in
           chunks and push exactly what was read. Reading up to 128 and
           then pushing event.size bytes, as this used to, walks off the
           end of buff and leaves the rest of the event in the driver.

           The timeout is zero on purpose. uart_read_bytes() waits until
           the requested length is satisfied and releases the RX mutex
           between chunks, so a blocking read here can outlive its data:
           clear_rx_buffer() calls uart_flush_input(), and a flush that
           lands mid-drain leaves this task waiting forever for bytes
           that were just discarded. event.size is only a snapshot of
           what had arrived when the event was queued, so treat it as an
           upper bound and stop as soon as the driver has no more. */
        RingBuffer* rx = UART_unit_rx(context->unit_num);
        size_t remaining = event.size;
        while(0 < remaining) {
          size_t want = remaining < RECEIVE_BUFF_SIZE ? remaining : RECEIVE_BUFF_SIZE;
          int len = uart_read_bytes(context->unit_num, buff, want, 0);
          if(len <= 0) {
            break;
          }
          /* Drain the driver even with no ring; only the push is skipped. */
          if(rx) {
            for(int i = 0; i < len; i++) {
              UART_pushBuffer(rx, buff[i]);
            }
          }
          remaining -= (size_t)len;
        }
        break;
      }
      default:
        break;
      }
    }
  }

  vTaskDelete(NULL);
}

int
UART_unit_name_to_unit_num(const char *name)
{
  if(strcmp(name, "ESP32_UART0") == 0) {
    return PICORB_UART_ESP32_UART0;
  }
  if(strcmp(name, "ESP32_UART1") == 0) {
    return PICORB_UART_ESP32_UART1;
  }
#ifdef PICORB_UART_ESP32_UART2
  if(strcmp(name, "ESP32_UART2") == 0) {
    return PICORB_UART_ESP32_UART2;
  }
#endif
  return UART_ERROR_INVALID_UNIT;
}


void
UART_open(int unit_num, uint32_t txd_pin, uint32_t rxd_pin)
{
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  /* UART_UNIT_MAX only sizes the shared table; this chip is the
     authority on how many units actually exist. */
  if (unit_num < 0 || UART_NUM_MAX <= unit_num) {
    return;
  }

  /* The driver and its RX task are per unit, not per UART.new: installing
     them again would leak one driver and one task per object. The order
     within a first open is unchanged -- install, configure, then start
     the task, so it cannot receive before the pins are set. */
  bool first_open = !contexts[unit_num].producer_started;

  if (first_open) {
    RingBuffer* rx = UART_unit_rx(unit_num);
    if (rx == NULL) {
      return;
    }
    ESP_ERROR_CHECK(uart_driver_install(unit_num, rx->size, 0, QUEUE_LENGTH, &contexts[unit_num].queue, 0));
    contexts[unit_num].unit_num = unit_num;
  }

  ESP_ERROR_CHECK(uart_param_config(unit_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(unit_num, txd_pin, rxd_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  if (first_open) {
    char task_name[32];
    sprintf(task_name, "uart_event_task_%d", unit_num);
    /* Only mark the producer started once it actually is. Setting the
       flag first would leave a unit whose driver is installed but whose
       RX task never ran, and no later open would retry it. */
    if (xTaskCreate(uart_event_task, task_name, STACK_SIZE,
                    (void*)&contexts[unit_num], PRIORITY, NULL) != pdPASS) {
      uart_driver_delete(unit_num);
      ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
      return;
    }
    contexts[unit_num].producer_started = true;
  }
}

uint32_t
UART_set_baudrate(int unit_num, uint32_t baudrate)
{
  ESP_ERROR_CHECK(uart_set_baudrate(unit_num, baudrate));
  return 0;
}

void
UART_set_flow_control(int unit_num, bool cts, bool rts)
{
  ESP_ERROR_CHECK(
    uart_set_hw_flow_ctrl(
      unit_num,
      cts ? UART_HW_FLOWCTRL_CTS : UART_HW_FLOWCTRL_DISABLE,
      rts ? UART_HW_FLOWCTRL_RTS : UART_HW_FLOWCTRL_DISABLE
    )
  );
}

void
UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity)
{
  uart_word_length_t uart_word_length[4] = {
    UART_DATA_5_BITS,  // 5 bits
    UART_DATA_6_BITS,  // 6 bits
    UART_DATA_7_BITS,  // 7 bits
    UART_DATA_8_BITS   // 8 bits
  };
  uart_stop_bits_t uart_stop_bits[3] = {
    UART_STOP_BITS_1,        // 1 stop bit
    UART_STOP_BITS_2          // 2 stop bits
  };
  uart_parity_t uart_parity[3] = {
    UART_PARITY_DISABLE,  // PARITY_NONE
    UART_PARITY_EVEN,     // PARITY_EVEN
    UART_PARITY_ODD       // PARITY_ODD
  };
  if(data_bits >= 5 && data_bits <= 8) {
    ESP_ERROR_CHECK(
      uart_set_word_length(unit_num, uart_word_length[data_bits - 5])
    );
  }
  if(stop_bits >= 1 && stop_bits <= 2) {
    ESP_ERROR_CHECK(
      uart_set_stop_bits(unit_num, uart_stop_bits[stop_bits - 1])
    );
  }
  if(parity <= 2) {
    ESP_ERROR_CHECK(
      uart_set_parity(unit_num, uart_parity[parity])
    );
  }
}

void
UART_set_function(uint32_t pin)
{
  //no-op
}

bool
UART_is_writable(int unit_num)
{
  //no-op
  return true;
}

void
UART_write_blocking(int unit_num, const uint8_t *src, size_t len)
{
  /* uart_write_bytes() answers with a byte count, not an esp_err_t, so
     ESP_ERROR_CHECK on it aborts on success. Only -1 is an error. */
  if (uart_write_bytes(unit_num, (const char *)src, len) < 0) {
    ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
  }
}

bool
UART_is_readable(int unit_num)
{
  size_t buffered_len;
  uart_get_buffered_data_len(unit_num, &buffered_len);
  return (buffered_len > 0);
}

size_t
UART_read_nonblocking(int unit_num, uint8_t *dst, size_t maxlen)
{
  /* Same shape as UART_write_blocking: a byte count, not an esp_err_t. */
  int len = uart_read_bytes(unit_num, dst, maxlen, 0);
  return len < 0 ? 0 : (size_t)len;
}

void
UART_break(int unit_num, uint32_t interval)
{
  ESP_ERROR_CHECK(
    uart_write_bytes_with_break(unit_num, NULL, 0, interval)
  );
}

void
UART_flush(int unit_num)
{
  ESP_ERROR_CHECK(
    uart_wait_tx_done(unit_num, 100)
  );
}

void
UART_clear_rx_buffer(int unit_num)
{
  ESP_ERROR_CHECK(
    uart_flush_input(unit_num)
  );
}

void
UART_clear_tx_buffer(int unit_num)
{
  // no-op
}
