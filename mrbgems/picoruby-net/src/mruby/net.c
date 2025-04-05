#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"

void
mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void) level);
  printf("%s:%04d: %s", file, line, str);
}

static mrb_value
mrb_net_dns_s_resolve(mrb_state *mrb, mrb_value klass)
{
  const char *host;
  mrb_bool is_tcp;
  mrb_get_args(mrb, "sb", &host, &is_tcp);

  char ipaddr[16] = {0};
  DNS_resolve(host, ipaddr, is_tcp);
  if (ipaddr[0] != '\0') {
    return mrb_str_new_cstr(mrb, ipaddr);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_net_tcpclient__request_impl(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  mrb_value content;
  mrb_bool is_tls;
  mrb_get_args(mrb, "siSb", &host, &port, &content, &is_tls);

  recv_data_t *recv_data = (recv_data_t *)mrb_malloc(mrb, sizeof(recv_data_t));
  recv_data->data = NULL;
  recv_data->len = 0;
  TCPClient_send(mrb, host, port, RSTRING_PTR(content), RSTRING_LEN(content), is_tls, recv_data);
  mrb_value ret;
  if (recv_data->data == NULL) {
    ret = mrb_nil_value();
  } else {
    ret = mrb_str_new(mrb, recv_data->data, recv_data->len);
  }
  mrb_free(mrb, recv_data);
  return ret;
}

static mrb_value
mrb_net_udpclient__send_impl(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  mrb_value content;
  mrb_bool is_dtls;
  mrb_get_args(mrb, "siSb", &host, &port, &content, &is_dtls);

  recv_data_t *recv_data = (recv_data_t *)mrb_malloc(mrb, sizeof(recv_data_t));
  recv_data->data = NULL;
  recv_data->len = 0;
  UDPClient_send(mrb, host, port, RSTRING_PTR(content), RSTRING_LEN(content), is_dtls, recv_data);
  mrb_value ret;
  if (recv_data->data == NULL) {
    ret = mrb_nil_value();
  } else {
    ret = mrb_str_new(mrb, recv_data->data, recv_data->len);
  }
  mrb_free(mrb, recv_data);
  return ret;
}

void
mrb_picoruby_net_gem_init(mrb_state* mrb)
{
  struct RClass *class_Net = mrb_define_class_id(mrb, MRB_SYM(Net), mrb->object_class);

  struct RClass *class_Net_DNS = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(DNS), mrb->object_class);
  mrb_define_class_method_id(mrb, class_Net_DNS, MRB_SYM(resolve), mrb_net_dns_s_resolve, MRB_ARGS_REQ(1));

  struct RClass *class_Net_TCPClient = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(TCPClient), mrb->object_class);
  mrb_define_method_id(mrb, class_Net_TCPClient, MRB_SYM(_request_impl), mrb_net_tcpclient__request_impl, MRB_ARGS_REQ(4));

  struct RClass *class_Net_UDPClient = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(UDPClient), mrb->object_class);
  mrb_define_method_id(mrb, class_Net_UDPClient, MRB_SYM(_send_impl), mrb_net_udpclient__send_impl, MRB_ARGS_REQ(4));
}

void
mrb_picoruby_net_gem_final(mrb_state* mrb)
{
}
