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
#include "lwip/altcp_tcp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"

/* CYW43 includes for polling */
#include "pico/cyw43_arch.h"

/* Pre-allocated receive buffer size for accepted TCP connections. */
#ifndef TCP_SERVER_RECV_BUF_SIZE
#define TCP_SERVER_RECV_BUF_SIZE 4096
#endif

/* TCP Server structure */
struct picorb_tcp_server {
  struct altcp_pcb *listen_pcb;
  picorb_socket_t *accepted_socket;
  picorb_socket_t *pending_socket; /* pre-allocated, ready for next accept */
  picorb_state *vm;
  int port;
  int state;
};

/* Forward declarations for TCP socket callbacks */
static err_t tcp_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err);
static err_t tcp_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len);
static void tcp_err_callback(void *arg, err_t err);

/* Receive callback - runs in LwIP callback context (may be IRQ/PendSV).
 * Must NOT call heap allocator to avoid heap corruption. */
static err_t
tcp_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  picorb_socket_t *sock = (picorb_socket_t *)arg;
  D("tcp_server.c tcp_recv_callback: sock=%p, pbuf=%p, err=%d\n", (void*)sock, (void*)pbuf, err);

  if (!sock) {
    D("tcp_server.c tcp_recv_callback: sock is NULL");
    return ERR_ARG;
  }

  /* Handle errors */
  if (err != ERR_OK) {
    if (pbuf) pbuf_free(pbuf);
    sock->state = SOCKET_STATE_ERROR;
    sock->connected = false;
    D("tcp_server.c tcp_recv_callback: error, state set to ERROR");
    return err;
  }

  /* NULL pbuf means connection closed */
  if (!pbuf) {
    sock->state = SOCKET_STATE_CLOSED;
    sock->connected = false;
    sock->closed = true;
    D("tcp_server.c tcp_recv_callback: connection closed");
    return ERR_OK;
  }

  /* Copy data into pre-allocated buffer (no heap allocation) */
  size_t total_len = pbuf->tot_len;
  size_t new_size = sock->recv_len + total_len;
  D("tcp_server.c tcp_recv_callback: receiving %zu bytes, current recv_len=%zu\n", total_len, sock->recv_len);

  if (new_size > sock->recv_capacity) {
    /* Buffer full - cannot accept more data */
    D("tcp_server.c tcp_recv_callback: buffer full, dropping data");
    pbuf_free(pbuf);
    return ERR_MEM;
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

  D("tcp_server.c tcp_recv_callback: success, total recv_len=%zu\n", sock->recv_len);
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

/* Accept callback - runs in LwIP callback context (may be IRQ/PendSV).
 * Uses pre-allocated socket to avoid heap allocation in callback context. */
static err_t
tcp_accept_callback(void *arg, struct altcp_pcb *newpcb, err_t err)
{
  picorb_tcp_server_t *server = (picorb_tcp_server_t *)arg;
  if (!server || err != ERR_OK) {
    if (newpcb) altcp_abort(newpcb);
    return ERR_ABRT;
  }

  /* Use pre-allocated socket to avoid heap allocation in callback context. */
  picorb_socket_t *sock = server->pending_socket;
  if (!sock) {
    altcp_abort(newpcb);
    return ERR_MEM;
  }
  server->pending_socket = NULL;

  /* Save recv_buf across memset, then reinitialize socket fields. */
  char *recv_buf = sock->recv_buf;
  size_t recv_capacity = sock->recv_capacity;
  memset(sock, 0, sizeof(picorb_socket_t));
  sock->recv_buf = recv_buf;
  sock->recv_capacity = recv_capacity;

  sock->pcb = newpcb;
  sock->state = SOCKET_STATE_CONNECTED;
  sock->socktype = 1; /* SOCK_STREAM */
  sock->recv_len = 0;
  sock->connected = true;
  sock->closed = false;

  /* Store in server */
  server->accepted_socket = sock;
  server->state = 1; /* has connection */

  /* Set up callbacks with correct arg */
  altcp_arg(newpcb, sock);
  altcp_recv(newpcb, tcp_recv_callback);
  altcp_sent(newpcb, tcp_sent_callback);
  altcp_err(newpcb, tcp_err_callback);

  return ERR_OK;
}

/* Create TCP server */
picorb_tcp_server_t*
TCPServer_create(picorb_state *vm, int port, int backlog)
{
  if (port <= 0 || port > 65535) {
    return NULL;
  }

  picorb_tcp_server_t *server = (picorb_tcp_server_t *)picorb_alloc(vm, sizeof(picorb_tcp_server_t));
  if (!server) {
    return NULL;
  }

  memset(server, 0, sizeof(picorb_tcp_server_t));
  server->vm = vm;
  server->port = port;

  /* Pre-allocate socket for first accepted connection. */
  server->pending_socket = (picorb_socket_t *)picorb_alloc(vm, sizeof(picorb_socket_t));
  if (!server->pending_socket) {
    picorb_free(vm, server);
    return NULL;
  }
  memset(server->pending_socket, 0, sizeof(picorb_socket_t));
  server->pending_socket->recv_buf = (char *)picorb_alloc(vm, TCP_SERVER_RECV_BUF_SIZE + 1);
  if (!server->pending_socket->recv_buf) {
    picorb_free(vm, server->pending_socket);
    picorb_free(vm, server);
    return NULL;
  }
  server->pending_socket->recv_capacity = TCP_SERVER_RECV_BUF_SIZE;

  lwip_begin();

  /* Create tcp_pcb directly to set SO_REUSEADDR */
  struct tcp_pcb *tpcb = tcp_new();
  if (!tpcb) {
    D("TCPServer_create: tcp_new failed");
    lwip_end();
    picorb_free(vm, server->pending_socket->recv_buf);
    picorb_free(vm, server->pending_socket);
    picorb_free(vm, server);
    return NULL;
  }

  /* Set SO_REUSEADDR to allow port reuse after TIME_WAIT */
  ip_set_option(tpcb, SOF_REUSEADDR);

  /* Wrap tcp_pcb in altcp_pcb */
  server->listen_pcb = altcp_tcp_wrap(tpcb);
  if (!server->listen_pcb) {
    D("TCPServer_create: altcp_tcp_wrap failed");
    tcp_close(tpcb);
    lwip_end();
    picorb_free(vm, server->pending_socket->recv_buf);
    picorb_free(vm, server->pending_socket);
    picorb_free(vm, server);
    return NULL;
  }

  /* Bind with retry for TIME_WAIT state */
  err_t err;
  const int max_retries = 5;
  for (int i = 0; i <= max_retries; i++) {
    err = altcp_bind(server->listen_pcb, IP_ADDR_ANY, port);
    if (err == ERR_OK) {
      break;
    }
    if (err == ERR_USE && i < max_retries) {
      lwip_end();
      Net_busy_wait_ms(100);
      lwip_begin();
    } else {
      altcp_close(server->listen_pcb);
      lwip_end();
      picorb_free(vm, server->pending_socket->recv_buf);
      picorb_free(vm, server->pending_socket);
      picorb_free(vm, server);
      return NULL;
    }
  }

  server->listen_pcb = altcp_listen_with_backlog(server->listen_pcb, backlog);
  if (!server->listen_pcb) {
    D("TCPServer_create: altcp_listen_with_backlog failed");
    lwip_end();
    picorb_free(vm, server->pending_socket->recv_buf);
    picorb_free(vm, server->pending_socket);
    picorb_free(vm, server);
    return NULL;
  }

  /* CRITICAL: listen creates a new PCB, must set SO_REUSEADDR again */
  struct tcp_pcb *listen_tpcb = (struct tcp_pcb *)server->listen_pcb->state;
  if (listen_tpcb) {
    ip_set_option(listen_tpcb, SOF_REUSEADDR);
  }

  altcp_arg(server->listen_pcb, server);
  altcp_accept(server->listen_pcb, tcp_accept_callback);
  lwip_end();

  return server;
}

/* Accept connection (non-blocking) */
picorb_socket_t*
TCPServer_accept_nonblock(picorb_state *vm, picorb_tcp_server_t *server)
{
  if (!server) {
    return NULL;
  }

#ifdef PICO_CYW43_ARCH_POLL
  cyw43_arch_poll();
#endif

  /* Check for already accepted socket */
  if (!server->accepted_socket) {
    return NULL; /* No pending connection */
  }

  /* Return the pre-allocated socket that was claimed by the callback. */
  picorb_socket_t *sock = server->accepted_socket;

  /* Clear from server */
  server->accepted_socket = NULL;
  server->state = 0;

  /* Replenish pending_socket for the next accepted connection. */
  if (!server->pending_socket) {
    server->pending_socket = (picorb_socket_t *)picorb_alloc(vm, sizeof(picorb_socket_t));
    if (server->pending_socket) {
      memset(server->pending_socket, 0, sizeof(picorb_socket_t));
      server->pending_socket->recv_buf = (char *)picorb_alloc(vm, TCP_SERVER_RECV_BUF_SIZE + 1);
      if (!server->pending_socket->recv_buf) {
        picorb_free(vm, server->pending_socket);
        server->pending_socket = NULL;
      } else {
        server->pending_socket->recv_capacity = TCP_SERVER_RECV_BUF_SIZE;
      }
    }
  }

  return sock;
}

/* Close server */
bool
TCPServer_close(picorb_state *vm, picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }

  /* Clean up pending pre-allocated socket if not yet used. */
  if (server->pending_socket) {
    if (server->pending_socket->recv_buf) {
      picorb_free(vm, server->pending_socket->recv_buf);
    }
    picorb_free(vm, server->pending_socket);
    server->pending_socket = NULL;
  }

  /* Clean up accepted socket if present */
  if (server->accepted_socket) {
    lwip_begin();
    if (server->accepted_socket->pcb) {
      altcp_close(server->accepted_socket->pcb);
    }
    lwip_end();
    if (server->accepted_socket->recv_buf) {
      picorb_free(vm, server->accepted_socket->recv_buf);
    }
    picorb_free(vm, server->accepted_socket);
    server->accepted_socket = NULL;
  }

  if (server->listen_pcb) {
    lwip_begin();
    altcp_close(server->listen_pcb);
    server->listen_pcb = NULL;
    lwip_end();

    /* Poll LwIP to process cleanup */
    Net_busy_wait_ms(50);
  }

  picorb_free(vm, server);
  return true;
}

/* Get server port */
int
TCPServer_port(picorb_state *vm, picorb_tcp_server_t *server)
{
  if (!server) {
    return -1;
  }
  return server->port;
}

/* Check if server is listening */
bool
TCPServer_listening(picorb_state *vm, picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }
  return server->listen_pcb != NULL;
}
