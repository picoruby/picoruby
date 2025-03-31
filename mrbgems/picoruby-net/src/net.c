#include "../include/net.h"


static void
dns_found(const char *name, const ip_addr_t *ip, void *arg)
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
  err_t err = get_ip_impl(name, ip);
  if (err != ERR_OK) {
    return err;
  }
  while (!ip_addr_get_ip4_u32(ip)) {
    sleep_ms(50);
  }
  return ERR_OK;
}

static void
DNS_resolve(const char *name, char* ipaddr, bool is_tcp)
{
  (void)is_tcp;
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  Net_get_ip(name, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    ipaddr_ntoa_r(&ip, ipaddr, 16);
  }
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/net.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/net.c"

#endif

