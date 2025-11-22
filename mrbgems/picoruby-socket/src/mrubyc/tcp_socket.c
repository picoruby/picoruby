#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Magic number to identify wrapper pattern */
#define SOCKET_WRAPPER_MAGIC 0x534F434B  /* "SOCK" in hex */

/* Wrapper for sockets created by accept (pointer-based) */
typedef struct {
  uint32_t magic;  /* Must be SOCKET_WRAPPER_MAGIC */
  picorb_socket_t *ptr;
} socket_wrapper_t;

/*
 * Helper function to get socket pointer from instance->data.
 * Handles both patterns:
 * - Direct embedding (from TCPSocket.new)
 * - Wrapper pattern (from TCPServer.accept)
 */
static inline picorb_socket_t*
get_socket_ptr(mrbc_value *v)
{
  void *data = v[0].instance->data;
  socket_wrapper_t *potential_wrapper = (socket_wrapper_t *)data;

  /* Check magic number to identify wrapper pattern */
  if (potential_wrapper->magic == SOCKET_WRAPPER_MAGIC) {
    return potential_wrapper->ptr;
  }

  /* Otherwise it's direct embedding from TCPSocket.new */
  return (picorb_socket_t *)data;
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

  /* Create instance with socket structure embedded in instance->data */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(picorb_socket_t));
  picorb_socket_t *sock = (picorb_socket_t *)instance.instance->data;

  /* Initialize socket structure to zero */
  memset(sock, 0, sizeof(picorb_socket_t));
#ifdef PICORB_PLATFORM_POSIX
  sock->fd = -1;
#endif

  /* Connect to remote host */
  const char *host_str = (const char *)host.string->data;
  int port_num = (int)port.i;

  if (!TCPSocket_connect(sock, host_str, port_num)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to connect");
    return;
  }

  SET_RETURN(instance);
}
/*
 * socket.write(data) -> Integer
 */
static void
c_tcp_socket_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer (handles both patterns) */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  /* Check argument type */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }

  /* Send data */
  ssize_t sent = TCPSocket_send(sock, (const void *)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

/*
 * socket.read(maxlen = 4096) -> String or nil
 */
static void
c_tcp_socket_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc > 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = get_socket_ptr(v);
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  /* Get maxlen parameter (default: 4096) */
  int maxlen = 4096;
  if (argc == 1) {
    mrbc_value maxlen_arg = GET_ARG(1);
    if (maxlen_arg.tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
      return;
    }
    maxlen = (int)maxlen_arg.i;
    if (maxlen <= 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
      return;
    }
  }

  /* Allocate buffer */
  char *buffer = (char *)mrbc_raw_alloc(maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  /* Receive data */
  ssize_t received = TCPSocket_recv(sock, buffer, maxlen);

  if (received < 0) {
    mrbc_raw_free(buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "recv failed");
    return;
  }

  if (received == 0) {
    /* EOF */
    mrbc_raw_free(buffer);
    SET_NIL_RETURN();
    return;
  }

  /* Create string and return */
  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  mrbc_raw_free(buffer);
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
  TCPSocket_close(sock);

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
  bool is_closed = TCPSocket_closed(sock);
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
  const char *host = TCPSocket_remote_host(sock);
  if (!host || host[0] == '\0') {
    SET_NIL_RETURN();
    return;
  }

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
  int port = TCPSocket_remote_port(sock);
  if (port < 0) {
    SET_NIL_RETURN();
    return;
  }

  SET_INT_RETURN(port);
}
void
tcp_socket_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  mrbc_class *class_TCPSocket = mrbc_define_class(vm, "TCPSocket", class_BasicSocket);
  mrbc_define_destructor(class_TCPSocket, mrbc_socket_free);

  mrbc_define_method(vm, class_TCPSocket, "new", c_tcp_socket_new);
  mrbc_define_method(vm, class_TCPSocket, "write", c_tcp_socket_write);
  mrbc_define_method(vm, class_TCPSocket, "read", c_tcp_socket_read);
  mrbc_define_method(vm, class_TCPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_TCPSocket, "closed?", c_tcp_socket_closed_q);
  mrbc_define_method(vm, class_TCPSocket, "remote_host", c_tcp_socket_remote_host);
  mrbc_define_method(vm, class_TCPSocket, "remote_port", c_tcp_socket_remote_port);

}
