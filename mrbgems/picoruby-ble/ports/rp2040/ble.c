#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"


#include "gatt.h"

#define ADC_CHANNEL_TEMPSENSOR 4

//#define APP_AD_FLAGS 0x06
//static uint8_t adv_data[] = {
//    // Flags general discoverable
//    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
//    // Name
//    0x17, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'i', 'c', 'o', ' ', '0', '0', ':', '0', '0', ':', '0', '0', ':', '0', '0', ':', '0', '0', ':', '0', '0',
//    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x1a, 0x18,
//};
//static const uint8_t adv_data_len = sizeof(adv_data);

int le_notification_enabled;
hci_con_handle_t con_handle;
uint16_t current_temp;

void
poll_temp(void) {
  adc_select_input(ADC_CHANNEL_TEMPSENSOR);
  uint32_t raw32 = adc_read();
  const uint32_t bits = 12;
  // Scale raw reading to 16 bit value using a Taylor expansion (for 8 <= bits <= 16)
  uint16_t raw16 = raw32 << (16 - bits) | raw32 >> (2 * bits - 16);
  // ref https://github.com/raspberrypi/pico-micropython-examples/blob/master/adc/temperature.py
  const float conversion_factor = 3.3 / (65535);
  float reading = raw16 * conversion_factor;
  // The temperature sensor measures the Vbe voltage of a biased bipolar diode, connected to the fifth ADC channel
  // Typically, Vbe = 0.706V at 27 degrees C, with a slope of -1.721mV (0.001721) per degree. 
  float deg_c = 27 - (reading - 0.706) / 0.001721;
  current_temp = deg_c * 100;
  printf("Write temp %.2f degc\n", deg_c);
}


static uint8_t event_type = 0;
static uint8_t event_state = 0;

uint8_t
BLE_packet_event(void)
{
  return event_type;
}

void
BLE_down_packet_flag(void)
{
  event_type = 0;
}

static void
packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  if (packet_type != HCI_EVENT_PACKET) return;
  event_type = hci_event_packet_get_type(packet);
  event_state = btstack_event_state_get_state(packet);
}

void
BLE_gap_local_bd_addr(uint8_t *local_addr)
{
  gap_local_bd_addr(local_addr);
}

void
BLE_advertise(uint8_t *adv_data, uint8_t adv_data_len)
{
  if (event_state != HCI_STATE_WORKING) return;
  // setup advertisements
  uint16_t adv_int_min = 800;
  uint16_t adv_int_max = 800;
  uint8_t adv_type = 0;
  bd_addr_t null_addr;
  memset(null_addr, 0, 6);
  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
  gap_advertisements_set_data(adv_data_len, adv_data);
  gap_advertisements_enable(1);
  poll_temp();
}

void
BLE_enable_le_notification(void)
{
  le_notification_enabled = 1;
}

void
BLE_notify(void)
{
  att_server_notify(
    con_handle,
    ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE,
    (uint8_t *)&current_temp,
    sizeof(current_temp)
  );
}

/*
void
_bak_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  UNUSED(size);
  UNUSED(channel);
  bd_addr_t local_addr;
  if (packet_type != HCI_EVENT_PACKET) return;

  uint8_t event_type = hci_event_packet_get_type(packet);
  switch (event_type) {
    case BTSTACK_EVENT_STATE:
      if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
      gap_local_bd_addr(local_addr);
      printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));

      // setup advertisements
      uint16_t adv_int_min = 800;
      uint16_t adv_int_max = 800;
      uint8_t adv_type = 0;
      bd_addr_t null_addr;
      memset(null_addr, 0, 6);
      gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
      assert(adv_data_len <= 31); // ble limitation
      gap_advertisements_set_data(adv_data_len, (uint8_t *)adv_data);
      gap_advertisements_enable(1);

      poll_temp();

      break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
      le_notification_enabled = 0;
      break;
    case ATT_EVENT_CAN_SEND_NOW:
      att_server_notify(
        con_handle,
        ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE,
        (uint8_t *)&current_temp,
        sizeof(current_temp)
      );
      break;
    default:
      break;
  }
}
*/

uint16_t
att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(connection_handle);
  if (att_handle != ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_VALUE_HANDLE){
    return 0;
  }
  return att_read_callback_handle_blob(
           (const uint8_t *)&current_temp,
           sizeof(current_temp),
           offset,
           buffer,
           buffer_size
         );
}

int
att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(transaction_mode);
  UNUSED(offset);
  UNUSED(buffer_size);

  if (att_handle != ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_01_CLIENT_CONFIGURATION_HANDLE) return 0;
  le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
  con_handle = connection_handle;
  if (le_notification_enabled) {
    att_server_request_can_send_now_event(con_handle);
  }
  return 0;
}


#define HEARTBEAT_PERIOD_MS 1000

static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static bool heartbeat_on = false;

static void
heartbeat_handler(struct btstack_timer_source *ts)
{
//  static uint32_t counter = 0;
//  counter++;
//
//  // Update the temp every 10s
//  if (counter % 10 == 0) {
//    poll_temp();
//    if (le_notification_enabled) {
//      att_server_request_can_send_now_event(con_handle);
//    }
//  }
//
//  // Invert the led
//  static int led_on = true;
//  led_on = !led_on;
//  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
//

  heartbeat_on = true;
  // Restart timer
  btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
  btstack_run_loop_add_timer(ts);
}

void
BLE_poll_temp(void)
{
  poll_temp();
}

bool
BLE_heartbeat_on_q(void)
{
  return heartbeat_on;
}

void
BLE_heartbeat_off(void)
{
  heartbeat_on = false;
}

bool
BLE_le_notification_enabled_q(void)
{
  return le_notification_enabled;
}

void
BLE_request_can_send_now_event(void)
{
  att_server_request_can_send_now_event(con_handle);
}

void
BLE_cyw43_arch_gpio_put(uint8_t pin, uint8_t value)
{
  cyw43_arch_gpio_put(pin, value);
}

int
BLE_init(void)
{
  // initialize CYW43 driver architecture (will enable BT if/because CYW43_ENABLE_BLUETOOTH == 1)
  if (cyw43_arch_init()) {
    printf("failed to initialise cyw43_arch\n");
    return -1;
  }

  // Initialise adc for the temp sensor
  adc_init();
  adc_select_input(ADC_CHANNEL_TEMPSENSOR);
  adc_set_temp_sensor_enabled(true);

  l2cap_init();
  sm_init();

  att_server_init(profile_data, att_read_callback, att_write_callback);

  // inform about BTstack state
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  // register for ATT event
  att_server_register_packet_handler(packet_handler);

  // set one-shot btstack timer
  heartbeat.process = &heartbeat_handler;
  btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
  btstack_run_loop_add_timer(&heartbeat);
  return 0;
}

void BLE_hci_power_on(void)
{
  // turn on bluetooth!
  hci_power_control(HCI_POWER_ON);
}