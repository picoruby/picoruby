/*
 * TCP Server implementation for rp2040 using LwIP
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include "picoruby/debug.h"
#include <string.h>

/* LwIP includes */
#define PICORB_NO_LWIP_HELPERS
#include "lwip/altcp.h"

/* TCP Server structure */
struct picorb_tcp_server {
  struct altcp_pcb *listen_pcb;
  struct altcp_pcb *accepted_pcb;
  int port;
  int state;
};

/* Forward declarations for TCP socket callbacks */
static err_t tcp_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err);
static err_t tcp_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len);
static void tcp_err_callback(void *arg, err_t err);

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
    sock->connected = false;
    return err;
  }

  /* NULL pbuf means connection closed */
  if (!pbuf) {
    sock->state = SOCKET_STATE_CLOSED;
    sock->connected = false;
    sock->closed = true;
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
  sock->connected = false;
  sock->pcb = NULL; /* PCB is already freed by LwIP */
}

/* Accept callback */
static err_t
tcp_accept_callback(void *arg, struct altcp_pcb *newpcb, err_t err)
{
  picorb_tcp_server_t *server = (picorb_tcp_server_t *)arg;
  if (!server || err != ERR_OK) {
    if (newpcb) altcp_abort(newpcb);
    return ERR_ABRT;
  }

  /* Store accepted connection */
  server->accepted_pcb = newpcb;
  server->state = 1; /* has connection */

  return ERR_OK;
}

/* Create TCP server */
picorb_tcp_server_t*
TCPServer_create(int port, int backlog)
{
  if (port <= 0 || port > 65535) {
    return NULL;
  }

  picorb_tcp_server_t *server = (picorb_tcp_server_t *)picorb_alloc(NULL, sizeof(picorb_tcp_server_t));
  if (!server) {
    return NULL;
  }

  memset(server, 0, sizeof(picorb_tcp_server_t));
  server->port = port;

  lwip_begin();
  server->listen_pcb = altcp_new(NULL);
  if (!server->listen_pcb) {
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  err_t err = altcp_bind(server->listen_pcb, IP_ADDR_ANY, port);
  if (err != ERR_OK) {
    altcp_close(server->listen_pcb);
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  server->listen_pcb = altcp_listen_with_backlog(server->listen_pcb, backlog);
  if (!server->listen_pcb) {
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  altcp_arg(server->listen_pcb, server);
  altcp_accept(server->listen_pcb, tcp_accept_callback);
  lwip_end();

  return server;
}

/* Accept connection (non-blocking) */
picorb_socket_t*
TCPServer_accept_nonblock(picorb_tcp_server_t *server)
{
  if (!server) {
    return NULL;
  }

  /* Check for connection immediately without blocking */
  if (!server->accepted_pcb) {
    return NULL; /* No pending connection */
  }

  /* Create socket for accepted connection */
  picorb_socket_t *sock = (picorb_socket_t *)picorb_alloc(NULL, sizeof(picorb_socket_t));
  if (!sock) {
    lwip_begin();
    altcp_abort(server->accepted_pcb);
    lwip_end();
    server->accepted_pcb = NULL;
    return NULL;
  }

  memset(sock, 0, sizeof(picorb_socket_t));
  sock->pcb = server->accepted_pcb;
  sock->state = SOCKET_STATE_CONNECTED;
  sock->socktype = 1; /* SOCK_STREAM */
  sock->recv_buf = NULL;
  sock->recv_len = 0;
  sock->recv_capacity = 0;
  sock->connected = true;
  sock->closed = false;

  /* Setup callbacks for the accepted connection */
  lwip_begin();
  altcp_arg(sock->pcb, sock);
  altcp_recv(sock->pcb, tcp_recv_callback);
  altcp_sent(sock->pcb, tcp_sent_callback);
  altcp_err(sock->pcb, tcp_err_callback);
  lwip_end();

  D("TCPServer_accept_nonblock: created socket %p, state=%d, pcb=%p\n",
    (void*)sock, sock->state, (void*)sock->pcb);

  server->accepted_pcb = NULL;
  server->state = 0;

  return sock;
}

/* Close server */
bool
TCPServer_close(picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }

  if (server->listen_pcb) {
    lwip_begin();
    altcp_close(server->listen_pcb);
    lwip_end();
  }

  picorb_free(NULL, server);
  return true;
}

/* Get server port */
int
TCPServer_port(picorb_tcp_server_t *server)
{
  if (!server) {
    return -1;
  }
  return server->port;
}

/* Check if server is listening */
bool
TCPServer_listening(picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }
  return server->listen_pcb != NULL;
}
