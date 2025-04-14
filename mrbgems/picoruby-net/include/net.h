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

void lwip_begin(void);
void lwip_end(void);
void Net_sleep_ms(int);
err_t Net_get_ip(const char *name, ip_addr_t *ip);

mrb_value DNS_resolve(mrb_state *mrb, const char *name, bool is_tcp);
mrb_value TCPClient_send(const char *host, int port, mrb_state *mrb, mrb_value send_data, bool is_tls);
mrb_value UDPClient_send(const char *host, int port, mrb_state *mrb, mrb_value send_data, bool is_dtls);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
