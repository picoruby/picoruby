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

/* Initialize gem */
void
mrb_picoruby_socket_gem_init(mrb_state *mrb)
{
  struct RClass *basic_socket_class;
  struct RClass *tcp_socket_class;
  struct RClass *udp_socket_class;

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
}

void
mrb_picoruby_socket_gem_final(mrb_state *mrb)
{
  /* Nothing to do */
}
