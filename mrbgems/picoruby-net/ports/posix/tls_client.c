/*
 * TLS Client for PicoRuby Net (POSIX Implementation)
 * Uses mbedTLS for TLS/SSL support over POSIX sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

#include "net.h"

#define TLS_RECV_BUFFER_SIZE 8192

/*
 * TLS context structure for managing TLS connection
 */
typedef struct {
  mbedtls_net_context server_fd;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
} tls_context_t;

/*
 * Initialize TLS context
 */
static int
tls_init(tls_context_t *tls_ctx, const char *hostname)
{
  int ret;
  const char *pers = "picoruby_tls_client";

  /* Initialize structures */
  mbedtls_net_init(&tls_ctx->server_fd);
  mbedtls_ssl_init(&tls_ctx->ssl);
  mbedtls_ssl_config_init(&tls_ctx->conf);
  mbedtls_entropy_init(&tls_ctx->entropy);
  mbedtls_ctr_drbg_init(&tls_ctx->ctr_drbg);

  /* Seed the RNG */
  ret = mbedtls_ctr_drbg_seed(&tls_ctx->ctr_drbg, mbedtls_entropy_func,
                               &tls_ctx->entropy,
                               (const unsigned char *)pers, strlen(pers));
  if (ret != 0) {
    fprintf(stderr, "mbedtls_ctr_drbg_seed failed: -0x%04x\n", -ret);
    return ret;
  }

  /* Setup SSL/TLS config */
  ret = mbedtls_ssl_config_defaults(&tls_ctx->conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    fprintf(stderr, "mbedtls_ssl_config_defaults failed: -0x%04x\n", -ret);
    return ret;
  }

  /* Configure SSL/TLS settings */
  mbedtls_ssl_conf_authmode(&tls_ctx->conf, MBEDTLS_SSL_VERIFY_NONE);  /* Skip cert verification for now */
  mbedtls_ssl_conf_rng(&tls_ctx->conf, mbedtls_ctr_drbg_random, &tls_ctx->ctr_drbg);

  /* Setup SSL context */
  ret = mbedtls_ssl_setup(&tls_ctx->ssl, &tls_ctx->conf);
  if (ret != 0) {
    fprintf(stderr, "mbedtls_ssl_setup failed: -0x%04x\n", -ret);
    return ret;
  }

  /* Set hostname for SNI (Server Name Indication) */
  ret = mbedtls_ssl_set_hostname(&tls_ctx->ssl, hostname);
  if (ret != 0) {
    fprintf(stderr, "mbedtls_ssl_set_hostname failed: -0x%04x\n", -ret);
    return ret;
  }

  return 0;
}

/*
 * Free TLS context
 */
static void
tls_cleanup(tls_context_t *tls_ctx)
{
  mbedtls_net_free(&tls_ctx->server_fd);
  mbedtls_ssl_free(&tls_ctx->ssl);
  mbedtls_ssl_config_free(&tls_ctx->conf);
  mbedtls_ctr_drbg_free(&tls_ctx->ctr_drbg);
  mbedtls_entropy_free(&tls_ctx->entropy);
}

/*
 * Send TLS request and receive response
 *
 * @param mrb   mruby state (unused in POSIX implementation)
 * @param req   Request parameters
 * @param res   Response structure
 * @return      true on success, false on error
 */
bool
TLSClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  tls_context_t tls_ctx;
  int ret;
  char port_str[16];
  char *recv_buffer = NULL;
  size_t recv_buffer_size = TLS_RECV_BUFFER_SIZE;
  size_t total_received = 0;
  bool success = false;

  (void)mrb;  /* Unused */

  /* Initialize response */
  memset(res, 0, sizeof(*res));
  memset(&tls_ctx, 0, sizeof(tls_ctx));

  /* Validate request */
  if (!req || !req->host || req->port <= 0) {
    snprintf(res->error_message, sizeof(res->error_message), "Invalid request parameters");
    return false;
  }

  /* Initialize TLS context */
  ret = tls_init(&tls_ctx, req->host);
  if (ret != 0) {
    snprintf(res->error_message, sizeof(res->error_message), "TLS initialization failed: -0x%04x", -ret);
    goto cleanup;
  }

  /* Connect to server */
  snprintf(port_str, sizeof(port_str), "%d", req->port);
  ret = mbedtls_net_connect(&tls_ctx.server_fd, req->host, port_str, MBEDTLS_NET_PROTO_TCP);
  if (ret != 0) {
    snprintf(res->error_message, sizeof(res->error_message), "Connection failed: -0x%04x", -ret);
    fprintf(stderr, "mbedtls_net_connect to %s:%d failed: -0x%04x\n", req->host, req->port, -ret);
    goto cleanup;
  }

  /* Set BIO callbacks */
  mbedtls_ssl_set_bio(&tls_ctx.ssl, &tls_ctx.server_fd,
                      mbedtls_net_send, mbedtls_net_recv, NULL);

  /* Perform TLS handshake */
  fprintf(stderr, "Performing TLS handshake with %s...\n", req->host);
  while ((ret = mbedtls_ssl_handshake(&tls_ctx.ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      char error_buf[100];
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      snprintf(res->error_message, sizeof(res->error_message), "TLS handshake failed: -0x%04x (%s)", -ret, error_buf);
      fprintf(stderr, "mbedtls_ssl_handshake failed: -0x%04x - %s\n", -ret, error_buf);
      goto cleanup;
    }
  }
  fprintf(stderr, "TLS handshake successful\n");

  /* Send request data */
  if (req->send_data && req->send_data_len > 0) {
    size_t total_written = 0;
    while (total_written < req->send_data_len) {
      ret = mbedtls_ssl_write(&tls_ctx.ssl,
                               (const unsigned char *)req->send_data + total_written,
                               req->send_data_len - total_written);
      if (ret < 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
          snprintf(res->error_message, sizeof(res->error_message), "TLS write failed: -0x%04x", -ret);
          goto cleanup;
        }
      } else {
        total_written += ret;
      }
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
    if (total_received + TLS_RECV_BUFFER_SIZE > recv_buffer_size) {
      char *new_buffer = (char *)realloc(recv_buffer, recv_buffer_size * 2);
      if (!new_buffer) {
        snprintf(res->error_message, sizeof(res->error_message), "Memory reallocation failed");
        goto cleanup;
      }
      recv_buffer = new_buffer;
      recv_buffer_size *= 2;
    }

    /* Read data */
    ret = mbedtls_ssl_read(&tls_ctx.ssl,
                            (unsigned char *)recv_buffer + total_received,
                            TLS_RECV_BUFFER_SIZE);

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      continue;
    } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
      /* Connection closed */
      break;
    } else if (ret < 0) {
      snprintf(res->error_message, sizeof(res->error_message), "TLS read failed: -0x%04x", -ret);
      goto cleanup;
    }

    total_received += ret;
  }

  /* Success */
  res->recv_data = recv_buffer;
  res->recv_data_len = total_received;
  success = true;
  recv_buffer = NULL;  /* Ownership transferred */

cleanup:
  if (recv_buffer) {
    free(recv_buffer);
  }

  /* Close TLS connection gracefully */
  mbedtls_ssl_close_notify(&tls_ctx.ssl);

  /* Free TLS context */
  tls_cleanup(&tls_ctx);

  return success;
}
