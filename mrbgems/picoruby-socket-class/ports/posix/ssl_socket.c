/*
 * SSLSocket implementation for PicoRuby (POSIX)
 * Uses mbedTLS for TLS/SSL support over TCP sockets
 */

#include "../../include/socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

#define SSL_RECV_BUFFER_SIZE 8192

/*
 * Custom send function for mbedtls that uses POSIX socket
 */
static int ssl_send_callback(void *ctx, const unsigned char *buf, size_t len) {
  int fd = *(int*)ctx;
  int ret = send(fd, buf, len, 0);

  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    // Return generic error for other send failures
    return -1;
  }

  return ret;
}

/*
 * Custom receive function for mbedtls that uses POSIX socket
 */
static int ssl_recv_callback(void *ctx, unsigned char *buf, size_t len) {
  int fd = *(int*)ctx;
  int ret = recv(fd, buf, len, 0);

  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return MBEDTLS_ERR_SSL_WANT_READ;
    }
    // Return generic error for other recv failures
    return -1;
  }

  if (ret == 0) {
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }

  return ret;
}

/*
 * SSL socket structure (POSIX)
 */
typedef struct picorb_ssl_socket {
  picorb_socket_t *base_socket;      // Underlying TCP socket
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  bool ssl_initialized;
  bool handshake_done;
  char hostname[256];
} picorb_ssl_socket_t;

/*
 * Create SSL socket wrapping a TCP socket
 */
picorb_ssl_socket_t* SSLSocket_create(picorb_socket_t *tcp_socket, const char *hostname) {
  if (!tcp_socket || !tcp_socket->connected) {
    return NULL;
  }

  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t*)malloc(sizeof(picorb_ssl_socket_t));
  if (!ssl_sock) {
    return NULL;
  }

  memset(ssl_sock, 0, sizeof(picorb_ssl_socket_t));
  ssl_sock->base_socket = tcp_socket;
  ssl_sock->ssl_initialized = false;
  ssl_sock->handshake_done = false;

  if (hostname) {
    strncpy(ssl_sock->hostname, hostname, sizeof(ssl_sock->hostname) - 1);
    ssl_sock->hostname[sizeof(ssl_sock->hostname) - 1] = '\0';
  }

  return ssl_sock;
}

/*
 * Initialize SSL/TLS context and perform handshake
 */
bool SSLSocket_init(picorb_ssl_socket_t *ssl_sock) {
  if (!ssl_sock || ssl_sock->ssl_initialized) {
    return false;
  }

  int ret;
  const char *pers = "picoruby_ssl_socket";

  // Initialize structures
  mbedtls_ssl_init(&ssl_sock->ssl);
  mbedtls_ssl_config_init(&ssl_sock->conf);
  mbedtls_entropy_init(&ssl_sock->entropy);
  mbedtls_ctr_drbg_init(&ssl_sock->ctr_drbg);

  // Seed the RNG
  ret = mbedtls_ctr_drbg_seed(&ssl_sock->ctr_drbg,
                               mbedtls_entropy_func,
                               &ssl_sock->entropy,
                               (const unsigned char *)pers,
                               strlen(pers));
  if (ret != 0) {
    fprintf(stderr, "SSL: ctr_drbg_seed failed: -0x%04x\n", -ret);
    goto error;
  }

  // Setup SSL/TLS config
  ret = mbedtls_ssl_config_defaults(&ssl_sock->conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    fprintf(stderr, "SSL: config_defaults failed: -0x%04x\n", -ret);
    goto error;
  }

  // Configure SSL/TLS settings
  // Note: VERIFY_NONE for now (can be made configurable later)
  mbedtls_ssl_conf_authmode(&ssl_sock->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&ssl_sock->conf, mbedtls_ctr_drbg_random, &ssl_sock->ctr_drbg);

  // Setup SSL context
  ret = mbedtls_ssl_setup(&ssl_sock->ssl, &ssl_sock->conf);
  if (ret != 0) {
    fprintf(stderr, "SSL: setup failed: -0x%04x\n", -ret);
    goto error;
  }

  // Set hostname for SNI (Server Name Indication)
  if (ssl_sock->hostname[0] != '\0') {
    ret = mbedtls_ssl_set_hostname(&ssl_sock->ssl, ssl_sock->hostname);
    if (ret != 0) {
      fprintf(stderr, "SSL: set_hostname failed: -0x%04x\n", -ret);
      goto error;
    }
  }

  // Set BIO callbacks to use underlying TCP socket
  mbedtls_ssl_set_bio(&ssl_sock->ssl,
                      &ssl_sock->base_socket->fd,
                      ssl_send_callback,
                      ssl_recv_callback,
                      NULL);

  ssl_sock->ssl_initialized = true;

  // Perform SSL/TLS handshake
  while ((ret = mbedtls_ssl_handshake(&ssl_sock->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      fprintf(stderr, "SSL: handshake failed: -0x%04x\n", -ret);
      goto error;
    }
  }

  ssl_sock->handshake_done = true;
  return true;

error:
  if (ssl_sock->ssl_initialized) {
    mbedtls_ssl_free(&ssl_sock->ssl);
    mbedtls_ssl_config_free(&ssl_sock->conf);
    mbedtls_ctr_drbg_free(&ssl_sock->ctr_drbg);
    mbedtls_entropy_free(&ssl_sock->entropy);
    ssl_sock->ssl_initialized = false;
  }
  return false;
}

/*
 * Send data over SSL socket
 */
ssize_t SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len) {
  if (!ssl_sock || !ssl_sock->handshake_done) {
    return -1;
  }

  int ret;
  size_t written = 0;

  while (written < len) {
    ret = mbedtls_ssl_write(&ssl_sock->ssl,
                            (const unsigned char *)data + written,
                            len - written);

    if (ret < 0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        fprintf(stderr, "SSL: write failed: -0x%04x\n", -ret);
        return -1;
      }
      // Would block, retry
      continue;
    }

    written += ret;
  }

  return (ssize_t)written;
}

/*
 * Receive data from SSL socket
 */
ssize_t SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len) {
  if (!ssl_sock || !ssl_sock->handshake_done) {
    return -1;
  }

  int ret = mbedtls_ssl_read(&ssl_sock->ssl, (unsigned char *)buf, len);

  if (ret < 0) {
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      // Would block
      return 0;
    }
    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      // Connection closed by peer
      return 0;
    }
    fprintf(stderr, "SSL: read failed: -0x%04x\n", -ret);
    return -1;
  }

  return ret;
}

/*
 * Close SSL socket
 */
bool SSLSocket_close(picorb_ssl_socket_t *ssl_sock) {
  if (!ssl_sock) {
    return false;
  }

  // Send close_notify alert
  if (ssl_sock->handshake_done) {
    mbedtls_ssl_close_notify(&ssl_sock->ssl);
  }

  // Free SSL resources
  if (ssl_sock->ssl_initialized) {
    mbedtls_ssl_free(&ssl_sock->ssl);
    mbedtls_ssl_config_free(&ssl_sock->conf);
    mbedtls_ctr_drbg_free(&ssl_sock->ctr_drbg);
    mbedtls_entropy_free(&ssl_sock->entropy);
  }

  // Close underlying TCP socket
  if (ssl_sock->base_socket) {
    TCPSocket_close(ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
  }

  free(ssl_sock);
  return true;
}

/*
 * Check if SSL socket is closed
 */
bool SSLSocket_closed(picorb_ssl_socket_t *ssl_sock) {
  if (!ssl_sock || !ssl_sock->base_socket) {
    return true;
  }
  return ssl_sock->base_socket->closed;
}

/*
 * Get remote host from underlying TCP socket
 */
const char* SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock) {
  if (!ssl_sock || !ssl_sock->base_socket) {
    return NULL;
  }
  return ssl_sock->base_socket->remote_host;
}

/*
 * Get remote port from underlying TCP socket
 */
int SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock) {
  if (!ssl_sock || !ssl_sock->base_socket) {
    return -1;
  }
  return ssl_sock->base_socket->remote_port;
}
