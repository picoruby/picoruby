/*
 * SSLSocket implementation for PicoRuby (POSIX)
 * Uses OpenSSL for TLS/SSL support over TCP sockets
 */

#include "../../include/socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

/* SSL Context and SSL Socket structures are now defined in socket.h */

/*
 * Initialize OpenSSL library (call once)
 */
static void
ssl_init_once(void)
{
  static bool initialized = false;
  if (!initialized) {
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
    initialized = true;
  }
}

/*
 * Create SSL context
 */
picorb_ssl_context_t*
SSLContext_create(void)
{
  ssl_init_once();

  picorb_ssl_context_t *ctx = (picorb_ssl_context_t*)malloc(sizeof(picorb_ssl_context_t));
  if (!ctx) {
    return NULL;
  }

  memset(ctx, 0, sizeof(picorb_ssl_context_t));

  // Create SSL_CTX with TLS client method
  ctx->ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx->ctx) {
    fprintf(stderr, "SSL: SSL_CTX_new failed\n");
    ERR_print_errors_fp(stderr);
    free(ctx);
    return NULL;
  }

  // Default: verify peer
  ctx->verify_mode = SSL_VERIFY_PEER;
  SSL_CTX_set_verify(ctx->ctx, SSL_VERIFY_PEER, NULL);

  // Load default CA certificates from system
  if (SSL_CTX_set_default_verify_paths(ctx->ctx) != 1) {
    fprintf(stderr, "SSL: Warning - failed to load default CA certificates\n");
  }

  return ctx;
}

/*
 * Set CA certificate file
 */
bool
SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file)
{
  if (!ctx || !ca_file) {
    return false;
  }

  // Free previous ca_file if set
  if (ctx->ca_file) {
    free(ctx->ca_file);
    ctx->ca_file = NULL;
  }

  // Store ca_file path
  ctx->ca_file = strdup(ca_file);
  if (!ctx->ca_file) {
    return false;
  }

  // Load CA certificate file
  if (SSL_CTX_load_verify_locations(ctx->ctx, ca_file, NULL) != 1) {
    free(ctx->ca_file);
    ctx->ca_file = NULL;
    fprintf(stderr, "SSL: Failed to load CA file: %s\n", ca_file);
    ERR_print_errors_fp(stderr);
    return false;
  }

  return true;
}

/*
 * Set CA certificate from memory
 * Not supported on POSIX - use set_ca_file instead
 */
bool
SSLContext_set_ca_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  (void)ctx;
  (void)addr;
  (void)size;
  fprintf(stderr, "Warning: SSLContext#set_ca_cert is not supported on POSIX platforms. Use ca_file= instead.\n");
  return true;  /* Return true to avoid errors, but do nothing */
}

/*
 * Set certificate file
 */
bool
SSLContext_set_cert_file(picorb_ssl_context_t *ctx, const char *cert_file)
{
  if (!ctx || !cert_file) {
    return false;
  }

  // Free previous cert_file if set
  if (ctx->cert_file) {
    free(ctx->cert_file);
    ctx->cert_file = NULL;
  }

  // Store cert_file path
  ctx->cert_file = strdup(cert_file);
  if (!ctx->cert_file) {
    return false;
  }

  // Load certificate file
  if (SSL_CTX_use_certificate_file(ctx->ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
    free(ctx->cert_file);
    ctx->cert_file = NULL;
    fprintf(stderr, "SSL: Failed to load certificate file: %s\n", cert_file);
    ERR_print_errors_fp(stderr);
    return false;
  }

  return true;
}

/*
 * Set certificate from memory
 * Not supported on POSIX - use set_cert_file instead
 */
bool
SSLContext_set_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  (void)ctx;
  (void)addr;
  (void)size;
  fprintf(stderr, "Warning: SSLContext#set_cert is not supported on POSIX platforms. Use cert_file= instead.\n");
  return true;  /* Return true to avoid errors, but do nothing */
}

/*
 * Set key file
 */
bool
SSLContext_set_key_file(picorb_ssl_context_t *ctx, const char *key_file)
{
  if (!ctx || !key_file) {
    return false;
  }

  // Free previous key_file if set
  if (ctx->key_file) {
    free(ctx->key_file);
    ctx->key_file = NULL;
  }

  // Store key_file path
  ctx->key_file = strdup(key_file);
  if (!ctx->key_file) {
    return false;
  }

  // Load key file
  if (SSL_CTX_use_PrivateKey_file(ctx->ctx, key_file, SSL_FILETYPE_PEM) != 1) {
    free(ctx->key_file);
    ctx->key_file = NULL;
    fprintf(stderr, "SSL: Failed to load key file: %s\n", key_file);
    ERR_print_errors_fp(stderr);
    return false;
  }

  return true;
}

/*
 * Set key from memory
 * Not supported on POSIX - use set_key_file instead
 */
bool
SSLContext_set_key(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  (void)ctx;
  (void)addr;
  (void)size;
  fprintf(stderr, "Warning: SSLContext#set_key is not supported on POSIX platforms. Use key_file= instead.\n");
  return true;  /* Return true to avoid errors, but do nothing */
}

/*
 * Set verification mode
 */
bool
SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode)
{
  if (!ctx) {
    return false;
  }

  ctx->verify_mode = mode;

  if (mode == SSL_VERIFY_NONE) {
    SSL_CTX_set_verify(ctx->ctx, SSL_VERIFY_NONE, NULL);
  } else {
    SSL_CTX_set_verify(ctx->ctx, SSL_VERIFY_PEER, NULL);
  }

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

  if (ctx->ctx) {
    SSL_CTX_free(ctx->ctx);
  }

  if (ctx->ca_file) {
    free(ctx->ca_file);
  }
  if (ctx->cert_file) {
    free(ctx->cert_file);
  }
  if (ctx->key_file) {
    free(ctx->key_file);
  }

  free(ctx);
}

/*
 * Create SSL socket
 * NOTE: This is called from SSLSocket.new(tcp_socket, ssl_context) in Ruby.
 * The tcp_socket provides hostname and port via remote_host/remote_port methods,
 * but we create a new TCP connection internally rather than reusing the existing one.
 */
picorb_ssl_socket_t*
SSLSocket_create(picorb_ssl_context_t *ssl_ctx)
{
  if (!ssl_ctx || !ssl_ctx->ctx) {
    return NULL;
  }

  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t*)malloc(sizeof(picorb_ssl_socket_t));
  if (!ssl_sock) {
    return NULL;
  }

  memset(ssl_sock, 0, sizeof(picorb_ssl_socket_t));
  ssl_sock->base_socket = NULL;
  ssl_sock->ssl_ctx = ssl_ctx;
  ssl_sock->ssl = NULL;
  ssl_sock->hostname = NULL;
  ssl_sock->port = 0;
  ssl_sock->connected = false;

  return ssl_sock;
}

/*
 * Set hostname for SNI (Server Name Indication)
 * Note: SNI is actually set during connect() when SSL structure is created
 */
bool
SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname)
{
  if (!ssl_sock || !hostname) {
    return false;
  }

  /* Free previous hostname if set */
  if (ssl_sock->hostname) {
    free(ssl_sock->hostname);
  }

  ssl_sock->hostname = strdup(hostname);
  if (!ssl_sock->hostname) {
    return false;
  }

  return true;
}

/*
 * Set port for SSL socket
 */
bool
SSLSocket_set_port(picorb_ssl_socket_t *ssl_sock, int port)
{
  if (!ssl_sock || port <= 0 || port > 65535) {
    return false;
  }

  ssl_sock->port = port;
  return true;
}

/*
 * Connect SSL socket
 * Performs TCP connection and SSL/TLS handshake
 */
bool
SSLSocket_connect(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || ssl_sock->connected || !ssl_sock->hostname) {
    return false;
  }

  /* Use default HTTPS port if not set */
  if (ssl_sock->port == 0) {
    ssl_sock->port = 443;
  }

  /* Create underlying TCP socket */
  ssl_sock->base_socket = (picorb_socket_t*)malloc(sizeof(picorb_socket_t));
  if (!ssl_sock->base_socket) {
    fprintf(stderr, "SSL: Failed to allocate TCP socket\n");
    return false;
  }

  if (!TCPSocket_create(ssl_sock->base_socket)) {
    fprintf(stderr, "SSL: Failed to create TCP socket\n");
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Connect TCP socket */
  if (!TCPSocket_connect(ssl_sock->base_socket, ssl_sock->hostname, ssl_sock->port)) {
    fprintf(stderr, "SSL: Failed to connect TCP socket to %s:%d\n",
            ssl_sock->hostname, ssl_sock->port);
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Create SSL structure */
  ssl_sock->ssl = SSL_new(ssl_sock->ssl_ctx->ctx);
  if (!ssl_sock->ssl) {
    fprintf(stderr, "SSL: SSL_new failed\n");
    ERR_print_errors_fp(stderr);
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Set file descriptor for SSL */
  if (SSL_set_fd(ssl_sock->ssl, ssl_sock->base_socket->fd) != 1) {
    fprintf(stderr, "SSL: SSL_set_fd failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(ssl_sock->ssl);
    ssl_sock->ssl = NULL;
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Set SNI hostname */
  if (SSL_set_tlsext_host_name(ssl_sock->ssl, ssl_sock->hostname) != 1) {
    fprintf(stderr, "SSL: SSL_set_tlsext_host_name failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(ssl_sock->ssl);
    ssl_sock->ssl = NULL;
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Set hostname for certificate verification */
  if (SSL_set1_host(ssl_sock->ssl, ssl_sock->hostname) != 1) {
    fprintf(stderr, "SSL: SSL_set1_host failed\n");
    ERR_print_errors_fp(stderr);
    SSL_free(ssl_sock->ssl);
    ssl_sock->ssl = NULL;
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Perform SSL handshake */
  int ret = SSL_connect(ssl_sock->ssl);
  if (ret != 1) {
    int err = SSL_get_error(ssl_sock->ssl, ret);
    fprintf(stderr, "SSL: SSL_connect failed with error %d\n", err);
    ERR_print_errors_fp(stderr);
    SSL_free(ssl_sock->ssl);
    ssl_sock->ssl = NULL;
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
    return false;
  }

  /* Verify certificate if in VERIFY_PEER mode */
  if (ssl_sock->ssl_ctx->verify_mode == SSL_VERIFY_PEER) {
    long verify_result = SSL_get_verify_result(ssl_sock->ssl);
    if (verify_result != X509_V_OK) {
      fprintf(stderr, "SSL: Certificate verification failed: %ld\n", verify_result);
      SSL_free(ssl_sock->ssl);
      ssl_sock->ssl = NULL;
      TCPSocket_close(ssl_sock->base_socket);
      free(ssl_sock->base_socket);
      ssl_sock->base_socket = NULL;
      return false;
    }
  }

  ssl_sock->connected = true;
  return true;
}

/*
 * Send data over SSL socket
 */
ssize_t
SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len)
{
  if (!ssl_sock || !ssl_sock->connected) {
    return -1;
  }

  int ret = SSL_write(ssl_sock->ssl, data, (int)len);
  if (ret <= 0) {
    int err = SSL_get_error(ssl_sock->ssl, ret);
    fprintf(stderr, "SSL: SSL_write failed with error %d\n", err);
    ERR_print_errors_fp(stderr);
    return -1;
  }

  return (ssize_t)ret;
}

/*
 * Receive data from SSL socket
 */
ssize_t
SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len)
{
  if (!ssl_sock || !ssl_sock->connected) {
    return -1;
  }

  int ret = SSL_read(ssl_sock->ssl, buf, (int)len);
  if (ret < 0) {
    int err = SSL_get_error(ssl_sock->ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      // Would block
      return 0;
    }
    fprintf(stderr, "SSL: SSL_read failed with error %d\n", err);
    ERR_print_errors_fp(stderr);
    return -1;
  }

  return (ssize_t)ret;
}

/*
 * Close SSL socket
 */
bool
SSLSocket_close(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return false;
  }

  /* Send close_notify alert if connected */
  if (ssl_sock->connected && ssl_sock->ssl) {
    SSL_shutdown(ssl_sock->ssl);
  }

  /* Free SSL structure */
  if (ssl_sock->ssl) {
    SSL_free(ssl_sock->ssl);
    ssl_sock->ssl = NULL;
  }

  /* Free hostname */
  if (ssl_sock->hostname) {
    free(ssl_sock->hostname);
    ssl_sock->hostname = NULL;
  }

  /* Close and free underlying TCP socket */
  if (ssl_sock->base_socket) {
    TCPSocket_close(ssl_sock->base_socket);
    free(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
  }

  free(ssl_sock);
  return true;
}

/*
 * Check if SSL socket is closed
 */
bool
SSLSocket_closed(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return true;
  }
  return !ssl_sock->connected;
}

/*
 * Get remote host
 */
const char*
SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return NULL;
  }
  return ssl_sock->hostname;
}

/*
 * Get remote port
 */
int
SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return -1;
  }
  return ssl_sock->port;
}
