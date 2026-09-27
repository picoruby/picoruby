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

/* Upper bound for the number of accept slots. The backlog argument of
 * TCPServer.new selects the actual number (clamped to this value). Each
 * slot pre-allocates a picorb_socket_t plus a TCP_SERVER_RECV_BUF_SIZE
 * receive buffer, so a slot costs about 4.8 KB of heap. */
#ifndef TCP_SERVER_MAX_BACKLOG
#define TCP_SERVER_MAX_BACKLOG 8
#endif

/* TCP Server structure.
 *
 * Sockets move through three stages:
 *   free_sockets -> (accept callback) -> accepted ring -> (accept_nonblock)
 *   -> owned by the Ruby TCPSocket object.
 * free_count + accepted_count never exceeds slots, so the accept callback
 * only has to abort a connection when every slot is in use. */
struct picorb_tcp_server {
  struct altcp_pcb *listen_pcb;
  picorb_socket_t *free_sockets[TCP_SERVER_MAX_BACKLOG];
  int free_count;
  picorb_socket_t *accepted[TCP_SERVER_MAX_BACKLOG]; /* FIFO ring */
  int accepted_head;
  int accepted_count;
  int slots;
  picorb_state *vm;
  void *event_queue;
  bool event_pending;
  int port;
};

/* Allocate one pre-initialized socket with its receive buffer. */
static picorb_socket_t *
server_socket_alloc(picorb_state *vm)
{
  picorb_socket_t *sock = (picorb_socket_t *)picorb_alloc(vm, sizeof(picorb_socket_t));
  if (!sock) {
    return NULL;
  }
  memset(sock, 0, sizeof(picorb_socket_t));
  sock->recv_buf = (char *)picorb_alloc(vm, TCP_SERVER_RECV_BUF_SIZE + 1);
  if (!sock->recv_buf) {
    picorb_free(vm, sock);
    return NULL;
  }
  sock->recv_capacity = TCP_SERVER_RECV_BUF_SIZE;
  return sock;
}

static void
server_socket_free(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) {
    return;
  }
  if (sock->recv_buf) {
    picorb_free(vm, sock->recv_buf);
  }
  picorb_free(vm, sock);
}

/* Top up the free slots. Runs in task context only (never from a LwIP
 * callback), so it may use the heap. Stops silently when the heap is
 * exhausted; the accept callback then aborts extra connections. */
static void
server_replenish(picorb_state *vm, picorb_tcp_server_t *server)
{
  while (server->free_count + server->accepted_count < server->slots) {
    picorb_socket_t *sock = server_socket_alloc(vm);
    if (!sock) {
      break;
    }
    server->free_sockets[server->free_count++] = sock;
  }
}

/* Release every socket the server still owns. Accepted sockets that Ruby
 * has not picked up yet are closed as well. */
static void
server_release_sockets(picorb_state *vm, picorb_tcp_server_t *server)
{
  while (server->free_count > 0) {
    server_socket_free(vm, server->free_sockets[--server->free_count]);
  }
  while (server->accepted_count > 0) {
    picorb_socket_t *sock = server->accepted[server->accepted_head];
    server->accepted_head = (server->accepted_head + 1) % server->slots;
    server->accepted_count--;
    /* Closes the PCB and releases recv_buf and any pending pbuf. */
    TCPSocket_close(vm, sock);
    picorb_free(vm, sock);
  }
  server->accepted_head = 0;
}

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
    picorb_socket_notify_readable(sock);
    D("tcp_server.c tcp_recv_callback: error, state set to ERROR");
    return err;
  }

  /* NULL pbuf means connection closed */
  if (!pbuf) {
    sock->state = SOCKET_STATE_CLOSED;
    sock->connected = false;
    sock->closed = true;
    picorb_socket_notify_readable(sock);
    D("tcp_server.c tcp_recv_callback: connection closed");
    return ERR_OK;
  }

  /* Copy what fits into the pre-allocated buffer (no heap allocation) and
   * keep the rest as a pending pbuf until the application reads. */
  D("tcp_server.c tcp_recv_callback: receiving %u bytes, current recv_len=%zu\n", pbuf->tot_len, sock->recv_len);
  TCPSocket_store_pbuf(sock, pbuf);

  picorb_socket_notify_readable(sock);
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
  picorb_socket_notify_readable(sock);
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

  /* Take a pre-allocated socket to avoid heap allocation in callback context. */
  if (server->free_count == 0) {
    /* Every slot is in use: abort the connection. The callback must return
     * ERR_ABRT after aborting, otherwise LwIP aborts the same PCB again
     * (tcp_in.c: tcp_process) and frees it twice, which corrupts the
     * TCP_PCB pool free list and makes tcp_input loop forever. */
    D("tcp_accept_callback: no free slot, aborting connection");
    altcp_abort(newpcb);
    return ERR_ABRT;
  }
  picorb_socket_t *sock = server->free_sockets[--server->free_count];

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

  /* Queue for accept_nonblock. The ring cannot overflow because
   * free_count + accepted_count <= slots. */
  server->accepted[(server->accepted_head + server->accepted_count) % server->slots] = sock;
  server->accepted_count++;

  /* Set up callbacks with correct arg */
  altcp_arg(newpcb, sock);
  altcp_recv(newpcb, tcp_recv_callback);
  altcp_sent(newpcb, tcp_sent_callback);
  altcp_err(newpcb, tcp_err_callback);
  TCPServer_notify_accepted(server);

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

  /* The backlog selects how many connections can be accepted by LwIP
   * before Ruby picks them up. Clamp it to the slot array size. */
  if (backlog < 1) backlog = 1;
  if (backlog > TCP_SERVER_MAX_BACKLOG) backlog = TCP_SERVER_MAX_BACKLOG;
  server->slots = backlog;

  /* Pre-allocate the accept slots. At least one is required. */
  server_replenish(vm, server);
  if (server->free_count == 0) {
    picorb_free(vm, server);
    return NULL;
  }

  lwip_begin();

  /* Create tcp_pcb directly to set SO_REUSEADDR */
  struct tcp_pcb *tpcb = tcp_new();
  if (!tpcb) {
    D("TCPServer_create: tcp_new failed");
    lwip_end();
    server_release_sockets(vm, server);
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
    server_release_sockets(vm, server);
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
      server_release_sockets(vm, server);
      picorb_free(vm, server);
      return NULL;
    }
  }

  server->listen_pcb = altcp_listen_with_backlog(server->listen_pcb, backlog);
  if (!server->listen_pcb) {
    D("TCPServer_create: altcp_listen_with_backlog failed");
    lwip_end();
    server_release_sockets(vm, server);
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

  if (server->accepted_count == 0) {
    return NULL; /* No pending connection */
  }

  /* Hand the oldest accepted socket to Ruby. It now owns the socket. */
  picorb_socket_t *sock = server->accepted[server->accepted_head];
  server->accepted_head = (server->accepted_head + 1) % server->slots;
  server->accepted_count--;
  server->event_pending = false;

  /* Refill the slot that was just released. */
  server_replenish(vm, server);

  return sock;
}

/* Close server */
bool
TCPServer_close(picorb_state *vm, picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }

  /* Release free slots and close connections Ruby never picked up. */
  server_release_sockets(vm, server);

  if (server->listen_pcb) {
    lwip_begin();
    altcp_close(server->listen_pcb);
    server->listen_pcb = NULL;
    lwip_end();

    /* Poll LwIP to process cleanup */
    Net_busy_wait_ms(50);
  }

  TCPServer_notify_accepted(server);
  if (server->event_queue) {
    picorb_free(vm, server->event_queue);
    server->event_queue = NULL;
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

void
TCPServer_set_event_queue(picorb_tcp_server_t *server, picorb_state *vm, void *queue)
{
  if (!server) return;
  server->vm = vm;
  server->event_queue = queue;
}

void*
TCPServer_event_queue(picorb_tcp_server_t *server)
{
  return server ? server->event_queue : NULL;
}

picorb_state*
TCPServer_vm(picorb_tcp_server_t *server)
{
  return server ? server->vm : NULL;
}

bool
TCPServer_event_pending(picorb_tcp_server_t *server)
{
  return server && server->event_pending;
}

void
TCPServer_set_event_pending(picorb_tcp_server_t *server, bool pending)
{
  if (server) server->event_pending = pending;
}
