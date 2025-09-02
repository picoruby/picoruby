#include "../include/net.h"
#include "lwip/pbuf.h"

err_t
DNS_resolve(const char *name, bool is_tcp, char *outbuf, size_t outlen)
{
  (void)is_tcp;
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  err_t err = Net_get_ip(name, &ip);
  if (err != ERR_OK) {
    outbuf[0] = '\0';
    return err;
  }
  if (!ip4_addr_isloopback(&ip)) {
    ipaddr_ntoa_r(&ip, outbuf, outlen);
  } else {
    outbuf[0] = '\0';
    return ERR_ARG;
  }
  return ERR_OK;
}

