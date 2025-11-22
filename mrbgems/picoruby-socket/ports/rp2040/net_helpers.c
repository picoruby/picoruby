/*
 * Network helper functions for rp2040 LwIP implementation
 */

#include "../../include/socket.h"
#include "picoruby/debug.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"

/* Sleep with CYW43 polling for rp2040 */
void
Net_sleep_ms(int ms)
{
  /* Poll before and after sleep to ensure events are processed */
  cyw43_arch_poll();
  sleep_ms(ms);
  cyw43_arch_poll();
}

/* Lock LwIP for thread safety */
void
lwip_begin(void)
{
  cyw43_arch_lwip_begin();
}

/* Unlock LwIP */
void
lwip_end(void)
{
  cyw43_arch_lwip_end();
}

/* DNS callback */
static void
dns_callback(const char *name, const ip_addr_t *ip, void *arg)
{
  ip_addr_t *result = (ip_addr_t *)arg;
  if (ip) {
    ip_addr_copy(*result, *ip);
  } else {
    ip_addr_set_zero(result);
  }
}

/* Resolve hostname to IP address */
int
Net_get_ip(const char *name, void *ip)
{
  if (!name || !ip) {
    return -1;
  }

  ip_addr_t *addr = (ip_addr_t *)ip;

  lwip_begin();
  err_t err = dns_gethostbyname(name, addr, dns_callback, addr);
  lwip_end();

  /* If ERR_OK, DNS was cached and ip is already set */
  if (err == ERR_OK) {
    return 0;
  }

  /* If ERR_INPROGRESS, wait for DNS resolution */
  if (err == ERR_INPROGRESS) {
    /* Wait for DNS callback */
    int max_wait = 100; /* 10 seconds */
    while (ip_addr_isany(addr) && max_wait-- > 0) {
      Net_sleep_ms(100);
    }

    if (!ip_addr_isany(addr)) {
      return 0;
    }
    D("Net_get_ip: DNS resolution timed out for %s\n", name);
  } else {
    D("Net_get_ip: DNS failed for %s with error %d\n", name, err);
  }

  return -1;
}
