#include "../../include/net.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"

void dns_found(const char *name, const ip_addr_t *ip, void *arg)
{
  ip_addr_t *result = (ip_addr_t *)arg;
  if (ip) {
    ip4_addr_copy(*result, *ip);
  } else {
    ip4_addr_set_loopback(result);
  }
  return;
}

err_t get_ip(const char *name, ip_addr_t *ip)
{
  cyw43_arch_lwip_begin();
  err_t err = dns_gethostbyname(name, ip, dns_found, ip);
  cyw43_arch_lwip_end();
  return err;
}

mrbc_value *DNS_resolve(mrbc_vm *vm, const char *name)
{
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  get_ip(name, &ip);
  while (!ip_addr_get_ip4_u32(&ip)) {
    sleep_ms(50);
  }
  if(!ip4_addr_isloopback(&ip)) {
    char buf[16];
    ipaddr_ntoa_r(ip, buf, 16);
    mrbc_value ret = mrbc_string_new(vm, buf, strlen(buf)));
    SET_RETURN(ret);
  } else {
    SET_NIL_RETURN();
  }
}
