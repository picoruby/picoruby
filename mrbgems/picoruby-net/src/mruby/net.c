#include "mruby.h"
#include "mruby/presym.h"
#include "mruby.h"
#include "../include/net.h"

static mrb_value
mrb_net_dns_s_resolve(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_bool is_tls;
  mrb_get_args(mrb, "zb", &host, &is_tls);
  char outbuf[OUTBUF_SIZE];
  DNS_resolve(host, is_tls, outbuf, sizeof(outbuf));
  if (outbuf[0] == '\0') {
    return mrb_nil_value();
  } else {
    return mrb_str_new_cstr(mrb, outbuf);
  }
}

static mrb_value
mrb_net_tcpclient_s__request_impl(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  mrb_value data;
  mrb_bool use_dtls;
  mrb_get_args(mrb, "ziob", &host, &port, &data, &use_dtls);
  return TCPClient_send(host, port, mrb, data, use_dtls);
}

static mrb_value
mrb_net_udpclient_s__send_impl(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  mrb_value data;
  mrb_bool use_dtls;
  mrb_get_args(mrb, "ziob", &host, &port, &data, &use_dtls);
  return UDPClient_send(host, port, mrb, data, use_dtls);
}

void
mrb_picoruby_net_gem_init(mrb_state* mrb)
{
  struct RClass *class_Net = mrb_define_class_id(mrb, MRB_SYM(Net), mrb->object_class);

  struct RClass *class_Net_DNS = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(DNS), mrb->object_class);
  mrb_define_class_method_id(mrb, class_Net_DNS, MRB_SYM(resolve), mrb_net_dns_s_resolve, MRB_ARGS_REQ(2));

  struct RClass *class_Net_TCPClient = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(TCPClient), mrb->object_class);
  mrb_define_class_method_id(mrb, class_Net_TCPClient, MRB_SYM(_request_impl), mrb_net_tcpclient_s__request_impl, MRB_ARGS_REQ(4));

  struct RClass *class_Net_UDPClient = mrb_define_class_under_id(mrb, class_Net, MRB_SYM(UDPClient), mrb->object_class);
  mrb_define_class_method_id(mrb, class_Net_UDPClient, MRB_SYM(_send_impl), mrb_net_udpclient_s__send_impl, MRB_ARGS_REQ(4));
}
void
mrb_picoruby_net_gem_final(mrb_state* mrb)
{
}
