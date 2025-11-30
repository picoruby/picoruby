#include "../../include/socket.h"
#include <stdbool.h>

struct picorb_tcp_server {
  int dummy;
};

picorb_tcp_server_t*
TCPServer_create(int port, int backlog)
{
  /* No operation implementation */
  return 0;
}

picorb_socket_t*
TCPServer_accept_nonblock(picorb_tcp_server_t *server)
{
  /* No operation implementation */
  return 0;
}

bool
TCPServer_close(picorb_tcp_server_t *server)
{
  /* No operation implementation */
  return false;
}

int
TCPServer_port(picorb_tcp_server_t *server)
{
  /* No operation implementation */
  return -1;
}

bool
TCPServer_listening(picorb_tcp_server_t *server)
{
  /* No operation implementation */
  return false;
}
