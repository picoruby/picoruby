#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/data.h>
#include <mruby/class.h>

static void
mrb_sdmmc_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_sdmmc_type = {
  "SDMMC", mrb_sdmmc_free
};

static mrb_value
mrb_clk_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->clk_pin);
}

static mrb_value
mrb_cmd_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->cmd_pin);
}

static mrb_value
mrb_d0_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->d0_pin);
}

static mrb_value
mrb_d1_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->d1_pin);
}

static mrb_value
mrb_d2_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->d2_pin);
}

static mrb_value
mrb_d3_pin(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->d3_pin);
}

static mrb_value
mrb_width(mrb_state *mrb, mrb_value self)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_sdmmc_type);
  return mrb_fixnum_value(unit_info->width);
}

static mrb_value
mrb_s_init(mrb_state *mrb, mrb_value klass)
{
  mrb_int clk_pin, cmd_pin, d0_pin, d1_pin, d2_pin, d3_pin, width;
  mrb_get_args(mrb, "iiiiiii", &clk_pin, &cmd_pin, &d0_pin, &d1_pin, &d2_pin, &d3_pin, &width);

  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)mrb_malloc(mrb, sizeof(sdmmc_unit_info_t));
  unit_info->clk_pin = (int8_t)clk_pin;
  unit_info->cmd_pin = (int8_t)cmd_pin;
  unit_info->d0_pin  = (int8_t)d0_pin;
  unit_info->d1_pin  = (int8_t)d1_pin;
  unit_info->d2_pin  = (int8_t)d2_pin;
  unit_info->d3_pin  = (int8_t)d3_pin;
  unit_info->width   = (uint8_t)width;

  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_sdmmc_type, unit_info));

  return self;
}

void
mrb_picoruby_sdmmc_gem_init(mrb_state* mrb)
{
  struct RClass *class_SDMMC = mrb_define_class_id(mrb, MRB_SYM(SDMMC), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_SDMMC, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_SDMMC, MRB_SYM(_init), mrb_s_init, MRB_ARGS_REQ(7));
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(clk_pin), mrb_clk_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(cmd_pin), mrb_cmd_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(d0_pin), mrb_d0_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(d1_pin), mrb_d1_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(d2_pin), mrb_d2_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(d3_pin), mrb_d3_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SDMMC, MRB_SYM(width), mrb_width, MRB_ARGS_NONE());
}

void
mrb_picoruby_sdmmc_gem_final(mrb_state* mrb)
{
}
