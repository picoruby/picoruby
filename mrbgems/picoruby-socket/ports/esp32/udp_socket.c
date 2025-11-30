#define PICORB_PLATFORM_POSIX 1

#include "../../include/socket.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <string.h>
#include <errno.h>

/* Prevent name collision with embedded Ruby bytecode */
#ifdef socket
#undef socket
#endif

bool
UDPSocket_create(picorb_socket_t *sock)
{
  if (!sock) return false;

  memset(sock, 0, sizeof(picorb_socket_t));
  sock->fd = -1;
  sock->family = AF_INET;
  sock->socktype = SOCK_DGRAM;
  sock->protocol = IPPROTO_UDP;
  sock->connected = false;
  sock->closed = false;
  sock->remote_port = 0;

  sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock->fd < 0) {
    return false;
  }

  int flags = fcntl(sock->fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(sock->fd, F_SETFL, flags | O_NONBLOCK);
  }

  return true;
}

bool
UDPSocket_bind(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || sock->fd < 0) return false;

  struct addrinfo hints;
  struct addrinfo *res = NULL;
  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  const char *bind_host = (host && *host) ? host : "0.0.0.0";

  int err = getaddrinfo(bind_host, port_str, &hints, &res);
  if (err != 0 || !res) {
    return false;
  }

  if (bind(sock->fd, res->ai_addr, res->ai_addrlen) < 0) {
    freeaddrinfo(res);
    return false;
  }

  freeaddrinfo(res);

  return true;
}

bool
UDPSocket_connect(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || sock->fd < 0 || !host) return false;

  struct addrinfo hints;
  struct addrinfo *res = NULL;
  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  int err = getaddrinfo(host, port_str, &hints, &res);
  if (err != 0 || !res) {
    return false;
  }

  if (connect(sock->fd, res->ai_addr, res->ai_addrlen) < 0) {
    freeaddrinfo(res);
    return false;
  }

  freeaddrinfo(res);

  sock->connected = true;
  strncpy(sock->remote_host, host, sizeof(sock->remote_host) - 1);
  sock->remote_host[sizeof(sock->remote_host) - 1] = '\0';
  sock->remote_port = port;

  return true;
}

ssize_t
UDPSocket_send(picorb_socket_t *sock, const void *data, size_t len)
{
  if (!sock || sock->fd < 0 || !data || !sock->connected) {
    return -1;
  }

  ssize_t sent = send(sock->fd, data, len, 0);
  if (sent < 0) {
    return -1;
  }

  return sent;
}

ssize_t
UDPSocket_sendto(picorb_socket_t *sock, const void *data, size_t len,
                  const char *host, int port)
{
  if (!sock || sock->fd < 0 || !data || !host) {
    return -1;
  }

  struct addrinfo hints;
  struct addrinfo *res = NULL;
  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  int err = getaddrinfo(host, port_str, &hints, &res);
  if (err != 0 || !res) {
    return -1;
  }

  ssize_t sent = sendto(sock->fd, data, len, 0, res->ai_addr, res->ai_addrlen);

  freeaddrinfo(res);

  if (sent < 0) {
    return -1;
  }

  return sent;
}

ssize_t
UDPSocket_recvfrom(picorb_socket_t *sock, void *buf, size_t len,
                    char *host, size_t host_len, int *port)
{
  if (!sock || sock->fd < 0 || !buf) {
    return -1;
  }

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));

  ssize_t received = recvfrom(sock->fd, buf, len, 0,
                               (struct sockaddr *)&addr, &addr_len);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    return -1;
  }

  if (host && host_len > 0) {
    inet_ntop(AF_INET, &addr.sin_addr, host, host_len);
  }
  if (port) {
    *port = ntohs(addr.sin_port);
  }

  return received;
}

bool
UDPSocket_close(picorb_socket_t *sock)
{
  if (!sock || sock->fd < 0) return false;

  if (close(sock->fd) < 0) {
    return false;
  }

  sock->fd = -1;
  sock->closed = true;
  sock->connected = false;

  return true;
}

bool
UDPSocket_closed(picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->closed || sock->fd < 0;
}
