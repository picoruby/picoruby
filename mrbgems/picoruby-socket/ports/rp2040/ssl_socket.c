/*
 * SSL Socket implementation for rp2040 using mbedTLS and LwIP
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include "picoruby/debug.h"
#include <string.h>

/* mbedTLS includes */
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

/* SSL Context structure (rp2040 - mbedTLS) */
struct picorb_ssl_context {
  int verify_mode;
  mbedtls_x509_crt ca_cert;       /* Parsed CA certificate chain */
  bool ca_cert_loaded;
};

/* SSL socket structure */
struct picorb_ssl_socket {
  picorb_socket_t *base_socket;
  picorb_ssl_context_t *ssl_ctx;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  bool ssl_initialized;
  bool connected;
  char *hostname;
};

/* Send callback for mbedTLS */
static int
ssl_send_callback(void *ctx, const unsigned char *buf, size_t len)
{
  picorb_socket_t *sock = (picorb_socket_t *)ctx;
  ssize_t sent = TCPSocket_send(sock, buf, len);

  if (sent < 0) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  return (int)sent;
}

/* Receive callback for mbedTLS */
static int
ssl_recv_callback(void *ctx, unsigned char *buf, size_t len)
{
  picorb_socket_t *sock = (picorb_socket_t *)ctx;
  ssize_t received = TCPSocket_recv(sock, buf, len);

  if (received < 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  if (received == 0) {
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }

  return (int)received;
}

/*
 * Create SSL context
 */
picorb_ssl_context_t*
SSLContext_create(void)
{
  picorb_ssl_context_t *ctx = (picorb_ssl_context_t *)picorb_alloc(NULL, sizeof(picorb_ssl_context_t));
  if (!ctx) {
    return NULL;
  }

  memset(ctx, 0, sizeof(picorb_ssl_context_t));
  ctx->verify_mode = SSL_VERIFY_PEER;  /* Default to VERIFY_PEER for security */
  ctx->ca_cert_loaded = false;
  mbedtls_x509_crt_init(&ctx->ca_cert);

  return ctx;
}

/*
 * Set CA certificate file (not supported on rp2040)
 * Use set_ca_cert with memory address instead
 */
bool
SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file)
{
  (void)ctx;
  (void)ca_file;
  return false;
}

/*
 * Set CA certificate from memory (ROM or RAM)
 * addr: pointer to PEM-formatted certificate data
 * size: size of certificate data in bytes (must include null terminator for PEM)
 */
bool
SSLContext_set_ca_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  if (!ctx || !addr || size == 0) {
    return false;
  }

  /* Free previous certificate if loaded */
  if (ctx->ca_cert_loaded) {
    mbedtls_x509_crt_free(&ctx->ca_cert);
    mbedtls_x509_crt_init(&ctx->ca_cert);
    ctx->ca_cert_loaded = false;
  }

  /* Parse PEM certificate from memory */
  int ret = mbedtls_x509_crt_parse(&ctx->ca_cert,
                                    (const unsigned char *)addr,
                                    size);
  if (ret != 0) {
    return false;
  }

  ctx->ca_cert_loaded = true;
  return true;
}

/*
 * Set verification mode (stored but not enforced on rp2040)
 */
bool
SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode)
{
  if (!ctx) {
    return false;
  }

  ctx->verify_mode = mode;
  return true;
}

/*
 * Get verification mode
 */
int
SSLContext_get_verify_mode(picorb_ssl_context_t *ctx)
{
  if (!ctx) {
    return -1;
  }
  return ctx->verify_mode;
}

/*
 * Free SSL context
 */
void
SSLContext_free(picorb_ssl_context_t *ctx)
{
  if (!ctx) {
    return;
  }

  if (ctx->ca_cert_loaded) {
    mbedtls_x509_crt_free(&ctx->ca_cert);
  }

  picorb_free(NULL, ctx);
}

/* Create SSL socket */
picorb_ssl_socket_t*
SSLSocket_create(picorb_socket_t *tcp_socket, picorb_ssl_context_t *ssl_ctx)
{
  if (!tcp_socket || tcp_socket->state != SOCKET_STATE_CONNECTED || !ssl_ctx) {
    return NULL;
  }

  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)picorb_alloc(NULL, sizeof(picorb_ssl_socket_t));
  if (!ssl_sock) {
    return NULL;
  }

  memset(ssl_sock, 0, sizeof(picorb_ssl_socket_t));
  ssl_sock->base_socket = tcp_socket;
  ssl_sock->ssl_ctx = ssl_ctx;
  ssl_sock->ssl_initialized = false;
  ssl_sock->connected = false;

  return ssl_sock;
}

/* Set hostname for SNI */
bool
SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname)
{
  if (!ssl_sock || !hostname) {
    return false;
  }

  // Free previous hostname if set
  if (ssl_sock->hostname) {
    picorb_free(NULL, ssl_sock->hostname);
  }

  ssl_sock->hostname = (char *)picorb_alloc(NULL, strlen(hostname) + 1);
  if (!ssl_sock->hostname) {
    return false;
  }
  strcpy(ssl_sock->hostname, hostname);

  return true;
}

/* Initialize SSL context and perform handshake */
bool
SSLSocket_connect(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || ssl_sock->connected) {
    return false;
  }

  /* Initialize mbedTLS structures */
  mbedtls_ssl_init(&ssl_sock->ssl);
  mbedtls_ssl_config_init(&ssl_sock->conf);
  mbedtls_entropy_init(&ssl_sock->entropy);
  mbedtls_ctr_drbg_init(&ssl_sock->ctr_drbg);

  /* Seed RNG */
  const char *pers = "picoruby_ssl";
  int ret = mbedtls_ctr_drbg_seed(&ssl_sock->ctr_drbg, mbedtls_entropy_func,
                                   &ssl_sock->entropy,
                                   (const unsigned char *)pers, strlen(pers));
  if (ret != 0) {
    goto cleanup;
  }

  /* Configure SSL */
  ret = mbedtls_ssl_config_defaults(&ssl_sock->conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    goto cleanup;
  }

  /* Configure certificate verification based on context settings */
  if (ssl_sock->ssl_ctx->verify_mode == SSL_VERIFY_PEER) {
    /* Only require verification if CA certificate is loaded */
    if (ssl_sock->ssl_ctx->ca_cert_loaded) {
      D("SSL: verify req\n");
      mbedtls_ssl_conf_authmode(&ssl_sock->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
      mbedtls_ssl_conf_ca_chain(&ssl_sock->conf, &ssl_sock->ssl_ctx->ca_cert, NULL);
    } else {
      /* No CA cert loaded - use optional verification (allows connection without verification) */
      D("SSL: verify opt\n");
      mbedtls_ssl_conf_authmode(&ssl_sock->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    }
  } else {
    D("SSL: verify none\n");
    mbedtls_ssl_conf_authmode(&ssl_sock->conf, MBEDTLS_SSL_VERIFY_NONE);
  }

  mbedtls_ssl_conf_rng(&ssl_sock->conf, mbedtls_ctr_drbg_random, &ssl_sock->ctr_drbg);

  ret = mbedtls_ssl_setup(&ssl_sock->ssl, &ssl_sock->conf);
  if (ret != 0) {
    goto cleanup;
  }

  if (ssl_sock->hostname) {
    mbedtls_ssl_set_hostname(&ssl_sock->ssl, ssl_sock->hostname);
  }

  /* Set I/O callbacks */
  mbedtls_ssl_set_bio(&ssl_sock->ssl, ssl_sock->base_socket,
                       ssl_send_callback, ssl_recv_callback, NULL);

  /* Perform handshake */
  while ((ret = mbedtls_ssl_handshake(&ssl_sock->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      goto cleanup;
    }
    Net_sleep_ms(10);
  }

  ssl_sock->ssl_initialized = true;
  ssl_sock->connected = true;
  ssl_sock->base_socket->connected = true;
  return true;

cleanup:
  mbedtls_ssl_free(&ssl_sock->ssl);
  mbedtls_ssl_config_free(&ssl_sock->conf);
  mbedtls_ctr_drbg_free(&ssl_sock->ctr_drbg);
  mbedtls_entropy_free(&ssl_sock->entropy);
  return false;
}

/* Send data */
ssize_t
SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len)
{
  if (!ssl_sock || !ssl_sock->connected || !data) {
    return -1;
  }

  int ret = mbedtls_ssl_write(&ssl_sock->ssl, (const unsigned char *)data, len);
  if (ret < 0) {
    return -1;
  }

  return (ssize_t)ret;
}

/* Receive data */
ssize_t
SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len)
{
  if (!ssl_sock || !ssl_sock->connected || !buf) {
    return -1;
  }

  int ret = mbedtls_ssl_read(&ssl_sock->ssl, (unsigned char *)buf, len);
  if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
    return 0;
  }

  if (ret < 0) {
    return -1;
  }

  return (ssize_t)ret;
}

/* Close SSL socket */
bool
SSLSocket_close(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return false;
  }

  if (ssl_sock->ssl_initialized) {
    mbedtls_ssl_close_notify(&ssl_sock->ssl);
    mbedtls_ssl_free(&ssl_sock->ssl);
    mbedtls_ssl_config_free(&ssl_sock->conf);
    mbedtls_ctr_drbg_free(&ssl_sock->ctr_drbg);
    mbedtls_entropy_free(&ssl_sock->entropy);
  }

  if (ssl_sock->hostname) {
    picorb_free(NULL, ssl_sock->hostname);
  }

  picorb_free(NULL, ssl_sock);
  return true;
}

/* Check if closed */
bool
SSLSocket_closed(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || !ssl_sock->base_socket) {
    return true;
  }
  return TCPSocket_closed(ssl_sock->base_socket);
}

/* Get remote host */
const char*
SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || !ssl_sock->base_socket) {
    return NULL;
  }
  return TCPSocket_remote_host(ssl_sock->base_socket);
}

/* Get remote port */
int
SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || !ssl_sock->base_socket) {
    return -1;
  }
  return TCPSocket_remote_port(ssl_sock->base_socket);
}
