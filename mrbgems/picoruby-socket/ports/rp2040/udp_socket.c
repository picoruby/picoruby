/*
 * UDP Socket implementation for rp2040 using LwIP
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include "picoruby/debug.h"
#include <string.h>
#include <stdio.h>

/* LwIP includes */
#define PICORB_NO_LWIP_HELPERS
#include "lwip/udp.h"
#include "lwip/dns.h"

/* Pre-allocated receive buffer size for UDP.
 * 1500 covers max Ethernet frame payload. Sufficient for NTP (48 bytes),
 * DNS (<512 bytes), and other typical UDP protocols. */
#ifndef UDP_RECV_BUF_SIZE
#define UDP_RECV_BUF_SIZE 1500
#endif

/* Receive callback - runs in LwIP callback context (may be IRQ/PendSV).
 * Must NOT call heap allocator to avoid heap corruption. Uses only
 * the pre-allocated recv_buf. */
static void
udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf,
                   const ip_addr_t *addr, u16_t port)
{
  picorb_socket_t *sock = (picorb_socket_t *)arg;
  if (!sock || !pbuf) {
    if (pbuf) pbuf_free(pbuf);
    return;
  }

  /* Store sender information */
  if (addr) {
    snprintf(sock->last_sender_host, sizeof(sock->last_sender_host),
             "%u.%u.%u.%u",
             (unsigned int)(ip4_addr1(addr)),
             (unsigned int)(ip4_addr2(addr)),
             (unsigned int)(ip4_addr3(addr)),
             (unsigned int)(ip4_addr4(addr)));
    sock->last_sender_port = port;
  } else {
    sock->last_sender_host[0] = '\0';
    sock->last_sender_port = 0;
  }

  /* Copy data into pre-allocated buffer (no heap allocation) */
  size_t total_len = pbuf->tot_len;
  size_t new_size = sock->recv_len + total_len;

  if (new_size > sock->recv_capacity) {
    /* Buffer full, drop packet (acceptable for UDP) */
    pbuf_free(pbuf);
    return;
  }

  struct pbuf *current = pbuf;
  size_t offset = sock->recv_len;
  while (current) {
    memcpy(sock->recv_buf + offset, current->payload, current->len);
    offset += current->len;
    current = current->next;
  }
  sock->recv_len = offset;
  sock->recv_buf[sock->recv_len] = '\0';

  pbuf_free(pbuf);
}

/* Create UDP socket */
bool
UDPSocket_create(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return false;

  memset(sock, 0, sizeof(picorb_socket_t));

  /* Pre-allocate receive buffer to eliminate heap allocation in callbacks. */
  sock->recv_buf = (char *)picorb_alloc(vm, UDP_RECV_BUF_SIZE + 1);
  if (!sock->recv_buf) {
    return false;
  }
  sock->recv_capacity = UDP_RECV_BUF_SIZE;
  sock->recv_len = 0;

  lwip_begin();
  sock->pcb = (struct altcp_pcb *)udp_new();
  lwip_end();

  if (!sock->pcb) {
    D("UDPSocket_create: udp_new failed");
    picorb_free(vm, sock->recv_buf);
    sock->recv_buf = NULL;
    sock->recv_capacity = 0;
    return false;
  }

  sock->state = SOCKET_STATE_NONE;
  sock->socktype = 2; /* SOCK_DGRAM equivalent */
  sock->connected = false;
  sock->closed = false;
  sock->last_sender_host[0] = '\0';
  sock->last_sender_port = 0;

  lwip_begin();
  udp_recv((struct udp_pcb *)sock->pcb, udp_recv_callback, sock);
  lwip_end();

  return true;
}

/* Bind to local address/port */
bool
UDPSocket_bind(picorb_state *vm, picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || !sock->pcb) {
    D("UDPSocket_bind: sock or pcb is NULL");
    return false;
  }

  ip_addr_t addr;
  if (!host || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "") == 0) {
    ip_addr_set_any(false, &addr);
  } else {
    if (Net_get_ip(host, &addr) != 0) {
      D("UDPSocket_bind: failed to resolve host %s\n", host);
      snprintf(sock->errmsg, sizeof(sock->errmsg),
               "getaddrinfo(\"%s\"): Name or service not known", host);
      return false;
    }
  }

  lwip_begin();
  err_t err = udp_bind((struct udp_pcb *)sock->pcb, &addr, port);
  lwip_end();

  if (err != ERR_OK) {
    D("UDPSocket_bind: udp_bind failed with error %d\n", err);
  }

  return err == ERR_OK;
}

/* Connect to remote address (sets default destination) */
bool
UDPSocket_connect(picorb_state *vm, picorb_socket_t *sock, const char *host, int port)
{
  if (!sock || !sock->pcb || !host) return false;

  ip_addr_t ip_addr;
  if (Net_get_ip(host, &ip_addr) != 0) {
    snprintf(sock->errmsg, sizeof(sock->errmsg),
             "getaddrinfo(\"%s\"): Name or service not known", host);
    return false;
  }

  lwip_begin();
  err_t err = udp_connect((struct udp_pcb *)sock->pcb, &ip_addr, port);
  lwip_end();

  if (err == ERR_OK) {
    strncpy(sock->remote_host, host, sizeof(sock->remote_host) - 1);
    sock->remote_host[sizeof(sock->remote_host) - 1] = '\0';
    sock->remote_port = port;
    sock->state = SOCKET_STATE_CONNECTED;
    sock->connected = true;
  }

  return err == ERR_OK;
}

/* Send to connected peer */
ssize_t
UDPSocket_send(picorb_state *vm, picorb_socket_t *sock, const void *data, size_t len)
{
  if (!sock || !sock->pcb || !data) return -1;

  struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (!pbuf) return -1;

  memcpy(pbuf->payload, data, len);

  lwip_begin();
  err_t err = udp_send((struct udp_pcb *)sock->pcb, pbuf);
  lwip_end();

  pbuf_free(pbuf);

#ifdef PICO_CYW43_ARCH_POLL
  cyw43_arch_poll();
#endif

  return (err == ERR_OK) ? (ssize_t)len : -1;
}

/* Send to specific address */
ssize_t
UDPSocket_sendto(picorb_state *vm, picorb_socket_t *sock, const void *data, size_t len,
                  const char *host, int port)
{
  if (!sock || !sock->pcb || !data || !host) return -1;

  ip_addr_t ip_addr;
  if (Net_get_ip(host, &ip_addr) != 0) {
    snprintf(sock->errmsg, sizeof(sock->errmsg),
             "getaddrinfo(\"%s\"): Name or service not known", host);
    return -1;
  }

  struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (!pbuf) return -1;

  memcpy(pbuf->payload, data, len);

  lwip_begin();
  err_t err = udp_sendto((struct udp_pcb *)sock->pcb, pbuf, &ip_addr, port);
  lwip_end();

  pbuf_free(pbuf);

#ifdef PICO_CYW43_ARCH_POLL
  cyw43_arch_poll();
#endif

  return (err == ERR_OK) ? (ssize_t)len : -1;
}

/* Receive from any peer */
ssize_t
UDPSocket_recvfrom(picorb_state *vm, picorb_socket_t *sock, void *buf, size_t len,
                    char *host, size_t host_len, int *port)
{
  if (!sock || !buf) return -1;

#ifdef PICO_CYW43_ARCH_POLL
  cyw43_arch_poll();
#endif

  /* Check if data is available (non-blocking) */
  if (sock->recv_len == 0) {
    return 0; /* No data available */
  }

  /* Copy data */
  size_t to_copy = (len < sock->recv_len) ? len : sock->recv_len;
  memcpy(buf, sock->recv_buf, to_copy);

  /* Copy sender information if requested */
  if (host && host_len > 0) {
    strncpy(host, sock->last_sender_host, host_len - 1);
    host[host_len - 1] = '\0';
  }
  if (port) {
    *port = sock->last_sender_port;
  }

  /* Update buffer */
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
UDPSocket_close(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return false;

  if (sock->pcb) {
    lwip_begin();
    udp_remove((struct udp_pcb *)sock->pcb);
    lwip_end();
    sock->pcb = NULL;
  }

  if (sock->recv_buf) {
    picorb_free(vm, sock->recv_buf);
    sock->recv_buf = NULL;
  }

  sock->state = SOCKET_STATE_CLOSED;
  sock->connected = false;
  sock->closed = true;
  sock->recv_len = 0;
  sock->recv_capacity = 0;

  return true;
}

/* Check if closed */
bool
UDPSocket_closed(picorb_state *vm, picorb_socket_t *sock)
{
  if (!sock) return true;
  return sock->state == SOCKET_STATE_CLOSED || !sock->pcb;
}
