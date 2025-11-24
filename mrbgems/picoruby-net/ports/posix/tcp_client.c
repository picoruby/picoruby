/*
 * TCP Client for PicoRuby Net (POSIX Implementation)
 * Uses standard POSIX sockets for TCP communication
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "net.h"

/* Forward declaration for TLS implementation */
bool TLSClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res);

#define RECV_BUFFER_SIZE 8192
#define CONNECT_TIMEOUT_SEC 10
#define RECV_TIMEOUT_SEC 30

/*
 * Set socket to non-blocking mode with timeout
 */
static int
set_socket_timeout(int sockfd)
{
  struct timeval tv;

  /* Set receive timeout */
  tv.tv_sec = RECV_TIMEOUT_SEC;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    return -1;
  }

  /* Set send timeout */
  tv.tv_sec = CONNECT_TIMEOUT_SEC;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
    return -1;
  }

  return 0;
}

/*
 * Connect to host:port using POSIX sockets
 */
static int
tcp_connect(const char *host, int port, char *error_message, size_t error_len)
{
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *rp;
  int sockfd = -1;
  int ret;
  char port_str[16];

  /* Convert port to string */
  snprintf(port_str, sizeof(port_str), "%d", port);

  /* Set up hints for getaddrinfo() */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        /* IPv4 */
  hints.ai_socktype = SOCK_STREAM;  /* TCP */
  /* Note: AI_ADDRCONFIG removed - it can cause issues in some environments */

  /* Resolve hostname */
  ret = getaddrinfo(host, port_str, &hints, &result);
  if (ret != 0) {
    snprintf(error_message, error_len, "DNS resolution failed: %s", gai_strerror(ret));
    fprintf(stderr, "TCP DNS resolution failed for '%s:%d': %s\n", host, port, gai_strerror(ret));
    return -1;
  }

  /* Try each address until we successfully connect */
  int last_errno = 0;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    /* Create socket */
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      last_errno = errno;
      continue;
    }

    /* Set socket options (timeouts) */
    if (set_socket_timeout(sockfd) < 0) {
      last_errno = errno;
      close(sockfd);
      sockfd = -1;
      continue;
    }

    /* Connect to the address - retry on EINTR */
    int connect_result;
    do {
      connect_result = connect(sockfd, rp->ai_addr, rp->ai_addrlen);
    } while (connect_result == -1 && errno == EINTR);

    if (connect_result != -1) {
      /* Success! */
      break;
    }

    /* Connection failed, try next address */
    last_errno = errno;
    close(sockfd);
    sockfd = -1;
  }

  /* Free the result structure */
  freeaddrinfo(result);

  /* Check if we successfully connected */
  if (sockfd == -1) {
    snprintf(error_message, error_len, "Failed to connect to %s:%d: %s",
             host, port, strerror(last_errno));
    fprintf(stderr, "TCP connect failed for '%s:%d': %s\n", host, port, strerror(last_errno));
    return -1;
  }

  return sockfd;
}

/*
 * Send TCP request and receive response
 *
 * @param mrb   mruby state (unused in POSIX implementation, but kept for API compatibility)
 * @param req   Request parameters
 * @param res   Response structure (allocated inside)
 * @return      true on success, false on error
 */
bool
TCPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  int sockfd = -1;
  ssize_t sent_bytes, recv_bytes;
  char *recv_buffer = NULL;
  size_t recv_buffer_size = RECV_BUFFER_SIZE;
  size_t total_received = 0;
  bool success = false;

  (void)mrb;  /* Unused in POSIX implementation */

  /* Initialize response */
  memset(res, 0, sizeof(*res));

  /* Validate request */
  if (!req || !req->host || req->port <= 0) {
    snprintf(res->error_message, sizeof(res->error_message), "Invalid request parameters");
    return false;
  }

  /* Use TLS implementation if requested */
  if (req->is_tls) {
    return TLSClient_send(mrb, req, res);
  }

  /* Connect to server */
  sockfd = tcp_connect(req->host, req->port, res->error_message, sizeof(res->error_message));
  if (sockfd < 0) {
    return false;
  }

  /* Send request data */
  if (req->send_data && req->send_data_len > 0) {
    /* Retry send on EINTR */
    do {
      sent_bytes = send(sockfd, req->send_data, req->send_data_len, 0);
    } while (sent_bytes < 0 && errno == EINTR);

    if (sent_bytes < 0) {
      snprintf(res->error_message, sizeof(res->error_message), "Send failed: %s", strerror(errno));
      goto cleanup;
    }
    if ((size_t)sent_bytes != req->send_data_len) {
      snprintf(res->error_message, sizeof(res->error_message), "Incomplete send: %zd/%zu bytes", sent_bytes, req->send_data_len);
      goto cleanup;
    }
  }

  /* Allocate initial receive buffer */
  recv_buffer = (char *)malloc(recv_buffer_size);
  if (!recv_buffer) {
    snprintf(res->error_message, sizeof(res->error_message), "Memory allocation failed");
    goto cleanup;
  }

  /* Receive response data */
  while (1) {
    /* Ensure buffer has space */
    if (total_received + RECV_BUFFER_SIZE > recv_buffer_size) {
      char *new_buffer = (char *)realloc(recv_buffer, recv_buffer_size * 2);
      if (!new_buffer) {
        snprintf(res->error_message, sizeof(res->error_message), "Memory reallocation failed");
        goto cleanup;
      }
      recv_buffer = new_buffer;
      recv_buffer_size *= 2;
    }

    /* Receive data - retry on EINTR */
    do {
      recv_bytes = recv(sockfd, recv_buffer + total_received, RECV_BUFFER_SIZE, 0);
    } while (recv_bytes < 0 && errno == EINTR);

    if (recv_bytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Timeout - consider this as end of data */
        break;
      }
      snprintf(res->error_message, sizeof(res->error_message), "Receive failed: %s", strerror(errno));
      goto cleanup;
    } else if (recv_bytes == 0) {
      /* Connection closed by server */
      break;
    }

    total_received += recv_bytes;
  }

  /* Success */
  res->recv_data = recv_buffer;
  res->recv_data_len = total_received;
  success = true;
  recv_buffer = NULL;  /* Ownership transferred to response */

cleanup:
  if (sockfd >= 0) {
    close(sockfd);
  }
  if (recv_buffer) {
    free(recv_buffer);
  }

  return success;
}
