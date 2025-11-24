/*
 * SSL Socket implementation for rp2040 using LwIP altcp_tls
 * Similar approach to picoruby-net for stability
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include "picoruby/debug.h"
#include <string.h>

/* LwIP altcp_tls includes */
#define PICORB_NO_LWIP_HELPERS
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "mbedtls/ssl.h"

/* SSL connection states */
#define SSL_STATE_NONE           0
#define SSL_STATE_CONNECTING     1
#define SSL_STATE_CONNECTED      2
#define SSL_STATE_ERROR          3

/* SSL Context structure */
struct picorb_ssl_context {
  int verify_mode;
  struct altcp_tls_config *tls_config;
  const unsigned char *ca_cert_data;
  size_t ca_cert_len;
};

/* SSL socket structure */
struct picorb_ssl_socket {
  picorb_socket_t *base_socket;   /* For buffer compatibility */
  picorb_ssl_context_t *ssl_ctx;
  struct altcp_pcb *tls_pcb;
  int state;
  bool connected;
  char *hostname;
  int port;
};

/* Forward declarations */
static err_t ssl_connected_callback(void *arg, struct altcp_pcb *pcb, err_t err);
static err_t ssl_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err);
static err_t ssl_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len);
static void ssl_err_callback(void *arg, err_t err);
static err_t ssl_poll_callback(void *arg, struct altcp_pcb *pcb);

/* ========================================================================
 * Callback Functions
 * ======================================================================== */

static err_t
ssl_connected_callback(void *arg, struct altcp_pcb *pcb, err_t err)
{
  D("SSL callback: connected\n");
  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)arg;
  if (!ssl_sock) {
    D("SSL callback: no sock\n");
    return ERR_ARG;
  }

  if (err != ERR_OK) {
    D("SSL callback: error\n");
    ssl_sock->state = SSL_STATE_ERROR;
    ssl_sock->connected = false;
    return err;
  }

  D("SSL callback: success\n");
  ssl_sock->state = SSL_STATE_CONNECTED;
  ssl_sock->connected = true;
  return ERR_OK;
}

static err_t
ssl_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)arg;
  if (!ssl_sock || !ssl_sock->base_socket) return ERR_ARG;

  /* Handle errors */
  if (err != ERR_OK) {
    if (pbuf) pbuf_free(pbuf);
    ssl_sock->state = SSL_STATE_ERROR;
    ssl_sock->connected = false;
    return err;
  }

  /* NULL pbuf means connection closed */
  if (!pbuf) {
    ssl_sock->state = SSL_STATE_NONE;
    ssl_sock->connected = false;
    return ERR_OK;
  }

  /* Allocate or expand receive buffer in base_socket */
  picorb_socket_t *sock = ssl_sock->base_socket;
  size_t total_len = pbuf->tot_len;
  size_t new_size = sock->recv_len + total_len;

  if (new_size > sock->recv_capacity) {
    char *new_buf = (char *)picorb_realloc(NULL, sock->recv_buf, new_size + 1);
    if (!new_buf) {
      pbuf_free(pbuf);
      ssl_sock->state = SSL_STATE_ERROR;
      return ERR_MEM;
    }
    sock->recv_buf = new_buf;
    sock->recv_capacity = new_size;
  }

  /* Copy data from pbuf chain */
  struct pbuf *current = pbuf;
  size_t offset = sock->recv_len;
  while (current) {
    memcpy(sock->recv_buf + offset, current->payload, current->len);
    offset += current->len;
    current = current->next;
  }
  sock->recv_len = offset;
  sock->recv_buf[sock->recv_len] = '\0';

  /* Tell LwIP we processed the data */
  altcp_recved(pcb, total_len);
  pbuf_free(pbuf);

  return ERR_OK;
}

static err_t
ssl_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len)
{
  return ERR_OK;
}

static void
ssl_err_callback(void *arg, err_t err)
{
  D("SSL callback: error code=");
  debug_printf("%d\n", (int)err);
  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)arg;
  if (!ssl_sock) return;

  ssl_sock->state = SSL_STATE_ERROR;
  ssl_sock->connected = false;
  ssl_sock->tls_pcb = NULL;  /* PCB is already freed by LwIP */
}

static err_t
ssl_poll_callback(void *arg, struct altcp_pcb *pcb)
{
  return ERR_OK;
}

/* ========================================================================
 * SSLContext Functions
 * ======================================================================== */

picorb_ssl_context_t*
SSLContext_create(void)
{
  picorb_ssl_context_t *ctx = (picorb_ssl_context_t *)picorb_alloc(NULL, sizeof(picorb_ssl_context_t));
  if (!ctx) {
    return NULL;
  }

  memset(ctx, 0, sizeof(picorb_ssl_context_t));
  ctx->verify_mode = SSL_VERIFY_PEER;
  ctx->tls_config = NULL;
  ctx->ca_cert_data = NULL;
  ctx->ca_cert_len = 0;

  return ctx;
}

bool
SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file)
{
  (void)ctx;
  (void)ca_file;
  return false;  /* Not supported on rp2040 */
}

bool
SSLContext_set_ca_cert(picorb_ssl_context_t *ctx, const void *addr, size_t size)
{
  if (!ctx || !addr || size == 0) {
    return false;
  }

  ctx->ca_cert_data = (const unsigned char *)addr;
  ctx->ca_cert_len = size;

  return true;
}

bool
SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode)
{
  if (!ctx) {
    return false;
  }

  ctx->verify_mode = mode;
  return true;
}

int
SSLContext_get_verify_mode(picorb_ssl_context_t *ctx)
{
  if (!ctx) {
    return -1;
  }
  return ctx->verify_mode;
}

void
SSLContext_free(picorb_ssl_context_t *ctx)
{
  if (!ctx) {
    return;
  }

  if (ctx->tls_config) {
    altcp_tls_free_config(ctx->tls_config);
    ctx->tls_config = NULL;
  }

  picorb_free(NULL, ctx);
}

/* ========================================================================
 * SSLSocket Functions
 * ======================================================================== */

/*
 * Create SSL socket
 */
picorb_ssl_socket_t*
SSLSocket_create(picorb_ssl_context_t *ssl_ctx)
{
  if (!ssl_ctx) {
    return NULL;
  }

  picorb_ssl_socket_t *ssl_sock = (picorb_ssl_socket_t *)picorb_alloc(NULL, sizeof(picorb_ssl_socket_t));
  if (!ssl_sock) {
    return NULL;
  }

  memset(ssl_sock, 0, sizeof(picorb_ssl_socket_t));

  /* Create a dummy base_socket for buffer management */
  ssl_sock->base_socket = (picorb_socket_t *)picorb_alloc(NULL, sizeof(picorb_socket_t));
  if (!ssl_sock->base_socket) {
    picorb_free(NULL, ssl_sock);
    return NULL;
  }
  memset(ssl_sock->base_socket, 0, sizeof(picorb_socket_t));

  ssl_sock->ssl_ctx = ssl_ctx;
  ssl_sock->tls_pcb = NULL;
  ssl_sock->state = SSL_STATE_NONE;
  ssl_sock->connected = false;
  ssl_sock->hostname = NULL;
  ssl_sock->port = 0;

  return ssl_sock;
}

bool
SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname)
{
  if (!ssl_sock || !hostname) {
    return false;
  }

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
 * Performs DNS resolution, creates TLS PCB, and initiates connection
 */
bool
SSLSocket_connect(picorb_ssl_socket_t *ssl_sock)
{
  D("SSL connect start\n");

  if (!ssl_sock || ssl_sock->connected || !ssl_sock->hostname) {
    D("SSL: bad params\n");
    return false;
  }

  /* Get port from base_socket if set, otherwise use 443 */
  if (ssl_sock->port == 0) {
    ssl_sock->port = 443;  /* Default HTTPS port */
  }

  D("SSL: resolving DNS\n");
  /* Resolve hostname to IP */
  ip_addr_t ip_addr;
  ip4_addr_set_zero(&ip_addr);
  int dns_result = Net_get_ip(ssl_sock->hostname, &ip_addr);
  if (dns_result != 0) {
    D("SSL: DNS failed\n");
    return false;
  }
  D("SSL: DNS ok, IP=");
  debug_printf("%u.%u.%u.%u\n",
    (unsigned int)ip4_addr1(&ip_addr),
    (unsigned int)ip4_addr2(&ip_addr),
    (unsigned int)ip4_addr3(&ip_addr),
    (unsigned int)ip4_addr4(&ip_addr));

  /* Create TLS config (always create new one, like picoruby-net) */
  D("SSL: creating TLS config\n");
  struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
  if (!tls_config) {
    D("SSL: TLS config failed\n");
    return false;
  }
  D("SSL: TLS config ok\n");

  /* Create TLS PCB */
  D("SSL: creating TLS PCB\n");
  ssl_sock->tls_pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
  if (!ssl_sock->tls_pcb) {
    D("SSL: TLS PCB failed\n");
    altcp_tls_free_config(tls_config);
    return false;
  }
  D("SSL: TLS PCB ok\n");

  /* Store config for cleanup */
  if (ssl_sock->ssl_ctx->tls_config) {
    altcp_tls_free_config(ssl_sock->ssl_ctx->tls_config);
  }
  ssl_sock->ssl_ctx->tls_config = tls_config;

  /* Set hostname for SNI */
  D("SSL: setting hostname\n");
  mbedtls_ssl_context *ssl_ctx = altcp_tls_context(ssl_sock->tls_pcb);
  if (!ssl_ctx) {
    D("SSL: altcp_tls_context failed\n");
    altcp_tls_free_config(tls_config);
    return false;
  }
  mbedtls_ssl_set_hostname(ssl_ctx, ssl_sock->hostname);

  /* Setup callbacks (same order as picoruby-net, with altcp_arg last) */
  D("SSL: setting callbacks\n");
  altcp_recv(ssl_sock->tls_pcb, ssl_recv_callback);
  altcp_sent(ssl_sock->tls_pcb, ssl_sent_callback);
  altcp_err(ssl_sock->tls_pcb, ssl_err_callback);
  altcp_poll(ssl_sock->tls_pcb, ssl_poll_callback, 10);
  altcp_arg(ssl_sock->tls_pcb, ssl_sock);

  /* Small delay before connecting (like picoruby-net's function boundary) */
  D("SSL: waiting before connect\n");
  Net_sleep_ms(100);

  /* Initiate connection */
  D("SSL: initiating connection to port ");
  debug_printf("%d\n", ssl_sock->port);
  lwip_begin();
  err_t err = altcp_connect(ssl_sock->tls_pcb, &ip_addr, ssl_sock->port, ssl_connected_callback);
  if (err != ERR_OK) {
    D("SSL: altcp_connect failed\n");
    ssl_sock->state = SSL_STATE_ERROR;
    lwip_end();
    return false;
  }
  lwip_end();
  D("SSL: altcp_connect ok\n");

  ssl_sock->state = SSL_STATE_CONNECTING;

  /* Wait for connection to establish */
  D("SSL: waiting for connection\n");
  int max_wait = 1000;  /* 10 seconds (10ms * 1000) */
  while (ssl_sock->state == SSL_STATE_CONNECTING && max_wait-- > 0) {
    Net_sleep_ms(10);
  }

  if (ssl_sock->state == SSL_STATE_CONNECTED) {
    D("SSL: connected\n");
    return true;
  } else {
    D("SSL: timeout or error, state=");
    debug_printf("%d\n", ssl_sock->state);
    /* Cleanup on timeout */
    if (ssl_sock->tls_pcb) {
      lwip_begin();
      altcp_abort(ssl_sock->tls_pcb);
      lwip_end();
      ssl_sock->tls_pcb = NULL;
    }
    ssl_sock->state = SSL_STATE_ERROR;
    return false;
  }
}

ssize_t
SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len)
{
  if (!ssl_sock || !ssl_sock->connected || !ssl_sock->tls_pcb || !data) {
    return -1;
  }

  lwip_begin();
  err_t err = altcp_write(ssl_sock->tls_pcb, data, len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    lwip_end();
    return -1;
  }

  err = altcp_output(ssl_sock->tls_pcb);
  lwip_end();

  if (err != ERR_OK) {
    return -1;
  }

  return (ssize_t)len;
}

ssize_t
SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len)
{
  if (!ssl_sock || !ssl_sock->base_socket || !buf) {
    return -1;
  }

  picorb_socket_t *sock = ssl_sock->base_socket;

  /* Wait for data with timeout */
  int max_wait = 600;  /* 60 seconds */
  while (sock->recv_len == 0 && ssl_sock->connected && max_wait-- > 0) {
    Net_sleep_ms(100);
  }

  /* Check if connection was closed */
  if (sock->recv_len == 0 && !ssl_sock->connected) {
    return 0;  /* EOF */
  }

  /* Check for timeout */
  if (sock->recv_len == 0) {
    return 0;
  }

  /* Copy available data */
  size_t to_copy = (len < sock->recv_len) ? len : sock->recv_len;
  memcpy(buf, sock->recv_buf, to_copy);

  /* Remove copied data from buffer */
  if (to_copy < sock->recv_len) {
    memmove(sock->recv_buf, sock->recv_buf + to_copy, sock->recv_len - to_copy);
    sock->recv_len -= to_copy;
  } else {
    sock->recv_len = 0;
  }

  return (ssize_t)to_copy;
}

bool
SSLSocket_close(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return false;
  }

  if (ssl_sock->tls_pcb) {
    lwip_begin();
    altcp_arg(ssl_sock->tls_pcb, NULL);
    altcp_recv(ssl_sock->tls_pcb, NULL);
    altcp_sent(ssl_sock->tls_pcb, NULL);
    altcp_err(ssl_sock->tls_pcb, NULL);

    err_t err = altcp_close(ssl_sock->tls_pcb);
    if (err != ERR_OK) {
      altcp_abort(ssl_sock->tls_pcb);
    }
    lwip_end();

    ssl_sock->tls_pcb = NULL;
  }

  if (ssl_sock->hostname) {
    picorb_free(NULL, ssl_sock->hostname);
    ssl_sock->hostname = NULL;
  }

  if (ssl_sock->base_socket) {
    if (ssl_sock->base_socket->recv_buf) {
      picorb_free(NULL, ssl_sock->base_socket->recv_buf);
    }
    picorb_free(NULL, ssl_sock->base_socket);
    ssl_sock->base_socket = NULL;
  }

  picorb_free(NULL, ssl_sock);
  return true;
}

bool
SSLSocket_closed(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return true;
  }
  return !ssl_sock->connected;
}

const char*
SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return NULL;
  }
  return ssl_sock->hostname;
}

int
SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock)
{
  if (!ssl_sock) {
    return -1;
  }
  return ssl_sock->port;
}
