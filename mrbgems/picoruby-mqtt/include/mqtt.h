#ifndef MQTT_DEFINED_H_
#define MQTT_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MQTT_VERIFY_NONE = 0,
    MQTT_VERIFY_PEER = 1,
    MQTT_VERIFY_FAIL_IF_NO_PEER_CERT = 2
} mqtt_verify_mode_t;

typedef enum {
    MQTT_TLS_V1_2 = 0,
    MQTT_TLS_V1_1 = 1,
    MQTT_TLS_V1   = 2
} mqtt_tls_version_t;

typedef struct {
    const char *ca_file;
    const char *cert_file;
    const char *key_file;
    const char *ca_cert;
    const char *client_cert;
    const char *client_key;
    mqtt_verify_mode_t verify_mode;
    mqtt_tls_version_t version;
    bool verify_hostname;
    uint32_t handshake_timeout_ms;
} mqtt_ssl_context_t;

mrbc_value MQTTClient_connect(mrbc_vm *vm, mrbc_value *self, const char *host, int port, const char *client_id, bool use_tls,
                             const char *ca_file, const char *cert_file, const char *key_file,
                             int tls_version, int verify_mode,
                             const char *ca_cert, const char *client_cert, const char *client_key,
                             bool verify_hostname, uint32_t handshake_timeout_ms);
mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic);
mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic);
mrbc_value MQTTClient_disconnect(mrbc_vm *vm);

void MQTT_callback(void);

#define MQTT_ERR_OK              0
#define MQTT_ERR_CONNECT         -1
#define MQTT_ERR_TIMEOUT         -2
#define MQTT_ERR_PROTOCOL        -3
#define MQTT_ERR_MEMORY          -4

#define MQTT_ERR_TLS_INIT        -10
#define MQTT_ERR_TLS_CERT        -11
#define MQTT_ERR_TLS_KEY         -12
#define MQTT_ERR_TLS_HANDSHAKE   -13
#define MQTT_ERR_TLS_VERIFY      -14
#define MQTT_ERR_TLS_VERSION     -15

// Connection timeout in seconds
#define MQTT_CONNECT_TIMEOUT     10

#ifdef __cplusplus
}
#endif

#endif /* MQTT_DEFINED_H_ */
