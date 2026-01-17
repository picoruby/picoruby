/*
 * SSL Socket implementation for esp32 using mbedtls
 */

#include "../../include/socket.h"
#include "picoruby.h"

/* mbedtls includes */
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

/* SSL connection states */
#define SSL_STATE_NONE           0
#define SSL_STATE_CONNECTING     1
#define SSL_STATE_CONNECTED      2
#define SSL_STATE_ERROR          3

/* SSL Context structure */
struct picorb_ssl_context {
  mbedtls_ssl_config ssl_config;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_x509_crt cacert;
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
  bool client_cert_loaded;
  bool client_key_loaded;
  int verify_mode;
};

/* SSL socket structure */
struct picorb_ssl_socket {
  picorb_ssl_context_t *ssl_ctx;
  mbedtls_net_context net_ctx;
  mbedtls_ssl_context ssl;
  int state;
  char *hostname;
  int port;
};

/* ========================================================================
 * SSLContext Functions
 * ======================================================================== */

picorb_ssl_context_t*
SSLContext_create(void)
{
  picorb_ssl_context_t *ctx = (picorb_ssl_context_t *)picorb_alloc(NULL, sizeof(picorb_ssl_context_t));
  if (!ctx) return NULL;

  mbedtls_ssl_config_init(&ctx->ssl_config);
  mbedtls_entropy_init(&ctx->entropy);
  mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
  mbedtls_x509_crt_init(&ctx->cacert);
  mbedtls_x509_crt_init(&ctx->cert);
  mbedtls_pk_init(&ctx->key);
  ctx->client_cert_loaded = false;
  ctx->client_key_loaded = false;
  ctx->verify_mode = SSL_VERIFY_PEER;

  /* Seed the random number generator */
  if (mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy, NULL, 0) != 0) {
    SSLContext_free(ctx);
    return NULL;
  }

  /* Setup SSL/TLS configuration */
  if (mbedtls_ssl_config_defaults(&ctx->ssl_config,
                                MBEDTLS_SSL_IS_CLIENT,
                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    SSLContext_free(ctx);
    return NULL;
  }

  mbedtls_ssl_conf_rng(&ctx->ssl_config, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

  /* Default to verifying peer certificate */
  mbedtls_ssl_conf_authmode(&ctx->ssl_config, MBEDTLS_SSL_VERIFY_REQUIRED);

  return ctx;
}

void
SSLContext_free(picorb_ssl_context_t *ctx)
{
  if (!ctx) return;
  mbedtls_x509_crt_free(&ctx->cacert);
  mbedtls_x509_crt_free(&ctx->cert);
  mbedtls_pk_free(&ctx->key);
  mbedtls_ssl_config_free(&ctx->ssl_config);
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_entropy_free(&ctx->entropy);
  picorb_free(NULL, ctx);
}

bool
SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file)
{
  (void)ctx;
  (void)ca_file;
  return false;  /* Not supported on ESP32 */
}

bool
SSLContext_set_ca_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  if (!ctx || !addr || size == 0) return false;

  int ret = mbedtls_x509_crt_parse(&ctx->cacert, (const unsigned char *)addr, size + 1);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    return false;
  }
  mbedtls_ssl_conf_ca_chain(&ctx->ssl_config, &ctx->cacert, NULL);
  return true;
}

bool
SSLContext_set_cert_file(picorb_ssl_context_t *ctx, const char *cert_file)
{
  (void)ctx;
  (void)cert_file;
  return false;  /* Not supported on ESP32 */
}

bool
SSLContext_set_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  if (!ctx || !addr || size == 0) return false;

  int ret = mbedtls_x509_crt_parse(&ctx->cert, (const unsigned char *)addr, size + 1);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    return false;
  }
  if (ctx->client_key_loaded) {
    ret = mbedtls_ssl_conf_own_cert(&ctx->ssl_config, &ctx->cert, &ctx->key);
    if (ret != 0) {
      char error_buf[100];
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      return false;
    }
  }
  ctx->client_cert_loaded = true;
  return true;
}

bool
SSLContext_set_key_file(picorb_ssl_context_t *ctx, const char *key_file)
{
  (void)ctx;
  (void)key_file;
  return false;  /* Not supported on ESP32 */
}

bool
SSLContext_set_key(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  if (!ctx || !addr || size == 0) return false;

  int ret = mbedtls_pk_parse_key(&ctx->key, (const unsigned char *)addr, size + 1, NULL, 0, NULL, NULL);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    return false;
  }
  if (ctx->client_cert_loaded) {
    ret = mbedtls_ssl_conf_own_cert(&ctx->ssl_config, &ctx->cert, &ctx->key);
    if (ret != 0) {
      char error_buf[100];
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      return false;
    }
  }
  ctx->client_key_loaded = true;
  return true;
}

bool
SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode)
{
  if (!ctx) return false;
  int mbedtls_mode;
  switch (mode) {
    case SSL_VERIFY_NONE:
      mbedtls_mode = MBEDTLS_SSL_VERIFY_NONE;
      break;
    case SSL_VERIFY_PEER:
      mbedtls_mode = MBEDTLS_SSL_VERIFY_REQUIRED;
      break;
    default:
      return false;
  }
  mbedtls_ssl_conf_authmode(&ctx->ssl_config, mbedtls_mode);
  ctx->verify_mode = mode;
  return true;
}

int
SSLContext_get_verify_mode(picorb_ssl_context_t *ctx)
{
  if (!ctx) return -1;
  return ctx->verify_mode;
}

/* ========================================================================
 * SSLSocket Functions
 * ======================================================================== */

picorb_ssl_socket_t*
SSLSocket_create(picorb_ssl_context_t *ssl_ctx)
{
  if (!ssl_ctx) return NULL;

  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)picorb_alloc(NULL, sizeof(picorb_ssl_socket_t));
  if (!ssl_sock) return NULL;
  memset(ssl_sock, 0, sizeof(picorb_ssl_socket_t));

  ssl_sock->ssl_ctx = ssl_ctx;
  ssl_sock->state = SSL_STATE_NONE;

  mbedtls_net_init(&ssl_sock->net_ctx);
  mbedtls_ssl_init(&ssl_sock->ssl);

  return ssl_sock;
}

bool
SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname)
{
  if (!ssl_sock || !hostname) return false;
  if (ssl_sock->hostname) picorb_free(NULL, ssl_sock->hostname);
  ssl_sock->hostname = (char *)picorb_alloc(NULL, strlen(hostname) + 1);
  if (!ssl_sock->hostname) return false;
  strcpy(ssl_sock->hostname, hostname);
  return true;
}

bool
SSLSocket_set_port(picorb_ssl_socket_t *ssl_sock, int port)
{
  if (!ssl_sock || port <= 0 || port > 65535) return false;
  ssl_sock->port = port;
  return true;
}

bool
SSLSocket_connect(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock || !ssl_sock->hostname || ssl_sock->state != SSL_STATE_NONE) return false;

  ssl_sock->state = SSL_STATE_CONNECTING;
  int ret;

  if (ssl_sock->port == 0) ssl_sock->port = 443;
  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", ssl_sock->port);

  /* 1. TCP Connect */
  ret = mbedtls_net_connect(&ssl_sock->net_ctx, ssl_sock->hostname, port_str, MBEDTLS_NET_PROTO_TCP);
  if (ret != 0) {
    ssl_sock->state = SSL_STATE_ERROR;
    return false;
  }

  /* 2. Setup SSL */
  ret = mbedtls_ssl_setup(&ssl_sock->ssl, &ssl_sock->ssl_ctx->ssl_config);
  if (ret != 0) {
    mbedtls_net_free(&ssl_sock->net_ctx);
    ssl_sock->state = SSL_STATE_ERROR;
    return false;
  }

  ret = mbedtls_ssl_set_hostname(&ssl_sock->ssl, ssl_sock->hostname);
  if (ret != 0) {
    mbedtls_net_free(&ssl_sock->net_ctx);
    ssl_sock->state = SSL_STATE_ERROR;
    return false;
  }

  mbedtls_ssl_set_bio(&ssl_sock->ssl, &ssl_sock->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

  /* 3. Handshake */
  while ((ret = mbedtls_ssl_handshake(&ssl_sock->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      mbedtls_net_free(&ssl_sock->net_ctx);
      ssl_sock->state = SSL_STATE_ERROR;
      return false;
    }
  }

  ssl_sock->state = SSL_STATE_CONNECTED;
  return true;
}

ssize_t
SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len)
{
  if (!ssl_sock || ssl_sock->state != SSL_STATE_CONNECTED || !data) return -1;

  int ret = mbedtls_ssl_write(&ssl_sock->ssl, (const unsigned char *)data, len);
  if (ret < 0) {
    if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ) {
      return 0; // Would block
    }
    ssl_sock->state = SSL_STATE_ERROR;
    return -1;
  }
  return (ssize_t)ret;
}

ssize_t
SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len)
{
  if (!ssl_sock || ssl_sock->state != SSL_STATE_CONNECTED || !buf) return -1;

  int ret = mbedtls_ssl_read(&ssl_sock->ssl, (unsigned char *)buf, len);
  if (ret < 0) {
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      return 0; // Would block
    }
    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      // Connection closed by peer
    }
    ssl_sock->state = SSL_STATE_ERROR;
    return -1;
  }
  if (ret == 0) {
    // EOF
    ssl_sock->state = SSL_STATE_NONE;
  }
  return (ssize_t)ret;
}

bool
SSLSocket_close(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) return false;

  if (ssl_sock->state == SSL_STATE_CONNECTED) {
    mbedtls_ssl_close_notify(&ssl_sock->ssl);
  }
  mbedtls_net_free(&ssl_sock->net_ctx);
  mbedtls_ssl_free(&ssl_sock->ssl);

  if (ssl_sock->hostname) {
    picorb_free(NULL, ssl_sock->hostname);
    ssl_sock->hostname = NULL;
  }

  picorb_free(NULL, ssl_sock);
  return true;
}

bool
SSLSocket_closed(picorb_ssl_socket_t *ssl_sock)
{
  return !ssl_sock || ssl_sock->state != SSL_STATE_CONNECTED;
}

const char*
SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) return NULL;
  return ssl_sock->hostname;
}

int
SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) return -1;
  return ssl_sock->port;
}
