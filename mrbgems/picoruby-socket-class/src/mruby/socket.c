#include "../../include/socket.h"
#include "mruby/data.h"

const struct mrb_data_type mrb_tcp_socket_type = {
  "TCPSocket", mrb_tcp_socket_free,
};

/* Data type for TCPSocket */
void
mrb_tcp_socket_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_socket_t *sock = (picorb_socket_t *)ptr;
    if (!sock->closed) {
      TCPSocket_close(sock);
    }
    mrb_free(mrb, sock);
  }
}

/* Initialize gem */
void
mrb_picoruby_socket_class_gem_init(mrb_state *mrb)
{
  struct RClass *basic_socket_class;

  /* BasicSocket class */
  basic_socket_class = mrb_define_class(mrb, "BasicSocket", mrb->object_class);

  /* Initialize each socket type */
  tcp_socket_init(mrb, basic_socket_class);
  udp_socket_init(mrb, basic_socket_class);
  tcp_server_init(mrb, basic_socket_class);
  ssl_socket_init(mrb, basic_socket_class);
}

void
mrb_picoruby_socket_class_gem_final(mrb_state *mrb)
{
  /* Nothing to do */
}
