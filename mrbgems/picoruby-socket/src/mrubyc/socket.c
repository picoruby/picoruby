#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"

/* Magic number to identify wrapper pattern */
#define SOCKET_WRAPPER_MAGIC 0x534F434B  /* "SOCK" in hex */

/* Wrapper for sockets created by accept (pointer-based) */
typedef struct {
  uint32_t magic;  /* Must be SOCKET_WRAPPER_MAGIC */
  picorb_socket_t *ptr;
} socket_wrapper_t;

void
mrbc_socket_free(mrbc_value *self)
{
  void *data = self->instance->data;
  picorb_socket_t *sock = NULL;
  bool is_wrapper = false;
  socket_wrapper_t *potential_wrapper = (socket_wrapper_t *)data;

  /* Check magic number to identify wrapper pattern */
  if (potential_wrapper->magic == SOCKET_WRAPPER_MAGIC) {
    /* Wrapper pattern (from accept) */
    sock = potential_wrapper->ptr;
    is_wrapper = true;
  } else {
    /* Direct embedding (from TCPSocket.new) */
    sock = (picorb_socket_t *)data;
  }

  if (sock && !sock->closed) {
    TCPSocket_close(sock);
  }

  /* If wrapper pattern, free the allocated socket structure */
  if (is_wrapper && sock) {
    picorb_free(NULL, sock);
  }
}

void
mrbc_socket_init(mrbc_vm *vm)
{
  mrbc_class *class_BasicSocket = mrbc_define_class(vm, "BasicSocket", mrbc_class_object);

  tcp_socket_init(vm, class_BasicSocket);
  udp_socket_init(vm, class_BasicSocket);
  ssl_socket_init(vm, class_BasicSocket);
  tcp_server_init(vm, class_BasicSocket);
}
