#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/array.h>

static mrb_value
mrb_rmt__init(mrb_state *mrb, mrb_value self)
{
  mrb_int pin, t0h_ns, t0l_ns, t1h_ns, t1l_ns, reset_ns;
  mrb_get_args(mrb, "iiiiii", &pin, &t0h_ns, &t0l_ns, &t1h_ns, &t1l_ns, &reset_ns);

  RMT_symbol_dulation_t rmt_symbol_dulation = {
    .t0h_ns = t0h_ns,
    .t0l_ns = t0l_ns,
    .t1h_ns = t1h_ns,
    .t1l_ns = t1l_ns,
    .reset_ns = reset_ns
  };

  int ret = RMT_init((uint32_t)pin, &rmt_symbol_dulation);
  return mrb_fixnum_value(ret);
}

static mrb_value
mrb_rmt__write(mrb_state *mrb, mrb_value self)
{
  mrb_value value_ary;
  mrb_get_args(mrb, "A", &value_ary);
  int len = RARRAY_LEN(value_ary);
  uint8_t txdata[len];

  for (int i = 0 ; i < len ; i++) {
    txdata[i] = mrb_integer(mrb_ary_ref(mrb, value_ary, i));
  }

  int ret = RMT_write(txdata, len);
  return mrb_fixnum_value(ret);
}

void
mrb_picoruby_rmt_gem_init(mrb_state* mrb)
{
  struct RClass *class_RMT = mrb_define_class_id(mrb, MRB_SYM(RMT), mrb->object_class);

  mrb_define_method_id(mrb, class_RMT, MRB_SYM(_init), mrb_rmt__init, MRB_ARGS_REQ(6));
  mrb_define_method_id(mrb, class_RMT, MRB_SYM(_write), mrb_rmt__write, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_rmt_gem_final(mrb_state* mrb)
{
}
