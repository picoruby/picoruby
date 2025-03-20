#include <stdbool.h>
#include <mrubyc.h>
#include "../include/mqtt.h"
#include "value.h"

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>

#include "symbol.h"
#include "class.h"

static mrbc_tcb *callback_task = NULL;

void save_compiled_code(uint8_t *mrb, size_t mrb_size);

static uint8_t *compiled_code = NULL;
static size_t compiled_code_size = 0;

static mrbc_vm *
prepare_vm(const char *code)
{
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  const uint8_t *utf8_code = (const uint8_t *)code;
  mrc_irep *irep = mrc_load_string_cxt(c, &utf8_code, strlen(code));

  uint8_t *mrb = NULL;
  size_t mrb_size = 0;
  mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);

  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);

  save_compiled_code(mrb, mrb_size);
  return NULL;
}

void save_compiled_code(uint8_t *mrb, size_t mrb_size) {
  if (compiled_code) {
    free(compiled_code);
  }
  compiled_code = malloc(mrb_size);
  memcpy(compiled_code, mrb, mrb_size);
  compiled_code_size = mrb_size;
}

void MQTT_callback(void)
{
  if (!callback_task) {
    callback_task = mrbc_create_task(compiled_code, NULL);
  }

  callback_task->vm.flag_preemption = 0;
  mrbc_vm_begin(&callback_task->vm);
  mrbc_vm_run(&callback_task->vm);
  mrbc_vm_end(&callback_task->vm);
}

static void c_mqtt_client_connect(mrbc_vm *vm, mrbc_value *v, int argc) {
  if (argc < 4 || argc > 14) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  prepare_vm("MQTTClient.instance.callback");

  const char *host = mrbc_string_cstr(&v[1]);
  const int port = mrbc_integer(v[2]);
  const char *client_id = mrbc_string_cstr(&v[3]);
  const bool use_tls = (v[4].tt == MRBC_TT_TRUE);

  const char *ca_file = (argc > 4 && v[5].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[5]) : NULL;
  const char *cert_file = (argc > 5 && v[6].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[6]) : NULL;
  const char *key_file = (argc > 6 && v[7].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[7]) : NULL;
  int tls_version = (argc > 7 && v[8].tt == MRBC_TT_FIXNUM) ? mrbc_integer(v[8]) : 0;
  int verify_mode = (argc > 8 && v[9].tt == MRBC_TT_FIXNUM) ? mrbc_integer(v[9]) : 1;
  const char *ca_cert = (argc > 9 && v[10].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[10]) : NULL;
  const char *client_cert = (argc > 10 && v[11].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[11]) : NULL;
  const char *client_key = (argc > 11 && v[12].tt == MRBC_TT_STRING) ? mrbc_string_cstr(&v[12]) : NULL;
  bool verify_hostname = (argc > 12) ? (v[13].tt == MRBC_TT_TRUE) : true;
  uint32_t handshake_timeout_ms = (argc > 13 && v[14].tt == MRBC_TT_FIXNUM) ? mrbc_integer(v[14]) : 30000;

  mrbc_value ret = MQTTClient_connect(vm, v, host, port, client_id, use_tls,
                                    ca_file, cert_file, key_file,
                                    tls_version, verify_mode,
                                    ca_cert, client_cert, client_key,
                                    verify_hostname, handshake_timeout_ms);
  SET_RETURN(ret);
}

static void c_mqtt_client_publish(mrbc_vm *vm, mrbc_value *v, int argc) {
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  
  const char *topic = mrbc_string_cstr(&v[2]);
  mrbc_value ret = MQTTClient_publish(vm, &v[1], topic);
  SET_RETURN(ret);
}

static void c_mqtt_client_subscribe(mrbc_vm *vm, mrbc_value *v, int argc) {
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  
  const char *topic = mrbc_string_cstr(&v[1]);
  mrbc_value ret = MQTTClient_subscribe(vm, topic);
  SET_RETURN(ret);
}

static void c_mqtt_client_disconnect(mrbc_vm *vm, mrbc_value *v, int argc) {
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  
  mrbc_value ret = MQTTClient_disconnect(vm);
  SET_RETURN(ret);
}

void
mrbc_mqtt_init(mrbc_vm *vm)
{
  mrbc_class *class_MQTTClient = mrbc_define_class(vm, "MQTTClient", mrbc_class_object);

  mrbc_define_method(vm, class_MQTTClient, "_connect_impl", c_mqtt_client_connect);
  mrbc_define_method(vm, class_MQTTClient, "_publish_impl", c_mqtt_client_publish);
  mrbc_define_method(vm, class_MQTTClient, "_subscribe_impl", c_mqtt_client_subscribe);
  mrbc_define_method(vm, class_MQTTClient, "_disconnect_impl", c_mqtt_client_disconnect);
}
