#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
 * socket.close -> nil
 */
static void
c_udp_socket_close(mrbc_vm *vm, mrbc_value *v, int argc)
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
  UDPSocket_close(sock);

  SET_NIL_RETURN();
}

/*
 * socket.closed? -> true or false
 */
static void
c_udp_socket_closed_q(mrbc_vm *vm, mrbc_value *v, int argc)
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
  bool is_closed = UDPSocket_closed(sock);
  if (is_closed) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
udp_socket_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  mrbc_class *class_UDPSocket = mrbc_define_class(vm, "UDPSocket", class_BasicSocket);
  mrbc_define_destructor(class_UDPSocket, mrbc_socket_free);

  mrbc_define_method(vm, class_UDPSocket, "new", c_udp_socket_new);
  mrbc_define_method(vm, class_UDPSocket, "bind", c_udp_socket_bind);
  mrbc_define_method(vm, class_UDPSocket, "connect", c_udp_socket_connect);
  mrbc_define_method(vm, class_UDPSocket, "send", c_udp_socket_send);
  mrbc_define_method(vm, class_UDPSocket, "recvfrom", c_udp_socket_recvfrom);
  mrbc_define_method(vm, class_UDPSocket, "close", c_udp_socket_close);
  mrbc_define_method(vm, class_UDPSocket, "closed?", c_udp_socket_closed_q);
}
