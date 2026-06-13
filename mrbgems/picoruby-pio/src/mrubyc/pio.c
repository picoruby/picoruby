#include <mrubyc.h>

static void
c_sm_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(pio_sm_config_t));
  pio_sm_config_t *config = (pio_sm_config_t *)self.instance->data;
  memset(config, 0, sizeof(pio_sm_config_t));

  /* Copy instructions from Ruby array to C array */
  mrbc_value ary = GET_ARG(3);
  int instr_count = GET_INT_ARG(4);
  config->instructions = (uint16_t *)mrbc_alloc(vm, sizeof(uint16_t) * instr_count);
  if (!config->instructions) {
    mrbc_raise(vm, MRBC_CLASS(StandardError), "Failed to allocate instruction memory");
    return;
  }
  for (int i = 0; i < instr_count; i++) {
    config->instructions[i] = (uint16_t)mrbc_integer(mrbc_array_get(&ary, i));
  }

  config->pio_num             = (uint8_t) GET_INT_ARG(1);
  config->sm_num              = (uint8_t) GET_INT_ARG(2);
  config->instruction_count   = (uint8_t) instr_count;
  config->side_set_count      = (uint8_t) GET_INT_ARG(5);
  config->side_set_optional   = (uint8_t) GET_INT_ARG(6);
  config->wrap_target         = (uint8_t) GET_INT_ARG(7);
  config->wrap                = (uint8_t) GET_INT_ARG(8);
  config->freq                = (uint32_t)GET_INT_ARG(9);
  config->out_pins            = (int8_t)  GET_INT_ARG(10);
  config->out_pin_count       = (uint8_t) GET_INT_ARG(11);
  config->set_pins            = (int8_t)  GET_INT_ARG(12);
  config->set_pin_count       = (uint8_t) GET_INT_ARG(13);
  config->in_pins             = (int8_t)  GET_INT_ARG(14);
  config->sideset_pins        = (int8_t)  GET_INT_ARG(15);
  config->jmp_pin             = (int8_t)  GET_INT_ARG(16);
  config->out_shift_right     = (uint8_t) GET_INT_ARG(17);
  config->out_shift_autopull  = (uint8_t) GET_INT_ARG(18);
  config->out_shift_threshold = (uint8_t) GET_INT_ARG(19);
  config->in_shift_right      = (uint8_t) GET_INT_ARG(20);
  config->in_shift_autopush   = (uint8_t) GET_INT_ARG(21);
  config->in_shift_threshold  = (uint8_t) GET_INT_ARG(22);
  config->fifo_join           = (uint8_t) GET_INT_ARG(23);
  config->enabled             = 0;

  pio_error_t err = PIO_init(config);
  if (err != PIO_ERROR_NONE) {
    const char *message;
    switch (err) {
      case PIO_ERROR_NO_SPACE:    message = "No space in PIO instruction memory"; break;
      case PIO_ERROR_INVALID_PIO: message = "Invalid PIO number"; break;
      case PIO_ERROR_INVALID_SM:  message = "Invalid state machine number"; break;
      case PIO_ERROR_INVALID_PIN: message = "Invalid pin number"; break;
      default:                    message = "PIO init failed"; break;
    }
    mrbc_raise(vm, MRBC_CLASS(IOError), message);
    return;
  }

  SET_RETURN(self);
}

static void
c_sm_start(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_set_enabled(config, true);
  config->enabled = 1;
}

static void
c_sm_stop(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_set_enabled(config, false);
  config->enabled = 0;
}

static void
c_sm_restart(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_restart(config);
}

static void
c_sm_put_buffer(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  mrbc_value ary = GET_ARG(1);
  int len = mrbc_array_size(&ary);
  int i = 0;
  while (i < len) {
    PIO_put_blocking(config, (uint32_t)mrbc_integer(mrbc_array_get(&ary, i)));
    i++;
  }
}

static void
c_sm_put_bytes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  mrbc_value str = GET_ARG(1);
  const uint8_t *p = (const uint8_t *)mrbc_string_cstr(&str);
  int len = mrbc_string_size(&str);
  int i = 0;
  while (i < len) {
    PIO_put_blocking(config, (uint32_t)p[i]);
    i++;
  }
}

static void
c_sm_put_nonblocking(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  bool ok = PIO_put_nonblocking(config, (uint32_t)GET_INT_ARG(1));
  if (ok) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_sm_get(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  uint32_t value = PIO_get_blocking(config);
  SET_INT_RETURN((mrbc_int_t)value);
}

static void
c_sm_get_nonblocking(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  uint32_t value;
  bool ok = PIO_get_nonblocking(config, &value);
  if (ok) {
    SET_INT_RETURN((mrbc_int_t)value);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_sm_tx_level(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_tx_level(config));
}

static void
c_sm_rx_level(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_rx_level(config));
}

static void
c_sm_tx_full(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_tx_full(config) ? 1 : 0);
}

static void
c_sm_tx_empty(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_tx_empty(config) ? 1 : 0);
}

static void
c_sm_rx_full(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_rx_full(config) ? 1 : 0);
}

static void
c_sm_rx_empty(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN(PIO_rx_empty(config) ? 1 : 0);
}

static void
c_sm_clear_fifos(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_clear_fifos(config);
}

static void
c_sm_drain_tx(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_drain_tx(config);
}

static void
c_sm_exec(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  PIO_exec(config, (uint16_t)GET_INT_ARG(1));
}

static void
c_sm_freq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  SET_INT_RETURN((mrbc_int_t)config->freq);
}

static void
c_sm_freq_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pio_sm_config_t *config = (pio_sm_config_t *)v->instance->data;
  mrbc_int_t freq = GET_INT_ARG(1);
  if (freq <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "freq must be positive");
    return;
  }
  PIO_set_freq(config, (uint32_t)freq);
  SET_INT_RETURN(freq);
}

void
mrbc_pio_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_module_PIO = mrbc_define_module(vm, "PIO");
  mrbc_class *mrbc_class_SM = mrbc_define_class_under(vm, mrbc_module_PIO, "StateMachine", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_SM, "init", c_sm_init);
  mrbc_define_method(vm, mrbc_class_SM, "start", c_sm_start);
  mrbc_define_method(vm, mrbc_class_SM, "stop", c_sm_stop);
  mrbc_define_method(vm, mrbc_class_SM, "restart", c_sm_restart);
  mrbc_define_method(vm, mrbc_class_SM, "put_buffer", c_sm_put_buffer);
  mrbc_define_method(vm, mrbc_class_SM, "put_bytes", c_sm_put_bytes);
  mrbc_define_method(vm, mrbc_class_SM, "put_nonblocking", c_sm_put_nonblocking);
  mrbc_define_method(vm, mrbc_class_SM, "get", c_sm_get);
  mrbc_define_method(vm, mrbc_class_SM, "get_nonblocking", c_sm_get_nonblocking);
  mrbc_define_method(vm, mrbc_class_SM, "tx_level", c_sm_tx_level);
  mrbc_define_method(vm, mrbc_class_SM, "rx_level", c_sm_rx_level);
  mrbc_define_method(vm, mrbc_class_SM, "tx_full", c_sm_tx_full);
  mrbc_define_method(vm, mrbc_class_SM, "tx_empty", c_sm_tx_empty);
  mrbc_define_method(vm, mrbc_class_SM, "rx_full", c_sm_rx_full);
  mrbc_define_method(vm, mrbc_class_SM, "rx_empty", c_sm_rx_empty);
  mrbc_define_method(vm, mrbc_class_SM, "clear_fifos", c_sm_clear_fifos);
  mrbc_define_method(vm, mrbc_class_SM, "drain_tx", c_sm_drain_tx);
  mrbc_define_method(vm, mrbc_class_SM, "exec", c_sm_exec);
  mrbc_define_method(vm, mrbc_class_SM, "freq", c_sm_freq);
  mrbc_define_method(vm, mrbc_class_SM, "freq=", c_sm_freq_eq);
}
