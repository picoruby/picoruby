#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdbool.h>

#include "mrubyc.h"

#define NET_TCP_STATE_NONE 0
#define NET_TCP_STATE_CONNECTED 1
#define NET_TCP_STATE_ERROR 99

typedef struct {
  int state;
  int socket;
  SSL *ssl;
  SSL_CTX *ssl_ctx;
} tcp_connection_state;

mrbc_value
DNS_resolve(mrbc_vm *vm, const char *name)
{
  struct addrinfo hints, *res;
  char addrstr[INET_ADDRSTRLEN];
  mrbc_value ret;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(name, NULL, &hints, &res) != 0) {
    return mrbc_nil_value();
  }

  struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
  inet_ntop(AF_INET, &(addr->sin_addr), addrstr, INET_ADDRSTRLEN);

  ret = mrbc_string_new(vm, addrstr, strlen(addrstr));
  freeaddrinfo(res);
  return ret;
}

static tcp_connection_state *
TCPClient_connect(const char *host, int port, bool is_tls)
{
  tcp_connection_state *cs = (tcp_connection_state *)malloc(sizeof(tcp_connection_state));
  cs->state = NET_TCP_STATE_NONE;
  cs->ssl = NULL;
  cs->ssl_ctx = NULL;

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  if (getaddrinfo(host, port_str, &hints, &res) != 0) {
    free(cs);
    return NULL;
  }

  cs->socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (cs->socket == -1) {
    freeaddrinfo(res);
    free(cs);
    return NULL;
  }

  if (connect(cs->socket, res->ai_addr, res->ai_addrlen) == -1) {
    close(cs->socket);
    freeaddrinfo(res);
    free(cs);
    return NULL;
  }

  freeaddrinfo(res);

  if (is_tls) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    cs->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (cs->ssl_ctx == NULL) {
      close(cs->socket);
      free(cs);
      return NULL;
    }

    cs->ssl = SSL_new(cs->ssl_ctx);
    SSL_set_fd(cs->ssl, cs->socket);

    if (SSL_connect(cs->ssl) <= 0) {
      SSL_free(cs->ssl);
      SSL_CTX_free(cs->ssl_ctx);
      close(cs->socket);
      free(cs);
      return NULL;
    }
  }

  cs->state = NET_TCP_STATE_CONNECTED;
  return cs;
}

static void
TCPClient_close(tcp_connection_state *cs)
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
TCPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_tls)
{
  tcp_connection_state *cs = TCPClient_connect(host, port, is_tls);
  if (cs == NULL) {
    return mrbc_nil_value();
  }

  ssize_t sent;
  if (is_tls) {
    sent = SSL_write(cs->ssl, send_data->string->data, send_data->string->size);
  } else {
    sent = send(cs->socket, send_data->string->data, send_data->string->size, 0);
  }

  if (sent < 0) {
    TCPClient_close(cs);
    return mrbc_nil_value();
  }

  char buffer[1024];
  ssize_t received;
  mrbc_value recv_data = mrbc_string_new(vm, NULL, 0);

  while (1) {
    if (is_tls) {
      received = SSL_read(cs->ssl, buffer, sizeof(buffer));
    } else {
      received = recv(cs->socket, buffer, sizeof(buffer), 0);
    }

    if (received <= 0) break;

    mrbc_string_append_cbuf(&recv_data, buffer, received);
  }

  TCPClient_close(cs);
  return recv_data;
}
