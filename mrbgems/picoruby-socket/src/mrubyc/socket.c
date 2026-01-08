#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"

/* Socket type constants (matching socket.h) */
#if defined(PICORB_PLATFORM_POSIX)
#include <sys/socket.h>
#else
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#endif

void
mrbc_socket_free(mrbc_value *self)
{
  void *data = self->instance->data;
  if (!data) {
    return;
  }

  /* Get socket pointer (always stored as pointer now) */
  picorb_socket_t **sock_ptr = (picorb_socket_t **)data;
  picorb_socket_t *sock = *sock_ptr;

  if (!sock) {
    return;
  }

  if (!sock->closed) {
    /* Close socket based on socket type */
    if (sock->socktype == SOCK_DGRAM) {
      UDPSocket_close(sock);
    } else {
      TCPSocket_close(sock);
    }
  }

  /* Free the allocated socket structure */
  mrbc_raw_free(sock);
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
