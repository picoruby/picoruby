#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

//#define PICO_CYW43_ARCH_POLL 1
#include "lwipopts.h"
//#define LWIP_DNS 1
#include "lwip/dns.h"
#include "lwip/pbuf.h"
//#define LWIP_ALTCP 1
//#define LWIP_ALTCP_TLS 1
#include "lwip/altcp_tls.h"
#include "lwip/udp.h"

#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char *data;
  size_t len;
} recv_data_t;

#if !defined(sleep_ms)
int sleep_ms(long milliseconds);
#endif
void lwip_begin(void);
void lwip_end(void);

err_t Net_get_ip(const char *name, ip_addr_t *ip);
void TCPClient_send(const char *host, int port, const char *send_data, size_t send_data_len, bool is_tls, recv_data_t *recv_data);
void UDPClient_send(const char *host, int port, const char *send_data, size_t send_data_len, bool is_dtls, recv_data_t *recv_data);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
