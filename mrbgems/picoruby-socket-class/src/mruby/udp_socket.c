#include <stdlib.h>
#include <string.h>
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"

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

/* socket.close */
static mrb_value
mrb_udp_socket_close(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  if (!UDPSocket_close(sock)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "close failed");
  }

  return mrb_nil_value();
}

/* socket.closed? */
static mrb_value
mrb_udp_socket_closed_p(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_socket_type);
  if (!sock) {
    return mrb_true_value();
  }

  return mrb_bool_value(UDPSocket_closed(sock));
}

void
udp_socket_init(mrb_state *mrb, struct RClass *basic_socket_class)
{
  struct RClass *udp_socket_class;

  udp_socket_class = mrb_define_class(mrb, "UDPSocket", basic_socket_class);
  MRB_SET_INSTANCE_TT(udp_socket_class, MRB_TT_DATA);

  mrb_define_method(mrb, udp_socket_class, "initialize", mrb_udp_socket_initialize, MRB_ARGS_NONE());
  mrb_define_method(mrb, udp_socket_class, "bind", mrb_udp_socket_bind, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, udp_socket_class, "connect", mrb_udp_socket_connect, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, udp_socket_class, "send", mrb_udp_socket_send, MRB_ARGS_ARG(1, 3));
  mrb_define_method(mrb, udp_socket_class, "recvfrom", mrb_udp_socket_recvfrom, MRB_ARGS_ARG(1, 1));
  mrb_define_method(mrb, udp_socket_class, "close", mrb_udp_socket_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, udp_socket_class, "closed?", mrb_udp_socket_closed_p, MRB_ARGS_NONE());
}
