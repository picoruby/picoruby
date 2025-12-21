#define PICORB_PLATFORM_POSIX 1

#include "../../include/socket.h"
#include "picoruby.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "errno.h"
#include "fcntl.h"
#include "unistd.h"
#include <string.h>
#include <stdlib.h>

picorb_tcp_server_t*
TCPServer_create(int port, int backlog)
{
  picorb_tcp_server_t *server = picorb_alloc(NULL, sizeof(picorb_tcp_server_t));
  if (!server) return NULL;

  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    picorb_free(NULL, server);
    return NULL;
  }

  int opt = 1;
  if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    close(server->listen_fd);
    picorb_free(NULL, server);
    return NULL;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(server->listen_fd);
    picorb_free(NULL, server);
    return NULL;
  }

  if (listen(server->listen_fd, backlog) < 0) {
    close(server->listen_fd);
    picorb_free(NULL, server);
    return NULL;
  }

  server->port = port;
  server->backlog = backlog;
  server->listening = true;

  return server;
}

picorb_socket_t*
TCPServer_accept_nonblock(picorb_tcp_server_t *server)
{
  if (!server || !server->listening) {
    return NULL;
  }

  int flags = fcntl(server->listen_fd, F_GETFL, 0);
  if (flags == -1) {
    return NULL;
  }
  if (fcntl(server->listen_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return NULL;
  }

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int client_fd = accept(server->listen_fd,
                         (struct sockaddr*)&client_addr,
                         &addr_len);

  int saved_errno = errno;

  fcntl(server->listen_fd, F_SETFL, flags);

  errno = saved_errno;

  if (client_fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return NULL;
    }
    return NULL;
  }

  picorb_socket_t *client = (picorb_socket_t *)picorb_alloc(NULL, sizeof(picorb_socket_t));
  if (!client) {
    close(client_fd);
    return NULL;
  }

  client->fd = client_fd;
  client->family = AF_INET;
  client->socktype = SOCK_STREAM;
  client->protocol = IPPROTO_TCP;
  client->connected = true;
  client->closed = false;

  inet_ntop(AF_INET, &client_addr.sin_addr,
            client->remote_host, sizeof(client->remote_host));
  client->remote_port = ntohs(client_addr.sin_port);

  return client;
}

bool
TCPServer_close(picorb_tcp_server_t *server)
{
  if (!server) return false;

  if (server->listen_fd >= 0) {
    close(server->listen_fd);
    server->listen_fd = -1;
    server->listening = false;
  }

  picorb_free(NULL, server);
  return true;
}

int
TCPServer_port(picorb_tcp_server_t *server)
{
  if (!server) return -1;
  return server->port;
}

bool
TCPServer_listening(picorb_tcp_server_t *server)
{
  if (!server) return false;
  return server->listening;
}