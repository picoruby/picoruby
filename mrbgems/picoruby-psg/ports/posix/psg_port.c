#include "../../include/psg.h"

static void
noop_init(uint8_t p1, uint8_t p2)
{
  (void)p1;
  (void)p2;
}

static void
noop_void(void)
{
}

static void
noop_write(uint16_t left, uint16_t right)
{
  (void)left;
  (void)right;
}

const psg_output_api_t psg_drv_pwm = {
  .init = noop_init,
  .start = noop_void,
  .stop = noop_void,
  .write = noop_write,
  .write_buffer = NULL,
};

const psg_output_api_t psg_drv_mcp4922 = {
  .init = noop_init,
  .start = noop_void,
  .stop = noop_void,
  .write = noop_write,
  .write_buffer = NULL,
};

psg_cs_token_t
PSG_enter_critical(void)
{
  return 0;
}

void
PSG_exit_critical(psg_cs_token_t token)
{
  (void)token;
}

void
PSG_tick_start_core1(uint8_t p1, uint8_t p2)
{
  if (psg_drv) {
    psg_drv->init(p1, p2);
    psg_drv->start();
  }
}

void
PSG_tick_stop_core1(void)
{
  if (psg_drv) {
    psg_drv->stop();
    psg_drv = NULL;
  }
}
