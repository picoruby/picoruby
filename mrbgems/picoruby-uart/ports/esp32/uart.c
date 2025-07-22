#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

#include "../../include/uart.h"

#define PICORUBY_UART_ESP32_UART0 (UART_NUM_0)
#define PICORUBY_UART_ESP32_UART1 (UART_NUM_1)
#ifdef UART_NUM_2
#define PICORUBY_UART_ESP32_UART2 (UART_NUM_2)
#endif
#define RECEIVE_BUFF_SIZE         (128)
#define QUEUE_LENGTH              (20)
#define STACK_SIZE                (4096)
#define PRIORITY                  (12)

typedef struct uart_context {
  int unit_num;
  QueueHandle_t queue;
  RingBuffer* buff;
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
      case UART_DATA:
        uart_read_bytes(context->unit_num, buff, event.size > RECEIVE_BUFF_SIZE ? RECEIVE_BUFF_SIZE : event.size, portMAX_DELAY);
        for(size_t i = 0; i < event.size; i++) {
          UART_pushBuffer(context->buff, buff[i]);
        }
        break;
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
    return PICORUBY_UART_ESP32_UART0;
  }
  if(strcmp(name, "ESP32_UART1") == 0) {
    return PICORUBY_UART_ESP32_UART1;
  }
#ifdef PICORUBY_UART_ESP32_UART2
  if(strcmp(name, "ESP32_UART2") == 0) {
    return PICORUBY_UART_ESP32_UART2;
  }
#endif
  return UART_ERROR_INVALID_UNIT;
}


void
UART_init(int unit_num, uint32_t txd_pin, uint32_t rxd_pin, RingBuffer *ring_buffer)
{
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  ESP_ERROR_CHECK(uart_driver_install(unit_num, ring_buffer->size, 0, QUEUE_LENGTH, &contexts[unit_num].queue, 0));
  ESP_ERROR_CHECK(uart_param_config(unit_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(unit_num, txd_pin, rxd_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  contexts[unit_num].unit_num = unit_num;
  contexts[unit_num].buff = ring_buffer;

  char task_name[32];
  sprintf(task_name, "uart_event_task_%d", unit_num);
  xTaskCreate(uart_event_task, task_name, STACK_SIZE, (void*)&contexts[unit_num], PRIORITY, NULL);
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
  ESP_ERROR_CHECK(
    uart_write_bytes(unit_num, (const char *)src, len)
  );
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
  size_t len = 0;
  ESP_ERROR_CHECK(
    len = uart_read_bytes(unit_num, dst, maxlen, 0)
  );
  return len;
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
