/*
 * Network helper functions for rp2040 LwIP implementation
 */

#include "../../include/socket.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"

/* Sleep with CYW43 polling for rp2040 */
void
Net_sleep_ms(int ms)
{
  cyw43_arch_poll();
  sleep_ms(ms);
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
dns_found_callback(const char *name, const ip_addr_t *ip, void *arg)
{
  ip_addr_t *result = (ip_addr_t *)arg;
  if (ip) {
    ip4_addr_copy(*result, *ip);
  } else {
    ip4_addr_set_loopback(result);
  }
}

/* Resolve hostname to IP address */
int
Net_get_ip(const char *name, struct ip_addr *ip)
{
  if (!name || !ip) {
    return -1;
  }

  lwip_begin();
  err_t err = dns_gethostbyname(name, ip, dns_found_callback, ip);
  lwip_end();

  /* If ERR_OK, DNS was cached and ip is already set */
  if (err == ERR_OK) {
    return 0;
  }

  /* If ERR_INPROGRESS, wait for DNS resolution */
  if (err == ERR_INPROGRESS) {
    /* Wait for DNS callback */
    int max_wait = 100; /* 10 seconds */
    while (!ip_addr_get_ip4_u32(ip) && max_wait-- > 0) {
      Net_sleep_ms(100);
    }

    if (ip_addr_get_ip4_u32(ip)) {
      return 0;
    }
  }

  return -1;
}
