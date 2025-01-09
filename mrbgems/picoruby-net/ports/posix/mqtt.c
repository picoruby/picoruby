#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "mrubyc.h"
#include "../../include/net.h"

#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_SUBSCRIBE   0x82
#define MQTT_DISCONNECT  0xE0

typedef struct {
  int socket;
  SSL *ssl;
  SSL_CTX *ssl_ctx;
  bool connected;
} mqtt_connection_t;

static uint8_t *encode_remaining_length(uint8_t *buf, int length) {
  do {
    uint8_t byte = length % 128;
    length /= 128;
    if (length > 0) {
      byte |= 0x80;
    }
    *buf++ = byte;
  } while (length > 0);
  return buf;
}

static mqtt_connection_t *mqtt_connect(const char *host, int port, const char *client_id, bool use_tls) {
  mqtt_connection_t *conn = (mqtt_connection_t *)malloc(sizeof(mqtt_connection_t));
  if (!conn) return NULL;
  memset(conn, 0, sizeof(mqtt_connection_t));

  // ソケットを作成
  conn->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (conn->socket < 0) {
    free(conn);
    return NULL;
  }

  // サーバーに接続
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) {
    close(conn->socket);
    free(conn);
    return NULL;
  }

  if (connect(conn->socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
    close(conn->socket);
    free(conn);
    return NULL;
  }

  // TLSの設定（必要な場合）
  if (use_tls) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    conn->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!conn->ssl_ctx) {
      close(conn->socket);
      free(conn);
      return NULL;
    }

    conn->ssl = SSL_new(conn->ssl_ctx);
    SSL_set_fd(conn->ssl, conn->socket);

    if (SSL_connect(conn->ssl) != 1) {
      SSL_free(conn->ssl);
      SSL_CTX_free(conn->ssl_ctx);
      close(conn->socket);
      free(conn);
      return NULL;
    }
  }

  // MQTTの接続パケットを構築
  uint8_t connect_packet[128];
  uint8_t *ptr = connect_packet;
  
  // Fixed header
  *ptr++ = MQTT_CONNECT;
  ptr++; // Length will be filled later

  // Variable header
  // Protocol name
  *ptr++ = 0; *ptr++ = 4; // Length of "MQTT"
  *ptr++ = 'M'; *ptr++ = 'Q'; *ptr++ = 'T'; *ptr++ = 'T';
  *ptr++ = 4; // Protocol version 4
  *ptr++ = 0x02; // Clean session
  *ptr++ = 0; *ptr++ = 60; // Keep alive 60 seconds

  // Payload
  size_t client_id_len = strlen(client_id);
  *ptr++ = (client_id_len >> 8) & 0xFF;
  *ptr++ = client_id_len & 0xFF;
  memcpy(ptr, client_id, client_id_len);
  ptr += client_id_len;

  // Fill in remaining length
  size_t remaining_length = ptr - connect_packet - 2;
  encode_remaining_length(connect_packet + 1, remaining_length);

  // パケットを送信
  size_t total_length = ptr - connect_packet;
  if (use_tls) {
    SSL_write(conn->ssl, connect_packet, total_length);
  } else {
    send(conn->socket, connect_packet, total_length, 0);
  }

  // CONNACK待機
  uint8_t connack[4];
  int received;
  if (use_tls) {
    received = SSL_read(conn->ssl, connack, sizeof(connack));
  } else {
    received = recv(conn->socket, connack, sizeof(connack), 0);
  }

  if (received != 4 || connack[0] != MQTT_CONNACK || connack[3] != 0) {
    if (use_tls) {
      SSL_free(conn->ssl);
      SSL_CTX_free(conn->ssl_ctx);
    }
    close(conn->socket);
    free(conn);
    return NULL;
  }

  conn->connected = true;
  return conn;
}

static mqtt_connection_t *current_connection = NULL;

mrbc_value MQTTClient_connect(mrbc_vm *vm, const char *host, int port, const char *client_id, bool use_tls) {
  if (current_connection != NULL) {
    // 既に接続が存在する場合は切断
    if (current_connection->ssl) {
      SSL_free(current_connection->ssl);
      SSL_CTX_free(current_connection->ssl_ctx);
    }
    close(current_connection->socket);
    free(current_connection);
  }

  current_connection = mqtt_connect(host, port, client_id, use_tls);
  if (!current_connection) return mrbc_false_value();
  
  return mrbc_true_value();
}

mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic) {
  if (!current_connection || !current_connection->connected) {
    return mrbc_false_value();
  }

  // QoS 0のパブリッシュパケットを構築
  uint8_t header[4] = {MQTT_PUBLISH};
  size_t topic_len = strlen(topic);
  size_t payload_len = payload->string->size;
  size_t remaining_length = 2 + topic_len + payload_len;

  // 残りの長さをエンコード
  size_t header_len = 1;
  encode_remaining_length(header + 1, remaining_length);
  header_len++;

  // パケットを送信
  if (current_connection->ssl) {
    SSL_write(current_connection->ssl, header, header_len);
    SSL_write(current_connection->ssl, topic, topic_len);
    SSL_write(current_connection->ssl, payload->string->data, payload_len);
  } else {
    send(current_connection->socket, header, header_len, 0);
    send(current_connection->socket, topic, topic_len, 0);
    send(current_connection->socket, payload->string->data, payload_len, 0);
  }

  return mrbc_true_value();
}

mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic) {
  if (!current_connection || !current_connection->connected) {
    return mrbc_false_value();
  }

  // QoS 0のサブスクライブパケットを構築
  uint8_t header[4] = {MQTT_SUBSCRIBE, 0};
  size_t topic_len = strlen(topic);
  size_t remaining_length = 2 + 2 + topic_len + 1; // packet ID + topic length + topic + QoS

  // 残りの長さをエンコード
  size_t header_len = 1;
  encode_remaining_length(header + 1, remaining_length);
  header_len++;

  // パケットを送信
  if (current_connection->ssl) {
    SSL_write(current_connection->ssl, header, header_len);
    uint8_t packet_id[2] = {0, 1}; // パケットID
    SSL_write(current_connection->ssl, packet_id, 2);
    SSL_write(current_connection->ssl, topic, topic_len);
    uint8_t qos = 0;
    SSL_write(current_connection->ssl, &qos, 1);
  } else {
    send(current_connection->socket, header, header_len, 0);
    uint8_t packet_id[2] = {0, 1}; // パケットID
    send(current_connection->socket, packet_id, 2, 0);
    send(current_connection->socket, topic, topic_len, 0);
    uint8_t qos = 0;
    send(current_connection->socket, &qos, 1, 0);
  }

  return mrbc_true_value();
}

mrbc_value MQTTClient_disconnect(mrbc_vm *vm) {
  if (!current_connection || !current_connection->connected) {
    return mrbc_false_value();
  }

  uint8_t disconnect_packet = MQTT_DISCONNECT;
  
  if (current_connection->ssl) {
    SSL_write(current_connection->ssl, &disconnect_packet, 1);
    SSL_free(current_connection->ssl);
    SSL_CTX_free(current_connection->ssl_ctx);
  } else {
    send(current_connection->socket, &disconnect_packet, 1, 0);
  }

  close(current_connection->socket);
  free(current_connection);
  current_connection = NULL;

  return mrbc_true_value();
}
