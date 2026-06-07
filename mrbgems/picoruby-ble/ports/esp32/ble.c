#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/ble.h"

#include "btstack.h"

#include "ble_common.h"
#include "btstack_owner.h"

// Forward declaration for att_db_byte_length (defined later in this file)
static size_t att_db_byte_length(const uint8_t *db);

static enum BLE_role_t role = BLE_ROLE_NONE;
static btstack_packet_callback_registration_t ble_hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static btstack_timer_source_t heartbeat;

static uint16_t
att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(connection_handle);
  BLE_read_value_t read_value = { .att_handle = att_handle, .data = NULL, .size = 0 };
  if (BLE_read_data(&read_value) < 0) return 0;
  return att_read_callback_handle_blob(
           (const uint8_t *)read_value.data,
           read_value.size,
           offset,
           buffer,
           buffer_size
         );
}

static int
att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(transaction_mode);
  UNUSED(offset);

  if (0 == BLE_write_data(att_handle, (const uint8_t *)buffer, buffer_size)) {
    con_handle = connection_handle;
  }
  return 0;
}

static void
packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  if (packet_type != HCI_EVENT_PACKET) return;
  switch (role) {
    case BLE_ROLE_PERIPHERAL:
      switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
        case HCI_EVENT_DISCONNECTION_COMPLETE:
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        case ATT_EVENT_CAN_SEND_NOW:
          BLE_push_event(packet, size);
          break;
        case SM_EVENT_JUST_WORKS_REQUEST:
          sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
          break;
        default:
          break;
      }
      break;
    case BLE_ROLE_BROADCASTER:
      switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
          BLE_push_event(packet, size);
          break;
        default:
          break;
      }
      break;
    case BLE_ROLE_CENTRAL:
      switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
        case HCI_EVENT_LE_META:
        case GAP_EVENT_ADVERTISING_REPORT:
        case GATT_EVENT_QUERY_COMPLETE:
        case GATT_EVENT_SERVICE_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_NOTIFICATION:
        case GATT_EVENT_INDICATION:
        case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
          BLE_push_event(packet, size);
          break;
        default:
          break;
      }
      break;
    case BLE_ROLE_OBSERVER:
      switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
        case GAP_EVENT_ADVERTISING_REPORT:
          BLE_push_event(packet, size);
          break;
        default:
          break;
      }
    default:
      break;
  }
}

#define HEARTBEAT_PERIOD_MS 1000

static bool heartbeat_active = false;

static void
heartbeat_handler(struct btstack_timer_source *ts)
{
  BLE_heartbeat();
  // Restart timer
  if (heartbeat_active) {
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
  }
}

// All BTstack APIs must run on the BTstack thread. BLE_init's setup runs from
// picoruby_btstack_ensure_started's task entry, between btstack_init() and
// btstack_run_loop_execute(). Subsequent runtime calls (hci_power_control etc.)
// are dispatched via picoruby_btstack_run_sync.

typedef struct {
  const uint8_t *profile_data;
  int ble_role;
} ble_init_ctx_t;

static void
ble_init_on_btstack_thread(void *ctx_p)
{
  ble_init_ctx_t *p = (ble_init_ctx_t *)ctx_p;

  l2cap_init();
  sm_init();

  sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
  // Phase 1 fix: don't require bonding/SC at advertise time. The previous
  // setting caused BTstack to rotate random private addresses and reject
  // unauthenticated ATT discovery requests, manifesting as
  //   - duplicate device entries in scanners (public 44:.. + random 11:..)
  //   - GATT connect OK but "No Services Found"
  // (2026-05-15 Phase 1 fix.)
  sm_set_authentication_requirements(0);
  sm_set_secure_connections_only_mode(false);
  gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_OFF);

  role = p->ble_role;

  switch (role) {
    case BLE_ROLE_PERIPHERAL:
      att_server_init(p->profile_data, att_read_callback, att_write_callback);
      att_server_register_packet_handler(packet_handler);
      break;
    case BLE_ROLE_BROADCASTER:
      att_server_register_packet_handler(packet_handler);
      break;
    case BLE_ROLE_OBSERVER:
    case BLE_ROLE_CENTRAL:
      sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
      att_server_init(NULL, NULL, NULL);
      gatt_client_init();
      break;
    default:
      break;
  }

  ble_hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&ble_hci_event_callback_registration);

  sm_event_callback_registration.callback = &packet_handler;
  sm_add_event_handler(&sm_event_callback_registration);

  heartbeat.process = &heartbeat_handler;

  // Power on the HCI controller from inside btstack_task before run_loop_execute,
  // matching the Phase 0 btstack_smoke ordering. Later Ruby BLE_hci_power_control
  // calls become no-ops since BTstack tracks power state internally.
  hci_power_control(HCI_POWER_ON);
}

// BTstack att_db_util format byte length walk.
// Layout: [version:1B][entry...][terminator:0x0000]
// Each entry starts with a 2-byte LE size field that includes itself.
// att_server_init() keeps the pointer indefinitely, so we make a port-owned
// copy here — the Ruby String holding the original bytes can be GC'd safely
// once BLE_init returns. (2026-05-15 Phase 1 fix: GATT "No Services Found"
// after connect was caused by the dangling original pointer.)
static size_t
att_db_byte_length(const uint8_t *db)
{
  if (!db) return 0;
  size_t pos = 1; // skip version byte
  while (pos < 16384) { // sanity cap
    uint16_t entry_size = (uint16_t)db[pos] | ((uint16_t)db[pos + 1] << 8);
    if (entry_size == 0) break;
    pos += entry_size;
  }
  return pos + 2; // include 0x0000 terminator
}

static uint8_t *owned_profile_data = NULL;

int
BLE_init(const uint8_t *profile_data, int ble_role)
{
  const uint8_t *use_profile = NULL;
  if (profile_data) {
    size_t len = att_db_byte_length(profile_data);
    uint8_t *copy = (uint8_t *)malloc(len);
    if (!copy) return -1;
    memcpy(copy, profile_data, len);
    if (owned_profile_data) free(owned_profile_data);
    owned_profile_data = copy;
    use_profile = copy;
  }
  ble_init_ctx_t ctx = { .profile_data = use_profile, .ble_role = ble_role };
  picoruby_btstack_ensure_started(ble_init_on_btstack_thread, &ctx);
  return 0;
}

static void
ble_hci_power_control_on_btstack_thread(void *ctx_p)
{
  uint8_t power_mode = (uint8_t)(uintptr_t)ctx_p;
  hci_power_control(power_mode);
  if (power_mode == HCI_POWER_ON) {
    heartbeat_active = true;
    btstack_run_loop_remove_timer(&heartbeat);
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);
  } else {
    heartbeat_active = false;
  }
}

void
BLE_hci_power_control(uint8_t power_mode)
{
  picoruby_btstack_run_sync(ble_hci_power_control_on_btstack_thread,
                            (void *)(uintptr_t)power_mode);
}

void
BLE_gap_local_bd_addr(uint8_t *local_addr)
{
  gap_local_bd_addr(local_addr);
}

uint8_t
BLE_discover_primary_services(uint16_t conn_handle)
{
  return gatt_client_discover_primary_services(&packet_handler, conn_handle);
}

uint8_t
BLE_discover_characteristics_for_service(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
  gatt_client_service_t service = {
    .start_group_handle = start_handle,
    .end_group_handle = end_handle,
    .uuid16 = 0,
    .uuid128 = { 0 }
  };
  return gatt_client_discover_characteristics_for_service(&packet_handler, conn_handle, &service);
}

uint8_t
BLE_read_value_of_characteristic_using_value_handle(uint16_t conn_handle, uint16_t value_handle)
{
  return gatt_client_read_value_of_characteristic_using_value_handle(&packet_handler, conn_handle, value_handle);
}

uint8_t
BLE_discover_characteristic_descriptors(uint16_t conn_handle, uint16_t value_handle, uint16_t end_handle)
{
  gatt_client_characteristic_t characteristic = {
    .start_handle = 0,
    .value_handle = value_handle,
    .end_handle = end_handle,
    .properties = 0,
    .uuid16 = 0,
    .uuid128 = { 0 }
  };
  return gatt_client_discover_characteristic_descriptors(&packet_handler, conn_handle, &characteristic);
}

uint8_t
BLE_write_value_of_characteristic_without_response(uint16_t conn_handle, uint16_t value_handle, const uint8_t *data, uint16_t size)
{
  return gatt_client_write_value_of_characteristic_without_response(conn_handle, value_handle, size, (uint8_t *)data);
}

uint8_t
BLE_write_characteristic_descriptor_using_descriptor_handle(uint16_t conn_handle, uint16_t descriptor_handle, const uint8_t *data, uint16_t size)
{
  return gatt_client_write_characteristic_descriptor_using_descriptor_handle(&packet_handler, conn_handle, descriptor_handle, size, (uint8_t *)data);
}
