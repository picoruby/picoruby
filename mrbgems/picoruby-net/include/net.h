#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform-specific includes */
#ifdef PICORB_PLATFORM_POSIX
  /* POSIX: No LwIP headers needed here */
  /* Platform-specific headers are included in implementation files */
#else
  /* Microcontroller platforms: LwIP */
  #include "lwipopts.h"
  #include "lwip/dns.h"
  #include "lwip/pbuf.h"
  #include "lwip/altcp_tls.h"
  #include "lwip/udp.h"
#endif

#define DNS_OUTBUF_SIZE 16

#if defined(PICORB_VM_MRUBY)
#define MRB mrb_state *mrb = cs->mrb
#else
#define MRB
#endif

/* Common data structures (platform-independent) */
typedef struct {
  const char *host;
  int port;
  const char *send_data;
  size_t send_data_len;
  bool is_tls;
} net_request_t;

#define NET_ERROR_MESSAGE_SIZE 128

typedef struct {
  char *recv_data;
  size_t recv_data_len;
  char error_message[NET_ERROR_MESSAGE_SIZE];
} net_response_t;

/* Common API (implemented per platform) */
void DNS_resolve(const char *name, bool is_tcp, char *outbuf, size_t outlen);
bool TCPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res);
bool UDPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res);
void Net_sleep_ms(int ms);
const char *Net_get_ipv4_address(char *buf, size_t buflen);

#ifdef PICORB_PLATFORM_POSIX
  /* POSIX-specific functions */
  /* (none currently - lwip_begin/end not needed) */
#else
  /* LwIP-specific functions */
  void lwip_begin(void);
  void lwip_end(void);
  err_t Net_get_ip(const char *name, ip_addr_t *ip);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
