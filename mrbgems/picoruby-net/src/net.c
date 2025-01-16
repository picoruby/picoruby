#include "../include/net.h"
#include "lwipopts.h"
#include "lwip/dns.h"

static void
ds_found(const char *name, const ip_addr_t *ip, void *arg)
{
  ip_addr_t *result = (ip_addr_t *)arg;
  if (ip) {
    ip4_addr_copy(*result, *ip);
  } else {
    ip4_addr_set_loopback(result);
  }
}

static err_t
get_ip_impl(const char *name, ip_addr_t *ip)
{
  lwip_begin();
  err_t err = dns_gethostbyname(name, ip, dns_found, ip);
  lwip_end();
  return err;
}

err_t
Net_get_ip(const char *name, ip_addr_t *ip)
{
  get_ip_impl(name, ip);
  while (!ip_addr_get_ip4_u32(ip)) {
    Net_sleep_ms(50);
  }
  return ERR_OK;
}

static void c_mqtt_client_connect(mrbc_vm *vm, mrbc_value *v, int argc) {
  if (argc != 4) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  prepare_vm("Net::MQTTClient.instance.callback");

  const char *host = mrbc_string_cstr(&v[1]);
  const int port = mrbc_integer(v[2]);
  const char *client_id = mrbc_string_cstr(&v[3]);
  const bool use_tls = (v[4].tt == MRBC_TT_TRUE);

  mrbc_value ret = MQTTClient_connect(vm, v, host, port, client_id, use_tls);
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
mrbc_net_init(mrbc_vm *vm)
{
  mrbc_class *class_Net = mrbc_define_class(vm, "Net", mrbc_class_object);

#if defined(PICORB_VM_MRUBY)

#include "mruby/net.c"

  mrbc_class *class_Net_UDPClient = mrbc_define_class_under(vm, class_Net, "UDPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_UDPClient, "_send_impl", c_net_udpclient__send_impl);

  mrbc_class *class_Net_MQTTClient = mrbc_define_class_under(vm, class_Net, "MQTTClient", mrbc_class_object);

  mrbc_define_method(vm, class_Net_MQTTClient, "_connect_impl", c_mqtt_client_connect);
  mrbc_define_method(vm, class_Net_MQTTClient, "_publish_impl", c_mqtt_client_publish);
  mrbc_define_method(vm, class_Net_MQTTClient, "_subscribe_impl", c_mqtt_client_subscribe);
  mrbc_define_method(vm, class_Net_MQTTClient, "_disconnect_impl", c_mqtt_client_disconnect);
}
