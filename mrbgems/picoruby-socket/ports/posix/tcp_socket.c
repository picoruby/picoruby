#include "../../include/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* Create a new TCP socket */
bool
TCPSocket_create(picorb_socket_t *sock)
{
  if (!sock) return false;

  memset(sock, 0, sizeof(picorb_socket_t));

  sock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock->fd < 0) {
    return false;
  }

  sock->family = AF_INET;
  sock->socktype = SOCK_STREAM;
  sock->protocol = IPPROTO_TCP;
  sock->connected = false;
  sock->closed = false;

  return true;
}

/* Connect to remote host */
bool
TCPSocket_connect(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || !host || port <= 0 || port > 65535) {
    return false;
  }

  /* Create socket if not already created */
  if (sock->fd < 0) {
    if (!TCPSocket_create(sock)) {
      return false;
    }
  }

  /* Resolve hostname */
  struct hostent *he = gethostbyname(host);
  if (!he) {
    close(sock->fd);
    sock->fd = -1;
    return false;
  }

  /* Setup address structure */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

  /* Connect */
  if (connect(sock->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock->fd);
    sock->fd = -1;
    return false;
  }

  /* Save connection info */
  strncpy(sock->remote_host, host, sizeof(sock->remote_host) - 1);
  sock->remote_host[sizeof(sock->remote_host) - 1] = '\0';
  sock->remote_port = port;
  sock->connected = true;

  return true;
}

/* Send data */
ssize_t
TCPSocket_send(picorb_socket_t *sock, const void *data, size_t len)
{
  if (!sock || !data || sock->fd < 0 || sock->closed) {
    return -1;
  }

  ssize_t sent = send(sock->fd, data, len, 0);
  if (sent < 0) {
    return -1;
  }

  return sent;
}

/* Receive data */
ssize_t
TCPSocket_recv(picorb_socket_t *sock, void *buf, size_t len)
{
  if (!sock || !buf || sock->fd < 0 || sock->closed) {
    return -1;
  }

  ssize_t received = recv(sock->fd, buf, len, 0);
  if (received < 0) {
    return -1;
  }

  /* EOF */
  if (received == 0) {
    sock->connected = false;
  }

  return received;
}

/* Close socket */
bool
TCPSocket_close(picorb_socket_t *sock)
{
  if (!sock || sock->fd < 0) {
    return false;
  }

  close(sock->fd);
  sock->fd = -1;
  sock->connected = false;
  sock->closed = true;

  return true;
}

/* Get remote host */
const char*
TCPSocket_remote_host(picorb_socket_t *sock)
{
  if (!sock) return NULL;
  return sock->remote_host;
}

/* Get remote port */
int
TCPSocket_remote_port(picorb_socket_t *sock)
{
  if (!sock) return -1;
  return sock->remote_port;
}

/* Check if socket is closed */
bool
TCPSocket_closed(picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->closed || sock->fd < 0;
}
