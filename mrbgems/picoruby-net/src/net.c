#include <stdbool.h>
#include <mrubyc.h>
#include "../include/net.h"

static void
c_net_dns_resolve(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  mrbc_value ret = DNS_resolve(vm, GET_STRING_ARG(1));
  SET_RETURN(ret);
}

static void
c_net_tcpclient__request_impl(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 4) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value host = GET_ARG(1);
  if (host.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mrbc_value port = GET_ARG(2);
  if (port.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mrbc_value input = GET_ARG(3);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mrbc_value is_tls = GET_ARG(4);
  if (!((is_tls.tt == MRBC_TT_TRUE) || (is_tls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  mrbc_value ret = TCPClient_send(GET_STRING_ARG(1), port.i, vm, &input, (is_tls.tt == MRBC_TT_TRUE));
  SET_RETURN(ret);
}

void
mrbc_net_init(void)
{
  mrbc_class *mrbc_class_Net = mrbc_define_class(0, "Net", mrbc_class_object);

  mrbc_value *DNS = mrbc_get_class_const(mrbc_class_Net, mrbc_search_symid("DNS"));
  mrbc_class *mrbc_class_Net_DNS = DNS->cls;
  mrbc_define_method(0, mrbc_class_Net_DNS, "resolve", c_net_dns_resolve);

  mrbc_value *TCPClient = mrbc_get_class_const(mrbc_class_Net, mrbc_search_symid("TCPClient"));
  mrbc_class *mrbc_class_Net_TCPClient = TCPClient->cls;
  mrbc_define_method(0, mrbc_class_Net_TCPClient, "_request_impl", c_net_tcpclient__request_impl);
}
