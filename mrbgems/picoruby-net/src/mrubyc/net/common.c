#define PICO_CYW43_ARCH_POLL 1
#include "lwipopts.h"
#define LWIP_DNS 1
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#define LWIP_ALTCP 1
#define LWIP_ALTCP_TLS 1
#include "lwip/altcp_tls.h"
#include "lwip/udp.h"

//#include "../include/mbedtls_debug.h"

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
