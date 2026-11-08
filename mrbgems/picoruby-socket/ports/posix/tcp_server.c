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
#include <netinet/tcp.h>
#include <netdb.h>

/* Prevent name collision with embedded Ruby bytecode */
#ifdef socket
#undef socket
#endif

/* TCP Server structure is now defined in socket.h */

/* Create and start a TCP server */
picorb_tcp_server_t*
TCPServer_create(picorb_state *vm, const char *host, int port, int backlog)
{
  picorb_tcp_server_t *server = picorb_alloc(vm, sizeof(picorb_tcp_server_t));
  if (!server) return NULL;

  /* Create socket */
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    picorb_free(vm, server);
    return NULL;
  }

  /* Set SO_REUSEADDR option to allow immediate reuse of the port */
  int opt = 1;
  if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR failed");
    close(server->listen_fd);
    picorb_free(vm, server);
    return NULL;
  }

  /* No SO_REUSEPORT: with it a second server binds a port in use without
     error, and the two take turns at its connections. As with CRuby, the
     second one fails instead. */

  /* Bind to address */
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  /* Listen on the host given, as CRuby does: "127.0.0.1" or "localhost"
     takes loopback connections only. nil or "" is every interface. The
     socket is IPv4, so the host resolves to an IPv4 address. */
  if (host && host[0] != '\0') {
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
      if (res) freeaddrinfo(res);
      close(server->listen_fd);
      picorb_free(vm, server);
      return NULL;
    }
    addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
  }

  if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind failed");
    close(server->listen_fd);
    picorb_free(vm, server);
    return NULL;
  }

  /* Start listening */
  if (listen(server->listen_fd, backlog) < 0) {
    close(server->listen_fd);
    picorb_free(vm, server);
    return NULL;
  }

  server->port = port;
  server->backlog = backlog;
  server->listening = true;

  return (picorb_tcp_server_t*)server;
}

/* Accept a client connection (non-blocking) */
picorb_socket_t*
TCPServer_accept_nonblock(picorb_state *vm, picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;

  if (!srv) {
    return NULL;
  }
  if (!srv->listening) {
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

  /* Save errno before fcntl potentially modifies it */
  int saved_errno = errno;

  /* Restore blocking mode */
  if (fcntl(srv->listen_fd, F_SETFL, flags) == -1) {
    perror("fcntl F_SETFL restore failed");
  }

  /* Restore errno */
  errno = saved_errno;

  if (client_fd < 0) {
    /* EAGAIN or EWOULDBLOCK means no connection available; EINTR, a
       signal, means none was taken this time */
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return NULL;
    }
    /* Other errors */
    perror("accept failed");
    return NULL;
  }

  /* On macOS and the BSDs the accepted socket takes O_NONBLOCK from the
     listening one, which is non-blocking just above, and its blocking reads
     would fail with EAGAIN before data arrives. It blocks, as a socket
     accepted on Linux does. */
  int client_flags = fcntl(client_fd, F_GETFL, 0);
  if (client_flags != -1 && (client_flags & O_NONBLOCK)) {
    fcntl(client_fd, F_SETFL, client_flags & ~O_NONBLOCK);
  }

  /* Create TCPSocket object for client */
  picorb_socket_t *client = (picorb_socket_t *)picorb_alloc(vm, sizeof(picorb_socket_t));
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

  /* Suppress Nagle algorithm on accepted socket to prevent ACK-Nagle deadlock
   * when receiving large firmware payloads in DFU transfers. */
  int nodelay = 1;
  setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
#ifdef TCP_QUICKACK
  /* Linux: suppress delayed ACK so sender is not blocked waiting for ACK. */
  setsockopt(client_fd, IPPROTO_TCP, TCP_QUICKACK, &nodelay, sizeof(nodelay));
#endif

  return client;
}

/* Close the TCP server */
bool
TCPServer_close(picorb_state *vm, picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;

  if (!srv) return false;

  if (srv->listen_fd >= 0) {
    close(srv->listen_fd);
    srv->listen_fd = -1;
    srv->listening = false;
  }

  picorb_free(vm, srv);
  return true;
}

/* Get server port */
int
TCPServer_port(picorb_state *vm, picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;
  return srv ? srv->port : -1;
}

/* Check if server is listening */
bool
TCPServer_listening(picorb_state *vm, picorb_tcp_server_t *server)
{
  picorb_tcp_server_t *srv = (picorb_tcp_server_t*)server;
  return srv ? srv->listening : false;
}
