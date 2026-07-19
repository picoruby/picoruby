// ports/esp32/nimble_owner.c
#include "nimble_owner.h"
#include "ble_common.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "../../include/ble.h"

static const char *TAG = "prb_ble";

// ---------------------------------------------------------------------------
// Synthesized-event queue.
//
// BLE_push_event (src/mruby/ble.c) is a single-slot last-writer-wins mailbox
// polled by Ruby every 100 ms; NimBLE discovery callbacks burst results
// back-to-back, so pushing directly would lose all but the last packet.
// Dispatch one queued packet per 150 ms (> Ruby's polling period).

// Depth must absorb a whole ATT response worth of discovery callbacks: one
// read-by-type response at the default 256-byte MTU carries up to ~28
// 16-bit-uuid characteristic entries, each synthesized as a separate event.
#define EVQ_DEPTH 32
#define EVQ_PKT_MAX 100
#define EVQ_DISPATCH_PERIOD_US (150 * 1000)
#define HEARTBEAT_PERIOD_US (1000 * 1000)
#define SYNC_TIMEOUT_TICKS pdMS_TO_TICKS(2000)

typedef struct {
  uint16_t len;
  bool is_adv;
  uint8_t data[EVQ_PKT_MAX];
} evq_entry_t;

static evq_entry_t evq[EVQ_DEPTH];
static int evq_head = 0;
static int evq_count = 0;
static portMUX_TYPE evq_mux = portMUX_INITIALIZER_UNLOCKED;

static bool started = false;
static volatile bool synced = false;
static uint8_t own_addr_type = 0; // BLE_OWN_ADDR_PUBLIC
static SemaphoreHandle_t sync_sem = NULL;
static esp_timer_handle_t dispatch_timer = NULL;
static esp_timer_handle_t heartbeat_timer = NULL;

void
picoruby_nimble_enqueue_event(const uint8_t *pkt, uint16_t len, bool coalesce_adv)
{
  if (len > EVQ_PKT_MAX) {
    ESP_LOGW(TAG, "event 0x%02x truncated %u -> %u", pkt[0], len, EVQ_PKT_MAX);
    len = EVQ_PKT_MAX;
  }
  taskENTER_CRITICAL(&evq_mux);
  int slot = -1;
  if (coalesce_adv && evq_count > 0) {
    int tail = (evq_head + evq_count - 1) % EVQ_DEPTH;
    if (evq[tail].is_adv) slot = tail;
  }
  if (slot < 0) {
    if (evq_count == EVQ_DEPTH) {
      if (coalesce_adv) {
        taskEXIT_CRITICAL(&evq_mux);
        return; // drop incoming advertising report on overflow
      }
      evq_head = (evq_head + 1) % EVQ_DEPTH; // drop oldest
      evq_count--;
    }
    slot = (evq_head + evq_count) % EVQ_DEPTH;
    evq_count++;
  }
  evq[slot].len = len;
  evq[slot].is_adv = coalesce_adv;
  memcpy(evq[slot].data, pkt, len);
  taskEXIT_CRITICAL(&evq_mux);
}

static void
dispatch_timer_cb(void *arg)
{
  (void)arg;
  evq_entry_t entry;
  taskENTER_CRITICAL(&evq_mux);
  if (evq_count == 0) {
    taskEXIT_CRITICAL(&evq_mux);
    return;
  }
  entry = evq[evq_head];
  evq_head = (evq_head + 1) % EVQ_DEPTH;
  evq_count--;
  taskEXIT_CRITICAL(&evq_mux);
  BLE_push_event(entry.data, entry.len);
}

static void
heartbeat_timer_cb(void *arg)
{
  (void)arg;
  BLE_heartbeat();
}

void
picoruby_nimble_heartbeat_enable(bool enable)
{
  if (heartbeat_timer == NULL) return;
  if (enable) {
    if (!esp_timer_is_active(heartbeat_timer)) {
      esp_timer_start_periodic(heartbeat_timer, HEARTBEAT_PERIOD_US);
    }
  } else {
    esp_timer_stop(heartbeat_timer);
  }
}

// ---------------------------------------------------------------------------
// Host lifecycle

static void
on_sync(void)
{
  // Prefer the factory public address; no RPA rotation. This preserves the
  // BTstack port's "Phase 1 fix" (random-address rotation made scanners show
  // duplicate device entries and broke GATT discovery).
  ble_hs_util_ensure_addr(0);
  if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) {
    own_addr_type = 0;
  }
  synced = true;
  if (sync_sem) xSemaphoreGive(sync_sem);
}

static void
on_reset(int reason)
{
  ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
  synced = false;
}

static void
host_task(void *param)
{
  (void)param;
  nimble_port_run(); // returns when nimble_port_stop() completes
  nimble_port_freertos_deinit();
}

static void
ensure_timers(void)
{
  if (dispatch_timer == NULL) {
    const esp_timer_create_args_t dargs = {
      .callback = dispatch_timer_cb,
      .name = "prb_ble_evq",
    };
    esp_timer_create(&dargs, &dispatch_timer);
  }
  if (heartbeat_timer == NULL) {
    const esp_timer_create_args_t hargs = {
      .callback = heartbeat_timer_cb,
      .name = "prb_ble_hb",
    };
    esp_timer_create(&hargs, &heartbeat_timer);
  }
}

int
picoruby_nimble_start(picoruby_nimble_setup_fn setup)
{
  if (started) return 0;

  esp_err_t err = nvs_flash_init(); // controller needs NVS (PHY calibration)
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return -1;
  }

  if (nimble_port_init() != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed");
    return -1;
  }

  ble_hs_cfg.sync_cb = on_sync;
  ble_hs_cfg.reset_cb = on_reset;
  ble_hs_cfg.gatts_register_cb = picoruby_ble_gatts_register_cb;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 0;

  if (setup && setup() != 0) {
    nimble_port_deinit();
    return -1;
  }

  ensure_timers();
  taskENTER_CRITICAL(&evq_mux);
  evq_head = 0;
  evq_count = 0;
  taskEXIT_CRITICAL(&evq_mux);

  if (sync_sem == NULL) sync_sem = xSemaphoreCreateBinary();
  synced = false;
  nimble_port_freertos_init(host_task);

  if (xSemaphoreTake(sync_sem, SYNC_TIMEOUT_TICKS) != pdTRUE) {
    ESP_LOGE(TAG, "NimBLE host sync timeout");
    nimble_port_stop();
    nimble_port_deinit();
    return -1;
  }

  if (!esp_timer_is_active(dispatch_timer)) {
    esp_timer_start_periodic(dispatch_timer, EVQ_DISPATCH_PERIOD_US);
  }
  started = true;
  return 0;
}

int
picoruby_nimble_stop(void)
{
  if (!started) return 0;
  picoruby_nimble_heartbeat_enable(false);
  if (dispatch_timer) esp_timer_stop(dispatch_timer);
  ble_gap_adv_stop();     // ignore errors: may not be advertising
  ble_gap_disc_cancel();  // ignore errors: may not be scanning
  nimble_port_stop();     // blocks until host task exits nimble_port_run
  nimble_port_deinit();   // ble_gatts_stop() has zeroed registration counters
  synced = false;
  started = false;
  return 0;
}

bool
picoruby_nimble_started(void)
{
  return started;
}

uint8_t
picoruby_nimble_own_addr_type(void)
{
  return own_addr_type;
}
