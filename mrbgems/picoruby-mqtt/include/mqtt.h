#ifndef MQTT_DEFINED_H_
#define MQTT_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

mrbc_value MQTTClient_connect(mrbc_vm *vm, mrbc_value *self, const char *host, int port, const char *client_id, bool use_tls);
mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic);
mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic);
mrbc_value MQTTClient_disconnect(mrbc_vm *vm);

void MQTT_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_DEFINED_H_ */
