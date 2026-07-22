/*
 * Network helper functions for rp2040 LwIP implementation
 */

#include "../../include/socket.h"
#include <stdarg.h>
#include <stdio.h>
#include "picoruby/debug.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"

static char net_error[SOCKET_ERROR_MSG_LEN];

static void
Net_clear_error(void)
{
  net_error[0] = '\0';
}

void
Net_set_last_error(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(net_error, sizeof(net_error), format, args);
  va_end(args);

  printf("%s\n", net_error);
}

const char*
Net_get_last_error(void)
{
  return net_error;
}

static bool
cyw43_lwip_ready(const char *operation)
{
  if (cyw43_arch_async_context() != NULL) {
    return true;
  }

  Net_set_last_error(
    "%s: CYW43/LwIP is not initialized; call CYW43.init before sockets",
    operation
  );
  return false;
}

static bool
cyw43_wifi_ready(const char *operation)
{
  int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  if (status == CYW43_LINK_UP) {
    return true;
  }

  Net_set_last_error(
    "%s: CYW43 Wi-Fi is not connected; call CYW43.connect_timeout before sockets",
    operation
  );
  return false;
}

/* Busy wait with CYW43 polling for rp2040 */
void
Net_busy_wait_ms(int ms)
{
  if (cyw43_arch_async_context() != NULL) {
    cyw43_arch_poll();
  }
  busy_wait_ms(ms);
  if (cyw43_arch_async_context() != NULL) {
    cyw43_arch_poll();
  }
}

/* Lock LwIP for thread safety */
void
lwip_begin(void)
{
  if (!cyw43_lwip_ready("lwip_begin")) {
    return;
  }
  cyw43_arch_lwip_begin();
}

/* Unlock LwIP */
void
lwip_end(void)
{
  if (cyw43_arch_async_context() == NULL) {
    return;
  }
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

  Net_clear_error();

  ip_addr_t *addr = (ip_addr_t *)ip;
  ip_addr_set_zero(addr);

  /* First try to parse as numeric IP address */
  if (ip4addr_aton(name, addr)) {
    return 0;
  }

  /* Not a numeric IP, try DNS resolution */
  if (!cyw43_lwip_ready("Net_get_ip")) {
    return -1;
  }
  if (!cyw43_wifi_ready("Net_get_ip")) {
    return -1;
  }
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
      Net_busy_wait_ms(100);
    }

    if (!ip_addr_isany(addr)) {
      return 0;
    }
    D("Net_get_ip: DNS resolution timed out for %s\n", name);
    Net_set_last_error("Net_get_ip: DNS resolution timed out for %s", name);
  } else {
    D("Net_get_ip: DNS failed for %s with error %d\n", name, err);
    Net_set_last_error("Net_get_ip: DNS failed for %s with error %d", name, err);
  }

  return -1;
}
