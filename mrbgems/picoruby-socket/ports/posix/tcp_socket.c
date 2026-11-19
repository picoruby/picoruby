#include "../../include/socket.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>

/* Prevent name collision with embedded Ruby bytecode */
#ifdef socket
#undef socket
#endif

/* Create a new TCP socket */
bool
TCPSocket_create(picorb_state *vm, picorb_socket_t *sock)
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
TCPSocket_connect(picorb_state *vm, picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || !host || port <= 0 || port > 65535) {
    return false;
  }

  /* Create socket if not already created */
  if (sock->fd < 0) {
    if (!TCPSocket_create(vm, sock)) {
      return false;
    }
  }

  /* Resolve hostname */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    /* Not an IP address, try DNS resolution */
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0 || !res) {
      snprintf(sock->errmsg, sizeof(sock->errmsg),
               "getaddrinfo(\"%s\"): %s", host, gai_strerror(err));
      close(sock->fd);
      sock->fd = -1;
      return false;
    }

    struct sockaddr_in *ai_addr = (struct sockaddr_in *)res->ai_addr;
    addr.sin_addr = ai_addr->sin_addr;
    freeaddrinfo(res);
  }

  /* Connect. A signal (such as the VM's timer tick) can cut connect()
     short with EINTR while the connection goes on: wait until the socket
     is writable, then read how the connection ended. */
  int rc = connect(sock->fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0 && errno == EINTR) {
    struct pollfd pfd = { sock->fd, POLLOUT, 0 };
    int ready;
    do {
      ready = poll(&pfd, 1, -1);
    } while (ready < 0 && errno == EINTR);
    if (ready > 0) {
      int so_error = 0;
      socklen_t so_len = sizeof(so_error);
      if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &so_error, &so_len) == 0) {
        rc = so_error ? -1 : 0;
        errno = so_error;
      }
    }
  }
  if (rc < 0) {
    snprintf(sock->errmsg, sizeof(sock->errmsg),
             "connect(\"%s\":%d): %s", host, port, strerror(errno));
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

int
TCPSocket_connection_state(picorb_state *vm, picorb_socket_t *sock)
{
  (void)vm;
  return sock && sock->connected ? SOCKET_STATE_CONNECTED : SOCKET_STATE_ERROR;
}

/* Send data */
ssize_t
TCPSocket_send(picorb_state *vm, picorb_socket_t *sock, const void *data, size_t len)
{
  if (!sock || !data || sock->fd < 0 || sock->closed) {
    return -1;
  }

  ssize_t sent;
  do {   /* retried when a signal interrupts it before anything is sent */
    sent = send(sock->fd, data, len, 0);
  } while (sent < 0 && errno == EINTR);
  if (sent < 0) {
    return -1;
  }

  return sent;
}

/* Receive data.
 * If nonblock is true, uses MSG_DONTWAIT and returns
 * PICORB_RECV_WOULD_BLOCK when no data is available.
 * Otherwise uses a blocking recv() and returns as soon as any data
 * is available, or 0 on EOF, or -1 on error (readpartial semantics). */
ssize_t
TCPSocket_recv(picorb_state *vm, picorb_socket_t *sock, void *buf, size_t len, bool nonblock)
{
  if (!sock || !buf || sock->fd < 0 || sock->closed) {
    return -1;
  }

  if (nonblock) {
    ssize_t received = recv(sock->fd, buf, len, MSG_DONTWAIT);
    if (received < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return PICORB_RECV_WOULD_BLOCK;
      }
      return -1;
    }
    if (received == 0) {
      sock->connected = false;
    }
    return received;
  }

  /* Return as soon as any data is available (readpartial semantics). */
  ssize_t received;
  do {   /* retried when a signal interrupts the wait */
    received = recv(sock->fd, buf, len, 0);
  } while (received < 0 && errno == EINTR);

  if (received < 0) {
    return -1;
  }
  if (received == 0) {
    sock->connected = false;
  }
  return received;
}

/* Check if data is ready to read */
bool
Socket_ready(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock || sock->fd < 0 || sock->closed) {
    return false;
  }

  int available = 0;
  if (ioctl(sock->fd, FIONREAD, &available) < 0) {
    return false;
  }

  return available > 0;
}

/* Close socket */
bool
TCPSocket_close(picorb_state *vm, picorb_socket_t *sock)
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
TCPSocket_remote_host(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return NULL;
  return sock->remote_host;
}

/* Get remote port */
int
TCPSocket_remote_port(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return -1;
  return sock->remote_port;
}

/* Check if socket is closed */
bool
TCPSocket_closed(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->closed || sock->fd < 0;
}
