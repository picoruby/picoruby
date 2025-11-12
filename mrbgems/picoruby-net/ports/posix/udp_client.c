/*
 * UDP Client for PicoRuby Net (POSIX Implementation)
 * Uses standard POSIX sockets for UDP communication
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

#include "net.h"

#define UDP_RECV_BUFFER_SIZE 4096
#define UDP_RECV_TIMEOUT_SEC 5

/*
 * Send UDP datagram and receive response
 *
 * @param mrb   mruby state (unused in POSIX implementation, but kept for API compatibility)
 * @param req   Request parameters
 * @param res   Response structure (allocated inside)
 * @return      true on success, false on error
 */
bool
UDPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *rp;
  int sockfd = -1;
  int ret;
  char port_str[16];
  ssize_t sent_bytes, recv_bytes;
  char *recv_buffer = NULL;
  struct timeval tv;
  bool success = false;

  (void)mrb;  /* Unused in POSIX implementation */

  /* Initialize response */
  memset(res, 0, sizeof(*res));

  /* Validate request */
  if (!req || !req->host || req->port <= 0) {
    snprintf(res->error_message, sizeof(res->error_message), "Invalid request parameters");
    return false;
  }

  /* TLS not applicable for UDP */
  if (req->is_tls) {
    snprintf(res->error_message, sizeof(res->error_message), "TLS not supported for UDP");
    return false;
  }

  /* Convert port to string */
  snprintf(port_str, sizeof(port_str), "%d", req->port);

  /* Set up hints for getaddrinfo() */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        /* IPv4 */
  hints.ai_socktype = SOCK_DGRAM;   /* UDP */
  /* Note: AI_ADDRCONFIG removed - it can cause issues in some environments */

  /* Resolve hostname */
  ret = getaddrinfo(req->host, port_str, &hints, &result);
  if (ret != 0) {
    snprintf(res->error_message, sizeof(res->error_message), "DNS resolution failed: %s", gai_strerror(ret));
    fprintf(stderr, "UDP DNS resolution failed for '%s:%d': %s\n", req->host, req->port, gai_strerror(ret));
    return false;
  }

  /* Try each address until we successfully send */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    /* Create UDP socket */
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      continue;
    }

    /* Set receive timeout */
    tv.tv_sec = UDP_RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
      close(sockfd);
      sockfd = -1;
      continue;
    }

    /* Send datagram - retry on EINTR */
    if (req->send_data && req->send_data_len > 0) {
      do {
        sent_bytes = sendto(sockfd, req->send_data, req->send_data_len, 0,
                            rp->ai_addr, rp->ai_addrlen);
      } while (sent_bytes < 0 && errno == EINTR);

      if (sent_bytes >= 0) {
        /* Success */
        break;
      }
    }

    /* Send failed, try next address */
    close(sockfd);
    sockfd = -1;
  }

  /* Free the result structure */
  freeaddrinfo(result);

  /* Check if we successfully sent */
  if (sockfd == -1) {
    snprintf(res->error_message, sizeof(res->error_message), "Failed to send UDP datagram to %s:%d", req->host, req->port);
    return false;
  }

  /* Allocate receive buffer */
  recv_buffer = (char *)malloc(UDP_RECV_BUFFER_SIZE);
  if (!recv_buffer) {
    snprintf(res->error_message, sizeof(res->error_message), "Memory allocation failed");
    goto cleanup;
  }

  /* Receive response - retry on EINTR */
  do {
    recv_bytes = recvfrom(sockfd, recv_buffer, UDP_RECV_BUFFER_SIZE - 1, 0, NULL, NULL);
  } while (recv_bytes < 0 && errno == EINTR);

  if (recv_bytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      snprintf(res->error_message, sizeof(res->error_message), "Receive timeout");
    } else {
      snprintf(res->error_message, sizeof(res->error_message), "Receive failed: %s", strerror(errno));
    }
    goto cleanup;
  }

  /* Success */
  res->recv_data = recv_buffer;
  res->recv_data_len = recv_bytes;
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
