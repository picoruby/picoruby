#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/class.h>

static void
mrb_pio_sm_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    pio_sm_config_t *config = (pio_sm_config_t *)ptr;
    if (config->enabled) {
      PIO_set_enabled(config, false);
    }
    PIO_deinit(config);
    if (config->instructions) {
      mrb_free(mrb, config->instructions);
    }
    mrb_free(mrb, config);
  }
}

struct mrb_data_type mrb_pio_sm_type = {
  "PIO::StateMachine", mrb_pio_sm_free
};

static mrb_value
mrb_sm_init(mrb_state *mrb, mrb_value klass)
{
  mrb_int pio_num, sm_num;
  mrb_value instructions_ary;
  mrb_int instr_count, side_set_count, side_set_optional;
  mrb_int wrap_target, wrap_idx;
  mrb_int freq;
  mrb_int out_pins, out_pin_count, set_pins, set_pin_count;
  mrb_int in_pins, sideset_pins, jmp_pin;
  mrb_int out_shift_right, out_shift_autopull, out_shift_threshold;
  mrb_int in_shift_right, in_shift_autopush, in_shift_threshold;
  mrb_int fifo_join;

  mrb_get_args(mrb, "iiAiiiiiiiiiiiiiiiiiiii",
    &pio_num, &sm_num,
    &instructions_ary,
    &instr_count, &side_set_count, &side_set_optional,
    &wrap_target, &wrap_idx,
    &freq,
    &out_pins, &out_pin_count,
    &set_pins, &set_pin_count,
    &in_pins, &sideset_pins, &jmp_pin,
    &out_shift_right, &out_shift_autopull, &out_shift_threshold,
    &in_shift_right, &in_shift_autopush, &in_shift_threshold,
    &fifo_join
  );

  pio_sm_config_t *config = (pio_sm_config_t *)mrb_malloc(mrb, sizeof(pio_sm_config_t));
  memset(config, 0, sizeof(pio_sm_config_t));

  /* Copy instructions from Ruby array to C array */
  config->instructions = (uint16_t *)mrb_malloc(mrb, sizeof(uint16_t) * instr_count);
  for (mrb_int i = 0; i < instr_count; i++) {
    config->instructions[i] = (uint16_t)mrb_integer(mrb_ary_entry(instructions_ary, i));
  }

  config->pio_num             = (uint8_t)pio_num;
  config->sm_num              = (uint8_t)sm_num;
  config->instruction_count   = (uint8_t)instr_count;
  config->side_set_count      = (uint8_t)side_set_count;
  config->side_set_optional   = (uint8_t)side_set_optional;
  config->wrap_target         = (uint8_t)wrap_target;
  config->wrap                = (uint8_t)wrap_idx;
  config->freq                = (uint32_t)freq;
  config->out_pins            = (int8_t)out_pins;
  config->out_pin_count       = (uint8_t)out_pin_count;
  config->set_pins            = (int8_t)set_pins;
  config->set_pin_count       = (uint8_t)set_pin_count;
  config->in_pins             = (int8_t)in_pins;
  config->sideset_pins        = (int8_t)sideset_pins;
  config->jmp_pin             = (int8_t)jmp_pin;
  config->out_shift_right     = (uint8_t)out_shift_right;
  config->out_shift_autopull  = (uint8_t)out_shift_autopull;
  config->out_shift_threshold = (uint8_t)out_shift_threshold;
  config->in_shift_right      = (uint8_t)in_shift_right;
  config->in_shift_autopush   = (uint8_t)in_shift_autopush;
  config->in_shift_threshold  = (uint8_t)in_shift_threshold;
  config->fifo_join           = (uint8_t)fifo_join;
  config->enabled             = 0;

  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_pio_sm_type, config));

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
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, message);
  }

  return self;
}

static mrb_value
mrb_sm_start(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  PIO_set_enabled(config, true);
  config->enabled = 1;
  return mrb_nil_value();
}

static mrb_value
mrb_sm_stop(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  PIO_set_enabled(config, false);
  config->enabled = 0;
  return mrb_nil_value();
}

static mrb_value
mrb_sm_restart(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  PIO_restart(config);
  return mrb_nil_value();
}

static mrb_value
mrb_sm_put(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  mrb_int value;
  mrb_get_args(mrb, "i", &value);
  PIO_put_blocking(config, (uint32_t)value);
  return mrb_nil_value();
}

static mrb_value
mrb_sm_put_nonblocking(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  mrb_int value;
  mrb_get_args(mrb, "i", &value);
  bool ok = PIO_put_nonblocking(config, (uint32_t)value);
  return mrb_bool_value(ok);
}

static mrb_value
mrb_sm_get(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  uint32_t value = PIO_get_blocking(config);
  return mrb_fixnum_value((mrb_int)value);
}

static mrb_value
mrb_sm_get_nonblocking(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  uint32_t value;
  bool ok = PIO_get_nonblocking(config, &value);
  if (ok) {
    return mrb_fixnum_value((mrb_int)value);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_sm_tx_level(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_tx_level(config));
}

static mrb_value
mrb_sm_rx_level(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_rx_level(config));
}

static mrb_value
mrb_sm_tx_full(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_tx_full(config) ? 1 : 0);
}

static mrb_value
mrb_sm_tx_empty(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_tx_empty(config) ? 1 : 0);
}

static mrb_value
mrb_sm_rx_full(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_rx_full(config) ? 1 : 0);
}

static mrb_value
mrb_sm_rx_empty(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  return mrb_fixnum_value(PIO_rx_empty(config) ? 1 : 0);
}

static mrb_value
mrb_sm_clear_fifos(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  PIO_clear_fifos(config);
  return mrb_nil_value();
}

static mrb_value
mrb_sm_drain_tx(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  PIO_drain_tx(config);
  return mrb_nil_value();
}

static mrb_value
mrb_sm_exec(mrb_state *mrb, mrb_value self)
{
  pio_sm_config_t *config = (pio_sm_config_t *)mrb_data_get_ptr(mrb, self, &mrb_pio_sm_type);
  mrb_int instr;
  mrb_get_args(mrb, "i", &instr);
  PIO_exec(config, (uint16_t)instr);
  return mrb_nil_value();
}

void
mrb_picoruby_pio_gem_init(mrb_state* mrb)
{
  struct RClass *module_PIO = mrb_define_module_id(mrb, MRB_SYM(PIO));

  struct RClass *class_SM = mrb_define_class_under_id(mrb, module_PIO, MRB_SYM(StateMachine), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_SM, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_SM, MRB_SYM(init), mrb_sm_init, MRB_ARGS_REQ(23));
  mrb_define_method_id(mrb, class_SM, MRB_SYM(start), mrb_sm_start, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(stop), mrb_sm_stop, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(restart), mrb_sm_restart, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(put), mrb_sm_put, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SM, MRB_SYM(put_nonblocking), mrb_sm_put_nonblocking, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SM, MRB_SYM(get), mrb_sm_get, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(get_nonblocking), mrb_sm_get_nonblocking, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(tx_level), mrb_sm_tx_level, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(rx_level), mrb_sm_rx_level, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(tx_full), mrb_sm_tx_full, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(tx_empty), mrb_sm_tx_empty, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(rx_full), mrb_sm_rx_full, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(rx_empty), mrb_sm_rx_empty, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(clear_fifos), mrb_sm_clear_fifos, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(drain_tx), mrb_sm_drain_tx, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SM, MRB_SYM(exec), mrb_sm_exec, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_pio_gem_final(mrb_state* mrb)
{
}
