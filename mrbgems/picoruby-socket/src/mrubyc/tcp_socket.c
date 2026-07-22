#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"

/*
 * Helper function to get socket pointer from instance->data.
 */
static inline picorb_socket_t*
get_socket_ptr(mrbc_value *v)
{
  socket_wrapper_t *wrapper = (socket_wrapper_t *)v[0].instance->data;
  return wrapper ? wrapper->ptr : NULL;
}

/*
 * TCPSocket.new(host, port)
 */
static void
c_tcp_socket_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Check argument types */
  mrbc_value host = GET_ARG(1);
  mrbc_value port = GET_ARG(2);

  if (host.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "host must be a String");
    return;
  }

  if (port.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "port must be an Integer");
    return;
  }

  /* Validate port range */
  if (port.i < 0 || 65535 < port.i) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Allocate socket structure on heap */
  picorb_socket_t *sock = (picorb_socket_t *)picorb_alloc(vm, sizeof(picorb_socket_t));
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate socket");
    return;
  }

  /* Create instance with wrapper containing socket and VM state */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(socket_wrapper_t));
  socket_wrapper_t *wrapper = (socket_wrapper_t *)instance.instance->data;
  if (!wrapper) {
    picorb_free(vm, sock);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate socket wrapper");
    return;
  }
  wrapper->ptr = sock;
  wrapper->vm = vm;

  /* Initialize socket structure to zero */
  memset(sock, 0, sizeof(picorb_socket_t));
#ifdef PICORB_PLATFORM_POSIX
  sock->fd = -1;
#endif

  /* Connect to remote host */
  const char *host_str = (const char *)host.string->data;
  int port_num = (int)port.i;

  if (!TCPSocket_connect(vm, sock, host_str, port_num)) {
    char errmsg[SOCKET_ERROR_MSG_LEN];
    strncpy(errmsg, sock->errmsg, sizeof(errmsg) - 1);
    errmsg[sizeof(errmsg) - 1] = '\0';
    picorb_free(vm, sock);
    mrbc_raisef(vm, mrbc_get_class_by_name("SocketError"),
                "%s", errmsg[0] ? errmsg : "failed to connect");
    return;
  }

  SET_RETURN(instance);
}
/*
 * socket.send(data, flags) -> Integer
 */
static void
c_tcp_socket_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer (handles both patterns) */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  /* Check argument types */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }
  // flags is required parameter for compatibility with CRuby.
  // Currently check only if it's an Integer. It can be used for future extensions
  mrbc_value flags = GET_ARG(2);
  if (flags.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "flags must be an Integer");
    return;
  }

  /* Send data */
  ssize_t sent = TCPSocket_send(vm, sock, (const void *)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "send failed");
    return;
  }

  mrbc_incref(&v[0]);
  SET_INT_RETURN(sent);
}

/*
 * socket.readpartial(maxlen) -> String
 * Raises EOFError on EOF.
 */
static void
c_tcp_socket_readpartial(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  mrbc_value maxlen_arg = GET_ARG(1);
  if (maxlen_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
    return;
  }
  int maxlen = (int)maxlen_arg.i;
  if (maxlen <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
    return;
  }

  char stack_buf[PICORB_SOCKET_STACK_BUF_SIZE];
  char *buffer = (maxlen < PICORB_SOCKET_STACK_BUF_SIZE)
    ? stack_buf
    : (char *)picorb_alloc(vm, maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  ssize_t received = TCPSocket_recv(vm, sock, buffer, maxlen, false);

  if (received == 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("EOFError"), "end of file reached");
    return;
  }

  if (received == PICORB_RECV_TIMEOUT) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("SocketError"), "read timeout");
    return;
  }

  if (received < 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "read failed");
    return;
  }

  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  if (buffer != stack_buf) picorb_free(vm, buffer);
  mrbc_incref(&v[0]);
  SET_RETURN(ret);
}

/*
 * socket.read_nonblock(maxlen) -> String or nil
 */
static void
c_tcp_socket_read_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  mrbc_value maxlen_arg = GET_ARG(1);
  if (maxlen_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
    return;
  }

  int maxlen = (int)maxlen_arg.i;
  if (maxlen <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
    return;
  }

  char stack_buf[PICORB_SOCKET_STACK_BUF_SIZE];
  char *buffer = (maxlen < PICORB_SOCKET_STACK_BUF_SIZE)
    ? stack_buf
    : (char *)picorb_alloc(vm, maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  ssize_t received = TCPSocket_recv(vm, sock, buffer, maxlen, true);

  if (received == PICORB_RECV_WOULD_BLOCK) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    SET_NIL_RETURN();
    return;
  }

  if (received == 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("EOFError"), "end of file reached");
    return;
  }

  if (received < 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "read failed");
    return;
  }

  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  if (buffer != stack_buf) picorb_free(vm, buffer);
  mrbc_incref(&v[0]);
  SET_RETURN(ret);
}

/*
 * socket.close -> nil
 */
static void
c_tcp_socket_close(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close socket */
  TCPSocket_close(vm, sock);

  mrbc_incref(&v[0]);
  SET_NIL_RETURN();
}

/*
 * socket.closed? -> true or false
 */
static void
c_tcp_socket_closed_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    SET_TRUE_RETURN();
    return;
  }

  /* Check if socket is closed */
  bool is_closed = TCPSocket_closed(vm, sock);
  mrbc_incref(&v[0]);
  if (is_closed) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * socket.remote_host -> String
 */
static void
c_tcp_socket_remote_host(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote host */
  const char *host = TCPSocket_remote_host(vm, sock);
  if (!host || host[0] == '\0') {
    SET_NIL_RETURN();
    return;
  }

  mrbc_incref(&v[0]);
  SET_RETURN(mrbc_string_new_cstr(vm, host));
}

/*
 * socket.remote_port -> Integer
 */
static void
c_tcp_socket_remote_port(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote port */
  int port = TCPSocket_remote_port(vm, sock);
  if (port < 0) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_incref(&v[0]);
  SET_INT_RETURN(port);
}

/*
 * socket.ready? -> true or false
 */
static void
c_tcp_socket_ready_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    SET_FALSE_RETURN();
    return;
  }

  /* Check if data is ready to read */
  bool is_ready = Socket_ready(vm, sock);
  if (is_ready) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
tcp_socket_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  mrbc_class *class_TCPSocket = mrbc_define_class(vm, "TCPSocket", class_BasicSocket);
  mrbc_define_destructor(class_TCPSocket, mrbc_socket_free);

  mrbc_define_method(vm, class_TCPSocket, "new", c_tcp_socket_new);
  mrbc_define_method(vm, class_TCPSocket, "send", c_tcp_socket_send);
  mrbc_define_method(vm, class_TCPSocket, "readpartial", c_tcp_socket_readpartial);
  mrbc_define_method(vm, class_TCPSocket, "read_nonblock", c_tcp_socket_read_nonblock);
  mrbc_define_method(vm, class_TCPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_TCPSocket, "closed?", c_tcp_socket_closed_q);
  mrbc_define_method(vm, class_TCPSocket, "ready?", c_tcp_socket_ready_q);
  mrbc_define_method(vm, class_TCPSocket, "remote_host", c_tcp_socket_remote_host);
  mrbc_define_method(vm, class_TCPSocket, "remote_port", c_tcp_socket_remote_port);

}
