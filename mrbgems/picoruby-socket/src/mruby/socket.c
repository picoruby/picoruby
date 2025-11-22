#include "../../include/socket.h"
#include "mruby/data.h"

/* Socket type constants (matching socket.h) */
#ifndef PICORB_PLATFORM_POSIX
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#endif

const struct mrb_data_type mrb_socket_type = {
  "Socket", mrb_socket_free,
};

/* Data type for sockets (shared by TCP and UDP) */
void
mrb_socket_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_socket_t *sock = (picorb_socket_t *)ptr;
    if (!sock->closed) {
      /* Close socket based on socket type */
      if (sock->socktype == SOCK_DGRAM) {
        UDPSocket_close(sock);
      } else {
        TCPSocket_close(sock);
      }
    }
    mrb_free(mrb, sock);
  }
}

/* Initialize gem */
void
mrb_picoruby_socket_gem_init(mrb_state *mrb)
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
mrb_picoruby_socket_gem_final(mrb_state *mrb)
{
  /* Nothing to do */
}
