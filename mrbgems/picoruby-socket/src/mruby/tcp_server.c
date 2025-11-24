#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"

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

/*
 * TCPServer.new(host=nil, service, backlog=5) -> TCPServer
 *
 * CRuby compatible signature with optional backlog extension.
 * Examples:
 *   TCPServer.new(nil, 8080)         # host=nil, service=8080
 *   TCPServer.new("127.0.0.1", 8080) # host="127.0.0.1", service=8080
 *   TCPServer.new(nil, 8080, 10)     # host=nil, service=8080, backlog=10
 */
static mrb_value
mrb_tcp_server_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value host_arg;
  mrb_int service;
  mrb_int backlog = 5;
  const char *host = NULL;

  mrb_get_args(mrb, "oi|i", &host_arg, &service, &backlog);

  /* Parse host argument */
  if (mrb_string_p(host_arg)) {
    host = RSTRING_PTR(host_arg);
  } else if (!mrb_nil_p(host_arg)) {
    mrb_raise(mrb, E_TYPE_ERROR, "host must be a String or nil");
  }

  int port = (int)service;

  /* Validate port range */
  if (port < 0 || 65535 < port) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid port number: %i", port);
  }

  /* Validate backlog */
  if (backlog <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "backlog must be positive");
  }

  /* Note: host parameter is currently ignored in embedded implementation */
  /* All interfaces are bound regardless of host parameter */
  (void)host;

  /* Create TCP server */
  picorb_tcp_server_t *server = TCPServer_create(port, (int)backlog);
  if (!server) {
    mrb_raisef(mrb, E_RUNTIME_ERROR,
               "failed to create TCP server on port %i (port may be in TIME_WAIT state, wait ~2 minutes and retry)",
               port);
  }

  mrb_data_init(self, server, &mrb_tcp_server_type);

  return self;
}

/* server.accept_nonblock -> TCPSocket or nil */
static mrb_value
mrb_tcp_server_accept_nonblock(mrb_state *mrb, mrb_value self)
{
  picorb_tcp_server_t *server;
  picorb_socket_t *client;
  mrb_value client_obj;
  struct RClass *tcp_socket_class;

  server = (picorb_tcp_server_t *)mrb_data_get_ptr(mrb, self, &mrb_tcp_server_type);
  if (!server) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "server is not initialized");
  }

  /* Accept client connection (non-blocking) */
  client = TCPServer_accept_nonblock(server);
  fprintf(stderr, "DEBUG mruby: TCPServer_accept_nonblock returned %p\n", (void*)client);
  if (!client) {
    return mrb_nil_value();
  }

  /* Create TCPSocket object for client */
  tcp_socket_class = mrb_class_get_id(mrb, MRB_SYM(TCPSocket));
  client_obj = mrb_obj_value(mrb_data_object_alloc(mrb, tcp_socket_class, client, &mrb_socket_type));
  fprintf(stderr, "DEBUG mruby: returning client_obj\n");

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

  mrb_define_method(mrb, tcp_server_class, "initialize", mrb_tcp_server_initialize, MRB_ARGS_ARG(1, 2));
  mrb_define_method(mrb, tcp_server_class, "accept_nonblock", mrb_tcp_server_accept_nonblock, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp_server_class, "close", mrb_tcp_server_close, MRB_ARGS_NONE());
}
