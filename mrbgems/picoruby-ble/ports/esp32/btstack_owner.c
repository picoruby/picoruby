// ports/esp32/btstack_owner.c
#include "btstack_owner.h"
#include "btstack.h"
#include "btstack_port_esp32.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_defines.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static bool started = false;
static SemaphoreHandle_t init_done_sem = NULL;
static TaskHandle_t btstack_task_handle = NULL;
static picoruby_btstack_callback_t pending_setup = NULL;
static void *pending_setup_ctx = NULL;

typedef struct {
  picoruby_btstack_callback_t cb;
  void *ctx;
  SemaphoreHandle_t done;
} sync_dispatch_t;

static void sync_trampoline(void *context) {
  sync_dispatch_t *d = (sync_dispatch_t *)context;
  d->cb(d->ctx);
  xSemaphoreGive(d->done);
}

static void btstack_task(void *param) {
  (void)param;
  btstack_init();
  if (pending_setup) {
    pending_setup(pending_setup_ctx);
  }
  xSemaphoreGive(init_done_sem);
  btstack_run_loop_execute();
  vTaskDelete(NULL);
}

void picoruby_btstack_ensure_started(picoruby_btstack_callback_t setup, void *ctx) {
  if (started) {
    if (setup) picoruby_btstack_run_sync(setup, ctx);
    return;
  }
  started = true;
  pending_setup = setup;
  pending_setup_ctx = ctx;
  init_done_sem = xSemaphoreCreateBinary();
  xTaskCreate(btstack_task, "btstack", 8192, NULL, 5, &btstack_task_handle);
  xSemaphoreTake(init_done_sem, portMAX_DELAY);
}

void picoruby_btstack_run_sync(picoruby_btstack_callback_t cb, void *ctx) {
  if (btstack_task_handle && xTaskGetCurrentTaskHandle() == btstack_task_handle) {
    cb(ctx);
    return;
  }
  sync_dispatch_t d = { .cb = cb, .ctx = ctx, .done = xSemaphoreCreateBinary() };
  btstack_context_callback_registration_t reg = {
    .callback = sync_trampoline,
    .context = &d,
  };
  btstack_run_loop_execute_on_main_thread(&reg);
  xSemaphoreTake(d.done, portMAX_DELAY);
  vSemaphoreDelete(d.done);
}
