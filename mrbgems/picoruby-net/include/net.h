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

//typedef struct {
//  char *data;
//  size_t len;
//} recv_data_t;
//
//#if !defined(sleep_ms)
//int sleep_ms(long milliseconds);
//#endif
void lwip_begin(void);
void lwip_end(void);
void Net_sleep_ms(int);
//
err_t Net_get_ip(const char *name, ip_addr_t *ip);
//void TCPClient_send(void *mrb, const char *host, int port, const char *send_data, size_t send_data_len, bool is_tls, recv_data_t *recv_data);
//void UDPClient_send(void *mrb, const char *host, int port, const char *send_data, size_t send_data_len, bool is_dtls, recv_data_t *recv_data);

mrb_value DNS_resolve(mrb_state *mrb, const char *name, bool is_tcp);
mrb_value TCPClient_send(const char *host, int port, mrb_state *mrb, mrb_value send_data, bool is_tls);
mrb_value UDPClient_send(const char *host, int port, mrb_state *mrb, mrb_value send_data, bool is_dtls);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
