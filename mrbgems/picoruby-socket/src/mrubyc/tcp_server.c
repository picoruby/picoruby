#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"

/* Wrapper structure for storing TCP server pointer in instance->data */
typedef struct {
  picorb_tcp_server_t *ptr;
} tcp_server_wrapper_t;


/*
 * TCPServer.new(host=nil, service, backlog=5) -> TCPServer
 *
 * CRuby compatible signature with optional backlog extension.
 * Examples:
 *   TCPServer.new(nil, 8080)         # host=nil, service=8080
 *   TCPServer.new("127.0.0.1", 8080) # host="127.0.0.1", service=8080
 *   TCPServer.new(nil, 8080, 10)     # host=nil, service=8080, backlog=10
 */
static void
c_tcp_server_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 2 || argc > 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  const char *host = NULL;
  int port = 0;
  int backlog = 5;

  mrbc_value host_arg = GET_ARG(1);
  mrbc_value service_arg = GET_ARG(2);

  /* Parse host argument */
  if (host_arg.tt == MRBC_TT_STRING) {
    host = (const char *)host_arg.string->data;
  } else if (host_arg.tt != MRBC_TT_NIL) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "host must be a String or nil");
    return;
  }

  /* Parse service argument */
  if (service_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "service must be an Integer");
    return;
  }
  port = (int)service_arg.i;

  /* Parse optional backlog argument */
  if (argc == 3) {
    mrbc_value backlog_arg = GET_ARG(3);
    if (backlog_arg.tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "backlog must be an Integer");
      return;
    }
    backlog = (int)backlog_arg.i;
  }

  /* Validate port range */
  if (port < 0 || 65535 < port) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid port number");
    return;
  }

  /* Validate backlog */
  if (backlog <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "backlog must be positive");
    return;
  }

  /* Note: host parameter is currently ignored in embedded implementation */
  /* All interfaces are bound regardless of host parameter */
  (void)host;

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(tcp_server_wrapper_t));
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)instance.instance->data;

  /* Create TCP server and store pointer */
  wrapper->ptr = TCPServer_create(port, backlog);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError),
               "failed to create TCP server (port may be in TIME_WAIT state, wait ~2 minutes and retry)");
    return;
  }

  SET_RETURN(instance);
}

/*
 * server.accept_nonblock -> TCPSocket or nil
 */
static void
c_tcp_server_accept_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
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

  /* Accept client connection (non-blocking) */
  picorb_socket_t *client = TCPServer_accept_nonblock(wrapper->ptr);
  if (!client) {
    SET_NIL_RETURN();
    return;
  }

  /*
   * Create TCPSocket object with pointer pattern.
   * Store the pointer directly to preserve LwIP callback arg.
   */
  mrbc_class *class_TCPSocket = mrbc_get_class_by_name("TCPSocket");
  mrbc_value client_obj = mrbc_instance_new(vm, class_TCPSocket, sizeof(picorb_socket_t *));
  picorb_socket_t **sock_ptr = (picorb_socket_t **)client_obj.instance->data;
  *sock_ptr = client;

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
static void
mrbc_tcp_server_free(mrbc_value *self)
{
  tcp_server_wrapper_t *wrapper = (tcp_server_wrapper_t *)self->instance->data;
  if (wrapper->ptr) {
    TCPServer_close(wrapper->ptr);
    wrapper->ptr = NULL;
  }
}
void
tcp_server_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  mrbc_class *class_TCPServer = mrbc_define_class(vm, "TCPServer", class_BasicSocket);
  mrbc_define_destructor(class_TCPServer, mrbc_tcp_server_free);

  mrbc_define_method(vm, class_TCPServer, "new", c_tcp_server_new);
  mrbc_define_method(vm, class_TCPServer, "accept_nonblock", c_tcp_server_accept_nonblock);
  mrbc_define_method(vm, class_TCPServer, "close", c_tcp_server_close);
}
