#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void
mrbc_socket_free(mrbc_value *self)
{
  picorb_socket_t *sock = (picorb_socket_t *)self->instance->data;
  if (sock && !sock->closed) {
    TCPSocket_close(sock);
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
