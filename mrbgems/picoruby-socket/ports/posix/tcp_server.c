#include "../../include/socket.h"
#include "picoruby.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

/* Prevent name collision with embedded Ruby bytecode */
#ifdef socket
#undef socket
#endif

/* TCP Server structure is now defined in socket.h */

/* Create and start a TCP server */
picorb_tcp_server_t*
TCPServer_create(int port, int backlog)
{
  picorb_tcp_server_t *server = malloc(sizeof(picorb_tcp_server_t));
  if (!server) return NULL;

  /* Create socket */
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    free(server);
    return NULL;
  }

  /* Set SO_REUSEADDR option to allow immediate reuse of the port */
  int opt = 1;
  if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR failed");
    close(server->listen_fd);
    free(server);
    return NULL;
  }

#ifdef SO_REUSEPORT
  /* Set SO_REUSEPORT option for better load balancing (if available) */
  if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
    /* SO_REUSEPORT is optional, continue even if it fails */
  }
#endif

  /* Bind to address */
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind failed");
    close(server->listen_fd);
    free(server);
    return NULL;
  }

  /* Start listening */
  if (listen(server->listen_fd, backlog) < 0) {
    close(server->listen_fd);
    free(server);
    return NULL;
  }

  server->port = port;
  server->backlog = backlog;
  server->listening = true;

  fprintf(stderr, "DEBUG: TCPServer created, listen_fd=%d, port=%d, backlog=%d\n",
          server->listen_fd, port, backlog);

  return (picorb_tcp_server_t*)server;
}

/* Accept a client connection (non-blocking) */
picorb_socket_t*
TCPServer_accept_nonblock(picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;

  if (!srv) {
    fprintf(stderr, "DEBUG: accept_nonblock: srv is NULL\n");
    return NULL;
  }
  if (!srv->listening) {
    fprintf(stderr, "DEBUG: accept_nonblock: srv->listening is false\n");
    return NULL;
  }

  /* Set socket to non-blocking mode */
  int flags = fcntl(srv->listen_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL failed");
    return NULL;
  }
  if (fcntl(srv->listen_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL O_NONBLOCK failed");
    return NULL;
  }

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  /* Accept incoming connection (non-blocking) */
  int client_fd = accept(srv->listen_fd,
                         (struct sockaddr*)&client_addr,
                         &addr_len);

  if (client_fd >= 0) {
    fprintf(stderr, "DEBUG: accept returned client_fd=%d, errno=%d\n", client_fd, errno);
  }

  /* Save errno before fcntl potentially modifies it */
  int saved_errno = errno;

  /* Restore blocking mode */
  if (fcntl(srv->listen_fd, F_SETFL, flags) == -1) {
    perror("fcntl F_SETFL restore failed");
  }

  /* Restore errno */
  errno = saved_errno;

  if (client_fd < 0) {
    /* EAGAIN or EWOULDBLOCK means no connection available */
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return NULL;
    }
    /* Other errors */
    perror("accept failed");
    return NULL;
  }

  /* Create TCPSocket object for client */
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

  /* Save client information */
  inet_ntop(AF_INET, &client_addr.sin_addr,
            client->remote_host, sizeof(client->remote_host));
  client->remote_port = ntohs(client_addr.sin_port);

  return client;
}

/* Close the TCP server */
bool
TCPServer_close(picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;

  if (!srv) return false;

  if (srv->listen_fd >= 0) {
    close(srv->listen_fd);
    srv->listen_fd = -1;
    srv->listening = false;
  }

  free(srv);
  return true;
}

/* Get server port */
int
TCPServer_port(picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;
  return srv ? srv->port : -1;
}

/* Check if server is listening */
bool
TCPServer_listening(picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;
  return srv ? srv->listening : false;
}
