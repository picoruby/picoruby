/*
 * mruby/c VM bindings for picoruby-socket
 *
 * This provides socket functionality for the mruby/c VM (PicoRuby)
 */

#include "../../include/socket.h"
#include "mrubyc.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Global reference to TCPSocket class for accept() */
static mrbc_class *g_class_TCPSocket = NULL;

/* Wrapper structures for storing pointers in instance->data */
typedef struct {
  picorb_tcp_server_t *ptr;
} tcp_server_wrapper_t;

typedef struct {
  picorb_ssl_context_t *ptr;
} ssl_context_wrapper_t;

typedef struct {
  picorb_ssl_socket_t *ptr;
} ssl_socket_wrapper_t;

/*
 * Destructor for TCPSocket and UDPSocket
 */
static void
mrbc_socket_free(mrbc_value *self)
{
  picorb_socket_t *sock = (picorb_socket_t *)self->instance->data;
  if (sock && !sock->closed) {
    TCPSocket_close(sock);
  }
}

/*
 * Destructor for TCPServer
 */
static void
mrbc_tcp_server_free(mrbc_value *self)
{
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)self->instance->data;
  if (wrapper->ptr) {
    TCPServer_close(wrapper->ptr);
    wrapper->ptr = NULL;
  }
}

/*
 * Destructor for SSLContext
 */
static void
mrbc_ssl_context_free(mrbc_value *self)
{
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)self->instance->data;
  if (wrapper->ptr) {
    SSLContext_free(wrapper->ptr);
    wrapper->ptr = NULL;
  }
}

/*
 * Destructor for SSLSocket
 */
static void
mrbc_ssl_socket_free(mrbc_value *self)
{
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)self->instance->data;
  if (wrapper->ptr && !SSLSocket_closed(wrapper->ptr)) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
  }
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
  if (port.i <= 0 || port.i > 65535) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Create instance with socket structure embedded in instance->data */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(picorb_socket_t));
  picorb_socket_t *sock = (picorb_socket_t *)instance.instance->data;

  /* Initialize socket structure to zero */
  memset(sock, 0, sizeof(picorb_socket_t));
  sock->fd = -1;

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

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
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

/*
 * UDPSocket.new -> UDPSocket
 */
static void
c_udp_socket_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Create instance with socket structure embedded in instance->data */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(picorb_socket_t));
  picorb_socket_t *sock = (picorb_socket_t *)instance.instance->data;

  /* Create UDP socket */
  if (!UDPSocket_create(sock)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create UDP socket");
    return;
  }

  SET_RETURN(instance);
}

/*
 * socket.bind(host, port) -> nil
 */
static void
c_udp_socket_bind(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
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
  if (port.i <= 0 || port.i > 65535) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Bind socket */
  const char *host_str = (const char *)host.string->data;
  int port_num = (int)port.i;

  if (!UDPSocket_bind(sock, host_str, port_num)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to bind");
    return;
  }

  SET_NIL_RETURN();
}

/*
 * socket.connect(host, port) -> nil
 */
static void
c_udp_socket_connect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
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
  if (port.i <= 0 || port.i > 65535) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Connect socket */
  const char *host_str = (const char *)host.string->data;
  int port_num = (int)port.i;

  if (!UDPSocket_connect(sock, host_str, port_num)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to connect");
    return;
  }

  SET_NIL_RETURN();
}

/*
 * socket.send(data, flags=0, host=nil, port=nil) -> Integer
 */
static void
c_udp_socket_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || argc > 4) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  /* Get data argument */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }

  ssize_t sent;

  if (argc >= 3) {
    /* Send to specified host and port */
    mrbc_value host = GET_ARG(3);
    mrbc_value port = GET_ARG(4);

    if (host.tt != MRBC_TT_STRING) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "host must be a String");
      return;
    }

    if (port.tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "port must be an Integer");
      return;
    }

    if (port.i <= 0 || port.i > 65535) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
      return;
    }

    const char *host_str = (const char *)host.string->data;
    int port_num = (int)port.i;

    sent = UDPSocket_sendto(sock, (const void *)data.string->data, data.string->size, host_str, port_num);
  } else {
    /* Send to connected destination */
    sent = UDPSocket_send(sock, (const void *)data.string->data, data.string->size);
  }

  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

/*
 * socket.recvfrom(maxlen, flags=0) -> [data, [family, port, host, host]]
 */
static void
c_udp_socket_recvfrom(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || argc > 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get socket pointer from instance->data */
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "socket is not initialized");
    return;
  }

  /* Get maxlen parameter */
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

  /* Allocate buffer */
  char *buffer = (char *)mrbc_raw_alloc(maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  char host[256];
  int port;

  /* Receive data */
  ssize_t received = UDPSocket_recvfrom(sock, buffer, maxlen, host, sizeof(host), &port);

  if (received < 0) {
    mrbc_raw_free(buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "recvfrom failed");
    return;
  }

  /* Create data string */
  mrbc_value data = mrbc_string_new(vm, buffer, received);
  mrbc_raw_free(buffer);

  /* Create address info array [family, port, host, host] */
  mrbc_value addr_info = mrbc_array_new(vm, 4);
  mrbc_value family_val = mrbc_string_new_cstr(vm, "AF_INET");
  mrbc_array_set(&addr_info, 0, &family_val);
  mrbc_value port_val = mrbc_integer_value(port);
  mrbc_array_set(&addr_info, 1, &port_val);
  mrbc_value host_val = mrbc_string_new_cstr(vm, host);
  mrbc_array_set(&addr_info, 2, &host_val);
  mrbc_array_set(&addr_info, 3, &host_val);

  /* Return [data, addr_info] */
  mrbc_value result = mrbc_array_new(vm, 2);
  mrbc_array_set(&result, 0, &data);
  mrbc_array_set(&result, 1, &addr_info);

  SET_RETURN(result);
}

/*
 * TCPServer.new(port, backlog=5) -> TCPServer
 */
static void
c_tcp_server_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || argc > 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get port argument */
  mrbc_value port = GET_ARG(1);
  if (port.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "port must be an Integer");
    return;
  }

  /* Validate port range */
  if (port.i <= 0 || port.i > 65535) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Get backlog argument (default: 5) */
  int backlog = 5;
  if (argc == 2) {
    mrbc_value backlog_arg = GET_ARG(2);
    if (backlog_arg.tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "backlog must be an Integer");
      return;
    }
    backlog = (int)backlog_arg.i;
    if (backlog <= 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "backlog must be positive");
      return;
    }
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(tcp_server_wrapper_t));
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)instance.instance->data;

  /* Create TCP server and store pointer */
  wrapper->ptr = TCPServer_create((int)port.i, backlog);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create TCP server");
    return;
  }

  SET_RETURN(instance);
}

/*
 * server.accept -> TCPSocket
 */
static void
c_tcp_server_accept(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get server pointer from wrapper */
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "server is not initialized");
    return;
  }

  /* Check if TCPSocket class is available */
  if (!g_class_TCPSocket) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "TCPSocket class not initialized");
    return;
  }

  /* Accept client connection (blocking) */
  picorb_socket_t *client = TCPServer_accept(wrapper->ptr);
  if (!client) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to accept client");
    return;
  }

  /* Create TCPSocket object with socket structure embedded in instance->data */
  mrbc_value client_obj = mrbc_instance_new(vm, g_class_TCPSocket, sizeof(picorb_socket_t));
  picorb_socket_t *client_sock = (picorb_socket_t *)client_obj.instance->data;

  /* Copy socket data from malloc'd pointer to instance->data */
  memcpy(client_sock, client, sizeof(picorb_socket_t));

  /* Free the malloc'd pointer since we copied the data */
  free(client);

  SET_RETURN(client_obj);
}

/*
 * server.close -> nil
 */
static void
c_tcp_server_close(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get server pointer from wrapper */
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close server (also frees the server structure) */
  TCPServer_close(wrapper->ptr);

  /* Clear the pointer */
  wrapper->ptr = NULL;

  SET_NIL_RETURN();
}

/*
 * SSLContext.new() -> SSLContext
 */
static void
c_ssl_context_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_context_wrapper_t));
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)instance.instance->data;

  /* Create SSL context and store pointer */
  wrapper->ptr = SSLContext_create();
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL context");
    return;
  }

  SET_RETURN(instance);
}

/*
 * ssl_context.ca_file = path -> String
 */
static void
c_ssl_context_set_ca_file(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get ca_file argument */
  mrbc_value ca_file_arg = GET_ARG(1);
  if (ca_file_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "ca_file must be a String");
    return;
  }

  const char *ca_file = (const char *)ca_file_arg.string->data;

  /* Set CA file */
  if (!SSLContext_set_ca_file(wrapper->ptr, ca_file)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA file");
    return;
  }

  mrbc_incref(&ca_file_arg);
  SET_RETURN(ca_file_arg);
}

/*
 * ssl_context.verify_mode = mode -> Integer
 */
static void
c_ssl_context_set_verify_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get verify_mode argument */
  mrbc_value mode_arg = GET_ARG(1);
  if (mode_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "verify_mode must be an Integer");
    return;
  }

  int mode = mode_arg.i;

  /* Set verify mode */
  if (!SSLContext_set_verify_mode(wrapper->ptr, mode)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set verify mode");
    return;
  }

  mrbc_incref(&mode_arg);
  SET_RETURN(mode_arg);
}

/*
 * ssl_context.verify_mode -> Integer
 */
static void
c_ssl_context_get_verify_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get verify mode */
  int mode = SSLContext_get_verify_mode(wrapper->ptr);
  if (mode < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to get verify mode");
    return;
  }

  SET_INT_RETURN(mode);
}

/*
 * SSLSocket.new(tcp_socket, ssl_context) -> SSLSocket
 */
static void
c_ssl_socket_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get TCP socket argument */
  mrbc_value tcp_socket_obj = GET_ARG(1);
  picorb_socket_t *tcp_socket = (picorb_socket_t *)tcp_socket_obj.instance->data;

  if (!tcp_socket) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "first argument must be a TCPSocket");
    return;
  }

  if (!tcp_socket->connected) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "TCP socket is not connected");
    return;
  }

  /* Get SSL context argument */
  mrbc_value ssl_context_obj = GET_ARG(2);
  ssl_context_wrapper_t *ctx_wrapper = (ssl_context_wrapper_t *)ssl_context_obj.instance->data;

  if (!ctx_wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "second argument must be an SSLContext");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_socket_wrapper_t));
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)instance.instance->data;

  /* Create SSL socket wrapping TCP socket and store pointer */
  wrapper->ptr = SSLSocket_create(tcp_socket, ctx_wrapper->ptr);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  mrbc_incref(&tcp_socket_obj);
  //mrbc_incref(&ssl_context_obj);

  SET_RETURN(instance);
}

/*
 * ssl_socket.hostname = hostname -> String
 */
static void
c_ssl_socket_set_hostname(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Get hostname argument */
  mrbc_value hostname_arg = GET_ARG(1);
  if (hostname_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "hostname must be a String");
    return;
  }

  const char *hostname = (const char *)hostname_arg.string->data;

  /* Set hostname */
  if (!SSLSocket_set_hostname(wrapper->ptr, hostname)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set hostname");
    return;
  }

  mrbc_incref(&hostname_arg);
  SET_RETURN(hostname_arg);
}

/*
 * ssl_socket.connect -> self
 */
static void
c_ssl_socket_connect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Perform SSL handshake */
  if (!SSLSocket_connect(wrapper->ptr)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL handshake failed");
    return;
  }
}

/*
 * ssl_socket.write(data) -> Integer
 */
static void
c_ssl_socket_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Check argument type */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }

  /* Send data */
  ssize_t sent = SSLSocket_send(wrapper->ptr, (const void *)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

/*
 * ssl_socket.read(maxlen = 4096) -> String or nil
 */
static void
c_ssl_socket_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc > 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
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
  ssize_t received = SSLSocket_recv(wrapper->ptr, buffer, maxlen);

  if (received < 0) {
    mrbc_raw_free(buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL recv failed");
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
 * ssl_socket.close -> nil
 */
static void
c_ssl_socket_close(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close SSL socket (also frees the SSL socket structure) */
  SSLSocket_close(wrapper->ptr);

  /* Clear the pointer */
  wrapper->ptr = NULL;

  SET_NIL_RETURN();
}

/*
 * ssl_socket.closed? -> true or false
 */
static void
c_ssl_socket_closed_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_TRUE_RETURN();
    return;
  }

  /* Check if SSL socket is closed */
  bool is_closed = SSLSocket_closed(wrapper->ptr);
  if (is_closed) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * ssl_socket.remote_host -> String
 */
static void
c_ssl_socket_remote_host(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote host */
  const char *host = SSLSocket_remote_host(wrapper->ptr);
  if (!host || host[0] == '\0') {
    SET_NIL_RETURN();
    return;
  }

  SET_RETURN(mrbc_string_new_cstr(vm, host));
}

/*
 * ssl_socket.remote_port -> Integer
 */
static void
c_ssl_socket_remote_port(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote port */
  int port = SSLSocket_remote_port(wrapper->ptr);
  if (port < 0) {
    SET_NIL_RETURN();
    return;
  }

  SET_INT_RETURN(port);
}

/*
 * Initialize mruby/c socket bindings
 */
void
mrbc_socket_class_init(mrbc_vm *vm)
{
  /* Define BasicSocket class */
  mrbc_class *class_BasicSocket = mrbc_define_class(vm, "BasicSocket", mrbc_class_object);

  /* Define TCPSocket class */
  mrbc_class *class_TCPSocket = mrbc_define_class(vm, "TCPSocket", class_BasicSocket);
  g_class_TCPSocket = class_TCPSocket;  /* Save for accept() */
  mrbc_define_destructor(class_TCPSocket, mrbc_socket_free);

  /* TCPSocket methods */
  mrbc_define_method(vm, class_TCPSocket, "new", c_tcp_socket_new);
  mrbc_define_method(vm, class_TCPSocket, "write", c_tcp_socket_write);
  mrbc_define_method(vm, class_TCPSocket, "read", c_tcp_socket_read);
  mrbc_define_method(vm, class_TCPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_TCPSocket, "closed?", c_tcp_socket_closed_q);
  mrbc_define_method(vm, class_TCPSocket, "remote_host", c_tcp_socket_remote_host);
  mrbc_define_method(vm, class_TCPSocket, "remote_port", c_tcp_socket_remote_port);

  /* Define UDPSocket class */
  mrbc_class *class_UDPSocket = mrbc_define_class(vm, "UDPSocket", class_BasicSocket);
  mrbc_define_destructor(class_UDPSocket, mrbc_socket_free);

  /* UDPSocket methods */
  mrbc_define_method(vm, class_UDPSocket, "new", c_udp_socket_new);
  mrbc_define_method(vm, class_UDPSocket, "bind", c_udp_socket_bind);
  mrbc_define_method(vm, class_UDPSocket, "connect", c_udp_socket_connect);
  mrbc_define_method(vm, class_UDPSocket, "send", c_udp_socket_send);
  mrbc_define_method(vm, class_UDPSocket, "recvfrom", c_udp_socket_recvfrom);
  mrbc_define_method(vm, class_UDPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_UDPSocket, "closed?", c_tcp_socket_closed_q);

  /* Define TCPServer class */
  mrbc_class *class_TCPServer = mrbc_define_class(vm, "TCPServer", class_BasicSocket);
  mrbc_define_destructor(class_TCPServer, mrbc_tcp_server_free);

  /* TCPServer methods */
  mrbc_define_method(vm, class_TCPServer, "new", c_tcp_server_new);
  mrbc_define_method(vm, class_TCPServer, "accept", c_tcp_server_accept);
  mrbc_define_method(vm, class_TCPServer, "close", c_tcp_server_close);

  /* Define SSLContext class */
  mrbc_class *class_SSLContext = mrbc_define_class(vm, "SSLContext", mrbc_class_object);
  mrbc_define_destructor(class_SSLContext, mrbc_ssl_context_free);

  /* SSLContext methods */
  mrbc_define_method(vm, class_SSLContext, "new", c_ssl_context_new);
  mrbc_define_method(vm, class_SSLContext, "ca_file=", c_ssl_context_set_ca_file);
  mrbc_define_method(vm, class_SSLContext, "verify_mode=", c_ssl_context_set_verify_mode);
  mrbc_define_method(vm, class_SSLContext, "verify_mode", c_ssl_context_get_verify_mode);

  /* SSLContext constants */
  mrbc_value verify_none = mrbc_integer_value(SSL_VERIFY_NONE);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_NONE"), &verify_none);
  mrbc_value verify_peer = mrbc_integer_value(SSL_VERIFY_PEER);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_PEER"), &verify_peer);

  /* Define SSLSocket class */
  mrbc_class *class_SSLSocket = mrbc_define_class(vm, "SSLSocket", class_BasicSocket);
  mrbc_define_destructor(class_SSLSocket, mrbc_ssl_socket_free);

  /* SSLSocket methods */
  mrbc_define_method(vm, class_SSLSocket, "new", c_ssl_socket_new);
  mrbc_define_method(vm, class_SSLSocket, "hostname=", c_ssl_socket_set_hostname);
  mrbc_define_method(vm, class_SSLSocket, "connect", c_ssl_socket_connect);
  mrbc_define_method(vm, class_SSLSocket, "write", c_ssl_socket_write);
  mrbc_define_method(vm, class_SSLSocket, "read", c_ssl_socket_read);
  mrbc_define_method(vm, class_SSLSocket, "close", c_ssl_socket_close);
  mrbc_define_method(vm, class_SSLSocket, "closed?", c_ssl_socket_closed_q);
  mrbc_define_method(vm, class_SSLSocket, "remote_host", c_ssl_socket_remote_host);
  mrbc_define_method(vm, class_SSLSocket, "remote_port", c_ssl_socket_remote_port);
}
