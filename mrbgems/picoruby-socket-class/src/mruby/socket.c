#include "../../include/socket.h"
#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <stdlib.h>
#include <string.h>

/* Data type for TCPSocket */
static void
mrb_tcp_socket_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_socket_t *sock = (picorb_socket_t *)ptr;
    if (!sock->closed) {
      TCPSocket_close(sock);
    }
    mrb_free(mrb, sock);
  }
}

static const struct mrb_data_type mrb_tcp_socket_type = {
  "TCPSocket", mrb_tcp_socket_free,
};

/* Data type for TCPServer */
static void
mrb_tcp_server_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_tcp_server_t *server = (picorb_tcp_server_t *)ptr;
    TCPServer_close(server);
  }
}

static const struct mrb_data_type mrb_tcp_server_type = {
  "TCPServer", mrb_tcp_server_free,
};

/* TCPSocket.new(host, port) */
static mrb_value
mrb_tcp_socket_initialize(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;

  mrb_get_args(mrb, "zi", &host, &port);

  if (port <= 0 || port > 65535) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  /* Allocate socket structure */
  picorb_socket_t *sock = (picorb_socket_t *)mrb_malloc(mrb, sizeof(picorb_socket_t));
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate socket");
  }

  /* Connect to remote host */
  if (!TCPSocket_connect(sock, host, (int)port)) {
    mrb_free(mrb, sock);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to connect to %s:%i", host, port);
  }

  mrb_data_init(self, sock, &mrb_tcp_socket_type);

  return self;
}

/* socket.write(data) */
static mrb_value
mrb_tcp_socket_write(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  mrb_value data;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "S", &data);

  ssize_t sent = TCPSocket_send(sock, RSTRING_PTR(data), RSTRING_LEN(data));
  if (sent < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "send failed");
  }

  return mrb_fixnum_value(sent);
}

/* socket.read(maxlen = nil) */
static mrb_value
mrb_tcp_socket_read(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  mrb_int maxlen = 4096;  /* Default buffer size */
  mrb_value buf;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "|i", &maxlen);

  if (maxlen <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "maxlen must be positive");
  }

  /* Allocate buffer */
  char *read_buf = (char *)mrb_malloc(mrb, maxlen);
  if (!read_buf) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate read buffer");
  }

  ssize_t received = TCPSocket_recv(sock, read_buf, maxlen);
  if (received < 0) {
    mrb_free(mrb, read_buf);
    mrb_raise(mrb, E_RUNTIME_ERROR, "recv failed");
  }

  /* EOF */
  if (received == 0) {
    mrb_free(mrb, read_buf);
    return mrb_nil_value();
  }

  buf = mrb_str_new(mrb, read_buf, received);
  mrb_free(mrb, read_buf);

  return buf;
}

/* socket.close */
static mrb_value
mrb_tcp_socket_close(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  if (!TCPSocket_close(sock)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "close failed");
  }

  return mrb_nil_value();
}

/* socket.closed? */
static mrb_value
mrb_tcp_socket_closed_p(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    return mrb_true_value();
  }

  return mrb_bool_value(TCPSocket_closed(sock));
}

/* socket.remote_host */
static mrb_value
mrb_tcp_socket_remote_host(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  const char *host = TCPSocket_remote_host(sock);
  if (!host) {
    return mrb_nil_value();
  }

  return mrb_str_new_cstr(mrb, host);
}

/* socket.remote_port */
static mrb_value
mrb_tcp_socket_remote_port(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  int port = TCPSocket_remote_port(sock);
  if (port < 0) {
    return mrb_nil_value();
  }

  return mrb_fixnum_value(port);
}

/* UDPSocket.new() */
static mrb_value
mrb_udp_socket_initialize(mrb_state *mrb, mrb_value self)
{
  /* Allocate socket structure */
  picorb_socket_t *sock = (picorb_socket_t *)mrb_malloc(mrb, sizeof(picorb_socket_t));
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate socket");
  }

  /* Create UDP socket */
  if (!UDPSocket_create(sock)) {
    mrb_free(mrb, sock);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to create UDP socket");
  }

  mrb_data_init(self, sock, &mrb_tcp_socket_type);

  return self;
}

/* socket.bind(host, port) */
static mrb_value
mrb_udp_socket_bind(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  const char *host;
  mrb_int port;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "zi", &host, &port);

  if (port <= 0 || port > 65535) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  if (!UDPSocket_bind(sock, host, (int)port)) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to bind to %s:%i", host, port);
  }

  return mrb_nil_value();
}

/* socket.connect(host, port) */
static mrb_value
mrb_udp_socket_connect(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  const char *host;
  mrb_int port;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "zi", &host, &port);

  if (port <= 0 || port > 65535) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  if (!UDPSocket_connect(sock, host, (int)port)) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to connect to %s:%i", host, port);
  }

  return mrb_nil_value();
}

/* socket.send(data, flags=0, host=nil, port=nil) */
static mrb_value
mrb_udp_socket_send(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  mrb_value data;
  mrb_int flags = 0;
  const char *host = NULL;
  mrb_int port = 0;
  int argc;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  argc = mrb_get_args(mrb, "S|izi", &data, &flags, &host, &port);

  ssize_t sent;
  if (argc >= 3 && host) {
    /* Send to specified host and port */
    if (port <= 0 || port > 65535) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
    }
    sent = UDPSocket_sendto(sock, RSTRING_PTR(data), RSTRING_LEN(data), host, (int)port);
  } else {
    /* Send to connected destination */
    sent = UDPSocket_send(sock, RSTRING_PTR(data), RSTRING_LEN(data));
  }

  if (sent < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "send failed");
  }

  return mrb_fixnum_value(sent);
}

/* socket.recvfrom(maxlen, flags=0) -> [data, [family, port, host, host]] */
static mrb_value
mrb_udp_socket_recvfrom(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  mrb_int maxlen;
  mrb_int flags = 0;
  mrb_value data, addr_info;
  char *buf;
  char host[256];
  int port;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "i|i", &maxlen, &flags);

  if (maxlen <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "maxlen must be positive");
  }

  /* Allocate buffer */
  buf = (char *)mrb_malloc(mrb, maxlen);
  if (!buf) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate buffer");
  }

  ssize_t received = UDPSocket_recvfrom(sock, buf, maxlen, host, sizeof(host), &port);
  if (received < 0) {
    mrb_free(mrb, buf);
    mrb_raise(mrb, E_RUNTIME_ERROR, "recvfrom failed");
  }

  /* Create data string */
  data = mrb_str_new(mrb, buf, received);
  mrb_free(mrb, buf);

  /* Create address info array [family, port, host, host] */
  addr_info = mrb_ary_new(mrb);
  mrb_ary_push(mrb, addr_info, mrb_str_new_cstr(mrb, "AF_INET"));
  mrb_ary_push(mrb, addr_info, mrb_fixnum_value(port));
  mrb_ary_push(mrb, addr_info, mrb_str_new_cstr(mrb, host));
  mrb_ary_push(mrb, addr_info, mrb_str_new_cstr(mrb, host));

  /* Return [data, addr_info] */
  mrb_value result = mrb_ary_new(mrb);
  mrb_ary_push(mrb, result, data);
  mrb_ary_push(mrb, result, addr_info);

  return result;
}

/* TCPServer.new(port, backlog=5) */
static mrb_value
mrb_tcp_server_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_int port;
  mrb_int backlog = 5;

  mrb_get_args(mrb, "i|i", &port, &backlog);

  if (port <= 0 || port > 65535) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  if (backlog <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "backlog must be positive");
  }

  /* Create TCP server */
  picorb_tcp_server_t *server = TCPServer_create((int)port, (int)backlog);
  if (!server) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to create TCP server on port %i", port);
  }

  mrb_data_init(self, server, &mrb_tcp_server_type);

  return self;
}

/* server.accept -> TCPSocket */
static mrb_value
mrb_tcp_server_accept(mrb_state *mrb, mrb_value self)
{
  picorb_tcp_server_t *server;
  picorb_socket_t *client;
  mrb_value client_obj;
  struct RClass *tcp_socket_class;

  server = (picorb_tcp_server_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_server_type);
  if (!server) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "server is not initialized");
  }

  /* Accept client connection (blocking) */
  client = TCPServer_accept(server);
  if (!client) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to accept client connection");
  }

  /* Create TCPSocket object for client */
  tcp_socket_class = mrb_class_get_id(mrb, MRB_SYM(TCPSocket));
  client_obj = mrb_obj_value(mrb_data_object_alloc(mrb, tcp_socket_class, client, &mrb_tcp_socket_type));

  return client_obj;
}

/* server.close */
static mrb_value
mrb_tcp_server_close(mrb_state *mrb, mrb_value self)
{
  picorb_tcp_server_t *server;

  server = (picorb_tcp_server_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_server_type);
  if (!server) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "server is not initialized");
  }

  if (!TCPServer_close(server)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to close server");
  }

  /* Clear the data pointer to prevent double-free */
  DATA_PTR(self) = NULL;

  return mrb_nil_value();
}

/* Data type for SSLContext */
static void
mrb_ssl_context_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_ssl_context_t *ctx = (picorb_ssl_context_t *)ptr;
    SSLContext_free(ctx);
  }
}

static const struct mrb_data_type mrb_ssl_context_type = {
  "SSLContext", mrb_ssl_context_free,
};

/* SSLContext.new */
static mrb_value
mrb_ssl_context_initialize(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_context_t *ctx;

  ctx = SSLContext_create();
  if (!ctx) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to create SSL context");
  }

  mrb_data_init(self, ctx, &mrb_ssl_context_type);

  return self;
}

/* ssl_context.ca_file = path */
static mrb_value
mrb_ssl_context_set_ca_file(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_context_t *ctx;
  const char *ca_file;

  ctx = (picorb_ssl_context_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_context_type);
  if (!ctx) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL context is not initialized");
  }

  mrb_get_args(mrb, "z", &ca_file);

  if (!SSLContext_set_ca_file(ctx, ca_file)) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to set CA file: %s", ca_file);
  }

  return mrb_str_new_cstr(mrb, ca_file);
}

/* ssl_context.verify_mode = mode */
static mrb_value
mrb_ssl_context_set_verify_mode(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_context_t *ctx;
  mrb_int mode;

  ctx = (picorb_ssl_context_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_context_type);
  if (!ctx) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL context is not initialized");
  }

  mrb_get_args(mrb, "i", &mode);

  if (!SSLContext_set_verify_mode(ctx, (int)mode)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to set verify mode");
  }

  return mrb_fixnum_value(mode);
}

/* ssl_context.verify_mode */
static mrb_value
mrb_ssl_context_get_verify_mode(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_context_t *ctx;
  int mode;

  ctx = (picorb_ssl_context_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_context_type);
  if (!ctx) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL context is not initialized");
  }

  mode = SSLContext_get_verify_mode(ctx);
  if (mode < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to get verify mode");
  }

  return mrb_fixnum_value(mode);
}

/* Data type for SSLSocket */
static void
mrb_ssl_socket_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)ptr;
    if (!SSLSocket_closed(ssl_sock)) {
      SSLSocket_close(ssl_sock);
    }
  }
}

static const struct mrb_data_type mrb_ssl_socket_type = {
  "SSLSocket", mrb_ssl_socket_free,
};

/* SSLSocket.new(tcp_socket, ssl_context) */
static mrb_value
mrb_ssl_socket_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value tcp_socket_obj, ssl_context_obj;
  picorb_socket_t *tcp_socket;
  picorb_ssl_context_t *ssl_ctx;
  picorb_ssl_socket_t *ssl_sock;

  mrb_get_args(mrb, "oo", &tcp_socket_obj, &ssl_context_obj);

  /* Get underlying TCP socket */
  tcp_socket = (picorb_socket_t *)mrb_data_get_ptr(mrb, tcp_socket_obj, &mrb_tcp_socket_type);
  if (!tcp_socket) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "first argument must be a TCPSocket");
  }

  if (!tcp_socket->connected) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "TCP socket is not connected");
  }

  /* Get SSL context */
  ssl_ctx = (picorb_ssl_context_t *)mrb_data_get_ptr(mrb, ssl_context_obj, &mrb_ssl_context_type);
  if (!ssl_ctx) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "second argument must be an SSLContext");
  }

  /* Create SSL socket wrapping TCP socket */
  ssl_sock = SSLSocket_create(tcp_socket, ssl_ctx);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to create SSL socket");
  }

  mrb_data_init(self, ssl_sock, &mrb_ssl_socket_type);

  return self;
}

/* ssl_socket.hostname = hostname */
static mrb_value
mrb_ssl_socket_set_hostname(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;
  const char *hostname;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  mrb_get_args(mrb, "z", &hostname);

  if (!SSLSocket_set_hostname(ssl_sock, hostname)) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to set hostname: %s", hostname);
  }

  return mrb_str_new_cstr(mrb, hostname);
}

/* ssl_socket.connect */
static mrb_value
mrb_ssl_socket_connect(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  if (!SSLSocket_connect(ssl_sock)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL handshake failed");
  }

  return self;
}

/* ssl_socket.write(data) */
static mrb_value
mrb_ssl_socket_write(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;
  mrb_value data;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  mrb_get_args(mrb, "S", &data);

  ssize_t sent = SSLSocket_send(ssl_sock, RSTRING_PTR(data), RSTRING_LEN(data));
  if (sent < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL send failed");
  }

  return mrb_fixnum_value(sent);
}

/* ssl_socket.read(maxlen = nil) */
static mrb_value
mrb_ssl_socket_read(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;
  mrb_int maxlen = 4096;  /* Default buffer size */
  mrb_value buf;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  mrb_get_args(mrb, "|i", &maxlen);

  if (maxlen <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "maxlen must be positive");
  }

  /* Allocate buffer */
  char *read_buf = (char *)mrb_malloc(mrb, maxlen);
  if (!read_buf) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate read buffer");
  }

  ssize_t received = SSLSocket_recv(ssl_sock, read_buf, maxlen);
  if (received < 0) {
    mrb_free(mrb, read_buf);
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL recv failed");
  }

  /* EOF */
  if (received == 0) {
    mrb_free(mrb, read_buf);
    return mrb_nil_value();
  }

  buf = mrb_str_new(mrb, read_buf, received);
  mrb_free(mrb, read_buf);

  return buf;
}

/* ssl_socket.close */
static mrb_value
mrb_ssl_socket_close(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  if (!SSLSocket_close(ssl_sock)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL close failed");
  }

  /* Clear the data pointer to prevent double-free */
  DATA_PTR(self) = NULL;

  return mrb_nil_value();
}

/* ssl_socket.closed? */
static mrb_value
mrb_ssl_socket_closed_p(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    return mrb_true_value();
  }

  return mrb_bool_value(SSLSocket_closed(ssl_sock));
}

/* ssl_socket.remote_host */
static mrb_value
mrb_ssl_socket_remote_host(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  const char *host = SSLSocket_remote_host(ssl_sock);
  if (!host) {
    return mrb_nil_value();
  }

  return mrb_str_new_cstr(mrb, host);
}

/* ssl_socket.remote_port */
static mrb_value
mrb_ssl_socket_remote_port(mrb_state *mrb, mrb_value self)
{
  picorb_ssl_socket_t *ssl_sock;

  ssl_sock = (picorb_ssl_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_ssl_socket_type);
  if (!ssl_sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "SSL socket is not initialized");
  }

  int port = SSLSocket_remote_port(ssl_sock);
  if (port < 0) {
    return mrb_nil_value();
  }

  return mrb_fixnum_value(port);
}

/* Initialize gem */
void
mrb_picoruby_socket_class_gem_init(mrb_state *mrb)
{
  struct RClass *basic_socket_class;
  struct RClass *tcp_socket_class;
  struct RClass *udp_socket_class;
  struct RClass *tcp_server_class;
  struct RClass *ssl_context_class;
  struct RClass *ssl_socket_class;

  /* BasicSocket class */
  basic_socket_class = mrb_define_class(mrb, "BasicSocket", mrb->object_class);

  /* TCPSocket class */
  tcp_socket_class = mrb_define_class(mrb, "TCPSocket", basic_socket_class);
  MRB_SET_INSTANCE_TT(tcp_socket_class, MRB_TT_DATA);

  mrb_define_method(mrb, tcp_socket_class, "initialize", mrb_tcp_socket_initialize, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, tcp_socket_class, "write", mrb_tcp_socket_write, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, tcp_socket_class, "read", mrb_tcp_socket_read, MRB_ARGS_OPT(1));
  mrb_define_method(mrb, tcp_socket_class, "close", mrb_tcp_socket_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_socket_class, "closed?", mrb_tcp_socket_closed_p, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_socket_class, "remote_host", mrb_tcp_socket_remote_host, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_socket_class, "remote_port", mrb_tcp_socket_remote_port, MRB_ARGS_NONE());

  /* UDPSocket class */
  udp_socket_class = mrb_define_class(mrb, "UDPSocket", basic_socket_class);
  MRB_SET_INSTANCE_TT(udp_socket_class, MRB_TT_DATA);

  mrb_define_method(mrb, udp_socket_class, "initialize", mrb_udp_socket_initialize, MRB_ARGS_NONE());
  mrb_define_method(mrb, udp_socket_class, "bind", mrb_udp_socket_bind, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, udp_socket_class, "connect", mrb_udp_socket_connect, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, udp_socket_class, "send", mrb_udp_socket_send, MRB_ARGS_ARG(1, 3));
  mrb_define_method(mrb, udp_socket_class, "recvfrom", mrb_udp_socket_recvfrom, MRB_ARGS_ARG(1, 1));
  mrb_define_method(mrb, udp_socket_class, "close", mrb_tcp_socket_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, udp_socket_class, "closed?", mrb_tcp_socket_closed_p, MRB_ARGS_NONE());

  /* TCPServer class */
  tcp_server_class = mrb_define_class(mrb, "TCPServer", basic_socket_class);
  MRB_SET_INSTANCE_TT(tcp_server_class, MRB_TT_DATA);

  mrb_define_method(mrb, tcp_server_class, "initialize", mrb_tcp_server_initialize, MRB_ARGS_ARG(1, 1));
  mrb_define_method(mrb, tcp_server_class, "accept", mrb_tcp_server_accept, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_server_class, "close", mrb_tcp_server_close, MRB_ARGS_NONE());

  /* SSLContext class */
  ssl_context_class = mrb_define_class(mrb, "SSLContext", mrb->object_class);
  MRB_SET_INSTANCE_TT(ssl_context_class, MRB_TT_DATA);

  mrb_define_method(mrb, ssl_context_class, "initialize", mrb_ssl_context_initialize, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssl_context_class, "ca_file=", mrb_ssl_context_set_ca_file, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, ssl_context_class, "verify_mode=", mrb_ssl_context_set_verify_mode, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, ssl_context_class, "verify_mode", mrb_ssl_context_get_verify_mode, MRB_ARGS_NONE());

  /* SSLContext constants */
  mrb_define_const(mrb, ssl_context_class, "VERIFY_NONE", mrb_fixnum_value(SSL_VERIFY_NONE));
  mrb_define_const(mrb, ssl_context_class, "VERIFY_PEER", mrb_fixnum_value(SSL_VERIFY_PEER));

  /* SSLSocket class */
  ssl_socket_class = mrb_define_class(mrb, "SSLSocket", basic_socket_class);
  MRB_SET_INSTANCE_TT(ssl_socket_class, MRB_TT_DATA);

  mrb_define_method(mrb, ssl_socket_class, "initialize", mrb_ssl_socket_initialize, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, ssl_socket_class, "hostname=", mrb_ssl_socket_set_hostname, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, ssl_socket_class, "connect", mrb_ssl_socket_connect, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssl_socket_class, "write", mrb_ssl_socket_write, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, ssl_socket_class, "read", mrb_ssl_socket_read, MRB_ARGS_OPT(1));
  mrb_define_method(mrb, ssl_socket_class, "close", mrb_ssl_socket_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssl_socket_class, "closed?", mrb_ssl_socket_closed_p, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssl_socket_class, "remote_host", mrb_ssl_socket_remote_host, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssl_socket_class, "remote_port", mrb_ssl_socket_remote_port, MRB_ARGS_NONE());
}

void
mrb_picoruby_socket_class_gem_final(mrb_state *mrb)
{
  /* Nothing to do */
}
