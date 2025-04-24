#include "mrubyc.h"

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

  mrbc_value ret = DNS_resolve(vm, (const char *)GET_STRING_ARG(1), (GET_ARG(2).tt == MRBC_TT_TRUE));
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

  mrbc_value ret = TCPClient_send((const char *)GET_STRING_ARG(1), port.i, vm, &input, (is_tls.tt == MRBC_TT_TRUE));
  SET_RETURN(ret);
}

static void
c_net_udpclient__send_impl(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 4) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value host = GET_ARG(1);
  if (host.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for host");
    return;
  }
  mrbc_value port = GET_ARG(2);
  if (port.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for port");
    return;
  }
  mrbc_value data = GET_ARG(3);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for data");
    return;
  }
  mrbc_value use_dtls = GET_ARG(4);
  if (!((use_dtls.tt == MRBC_TT_TRUE) || (use_dtls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for use_dtls");
    return;
  }

  mrbc_value ret = UDPClient_send((const char *)GET_STRING_ARG(1), port.i, vm, &data, (use_dtls.tt == MRBC_TT_TRUE));
  SET_RETURN(ret);
}

void
mrbc_net_init(mrbc_vm *vm)
{
  mrbc_class *class_Net = mrbc_define_class(vm, "Net", mrbc_class_object);

  mrbc_class *class_Net_DNS = mrbc_define_class_under(vm, class_Net, "DNS", mrbc_class_object);
  mrbc_define_method(vm, class_Net_DNS, "resolve", c_net_dns_resolve);

  mrbc_class *class_Net_TCPClient = mrbc_define_class_under(vm, class_Net, "TCPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_TCPClient, "_request_impl", c_net_tcpclient__request_impl);

  mrbc_class *class_Net_UDPClient = mrbc_define_class_under(vm, class_Net, "UDPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_UDPClient, "_send_impl", c_net_udpclient__send_impl);
}
