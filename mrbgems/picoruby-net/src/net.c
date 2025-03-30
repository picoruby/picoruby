#include "../include/net.h"

#define PICO_CYW43_ARCH_POLL 1
#include "lwipopts.h"
#define LWIP_DNS 1
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#define LWIP_ALTCP 1
#define LWIP_ALTCP_TLS 1
#include "lwip/altcp_tls.h"
#include "lwip/udp.h"

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

static err_t
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
  mrbc_value ret;
  ip4_addr_set_zero(&ip);
  Net_get_ip(name, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    ipaddr_ntoa_r(&ip, ipaddr, 16);
  }
}

#if defined(PICORB_VM_MRUBY)

#include "mruby.h"
#include "mruby/mbedtls_debug.c"
#include "mruby/net/dns.c"
#include "mruby/net/tcp.c"
#include "mruby/net/udp.c"
#include "mruby/net.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc.h"
#include "mrubyc/mbedtls_debug.c"
#include "mrubyc/net/dns.c"
#include "mrubyc/net/tcp.c"
#include "mrubyc/net/udp.c"
#include "mrubyc/net.c"

#endif

