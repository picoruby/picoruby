#include "nimble_owner.h"
#include "ble_common.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "prb_ble_evq";

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "../../include/ble.h"

#define EVQ_DEPTH 32
#define EVQ_PKT_MAX 100
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
static uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
static SemaphoreHandle_t sync_sem = NULL;
static esp_timer_handle_t heartbeat_timer = NULL;

void
picoruby_nimble_enqueue_event(const uint8_t *pkt, uint16_t len, bool coalesce_adv)
{
  if (len > EVQ_PKT_MAX) {
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
        return;
      }
      evq_head = (evq_head + 1) % EVQ_DEPTH;
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

/* Pops one queued event into `out` (capacity `cap`), returns its length or 0
 * if empty/dropped. Touches only the plain evq ring buffer (critical-section
 * guarded) — never mruby. Must be called from the VM thread only: darwin's
 * port (ports/darwin/src/mruby/ble.c, PicoBLECentral.swift) documents that
 * BLE_push_event (which calls mrb_malloc/mrb_free against the GC-managed
 * heap) must have exactly one caller, the VM thread itself. This port used to
 * violate that by calling BLE_push_event directly from dispatch_timer_cb on
 * the esp_timer service task (tiny dedicated stack) — confirmed via hardware
 * trace to silently fail there (BLE_push_event's own trace lines never even
 * printed), which is why BTSTACK_EVENT_STATE never reached Ruby. See
 * stackchan-picoruby/docs/superpowers/handoff/2026-07-22-ble-role-coverage-verification-evidence.md. */
uint16_t
picoruby_nimble_dequeue_event(uint8_t *out, uint16_t cap)
{
  evq_entry_t entry;
  taskENTER_CRITICAL(&evq_mux);
  if (evq_count == 0) {
    taskEXIT_CRITICAL(&evq_mux);
    return 0;
  }
  entry = evq[evq_head];
  evq_head = (evq_head + 1) % EVQ_DEPTH;
  evq_count--;
  taskEXIT_CRITICAL(&evq_mux);
  if (entry.len > cap) return 0;
  memcpy(out, entry.data, entry.len);
  return entry.len;
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
      esp_err_t err = esp_timer_start_periodic(heartbeat_timer, HEARTBEAT_PERIOD_US);
      ESP_LOGI(TAG, "esp_timer_start_periodic(heartbeat) -> %s, is_active=%d",
               esp_err_to_name(err), esp_timer_is_active(heartbeat_timer));
    }
  } else {
    esp_timer_stop(heartbeat_timer);
  }
}

static void
on_sync(void)
{
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
  (void)reason;
  synced = false;
}

static void
host_task(void *param)
{
  (void)param;
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static void
ensure_timers(void)
{
  if (heartbeat_timer == NULL) {
    const esp_timer_create_args_t hargs = {
      .callback = heartbeat_timer_cb,
      .name = "prb_ble_hb",
    };
    esp_err_t err = esp_timer_create(&hargs, &heartbeat_timer);
    ESP_LOGI(TAG, "esp_timer_create(heartbeat) -> %s", esp_err_to_name(err));
  }
}

int
picoruby_nimble_start(picoruby_nimble_setup_fn setup)
{
  if (started) return 0;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    return -1;
  }

  if (nimble_port_init() != ESP_OK) {
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
    nimble_port_stop();
    nimble_port_deinit();
    return -1;
  }

  started = true;
  return 0;
}

int
picoruby_nimble_stop(void)
{
  if (!started) return 0;
  picoruby_nimble_heartbeat_enable(false);
  ble_gap_adv_stop();
  ble_gap_disc_cancel();
  nimble_port_stop();
  nimble_port_deinit();
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
