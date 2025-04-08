#include "../include/net.h"
#include "lwipopts.h"
#include "lwip/pbuf.h"

#include "mruby.h"

mrb_value
DNS_resolve(mrb_state *mrb, const char *name, bool is_tcp)
{
  (void)is_tcp;
  ip_addr_t ip;
  mrb_value ret;
  ip4_addr_set_zero(&ip);
  Net_get_ip(name, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    char buf[16];
    ipaddr_ntoa_r(&ip, buf, 16);
    ret = mrb_str_new(mrb, buf, strlen(buf));
  } else {
    ret = mrb_nil_value();
  }
  return ret;
}

