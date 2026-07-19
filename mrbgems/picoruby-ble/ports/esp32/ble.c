// ports/esp32/ble.c
//
// NimBLE-based ESP32 port of picoruby-ble.
//
// The Ruby layer speaks BTstack: GattDatabase builds a BTstack att_db_util
// blob with sequentially numbered handles, and mrblib parses BTstack-format
// event packets at fixed byte offsets. This file translates in both
// directions: the blob becomes ble_gatt_svc_def tables (with a blob-handle <->
// NimBLE-handle map), and NimBLE callbacks become BTstack-wire-format packets
// queued through nimble_owner.c.

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/ble.h"
#include "../../include/ble_peripheral.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include "esp_log.h"

#include "ble_common.h"
#include "nimble_owner.h"

static const char *TAG = "prb_ble";

// BTstack event type bytes hardcoded in mrblib (ble_central.rb, examples)
#define EVT_BTSTACK_STATE 0x60
#define EVT_DISCONNECTION_COMPLETE 0x05
#define EVT_LE_META 0x3E
#define EVT_ATT_MTU_EXCHANGE_COMPLETE 0xB5
#define EVT_ATT_CAN_SEND_NOW 0xB7
#define EVT_GAP_ADVERTISING_REPORT 0xDA
#define EVT_GATT_QUERY_COMPLETE 0xA0
#define EVT_GATT_SERVICE_QUERY_RESULT 0xA1
#define EVT_GATT_CHARACTERISTIC_QUERY_RESULT 0xA2
#define EVT_GATT_ALL_DESCRIPTORS_QUERY_RESULT 0xA4
#define EVT_GATT_CHARACTERISTIC_VALUE_QUERY_RESULT 0xA5
#define EVT_GATT_NOTIFICATION 0xA7
#define EVT_GATT_INDICATION 0xA8

#define HCI_STATE_WORKING 2

// Blob flag bits written by Ruby GattDatabase (BTstack att_db_util format)
#define BLOB_FLAG_READ 0x0002
#define BLOB_FLAG_WRITE_NO_RSP 0x0004
#define BLOB_FLAG_WRITE 0x0008
#define BLOB_FLAG_DYNAMIC 0x0100
#define BLOB_FLAG_LONG_UUID 0x0200

#define GATT_PRIMARY_SERVICE_UUID 0x2800
#define GATT_SECONDARY_SERVICE_UUID 0x2801
#define GATT_CHARACTERISTIC_UUID 0x2803
#define GATT_CCCD_UUID 0x2902

// Longest event payload that survives the queue (nimble_owner EVQ_PKT_MAX
// minus the 8-byte value-event header).
#define VALUE_EVENT_DATA_MAX 92

static enum BLE_role_t role = BLE_ROLE_NONE;

// get_le16/put_le16 come from NimBLE's os/endian.h (via ble_hs.h)

// Map a NimBLE uuid to the 16-byte little-endian form BTstack events carry
// (Ruby reverse_128s it back for display/uuid32 extraction).
static void
uuid_to_le128(const ble_uuid_any_t *u, uint8_t out[16])
{
  static const uint8_t base_le[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  switch (u->u.type) {
    case BLE_UUID_TYPE_16:
      memcpy(out, base_le, 16);
      put_le16(out + 12, u->u16.value);
      break;
    case BLE_UUID_TYPE_32:
      memcpy(out, base_le, 16);
      put_le16(out + 12, u->u32.value & 0xffff);
      put_le16(out + 14, u->u32.value >> 16);
      break;
    case BLE_UUID_TYPE_128:
      memcpy(out, u->u128.value, 16);
      break;
    default:
      memset(out, 0, 16);
      break;
  }
}

// ---------------------------------------------------------------------------
// GATT server: blob translation + handle map

#define MAX_ATTRS 48
#define MAX_SVCS 8
#define MAX_CHR_SLOTS (24 + MAX_SVCS)  // characteristic defs + per-service terminators
#define MAX_DSC_SLOTS (16 + 24)        // descriptor defs + per-characteristic terminators
#define MAX_UUIDS (MAX_SVCS + 24 + 16)

typedef struct {
  uint16_t ruby_handle;      // handle Ruby's GattDatabase assigned in the blob
  uint16_t nimble_handle;    // filled at registration
  uint16_t blob_flags;
  const uint8_t *static_value; // points into owned_profile_data
  uint16_t static_len;
  uint16_t cccd_ruby_handle; // characteristic value entries: their CCCD's ruby handle
} attr_map_t;

static attr_map_t attr_map[MAX_ATTRS];
static int attr_map_count = 0;

static struct ble_gatt_svc_def svc_defs[MAX_SVCS + 1];
static struct ble_gatt_chr_def chr_slots[MAX_CHR_SLOTS];
static struct ble_gatt_dsc_def dsc_slots[MAX_DSC_SLOTS];
static ble_uuid_any_t uuid_pool[MAX_UUIDS];
static int svc_count, chr_slot_count, dsc_slot_count, uuid_count;
static bool have_services = false;

static uint8_t *owned_profile_data = NULL;

uint16_t
picoruby_ble_handle_r2n(uint16_t ruby_handle)
{
  for (int i = 0; i < attr_map_count; i++) {
    if (attr_map[i].ruby_handle == ruby_handle) return attr_map[i].nimble_handle;
  }
  return 0;
}

uint16_t
picoruby_ble_handle_n2r(uint16_t nimble_handle)
{
  for (int i = 0; i < attr_map_count; i++) {
    if (attr_map[i].nimble_handle == nimble_handle) return attr_map[i].ruby_handle;
  }
  return 0;
}

static attr_map_t *
find_map_by_nimble(uint16_t nimble_handle)
{
  for (int i = 0; i < attr_map_count; i++) {
    if (attr_map[i].nimble_handle == nimble_handle) return &attr_map[i];
  }
  return NULL;
}

// BTstack att_db_util blob byte length:
// [version:1][entry: 2B LE self-inclusive size ...][terminator 0x0000]
static size_t
att_db_byte_length(const uint8_t *db)
{
  if (!db) return 0;
  size_t pos = 1;
  while (pos < 16384) {
    uint16_t entry_size = get_le16(db + pos);
    if (entry_size == 0) break;
    pos += entry_size;
  }
  return pos + 2;
}

static int
gatt_access_cb(uint16_t conn, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
  (void)attr_handle;
  attr_map_t *e = (attr_map_t *)arg;
  if (e == NULL) return BLE_ATT_ERR_UNLIKELY;

  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
    case BLE_GATT_ACCESS_OP_READ_DSC: {
      const uint8_t *data = e->static_value;
      uint16_t len = e->static_len;
      BLE_read_value_t rv = { .att_handle = e->ruby_handle, .data = NULL, .size = 0 };
      if ((e->blob_flags & BLOB_FLAG_DYNAMIC) && BLE_read_data(&rv) == 0) {
        data = rv.data;
        len = rv.size;
      }
      if (data == NULL || len == 0) return 0;
      return os_mbuf_append(ctxt->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
    case BLE_GATT_ACCESS_OP_WRITE_DSC: {
      static uint8_t buf[256]; // NimBLE host task only; single-threaded access
      uint16_t out_len = 0;
      if (OS_MBUF_PKTLEN(ctxt->om) > sizeof(buf)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
      }
      if (BLE_write_data(e->ruby_handle, buf, out_len) == 0) {
        con_handle = conn;
      }
      return 0;
    }
    default:
      return BLE_ATT_ERR_UNLIKELY;
  }
}

void
picoruby_ble_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
  (void)arg;
  // Characteristic value handles arrive via chr_def.val_handle out-pointers;
  // descriptors have no out-pointer, so recover them here through dsc_def.arg.
  if (ctxt->op == BLE_GATT_REGISTER_OP_DSC && ctxt->dsc.dsc_def->arg) {
    ((attr_map_t *)ctxt->dsc.dsc_def->arg)->nimble_handle = ctxt->dsc.handle;
  }
}

static ble_gatt_chr_flags
props_to_chr_flags(uint8_t props)
{
  ble_gatt_chr_flags f = 0;
  if (props & 0x01) f |= BLE_GATT_CHR_F_BROADCAST;
  if (props & 0x02) f |= BLE_GATT_CHR_F_READ;
  if (props & 0x04) f |= BLE_GATT_CHR_F_WRITE_NO_RSP;
  if (props & 0x08) f |= BLE_GATT_CHR_F_WRITE;
  if (props & 0x10) f |= BLE_GATT_CHR_F_NOTIFY;
  if (props & 0x20) f |= BLE_GATT_CHR_F_INDICATE;
  if (props & 0x40) f |= BLE_GATT_CHR_F_AUTH_SIGN_WRITE;
  return f;
}

static const ble_uuid_t *
alloc_uuid(const uint8_t *buf, uint16_t len)
{
  if (uuid_count >= MAX_UUIDS) return NULL;
  ble_uuid_any_t *u = &uuid_pool[uuid_count];
  if (ble_uuid_init_from_buf(u, buf, len) != 0) return NULL;
  uuid_count++;
  return &u->u;
}

static attr_map_t *
alloc_map(uint16_t ruby_handle)
{
  if (attr_map_count >= MAX_ATTRS) return NULL;
  attr_map_t *e = &attr_map[attr_map_count++];
  memset(e, 0, sizeof(*e));
  e->ruby_handle = ruby_handle;
  return e;
}

// Translate the blob into NimBLE service definition tables.
// Entry layout: [size:2][flags:2][handle:2][uuid:2|16][value...]
static int
parse_att_db(const uint8_t *db, size_t db_len)
{
  attr_map_count = 0;
  svc_count = chr_slot_count = dsc_slot_count = uuid_count = 0;
  memset(svc_defs, 0, sizeof(svc_defs));
  memset(chr_slots, 0, sizeof(chr_slots));
  memset(dsc_slots, 0, sizeof(dsc_slots));

  struct ble_gatt_svc_def *cur_svc = NULL;
  struct ble_gatt_chr_def *cur_chr = NULL;
  attr_map_t *cur_chr_map = NULL;
  uint16_t expected_value_handle = 0;
  const uint8_t *cur_chr_uuid = NULL;
  uint16_t cur_chr_uuid_len = 0;

  size_t pos = 1;
  while (pos + 2 <= db_len) {
    uint16_t entry_size = get_le16(db + pos);
    if (entry_size == 0) break;
    uint16_t flags = get_le16(db + pos + 2);
    uint16_t handle = get_le16(db + pos + 4);
    uint16_t uuid_len = (flags & BLOB_FLAG_LONG_UUID) ? 16 : 2;
    if (entry_size < 6 + uuid_len || pos + entry_size > db_len) {
      ESP_LOGE(TAG, "GATT blob: malformed entry at %u", (unsigned)pos);
      return -1;
    }
    const uint8_t *uuid = db + pos + 6;
    const uint8_t *value = uuid + uuid_len;
    uint16_t value_len = entry_size - 6 - uuid_len;
    uint16_t u16 = (uuid_len == 2) ? get_le16(uuid) : 0;
    pos += entry_size;

    if (u16 == GATT_PRIMARY_SERVICE_UUID || u16 == GATT_SECONDARY_SERVICE_UUID) {
      // close current characteristic and service
      if (cur_chr && cur_chr->descriptors) {
        if (dsc_slot_count >= MAX_DSC_SLOTS) return -1;
        dsc_slot_count++; // zeroed terminator
      }
      cur_chr = NULL;
      cur_chr_map = NULL;
      if (cur_svc) {
        if (chr_slot_count >= MAX_CHR_SLOTS) return -1;
        chr_slot_count++; // zeroed terminator
      }
      if (svc_count >= MAX_SVCS) return -1;
      cur_svc = &svc_defs[svc_count++];
      cur_svc->type = (u16 == GATT_PRIMARY_SERVICE_UUID)
                        ? BLE_GATT_SVC_TYPE_PRIMARY : BLE_GATT_SVC_TYPE_SECONDARY;
      cur_svc->uuid = alloc_uuid(value, value_len);
      cur_svc->characteristics = &chr_slots[chr_slot_count];
      if (cur_svc->uuid == NULL) return -1;
    } else if (u16 == GATT_CHARACTERISTIC_UUID && cur_svc) {
      if (cur_chr && cur_chr->descriptors) {
        if (dsc_slot_count >= MAX_DSC_SLOTS) return -1;
        dsc_slot_count++; // zeroed terminator
      }
      if (value_len < 3 || chr_slot_count >= MAX_CHR_SLOTS) return -1;
      uint8_t props = value[0];
      expected_value_handle = get_le16(value + 1);
      cur_chr_uuid = value + 3;
      cur_chr_uuid_len = value_len - 3;
      cur_chr = &chr_slots[chr_slot_count++];
      cur_chr->uuid = alloc_uuid(value + 3, value_len - 3);
      cur_chr->access_cb = gatt_access_cb;
      cur_chr->flags = props_to_chr_flags(props);
      cur_chr_map = alloc_map(expected_value_handle);
      if (cur_chr->uuid == NULL || cur_chr_map == NULL) return -1;
      cur_chr->arg = cur_chr_map;
      cur_chr->val_handle = &cur_chr_map->nimble_handle;
    } else if (cur_chr_map && handle == expected_value_handle && expected_value_handle != 0
               && uuid_len == cur_chr_uuid_len && memcmp(uuid, cur_chr_uuid, uuid_len) == 0) {
      // the characteristic's value attribute (same uuid as the declaration —
      // a descriptor following a value-less characteristic reuses the handle
      // number but has a different uuid): flags (DYNAMIC) + initial bytes
      cur_chr_map->blob_flags = flags;
      cur_chr_map->static_value = value;
      cur_chr_map->static_len = value_len;
      expected_value_handle = 0;
    } else if (u16 == GATT_CCCD_UUID && cur_chr_map) {
      // NimBLE auto-creates the CCCD; remember Ruby's handle so SUBSCRIBE
      // events can be replayed as CCCD writes into write_values.
      cur_chr_map->cccd_ruby_handle = handle;
    } else if (cur_chr) {
      // ordinary descriptor (0x2901 user description, 0x2900 ext props, ...)
      if (dsc_slot_count >= MAX_DSC_SLOTS) return -1;
      attr_map_t *m = alloc_map(handle);
      if (m == NULL) return -1;
      m->blob_flags = flags;
      m->static_value = value;
      m->static_len = value_len;
      struct ble_gatt_dsc_def *dsc = &dsc_slots[dsc_slot_count];
      if (cur_chr->descriptors == NULL) cur_chr->descriptors = dsc;
      dsc_slot_count++;
      dsc->uuid = alloc_uuid(uuid, uuid_len);
      dsc->att_flags = ((flags & BLOB_FLAG_READ) ? BLE_ATT_F_READ : 0)
                     | ((flags & (BLOB_FLAG_WRITE | BLOB_FLAG_WRITE_NO_RSP)) ? BLE_ATT_F_WRITE : 0);
      dsc->access_cb = gatt_access_cb;
      dsc->arg = m;
      if (dsc->uuid == NULL) return -1;
    } else {
      ESP_LOGW(TAG, "GATT blob: skipping entry handle=%u uuid16=0x%04x", handle, u16);
    }
  }

  // close trailing characteristic/service (terminators are pre-zeroed slots)
  if (cur_chr && cur_chr->descriptors) {
    if (dsc_slot_count >= MAX_DSC_SLOTS) return -1;
    dsc_slot_count++;
  }
  if (cur_svc) {
    if (chr_slot_count >= MAX_CHR_SLOTS) return -1;
    chr_slot_count++;
  }
  have_services = (svc_count > 0);
  return 0;
}

static int
register_services(void)
{
  if (!have_services) return 0;
  int rc = ble_gatts_count_cfg(svc_defs);
  if (rc == 0) rc = ble_gatts_add_svcs(svc_defs);
  if (rc != 0) ESP_LOGE(TAG, "GATT service registration failed: %d", rc);
  return rc;
}

// ---------------------------------------------------------------------------
// Event synthesis

void
picoruby_ble_synth_state_working(void)
{
  uint8_t p[3] = { EVT_BTSTACK_STATE, 1, HCI_STATE_WORKING };
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static void
synth_query_complete(uint16_t conn, uint8_t status)
{
  uint8_t p[5] = { EVT_GATT_QUERY_COMPLETE, 3, 0, 0, status };
  put_le16(p + 2, conn);
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static uint8_t
status_to_u8(uint16_t status)
{
  if (status == 0 || status == BLE_HS_EDONE) return 0;
  return (status & 0xff) ? (uint8_t)status : 0xff;
}

static void
synth_le_connection_complete(uint16_t handle)
{
  struct ble_gap_conn_desc desc;
  uint8_t p[21];
  memset(p, 0, sizeof(p));
  p[0] = EVT_LE_META;
  p[1] = 19;
  p[2] = 0x01; // HCI_SUBEVENT_LE_CONNECTION_COMPLETE
  p[3] = 0;    // status
  put_le16(p + 4, handle);
  if (ble_gap_conn_find(handle, &desc) == 0) {
    p[6] = (desc.role == BLE_GAP_ROLE_MASTER) ? 0 : 1;
    p[7] = desc.peer_id_addr.type;
    memcpy(p + 8, desc.peer_id_addr.val, 6);
    put_le16(p + 14, desc.conn_itvl);
    put_le16(p + 16, desc.conn_latency);
    put_le16(p + 18, desc.supervision_timeout);
  }
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static void
synth_disconnection_complete(uint16_t handle, uint8_t reason)
{
  uint8_t p[6] = { EVT_DISCONNECTION_COMPLETE, 4, 0, 0, 0, reason };
  put_le16(p + 3, handle);
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static void
synth_mtu_exchange_complete(uint16_t conn, uint16_t mtu)
{
  uint8_t p[6] = { EVT_ATT_MTU_EXCHANGE_COMPLETE, 4, 0, 0, 0, 0 };
  put_le16(p + 2, conn);
  put_le16(p + 4, mtu);
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static void
synth_advertising_report(const struct ble_gap_disc_desc *d)
{
  uint8_t p[12 + 31];
  uint8_t dlen = d->length_data > 31 ? 31 : d->length_data;
  p[0] = EVT_GAP_ADVERTISING_REPORT;
  p[1] = 10 + dlen;
  p[2] = d->event_type;
  p[3] = d->addr.type;
  memcpy(p + 4, d->addr.val, 6);
  p[10] = (uint8_t)d->rssi;
  p[11] = dlen;
  if (dlen) memcpy(p + 12, d->data, dlen);
  picoruby_nimble_enqueue_event(p, 12 + dlen, true);
}

// ---------------------------------------------------------------------------
// Shared GAP event callback

int
picoruby_ble_gap_event(struct ble_gap_event *event, void *arg)
{
  (void)arg;
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        con_handle = event->connect.conn_handle;
        if (role == BLE_ROLE_CENTRAL) {
          synth_le_connection_complete(event->connect.conn_handle);
        }
      } else if (role == BLE_ROLE_PERIPHERAL || role == BLE_ROLE_BROADCASTER) {
        picoruby_ble_peripheral_rearm_adv();
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      con_handle = 0xffff;
      if (role == BLE_ROLE_PERIPHERAL) {
        synth_disconnection_complete(event->disconnect.conn.conn_handle,
                                     (uint8_t)event->disconnect.reason);
        picoruby_ble_peripheral_rearm_adv();
      }
      break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
      if (role == BLE_ROLE_PERIPHERAL || role == BLE_ROLE_BROADCASTER) {
        picoruby_ble_peripheral_rearm_adv();
      }
      break;
    case BLE_GAP_EVENT_MTU:
      if (role == BLE_ROLE_PERIPHERAL) {
        synth_mtu_exchange_complete(event->mtu.conn_handle, event->mtu.value);
      }
      break;
    case BLE_GAP_EVENT_SUBSCRIBE: {
      if (role != BLE_ROLE_PERIPHERAL) break;
      attr_map_t *e = find_map_by_nimble(event->subscribe.attr_handle);
      if (e && e->cccd_ruby_handle) {
        uint8_t v[2] = { 0x00, 0x00 };
        if (event->subscribe.cur_notify) v[0] = 0x01;
        else if (event->subscribe.cur_indicate) v[0] = 0x02;
        BLE_write_data(e->cccd_ruby_handle, v, 2);
        con_handle = event->subscribe.conn_handle;
      }
      break;
    }
    case BLE_GAP_EVENT_DISC:
      if (role == BLE_ROLE_CENTRAL || role == BLE_ROLE_OBSERVER) {
        synth_advertising_report(&event->disc);
      }
      break;
    case BLE_GAP_EVENT_NOTIFY_RX: {
      if (role != BLE_ROLE_CENTRAL) break;
      uint16_t vlen = OS_MBUF_PKTLEN(event->notify_rx.om);
      if (vlen > VALUE_EVENT_DATA_MAX) vlen = VALUE_EVENT_DATA_MAX;
      uint8_t p[8 + VALUE_EVENT_DATA_MAX];
      p[0] = event->notify_rx.indication ? EVT_GATT_INDICATION : EVT_GATT_NOTIFICATION;
      p[1] = 6 + vlen;
      put_le16(p + 2, event->notify_rx.conn_handle);
      put_le16(p + 4, event->notify_rx.attr_handle);
      put_le16(p + 6, vlen);
      os_mbuf_copydata(event->notify_rx.om, 0, vlen, p + 8);
      picoruby_nimble_enqueue_event(p, 8 + vlen, false);
      break;
    }
    default:
      break;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Lifecycle

int
BLE_init(const uint8_t *profile_data, int ble_role)
{
  role = (enum BLE_role_t)ble_role;
  if (picoruby_nimble_started()) {
    picoruby_nimble_stop();
  }
  con_handle = 0xffff;
  have_services = false;
  attr_map_count = 0;

  if (owned_profile_data) {
    free(owned_profile_data);
    owned_profile_data = NULL;
  }
  if (profile_data) {
    // Own a copy: NimBLE keeps the def tables (which point into this buffer
    // for static values) and the source Ruby String may be GC'd.
    size_t len = att_db_byte_length(profile_data);
    owned_profile_data = (uint8_t *)malloc(len);
    if (owned_profile_data == NULL) return -1;
    memcpy(owned_profile_data, profile_data, len);
    if (parse_att_db(owned_profile_data, len) < 0) return -1;
  }

  return picoruby_nimble_start(register_services);
}

void
BLE_hci_power_control(uint8_t power_mode)
{
  if (!picoruby_nimble_started()) return;
  if (power_mode == 1) { // HCI_POWER_ON
    picoruby_nimble_heartbeat_enable(true);
    // Ruby's BLE#start waits for this event on every power-on cycle.
    picoruby_ble_synth_state_working();
  } else {
    // Soft off: stop radio activity but keep the host (and connections) up.
    // Ruby power-cycles per BLE#start session; a full stack restart here
    // would tear down connections between scan and connect.
    picoruby_nimble_heartbeat_enable(false);
    switch (role) {
      case BLE_ROLE_PERIPHERAL:
      case BLE_ROLE_BROADCASTER:
        BLE_peripheral_stop_advertise();
        break;
      case BLE_ROLE_CENTRAL:
      case BLE_ROLE_OBSERVER:
        ble_gap_disc_cancel(); // no-op error when not scanning
        break;
      default:
        break;
    }
  }
}

void
BLE_gap_local_bd_addr(uint8_t *local_addr)
{
  uint8_t le[6] = { 0 };
  if (picoruby_nimble_started()) {
    if (ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, le, NULL) != 0) {
      memset(le, 0, sizeof(le));
    }
  }
  for (int i = 0; i < 6; i++) local_addr[i] = le[5 - i]; // printed order
}

// ---------------------------------------------------------------------------
// GATT client (central role)

static uint16_t chr_disc_end_handle;
static bool chr_disc_have_prev;
static struct ble_gatt_chr chr_disc_prev;

static int
disc_svc_cb(uint16_t conn, const struct ble_gatt_error *error,
            const struct ble_gatt_svc *service, void *arg)
{
  (void)arg;
  if (error->status == 0 && service) {
    uint8_t p[24];
    p[0] = EVT_GATT_SERVICE_QUERY_RESULT;
    p[1] = 22;
    put_le16(p + 2, conn);
    put_le16(p + 4, service->start_handle);
    put_le16(p + 6, service->end_handle);
    uuid_to_le128(&service->uuid, p + 8);
    picoruby_nimble_enqueue_event(p, sizeof(p), false);
  } else {
    synth_query_complete(conn, status_to_u8(error->status));
  }
  return 0;
}

static void
emit_characteristic(uint16_t conn, const struct ble_gatt_chr *c, uint16_t end_handle)
{
  uint8_t p[30];
  p[0] = EVT_GATT_CHARACTERISTIC_QUERY_RESULT;
  p[1] = 28;
  put_le16(p + 2, conn);
  put_le16(p + 4, c->def_handle);
  put_le16(p + 6, c->val_handle);
  put_le16(p + 8, end_handle);
  put_le16(p + 10, c->properties);
  uuid_to_le128(&c->uuid, p + 12);
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

static int
disc_chr_cb(uint16_t conn, const struct ble_gatt_error *error,
            const struct ble_gatt_chr *chr, void *arg)
{
  (void)arg;
  // NimBLE reports no end handle per characteristic: emit the previous one
  // bounded by the current def_handle, and flush the last at completion
  // bounded by the request range.
  if (error->status == 0 && chr) {
    if (chr_disc_have_prev) {
      emit_characteristic(conn, &chr_disc_prev, chr->def_handle - 1);
    }
    chr_disc_prev = *chr;
    chr_disc_have_prev = true;
  } else {
    if (chr_disc_have_prev) {
      emit_characteristic(conn, &chr_disc_prev, chr_disc_end_handle);
      chr_disc_have_prev = false;
    }
    synth_query_complete(conn, status_to_u8(error->status));
  }
  return 0;
}

static int
disc_dsc_cb(uint16_t conn, const struct ble_gatt_error *error,
            uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
  (void)chr_val_handle;
  (void)arg;
  if (error->status == 0 && dsc) {
    uint8_t p[22];
    p[0] = EVT_GATT_ALL_DESCRIPTORS_QUERY_RESULT;
    p[1] = 20;
    put_le16(p + 2, conn);
    put_le16(p + 4, dsc->handle);
    uuid_to_le128(&dsc->uuid, p + 6);
    picoruby_nimble_enqueue_event(p, sizeof(p), false);
  } else {
    synth_query_complete(conn, status_to_u8(error->status));
  }
  return 0;
}

static int
read_attr_cb(uint16_t conn, const struct ble_gatt_error *error,
             struct ble_gatt_attr *attr, void *arg)
{
  (void)arg;
  if (error->status == 0 && attr && attr->om) {
    uint16_t vlen = OS_MBUF_PKTLEN(attr->om);
    if (vlen > VALUE_EVENT_DATA_MAX) vlen = VALUE_EVENT_DATA_MAX;
    uint8_t p[8 + VALUE_EVENT_DATA_MAX];
    p[0] = EVT_GATT_CHARACTERISTIC_VALUE_QUERY_RESULT;
    p[1] = 6 + vlen;
    put_le16(p + 2, conn);
    put_le16(p + 4, attr->handle);
    put_le16(p + 6, vlen);
    os_mbuf_copydata(attr->om, 0, vlen, p + 8);
    picoruby_nimble_enqueue_event(p, 8 + vlen, false);
  }
  synth_query_complete(conn, status_to_u8(error->status));
  return 0;
}

static int
write_attr_cb(uint16_t conn, const struct ble_gatt_error *error,
              struct ble_gatt_attr *attr, void *arg)
{
  (void)attr;
  (void)arg;
  synth_query_complete(conn, status_to_u8(error->status));
  return 0;
}

static uint8_t
rc_to_u8(int rc)
{
  if (rc == 0) return 0;
  return (rc & 0xff) ? (uint8_t)rc : 0xff;
}

uint8_t
BLE_discover_primary_services(uint16_t conn_handle)
{
  return rc_to_u8(ble_gattc_disc_all_svcs(conn_handle, disc_svc_cb, NULL));
}

uint8_t
BLE_discover_characteristics_for_service(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
  chr_disc_end_handle = end_handle;
  chr_disc_have_prev = false;
  return rc_to_u8(ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle, disc_chr_cb, NULL));
}

uint8_t
BLE_read_value_of_characteristic_using_value_handle(uint16_t conn_handle, uint16_t value_handle)
{
  return rc_to_u8(ble_gattc_read(conn_handle, value_handle, read_attr_cb, NULL));
}

uint8_t
BLE_discover_characteristic_descriptors(uint16_t conn_handle, uint16_t value_handle, uint16_t end_handle)
{
  return rc_to_u8(ble_gattc_disc_all_dscs(conn_handle, value_handle, end_handle, disc_dsc_cb, NULL));
}

uint8_t
BLE_write_value_of_characteristic_without_response(uint16_t conn_handle, uint16_t value_handle, const uint8_t *data, uint16_t size)
{
  return rc_to_u8(ble_gattc_write_no_rsp_flat(conn_handle, value_handle, data, size));
}

uint8_t
BLE_write_characteristic_descriptor_using_descriptor_handle(uint16_t conn_handle, uint16_t descriptor_handle, const uint8_t *data, uint16_t size)
{
  return rc_to_u8(ble_gattc_write_flat(conn_handle, descriptor_handle, data, size, write_attr_cb, NULL));
}
