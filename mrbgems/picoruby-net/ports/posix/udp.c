#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "mrubyc.h"

#define NET_UDP_STATE_NONE 0
#define NET_UDP_STATE_READY 1
#define NET_UDP_STATE_ERROR 99

typedef struct {
  int state;
  int socket;
  SSL *ssl;
  SSL_CTX *ssl_ctx;
  struct sockaddr_in server_addr;
} udp_connection_state;

static bool
UDPClient_init_dtls(udp_connection_state *cs)
{
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  const SSL_METHOD *method = DTLS_client_method();
  if (method == NULL) {
    fprintf(stderr, "Failed to get DTLS client method\n");
    return false;
  }

  cs->ssl_ctx = SSL_CTX_new(method);
  if (cs->ssl_ctx == NULL) {
    fprintf(stderr, "Failed to create SSL context\n");
    return false;
  }

  cs->ssl = SSL_new(cs->ssl_ctx);
  if (cs->ssl == NULL) {
    fprintf(stderr, "Failed to create SSL\n");
    SSL_CTX_free(cs->ssl_ctx);
    return false;
  }

  if (SSL_set_fd(cs->ssl, cs->socket) != 1) {
    fprintf(stderr, "Failed to set SSL file descriptor\n");
    SSL_free(cs->ssl);
    SSL_CTX_free(cs->ssl_ctx);
    return false;
  }

  SSL_set_connect_state(cs->ssl);
  return true;
}

static udp_connection_state *
UDPClient_init(const char *host, int port, bool use_dtls)
{
    if (host == NULL || port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid host or port\n");
        return NULL;
    }

    udp_connection_state *cs = (udp_connection_state *)calloc(1, sizeof(udp_connection_state));
    if (cs == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int status = getaddrinfo(host, port_str, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
        free(cs);
        return NULL;
    }

    cs->socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (cs->socket == -1) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        freeaddrinfo(result);
        free(cs);
        return NULL;
    }

    memcpy(&cs->server_addr, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    if (use_dtls) {
        if (!UDPClient_init_dtls(cs)) {
            close(cs->socket);
            free(cs);
            return NULL;
        }
    }

    cs->state = NET_UDP_STATE_READY;
    return cs;
}

static void
UDPClient_close(udp_connection_state *cs)
{
  if (cs->ssl) {
    SSL_shutdown(cs->ssl);
    SSL_free(cs->ssl);
  }
  if (cs->ssl_ctx) {
    SSL_CTX_free(cs->ssl_ctx);
  }
  close(cs->socket);
  free(cs);
}

mrbc_value
UDPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool use_dtls)
{
  udp_connection_state *cs = UDPClient_init(host, port, use_dtls);
  if (cs == NULL) {
    fprintf(stderr, "Failed to initialize UDP connection\n");
    return mrbc_nil_value();
  }

  ssize_t sent;
  do {
    if (use_dtls) {
      sent = SSL_write(cs->ssl, send_data->string->data, send_data->string->size);
    } else {
      sent = sendto(cs->socket, send_data->string->data, send_data->string->size, 0,
                    (struct sockaddr *)&cs->server_addr, sizeof(cs->server_addr));
    }
  } while (sent < 0 && errno == EINTR);

  if (sent < 0) {
    fprintf(stderr, "Failed to send data: %s\n", strerror(errno));
    UDPClient_close(cs);
    return mrbc_nil_value();
  }

  char buffer[1024];
  ssize_t received;
  mrbc_value recv_data = mrbc_string_new(vm, NULL, 0);
  if (recv_data.string == NULL) {
    fprintf(stderr, "Failed to allocate memory for receive buffer\n");
    UDPClient_close(cs);
    return mrbc_nil_value();
    return mrbc_nil_value();
  }

  struct timeval tv;
  tv.tv_sec = 10;  // 10 seconds timeout
  tv.tv_usec = 0;
  if (setsockopt(cs->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
    fprintf(stderr, "Failed to set socket timeout: %s\n", strerror(errno));
  }

  do {
    if (use_dtls) {
      received = SSL_read(cs->ssl, buffer, sizeof(buffer));
    } else {
      socklen_t server_len = sizeof(cs->server_addr);
      received = recvfrom(cs->socket, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&cs->server_addr, &server_len);
    }
  } while (received < 0 && errno == EINTR);

  if (received > 0) {
    if (mrbc_string_append_cbuf(&recv_data, buffer, received) != 0) {
      fprintf(stderr, "Failed to append received data to string\n");
      mrbc_string_delete(&recv_data);
      UDPClient_close(cs);
      return mrbc_nil_value();
    }
  } else if (received == 0) {
    fprintf(stderr, "Connection closed by peer\n");
  } else {
    fprintf(stderr, "Failed to receive data: %s\n", strerror(errno));
  }

  UDPClient_close(cs);
  return recv_data;
}
