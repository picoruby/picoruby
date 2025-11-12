/*
 * POSIX UDP Socket Implementation
 *
 * Provides UDP socket functionality using standard POSIX socket APIs.
 */

#include "../../include/socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * Create a new UDP socket
 */
bool
UDPSocket_create(picorb_socket_t *sock)
{
  if (!sock) return false;

  /* Initialize socket structure */
  memset(sock, 0, sizeof(picorb_socket_t));
  sock->fd = -1;
  sock->family = AF_INET;
  sock->socktype = SOCK_DGRAM;
  sock->protocol = IPPROTO_UDP;
  sock->connected = false;
  sock->closed = false;
  sock->remote_port = 0;

  /* Create UDP socket */
  sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock->fd < 0) {
    return false;
  }

  return true;
}

/*
 * Bind socket to local address and port
 */
bool
UDPSocket_bind(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || sock->fd < 0) return false;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  /* Parse host address */
  if (!host || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "") == 0) {
    /* Bind to all interfaces */
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    /* Try to parse as IP address */
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
      /* Not an IP address, try DNS resolution */
      struct hostent *he = gethostbyname(host);
      if (!he) {
        return false;
      }
      memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
    }
  }

  /* Bind socket */
  if (bind(sock->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return false;
  }

  return true;
}

/*
 * Connect socket to remote address (sets default destination)
 */
bool
UDPSocket_connect(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || sock->fd < 0 || !host) return false;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  /* Try to parse as IP address */
  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    /* Not an IP address, try DNS resolution */
    struct hostent *he = gethostbyname(host);
    if (!he) {
      return false;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
  }

  /* Connect socket (sets default destination) */
  if (connect(sock->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return false;
  }

  /* Store remote host information */
  sock->connected = true;
  strncpy(sock->remote_host, host, sizeof(sock->remote_host) - 1);
  sock->remote_host[sizeof(sock->remote_host) - 1] = '\0';
  sock->remote_port = port;

  return true;
}

/*
 * Send data to connected destination
 */
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

/*
 * Send data to specified destination
 */
ssize_t
UDPSocket_sendto(picorb_socket_t *sock, const void *data, size_t len,
                  const char *host, int port)
{
  if (!sock || sock->fd < 0 || !data || !host) {
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  /* Try to parse as IP address */
  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    /* Not an IP address, try DNS resolution */
    struct hostent *he = gethostbyname(host);
    if (!he) {
      return -1;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
  }

  ssize_t sent = sendto(sock->fd, data, len, 0,
                        (struct sockaddr *)&addr, sizeof(addr));
  if (sent < 0) {
    return -1;
  }

  return sent;
}

/*
 * Receive data from any source
 */
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
    return -1;
  }

  /* Extract sender information if requested */
  if (host && host_len > 0) {
    inet_ntop(AF_INET, &addr.sin_addr, host, host_len);
  }
  if (port) {
    *port = ntohs(addr.sin_port);
  }

  return received;
}

/*
 * Close UDP socket
 */
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

/*
 * Check if socket is closed
 */
bool
UDPSocket_closed(picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->closed || sock->fd < 0;
}
