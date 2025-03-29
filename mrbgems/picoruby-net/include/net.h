#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

//mrbc_value DNS_resolve(mrbc_vm *vm, const char *name, bool is_tcp);
//mrbc_value TCPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_tls);
//mrbc_value UDPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_dtls);

#if !defined(sleep_ms)
int sleep_ms(long milliseconds);
#endif
void lwip_begin(void);
void lwip_end(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
