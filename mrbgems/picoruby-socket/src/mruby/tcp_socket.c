#include <stdlib.h>
#include <string.h>
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/data.h"

#define E_SOCKET_ERROR (mrb_class_get_id(mrb, MRB_SYM(SocketError)))

/* TCPSocket.new(host, port) */
static mrb_value
mrb_tcp_socket_initialize(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;

  mrb_get_args(mrb, "zi", &host, &port);

  if (port < 0 || 65535 < port) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  /* Allocate socket structure */
  picorb_socket_t *sock = (picorb_socket_t *)mrb_malloc(mrb, sizeof(picorb_socket_t));
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to allocate socket");
  }

  /* Create socket */
  if (!TCPSocket_create(mrb, sock)) {
    mrb_free(mrb, sock);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to create socket");
  }

  /* Connect to remote host */
  if (!TCPSocket_connect(mrb, sock, host, (int)port)) {
    char errmsg[SOCKET_ERROR_MSG_LEN];
    strncpy(errmsg, sock->errmsg, sizeof(errmsg) - 1);
    errmsg[sizeof(errmsg) - 1] = '\0';
    mrb_free(mrb, sock);
    mrb_raise(mrb, E_SOCKET_ERROR, errmsg[0] ? errmsg : "failed to connect");
  }

  mrb_data_init(self, sock, &mrb_socket_type);

  return self;
}

/* socket.write(data) */
static mrb_value
mrb_tcp_socket_write(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;
  mrb_value data;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  mrb_get_args(mrb, "S", &data);

  ssize_t sent = TCPSocket_send(mrb, sock, RSTRING_PTR(data), RSTRING_LEN(data));
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
  mrb_int maxlen = 4096;
  mrb_value buf;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
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

  ssize_t received = TCPSocket_recv(mrb, sock, read_buf, maxlen, 0);
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

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  if (!TCPSocket_close(mrb, sock)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "close failed");
  }

  return mrb_nil_value();
}

/* socket.closed? */
static mrb_value
mrb_tcp_socket_closed_p(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    return mrb_true_value();
  }

  return mrb_bool_value(TCPSocket_closed(mrb, sock));
}

/* socket.remote_host */
static mrb_value
mrb_tcp_socket_remote_host(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  const char *host = TCPSocket_remote_host(mrb, sock);
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

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "socket is not initialized");
  }

  int port = TCPSocket_remote_port(mrb, sock);
  if (port < 0) {
    return mrb_nil_value();
  }

  return mrb_fixnum_value(port);
}

/* socket.ready? */
static mrb_value
mrb_tcp_socket_ready_p(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock;

  sock = (picorb_socket_t *)mrb_data_get_ptr(mrb, self, &mrb_socket_type);
  if (!sock) {
    return mrb_false_value();
  }

  return mrb_bool_value(Socket_ready(mrb, sock));
}

void
tcp_socket_init(mrb_state *mrb, struct RClass *basic_socket_class)
{
  struct RClass *tcp_socket_class;

  tcp_socket_class = mrb_define_class_id(mrb, MRB_SYM(TCPSocket), basic_socket_class);
  MRB_SET_INSTANCE_TT(tcp_socket_class, MRB_TT_DATA);

  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(initialize), mrb_tcp_socket_initialize, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(write), mrb_tcp_socket_write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(read), mrb_tcp_socket_read, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(close), mrb_tcp_socket_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM_Q(closed), mrb_tcp_socket_closed_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM_Q(ready), mrb_tcp_socket_ready_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(remote_host), mrb_tcp_socket_remote_host, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, tcp_socket_class, MRB_SYM(remote_port), mrb_tcp_socket_remote_port, MRB_ARGS_NONE());
}
