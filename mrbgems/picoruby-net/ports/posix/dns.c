/*
 * DNS Resolution for PicoRuby Net (POSIX Implementation)
 * Uses standard POSIX getaddrinfo() for DNS lookups
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

#include "net.h"

/*
 * Resolve hostname to IP address using getaddrinfo()
 *
 * @param name      Hostname to resolve
 * @param is_tcp    true for TCP (SOCK_STREAM), false for UDP (SOCK_DGRAM)
 * @param outbuf    Output buffer for IP address string
 * @param outlen    Size of output buffer
 */
void
DNS_resolve(const char *name, bool is_tcp, char *outbuf, size_t outlen)
{
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *rp;
  int ret;

  if (!name || !outbuf || outlen < DNS_OUTBUF_SIZE) {
    if (outbuf && outlen > 0) {
      outbuf[0] = '\0';
    }
    return;
  }

  /* Set up hints for getaddrinfo() */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;  /* IPv4 only for now */
  hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;
  /* Note: AI_ADDRCONFIG removed - it can cause issues in some environments */

  /* Perform DNS lookup */
  ret = getaddrinfo(name, NULL, &hints, &result);
  if (ret != 0) {
    /* Resolution failed - log error to stderr for debugging */
    fprintf(stderr, "DNS resolution failed for '%s': %s\n", name, gai_strerror(ret));
    outbuf[0] = '\0';
    return;
  }

  /* Find first valid IPv4 address */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if (rp->ai_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;

      /* Convert to string format */
      if (inet_ntop(AF_INET, &addr->sin_addr, outbuf, outlen)) {
        /* Success */
        break;
      }
    }
  }

  /* If no valid address found, clear output buffer */
  if (rp == NULL && outlen > 0) {
    outbuf[0] = '\0';
  }

  /* Free the result structure */
  freeaddrinfo(result);
}

/*
 * Get local IP address
 * For POSIX, this returns a placeholder since there's no single "network interface"
 * like on embedded systems
 */
const char *
Net_get_ipv4_address(char *buf, size_t buflen)
{
  if (buf && buflen > 0) {
    snprintf(buf, buflen, "0.0.0.0");
  }
  return buf;
}
