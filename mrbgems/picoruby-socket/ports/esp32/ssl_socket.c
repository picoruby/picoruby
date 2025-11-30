#include "../../include/socket.h"
#include <stdbool.h>
#include <stddef.h>

struct picorb_ssl_context {
  int dummy;
};

struct picorb_ssl_socket {
  int dummy;
};

picorb_ssl_context_t*
SSLContext_create(void)
{
  /* No operation implementation */
  return NULL;
}

bool
SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file)
{
  /* No operation implementation */
  return false;
}

bool
SSLContext_set_ca_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  /* No operation implementation */
  return false;
}

bool
SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode)
{
  /* No operation implementation */
  return false;
}

int
SSLContext_get_verify_mode(picorb_ssl_context_t *ctx)
{
  /* No operation implementation */
  return -1;
}

void
SSLContext_free(picorb_ssl_context_t *ctx)
{
  /* No operation implementation */
}

picorb_ssl_socket_t*
SSLSocket_create(picorb_ssl_context_t *ssl_ctx)
{
  /* No operation implementation */
  return NULL;
}

bool
SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname)
{
  /* No operation implementation */
  return false;
}

bool
SSLSocket_set_port(picorb_ssl_socket_t *ssl_sock, int port)
{
  /* No operation implementation */
  return false;
}

bool
SSLSocket_connect(picorb_ssl_socket_t *ssl_sock)
{
  /* No operation implementation */
  return false;
}

ssize_t
SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len)
{
  /* No operation implementation */
  return -1;
}

ssize_t
SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len)
{
  /* No operation implementation */
  return -1;
}

bool
SSLSocket_close(picorb_ssl_socket_t *ssl_sock)
{
  /* No operation implementation */
  return false;
}

bool
SSLSocket_closed(picorb_ssl_socket_t *ssl_sock)
{
  /* No operation implementation */
  return true;
}

const char*
SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock)
{
  /* No operation implementation */
  return NULL;
}

int
SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock)
{
  /* No operation implementation */
  return -1;
}
