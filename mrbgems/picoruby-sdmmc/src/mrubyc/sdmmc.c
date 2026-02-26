#include <mrubyc.h>

static void
c_clk_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->clk_pin);
}

static void
c_cmd_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->cmd_pin);
}

static void
c_d0_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->d0_pin);
}

static void
c_d1_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->d1_pin);
}

static void
c_d2_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->d2_pin);
}

static void
c_d3_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->d3_pin);
}

static void
c_width(mrbc_vm *vm, mrbc_value *v, int argc)
{
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->width);
}

static void
c_s_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(sdmmc_unit_info_t));
  sdmmc_unit_info_t *unit_info = (sdmmc_unit_info_t *)self.instance->data;
  memset(unit_info, 0, sizeof(sdmmc_unit_info_t));
  unit_info->clk_pin = (int8_t)GET_INT_ARG(1);
  unit_info->cmd_pin = (int8_t)GET_INT_ARG(2);
  unit_info->d0_pin  = (int8_t)GET_INT_ARG(3);
  unit_info->d1_pin  = (int8_t)GET_INT_ARG(4);
  unit_info->d2_pin  = (int8_t)GET_INT_ARG(5);
  unit_info->d3_pin  = (int8_t)GET_INT_ARG(6);
  unit_info->width   = (uint8_t)GET_INT_ARG(7);
  SET_RETURN(self);
}

void
mrbc_sdmmc_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_SDMMC = mrbc_define_class(vm, "SDMMC", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_SDMMC, "_init", c_s_init);
  mrbc_define_method(vm, mrbc_class_SDMMC, "clk_pin", c_clk_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "cmd_pin", c_cmd_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "d0_pin", c_d0_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "d1_pin", c_d1_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "d2_pin", c_d2_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "d3_pin", c_d3_pin);
  mrbc_define_method(vm, mrbc_class_SDMMC, "width", c_width);
}
