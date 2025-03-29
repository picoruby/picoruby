
//#include "include/common.h"

mrbc_value
DNS_resolve(mrbc_vm *vm, const char *name, bool is_tcp)
{
  (void)is_tcp;
  ip_addr_t ip;
  mrbc_value ret;
  ip4_addr_set_zero(&ip);
  Net_get_ip(name, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    char buf[16];
    ipaddr_ntoa_r(&ip, buf, 16);
    ret = mrbc_string_new(vm, buf, strlen(buf));
  } else {
    ret = mrbc_nil_value();
  }
  return ret;
}

