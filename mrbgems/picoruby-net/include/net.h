#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

mrbc_value DNS_resolve(mrbc_vm *vm, const char *name, bool is_tcp);
mrbc_value TCPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_tls);
mrbc_value UDPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_dtls);
mrbc_value MQTTClient_connect(mrbc_vm *vm, const char *host, int port, const char *client_id, bool use_tls);
mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic);
mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic);
mrbc_value MQTTClient_disconnect(mrbc_vm *vm);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
