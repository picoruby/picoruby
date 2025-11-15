#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Wrapper structures for storing pointers in instance->data */
typedef struct {
  picorb_tcp_server_t *ptr;
} tcp_server_wrapper_t;


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

  /* Accept client connection (blocking) */
  picorb_socket_t *client = TCPServer_accept(wrapper->ptr);
  if (!client) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to accept client");
    return;
  }

  /* Create TCPSocket object with socket structure embedded in instance->data */
  mrbc_class *class_TCPSocket = mrbc_get_class_by_name("TCPSocket");
  mrbc_value client_obj = mrbc_instance_new(vm, class_TCPSocket, sizeof(picorb_socket_t));
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
  mrbc_define_method(vm, class_TCPServer, "accept", c_tcp_server_accept);
  mrbc_define_method(vm, class_TCPServer, "close", c_tcp_server_close);
}
