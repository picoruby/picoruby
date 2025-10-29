#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#include "lwipopts.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tls.h"
#include "lwip/udp.h"

#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DNS_OUTBUF_SIZE 16

#if defined(PICORB_VM_MRUBY)
#define MRB mrb_state *mrb = cs->mrb
#else
#define MRB
#endif

typedef struct {
  const char *host;
  int port;
  const char *send_data;
  size_t send_data_len;
  bool is_tls;
} net_request_t;

typedef struct {
  char *recv_data;
  size_t recv_data_len;
} net_response_t;

void DNS_resolve(const char *name, bool is_tcp, char *outbuf, size_t outlen);
bool TCPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res);
bool UDPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res);

void lwip_begin(void);
void lwip_end(void);
void Net_sleep_ms(int);
err_t Net_get_ip(const char *name, ip_addr_t *ip);
const char *Net_ipaddr(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
