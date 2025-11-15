#include <stdlib.h>
#include <string.h>
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"

/* Data type for TCPServer */
static void
mrb_tcp_server_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_tcp_server_t *server = (picorb_tcp_server_t *)ptr;
    TCPServer_close(server);
  }
}

static const struct mrb_data_type mrb_tcp_server_type = {
  "TCPServer", mrb_tcp_server_free,
};

/* TCPServer.new(port, backlog=5) */
static mrb_value
mrb_tcp_server_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_int port;
  mrb_int backlog = 5;

  mrb_get_args(mrb, "i|i", &port, &backlog);

  if (port <= 0 || port > 65535) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  if (backlog <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "backlog must be positive");
  }

  /* Create TCP server */
  picorb_tcp_server_t *server = TCPServer_create((int)port, (int)backlog);
  if (!server) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "failed to create TCP server on port %i", port);
  }

  mrb_data_init(self, server, &mrb_tcp_server_type);

  return self;
}

/* server.accept -> TCPSocket */
static mrb_value
mrb_tcp_server_accept(mrb_state *mrb, mrb_value self)
{
  picorb_tcp_server_t *server;
  picorb_socket_t *client;
  mrb_value client_obj;
  struct RClass *tcp_socket_class;

  server = (picorb_tcp_server_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_server_type);
  if (!server) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "server is not initialized");
  }

  /* Accept client connection (blocking) */
  client = TCPServer_accept(server);
  if (!client) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to accept client connection");
  }

  /* Create TCPSocket object for client */
  tcp_socket_class = mrb_class_get_id(mrb, MRB_SYM(TCPSocket));
  client_obj = mrb_obj_value(mrb_data_object_alloc(mrb, tcp_socket_class, client, &mrb_tcp_socket_type));

  return client_obj;
}

/* server.close */
static mrb_value
mrb_tcp_server_close(mrb_state *mrb, mrb_value self)
{
  picorb_tcp_server_t *server;

  server = (picorb_tcp_server_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_server_type);
  if (!server) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "server is not initialized");
  }

  if (!TCPServer_close(server)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to close server");
  }

  /* Clear the data pointer to prevent double-free */
  DATA_PTR(self) = NULL;

  return mrb_nil_value();
}

void
tcp_server_init(mrb_state *mrb, struct RClass *basic_socket_class)
{
  struct RClass *tcp_server_class;

  tcp_server_class = mrb_define_class(mrb, "TCPServer", basic_socket_class);
  MRB_SET_INSTANCE_TT(tcp_server_class, MRB_TT_DATA);

  mrb_define_method(mrb, tcp_server_class, "initialize", mrb_tcp_server_initialize, MRB_ARGS_ARG(1, 1));
  mrb_define_method(mrb, tcp_server_class, "accept", mrb_tcp_server_accept, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_server_class, "close", mrb_tcp_server_close, MRB_ARGS_NONE());
}
