#include "mrubyc.h"

void
mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void) level);
  console_printf("%s:%04d: %s", file, line, str);
}

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

  char ipaddr[16] = {0};
  DNS_resolve((const char *)GET_STRING_ARG(1), ipaddr, (GET_ARG(2).tt == MRBC_TT_TRUE));
  if (ipaddr[0] != '\0') {
    mrbc_value ret = mrbc_string_new_cstr(vm, ipaddr);
    SET_RETURN(ret);
  } else {
    SET_NIL_RETURN();
  }
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
  if (v[3].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  const char *send_data = (const char *)GET_STRING_ARG(3);
  size_t send_data_len = v[3].string->size;
  mrbc_value is_tls = GET_ARG(4);
  if (!((is_tls.tt == MRBC_TT_TRUE) || (is_tls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  recv_data_t *recv_data = (recv_data_t *)mrbc_alloc(vm, sizeof(recv_data_t));
  recv_data->data = NULL;
  recv_data->len = 0;
  TCPClient_send((const char *)GET_STRING_ARG(1), port.i, send_data, send_data_len, (is_tls.tt == MRBC_TT_TRUE), recv_data);
  if (recv_data->data == NULL) {
    SET_NIL_RETURN();
  } else {
    mrbc_value ret = mrbc_string_new_alloc(vm, recv_data->data, recv_data->len);
    SET_RETURN(ret);
  }
  mrbc_free(vm, recv_data);
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
  if (v[3].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for data");
    return;
  }
  const char* data = (const char *)GET_STRING_ARG(3);
  size_t data_len = v[3].string->size;
  mrbc_value use_dtls = GET_ARG(4);
  if (!((use_dtls.tt == MRBC_TT_TRUE) || (use_dtls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for use_dtls");
    return;
  }

  recv_data_t *recv_data = (recv_data_t *)mrbc_alloc(vm, sizeof(recv_data_t));
  recv_data->data = NULL;
  recv_data->len = 0;
  UDPClient_send((const char *)GET_STRING_ARG(1), port.i, data, data_len, (use_dtls.tt == MRBC_TT_TRUE), recv_data);
  if (recv_data->data == NULL) {
    SET_NIL_RETURN();
  } else {
    mrbc_value ret = mrbc_string_new_alloc(vm, recv_data->data, recv_data->len);
    SET_RETURN(ret);
  }
  mrbc_free(vm, recv_data);
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
