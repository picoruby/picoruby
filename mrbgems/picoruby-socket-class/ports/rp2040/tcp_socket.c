/*
 * TCP Socket implementation for rp2040 using LwIP
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include <string.h>
#include <stdio.h>

/* LwIP includes */
#define PICORB_NO_LWIP_HELPERS
#include "lwip/altcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"

/* Callback prototypes */
static err_t tcp_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err);
static err_t tcp_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len);
static err_t tcp_connected_callback(void *arg, struct altcp_pcb *pcb, err_t err);
static void tcp_err_callback(void *arg, err_t err);

/* Create a new TCP socket */
bool
TCPSocket_create(picorb_socket_t *sock)
{
  if (!sock) return false;

  memset(sock, 0, sizeof(picorb_socket_t));

  lwip_begin();
  sock->pcb = altcp_new(NULL);
  lwip_end();

  if (!sock->pcb) {
    return false;
  }

  sock->state = SOCKET_STATE_NONE;
  sock->recv_buf = NULL;
  sock->recv_len = 0;
  sock->recv_capacity = 0;
  sock->socktype = 1; /* SOCK_STREAM equivalent */
  sock->remote_host[0] = '\0';
  sock->remote_port = 0;

  return true;
}

/* Connected callback - called when connection is established */
static err_t
tcp_connected_callback(void *arg, struct altcp_pcb *pcb, err_t err)
{
  picorb_socket_t *sock = (picorb_socket_t *)arg;
  if (!sock) return ERR_ARG;

  if (err != ERR_OK) {
    sock->state = SOCKET_STATE_ERROR;
    return err;
  }

  sock->state = SOCKET_STATE_CONNECTED;
  return ERR_OK;
}

/* Receive callback - called when data is received */
static err_t
tcp_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  picorb_socket_t *sock = (picorb_socket_t *)arg;
  if (!sock) return ERR_ARG;

  /* Handle errors */
  if (err != ERR_OK) {
    if (pbuf) pbuf_free(pbuf);
    sock->state = SOCKET_STATE_ERROR;
    return err;
  }

  /* NULL pbuf means connection closed */
  if (!pbuf) {
    sock->state = SOCKET_STATE_CLOSED;
    return ERR_OK;
  }

  /* Allocate or expand receive buffer */
  size_t total_len = pbuf->tot_len;
  size_t new_size = sock->recv_len + total_len;

  if (new_size > sock->recv_capacity) {
    char *new_buf = (char *)picorb_realloc(NULL, sock->recv_buf, new_size + 1);
    if (!new_buf) {
      pbuf_free(pbuf);
      sock->state = SOCKET_STATE_ERROR;
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

/* Sent callback - called when data is successfully sent */
static err_t
tcp_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len)
{
  /* Nothing special to do */
  return ERR_OK;
}

/* Error callback - called on connection error */
static void
tcp_err_callback(void *arg, err_t err)
{
  picorb_socket_t *sock = (picorb_socket_t *)arg;
  if (!sock) return;

  sock->state = SOCKET_STATE_ERROR;
  sock->pcb = NULL; /* PCB is already freed by LwIP */
}

/* Connect to remote host */
bool
TCPSocket_connect(picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || !host || port <= 0 || port > 65535) {
    return false;
  }

  /* Create socket if not already created */
  if (!sock->pcb) {
    if (!TCPSocket_create(sock)) {
      return false;
    }
  }

  /* Resolve hostname to IP address */
  ip_addr_t ip_addr;
  int dns_result = Net_get_ip(host, &ip_addr);
  if (dns_result != 0) {
    return false;
  }

  /* Setup callbacks */
  lwip_begin();
  altcp_arg(sock->pcb, sock);
  altcp_recv(sock->pcb, tcp_recv_callback);
  altcp_sent(sock->pcb, tcp_sent_callback);
  altcp_err(sock->pcb, tcp_err_callback);

  /* Initiate connection */
  err_t err = altcp_connect(sock->pcb, &ip_addr, port, tcp_connected_callback);
  lwip_end();

  if (err != ERR_OK) {
    return false;
  }

  /* Save connection info */
  strncpy(sock->remote_host, host, sizeof(sock->remote_host) - 1);
  sock->remote_host[sizeof(sock->remote_host) - 1] = '\0';
  sock->remote_port = port;
  sock->state = SOCKET_STATE_CONNECTING;

  /* Wait for connection to establish */
  int max_wait = 100; /* 10 seconds */
  while (sock->state == SOCKET_STATE_CONNECTING && max_wait-- > 0) {
    Net_sleep_ms(100);
  }

  return sock->state == SOCKET_STATE_CONNECTED;
}

/* Send data */
ssize_t
TCPSocket_send(picorb_socket_t *sock, const void *data, size_t len)
{
  if (!sock || !data || !sock->pcb || sock->state != SOCKET_STATE_CONNECTED) {
    return -1;
  }

  lwip_begin();
  err_t err = altcp_write(sock->pcb, data, len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    lwip_end();
    return -1;
  }

  err = altcp_output(sock->pcb);
  lwip_end();

  if (err != ERR_OK) {
    return -1;
  }

  return (ssize_t)len;
}

/* Receive data */
ssize_t
TCPSocket_recv(picorb_socket_t *sock, void *buf, size_t len)
{
  if (!sock || !buf || sock->state == SOCKET_STATE_ERROR) {
    return -1;
  }

  /* Wait for data with timeout */
  int max_wait = 600; /* 60 seconds */
  while (sock->recv_len == 0 &&
         sock->state == SOCKET_STATE_CONNECTED &&
         max_wait-- > 0) {
    Net_sleep_ms(100);
  }

  /* Check if connection was closed */
  if (sock->recv_len == 0 && sock->state == SOCKET_STATE_CLOSED) {
    return 0; /* EOF */
  }

  /* Check for timeout or error */
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

/* Close socket */
bool
TCPSocket_close(picorb_socket_t *sock)
{
  if (!sock) {
    return false;
  }

  if (sock->pcb) {
    lwip_begin();
    altcp_arg(sock->pcb, NULL);
    altcp_recv(sock->pcb, NULL);
    altcp_sent(sock->pcb, NULL);
    altcp_err(sock->pcb, NULL);

    err_t err = altcp_close(sock->pcb);
    if (err != ERR_OK) {
      altcp_abort(sock->pcb);
    }
    lwip_end();

    sock->pcb = NULL;
  }

  if (sock->recv_buf) {
    picorb_free(NULL, sock->recv_buf);
    sock->recv_buf = NULL;
  }

  sock->state = SOCKET_STATE_CLOSED;
  sock->recv_len = 0;
  sock->recv_capacity = 0;

  return true;
}

/* Get remote host */
const char*
TCPSocket_remote_host(picorb_socket_t *sock)
{
  if (!sock) return NULL;
  return sock->remote_host;
}

/* Get remote port */
int
TCPSocket_remote_port(picorb_socket_t *sock)
{
  if (!sock) return -1;
  return sock->remote_port;
}

/* Check if socket is closed */
bool
TCPSocket_closed(picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->state == SOCKET_STATE_CLOSED ||
         sock->state == SOCKET_STATE_ERROR ||
         !sock->pcb;
}
