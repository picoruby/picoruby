#include "../include/net.h"
#include "mrubyc.h"

void
err_to_message(err_t err, char *err_message)
{
  switch (err) {
  case ERR_OK:
    snprintf((char *)err_message, 64, "No error, everything OK.");
    break;
  case ERR_MEM:
    snprintf((char *)err_message, 64, "Out of memory error.");
    break;
      case ERR_BUF:
    snprintf((char *)err_message, 64, "Buffer error.");
    break;
  case ERR_TIMEOUT:
    snprintf((char *)err_message, 64, "Timeout.");
    break;
  case ERR_RTE:
    snprintf((char *)err_message, 64, "Routing problem.");
    break;
  case ERR_INPROGRESS:
    snprintf((char *)err_message, 64, "Operation in progress.");
    break;
  case ERR_VAL:
    snprintf((char *)err_message, 64, "Illegal value.");
    break;
  case ERR_WOULDBLOCK:
    snprintf((char *)err_message, 64, "Operation would block.");
    break;
  case ERR_USE:
    snprintf((char *)err_message, 64, "Address in use.");
    break;
  case ERR_ALREADY:
    snprintf((char *)err_message, 64, "Already connecting.");
    break;
  case ERR_ISCONN:
    snprintf((char *)err_message, 64, "Connection already established.");
    break;
  case ERR_CONN:
    snprintf((char *)err_message, 64, "Not connected.");
    break;
  case ERR_IF:
    snprintf((char *)err_message, 64, "Low-level netif error.");
    break;
  case ERR_ABRT:
    snprintf((char *)err_message, 64, "Connection aborted.");
    break;
  case ERR_RST:
    snprintf((char *)err_message, 64, "Connection reset.");
    break;
  case ERR_CLSD:
    snprintf((char *)err_message, 64, "Connection closed.");
    break;
  case ERR_ARG:
    snprintf((char *)err_message, 64, "Illegal argument.");
    break;
  default:
    snprintf((char *)err_message, 64, "Unknown error code: %d", err);
    break;
  }
}

static void
c_net_dns_resolve(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || 2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  const char *host = (const char *)GET_STRING_ARG(1);
  bool is_tls = (v[2].tt == MRBC_TT_TRUE);
  char outbuf[DNS_OUTBUF_SIZE];

  err_t err = DNS_resolve(host, is_tls, outbuf, DNS_OUTBUF_SIZE);
  if (err != ERR_OK) {
    char err_message[64];
    err_to_message(err, err_message);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), err_message);
    return;
  }
  if (outbuf[0] == '\0') {
    SET_NIL_RETURN();
  } else {
    mrbc_value ret = mrbc_string_new_cstr(vm, outbuf);
    SET_RETURN(ret);
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
  mrbc_value data = GET_ARG(3);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mrbc_value is_tls = GET_ARG(4);
  if (!((is_tls.tt == MRBC_TT_TRUE) || (is_tls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  net_request_t req = {
    .host = (const char *)host.string->data,
    .port = port.i,
    .send_data = (const char *)data.string->data,
    .send_data_len = data.string->size,
    .is_tls = (is_tls.tt == MRBC_TT_TRUE),
  };
  net_response_t res = {
    .recv_data = mrbc_raw_calloc(1, 1),
    .recv_data_len = 0
  };

  if (TCPClient_send(vm, &req, &res)) {
    mrb_value ret = mrbc_string_new(vm, res.recv_data, res.recv_data_len);
    SET_RETURN(ret);
  } else {
    SET_NIL_RETURN();
  }
  mrbc_free(vm, res.recv_data);
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
  mrbc_value is_tls = GET_ARG(4);
  if (!((is_tls.tt == MRBC_TT_TRUE) || (is_tls.tt == MRBC_TT_FALSE))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument for is_tls");
    return;
  }

  net_request_t req = {
    .host = (const char *)host.string->data,
    .port = port.i,
    .send_data = (const char *)data.string->data,
    .send_data_len = data.string->size,
    .is_tls = (is_tls.tt == MRBC_TT_TRUE),
  };
  net_response_t res = {
    .recv_data = mrbc_raw_calloc(1, 1),
    .recv_data_len = 0
  };

  if (UDPClient_send(vm, &req, &res)) {
    mrb_value ret = mrbc_string_new(vm, res.recv_data, res.recv_data_len);
    SET_RETURN(ret);
  } else {
    SET_NIL_RETURN();
  }
  mrbc_free(vm, res.recv_data);
}

void
mrbc_net_init(mrbc_vm *vm)
{
  mrbc_class *module_Net = mrbc_define_module(vm, "Net");

  mrbc_class *class_Net_DNS = mrbc_define_class_under(vm, module_Net, "DNS", mrbc_class_object);
  mrbc_define_method(vm, class_Net_DNS, "resolve", c_net_dns_resolve);

  mrbc_class *class_Net_TCPClient = mrbc_define_class_under(vm, module_Net, "TCPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_TCPClient, "_request_impl", c_net_tcpclient__request_impl);

  mrbc_class *class_Net_UDPClient = mrbc_define_class_under(vm, module_Net, "UDPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_UDPClient, "_send_impl", c_net_udpclient__send_impl);
}
