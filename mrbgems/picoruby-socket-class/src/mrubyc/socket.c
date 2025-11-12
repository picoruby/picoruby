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

/* Helper macros for instance variables */
#define GETIV(str)      mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

/* Instance variable name for socket handle */
#define SOCKET_HANDLE_IV "_socket_handle"

/* Instance variable name for server handle */
#define SERVER_HANDLE_IV "_server_handle"

/* Instance variable name for SSL socket handle */
#define SSL_SOCKET_HANDLE_IV "_ssl_socket_handle"

/* Global reference to TCPSocket class for accept() */
static mrbc_class *g_class_TCPSocket = NULL;

/*
 * Get socket pointer from instance variable
 */
static picorb_socket_t*
get_socket_ptr(mrbc_value *self)
{
  mrbc_value handle_val = mrbc_instance_getiv(self, mrbc_str_to_symid(SOCKET_HANDLE_IV));
  if (handle_val.tt != MRBC_TT_INTEGER) {
    return NULL;
  }
  return (picorb_socket_t *)(intptr_t)handle_val.i;
}

/*
 * Set socket pointer to instance variable
 */
static void
set_socket_ptr(mrbc_value *self, picorb_socket_t *sock)
{
  mrbc_value handle_val = mrbc_integer_value((intptr_t)sock);
  mrbc_instance_setiv(self, mrbc_str_to_symid(SOCKET_HANDLE_IV), &handle_val);
}

/*
 * Get server pointer from instance variable
 */
static picorb_tcp_server_t*
get_server_ptr(mrbc_value *self)
{
  mrbc_value handle_val = mrbc_instance_getiv(self, mrbc_str_to_symid(SERVER_HANDLE_IV));
  if (handle_val.tt != MRBC_TT_INTEGER) {
    return NULL;
  }
  return (picorb_tcp_server_t *)(intptr_t)handle_val.i;
}

/*
 * Set server pointer to instance variable
 */
static void
set_server_ptr(mrbc_value *self, picorb_tcp_server_t *server)
{
  mrbc_value handle_val = mrbc_integer_value((intptr_t)server);
  mrbc_instance_setiv(self, mrbc_str_to_symid(SERVER_HANDLE_IV), &handle_val);
}

/*
 * Get SSL socket pointer from instance variable
 */
static picorb_ssl_socket_t*
get_ssl_socket_ptr(mrbc_value *self)
{
  mrbc_value handle_val = mrbc_instance_getiv(self, mrbc_str_to_symid(SSL_SOCKET_HANDLE_IV));
  if (handle_val.tt != MRBC_TT_INTEGER) {
    return NULL;
  }
  return (picorb_ssl_socket_t *)(intptr_t)handle_val.i;
}

/*
 * Set SSL socket pointer to instance variable
 */
static void
set_ssl_socket_ptr(mrbc_value *self, picorb_ssl_socket_t *ssl_sock)
{
  mrbc_value handle_val = mrbc_integer_value((intptr_t)ssl_sock);
  mrbc_instance_setiv(self, mrbc_str_to_symid(SSL_SOCKET_HANDLE_IV), &handle_val);
}

/*
 * TCPSocket.new(host, port)
 */
static void
c_tcp_socket_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
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

  /* Allocate socket structure */
  picorb_socket_t *sock = (picorb_socket_t *)mrbc_raw_alloc(sizeof(picorb_socket_t));
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate socket");
    return;
  }

  /* Initialize socket structure to zero */
  memset(sock, 0, sizeof(picorb_socket_t));
  sock->fd = -1;

  /* Connect to remote host */
  const char *host_str = (const char *)host.string->data;
  int port_num = (int)port.i;

  if (!TCPSocket_connect(sock, host_str, port_num)) {
    mrbc_raw_free(sock);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to connect");
    return;
  }

  /* Store socket pointer in instance variable */
  set_socket_ptr(&v[0], sock);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
  if (!sock) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close socket */
  TCPSocket_close(sock);
  mrbc_raw_free(sock);

  /* Clear instance variable */
  mrbc_value zero_val = mrbc_integer_value(0);
  mrbc_instance_setiv(&v[0], mrbc_str_to_symid(SOCKET_HANDLE_IV), &zero_val);

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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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
c_udp_socket_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Allocate socket structure */
  picorb_socket_t *sock = (picorb_socket_t *)mrbc_raw_alloc(sizeof(picorb_socket_t));
  if (!sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate socket");
    return;
  }

  /* Create UDP socket */
  if (!UDPSocket_create(sock)) {
    mrbc_raw_free(sock);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create UDP socket");
    return;
  }

  /* Store socket pointer in instance variable */
  set_socket_ptr(&v[0], sock);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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

  /* Get socket pointer */
  picorb_socket_t *sock = get_socket_ptr(&v[0]);
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
c_tcp_server_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
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

  /* Create TCP server */
  picorb_tcp_server_t *server = TCPServer_create((int)port.i, backlog);
  if (!server) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create TCP server");
    return;
  }

  /* Store server pointer in instance variable */
  set_server_ptr(&v[0], server);
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

  /* Get server pointer */
  picorb_tcp_server_t *server = get_server_ptr(&v[0]);
  if (!server) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "server is not initialized");
    return;
  }

  /* Check if TCPSocket class is available */
  if (!g_class_TCPSocket) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "TCPSocket class not initialized");
    return;
  }

  /* Accept client connection (blocking) */
  picorb_socket_t *client = TCPServer_accept(server);
  if (!client) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to accept client");
    return;
  }

  /* Create TCPSocket object */
  mrbc_value client_obj = mrbc_instance_new(vm, g_class_TCPSocket, 0);
  set_socket_ptr(&client_obj, client);

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

  /* Get server pointer */
  picorb_tcp_server_t *server = get_server_ptr(&v[0]);
  if (!server) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close server */
  TCPServer_close(server);

  /* Clear instance variable */
  mrbc_value zero_val = mrbc_integer_value(0);
  mrbc_instance_setiv(&v[0], mrbc_str_to_symid(SERVER_HANDLE_IV), &zero_val);

  SET_NIL_RETURN();
}

/*
 * SSLSocket.new(tcp_socket, hostname=nil) -> SSLSocket
 */
static void
c_ssl_socket_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || argc > 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get TCP socket argument */
  mrbc_value tcp_socket_obj = GET_ARG(1);
  picorb_socket_t *tcp_socket = get_socket_ptr(&tcp_socket_obj);

  if (!tcp_socket) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "first argument must be a TCPSocket");
    return;
  }

  if (!tcp_socket->connected) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "TCP socket is not connected");
    return;
  }

  /* Get hostname argument (optional) */
  const char *hostname = NULL;
  if (argc == 2) {
    mrbc_value hostname_arg = GET_ARG(2);
    if (hostname_arg.tt == MRBC_TT_STRING) {
      hostname = (const char *)hostname_arg.string->data;
    }
  }

  /* Create SSL socket wrapping TCP socket */
  picorb_ssl_socket_t *ssl_sock = SSLSocket_create(tcp_socket, hostname);
  if (!ssl_sock) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  /* Initialize SSL/TLS and perform handshake */
  if (!SSLSocket_init(ssl_sock)) {
    SSLSocket_close(ssl_sock);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL handshake failed");
    return;
  }

  /* Store SSL socket pointer in instance variable */
  set_ssl_socket_ptr(&v[0], ssl_sock);
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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
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
  ssize_t sent = SSLSocket_send(ssl_sock, (const void *)data.string->data, data.string->size);
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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
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
  ssize_t received = SSLSocket_recv(ssl_sock, buffer, maxlen);

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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close SSL socket */
  SSLSocket_close(ssl_sock);

  /* Clear instance variable */
  mrbc_value zero_val = mrbc_integer_value(0);
  mrbc_instance_setiv(&v[0], mrbc_str_to_symid(SSL_SOCKET_HANDLE_IV), &zero_val);

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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
    SET_TRUE_RETURN();
    return;
  }

  /* Check if SSL socket is closed */
  bool is_closed = SSLSocket_closed(ssl_sock);
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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote host */
  const char *host = SSLSocket_remote_host(ssl_sock);
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

  /* Get SSL socket pointer */
  picorb_ssl_socket_t *ssl_sock = get_ssl_socket_ptr(&v[0]);
  if (!ssl_sock) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote port */
  int port = SSLSocket_remote_port(ssl_sock);
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

  /* TCPSocket methods */
  mrbc_define_method(vm, class_TCPSocket, "initialize", c_tcp_socket_initialize);
  mrbc_define_method(vm, class_TCPSocket, "write", c_tcp_socket_write);
  mrbc_define_method(vm, class_TCPSocket, "read", c_tcp_socket_read);
  mrbc_define_method(vm, class_TCPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_TCPSocket, "closed?", c_tcp_socket_closed_q);
  mrbc_define_method(vm, class_TCPSocket, "remote_host", c_tcp_socket_remote_host);
  mrbc_define_method(vm, class_TCPSocket, "remote_port", c_tcp_socket_remote_port);

  /* Define UDPSocket class */
  mrbc_class *class_UDPSocket = mrbc_define_class(vm, "UDPSocket", class_BasicSocket);

  /* UDPSocket methods */
  mrbc_define_method(vm, class_UDPSocket, "initialize", c_udp_socket_initialize);
  mrbc_define_method(vm, class_UDPSocket, "bind", c_udp_socket_bind);
  mrbc_define_method(vm, class_UDPSocket, "connect", c_udp_socket_connect);
  mrbc_define_method(vm, class_UDPSocket, "send", c_udp_socket_send);
  mrbc_define_method(vm, class_UDPSocket, "recvfrom", c_udp_socket_recvfrom);
  mrbc_define_method(vm, class_UDPSocket, "close", c_tcp_socket_close);
  mrbc_define_method(vm, class_UDPSocket, "closed?", c_tcp_socket_closed_q);

  /* Define TCPServer class */
  mrbc_class *class_TCPServer = mrbc_define_class(vm, "TCPServer", class_BasicSocket);

  /* TCPServer methods */
  mrbc_define_method(vm, class_TCPServer, "initialize", c_tcp_server_initialize);
  mrbc_define_method(vm, class_TCPServer, "accept", c_tcp_server_accept);
  mrbc_define_method(vm, class_TCPServer, "close", c_tcp_server_close);

  /* Define SSLSocket class */
  mrbc_class *class_SSLSocket = mrbc_define_class(vm, "SSLSocket", class_BasicSocket);

  /* SSLSocket methods */
  mrbc_define_method(vm, class_SSLSocket, "initialize", c_ssl_socket_initialize);
  mrbc_define_method(vm, class_SSLSocket, "write", c_ssl_socket_write);
  mrbc_define_method(vm, class_SSLSocket, "read", c_ssl_socket_read);
  mrbc_define_method(vm, class_SSLSocket, "close", c_ssl_socket_close);
  mrbc_define_method(vm, class_SSLSocket, "closed?", c_ssl_socket_closed_q);
  mrbc_define_method(vm, class_SSLSocket, "remote_host", c_ssl_socket_remote_host);
  mrbc_define_method(vm, class_SSLSocket, "remote_port", c_ssl_socket_remote_port);
}
