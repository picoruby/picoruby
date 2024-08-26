#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

mrbc_value DNS_resolve(mrbc_vm *vm, const char *name);
mrbc_value TCPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_tls);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
