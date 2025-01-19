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

void
mrbc_net_init(mrbc_vm *vm)
{
  mrbc_class *class_Net = mrbc_define_class(vm, "Net", mrbc_class_object);

#if defined(PICORB_VM_MRUBY)

#include "mruby/net.c"

  mrbc_class *class_Net_UDPClient = mrbc_define_class_under(vm, class_Net, "UDPClient", mrbc_class_object);
  mrbc_define_method(vm, class_Net_UDPClient, "_send_impl", c_net_udpclient__send_impl);
}
