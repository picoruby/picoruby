#include "../include/net.h"
#include "lwip/udp.h"

/* platform-dependent definitions */

/* state values for UDP connection */
#define NET_UDP_STATE_NONE          0
#define NET_UDP_STATE_SENDING       1
#define NET_UDP_STATE_WAITING       2
#define NET_UDP_STATE_RECEIVED      3
#define NET_UDP_STATE_FINISHED      4
#define NET_UDP_STATE_ERROR         99

/* UDP connection struct */
typedef struct udp_connection_state_str
{
  int state;
  struct udp_pcb *pcb;
  const char *send_data;
  size_t send_data_len;
  char *recv_data;
  size_t recv_data_len;
  mrb_state *mrb;
  ip_addr_t remote_ip;
  u16_t remote_port;
} udp_connection_state;

/* end of platform-dependent definitions */

static void
UDPClient_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  if (!arg) return;
  udp_connection_state *cs = (udp_connection_state *)arg;
  mrb_state *mrb = cs->mrb;

  if (p != NULL) {
    char *tmpbuf = picorb_alloc(mrb, p->tot_len + 1);
    pbuf_copy_partial(p, tmpbuf, p->tot_len, 0);
    tmpbuf[p->tot_len] = '\0';
    assert(cs->recv_data);
    char *new_recv_data = (char *)picorb_realloc(mrb, cs->recv_data, cs->recv_data_len + p->tot_len + 1);
    if (new_recv_data == NULL) {
      picorb_free(mrb, tmpbuf);
      cs->state = NET_UDP_STATE_ERROR;
      return;
    }
    cs->recv_data = new_recv_data;
    memcpy(cs->recv_data + cs->recv_data_len, tmpbuf, p->tot_len);
    cs->recv_data_len += p->tot_len;
    picorb_free(mrb, tmpbuf);
    cs->state = NET_UDP_STATE_RECEIVED;
    pbuf_free(p);
  } else {
    cs->state = NET_UDP_STATE_ERROR;
  }
}

static udp_connection_state *
UDPClient_new_connection(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  udp_connection_state *cs = (udp_connection_state *)picorb_alloc(mrb, sizeof(udp_connection_state));
  cs->state = NET_UDP_STATE_NONE;
  cs->pcb = udp_new();
  if (cs->pcb == NULL) {
    picorb_warn("Failed to create new UDP PCB\n");
    return NULL;
  }
  udp_recv(cs->pcb, UDPClient_recv_cb, cs);
  cs->send_data = req->send_data;
  cs->send_data_len = req->send_data_len;
  cs->recv_data = res->recv_data;
  cs->recv_data_len = res->recv_data_len;
  cs->mrb = mrb;
  return cs;
}

static udp_connection_state *
UDPClient_send_impl(mrb_state *mrb, ip_addr_t *ip, const net_request_t *req, net_response_t *res)
{
  udp_connection_state *cs = UDPClient_new_connection(mrb, req, res);
  if (cs == NULL) {
    picorb_warn("Failed to create new connection\n");
    return NULL;
  }

  cs->remote_ip = *ip;
  cs->remote_port = req->port;

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, cs->send_data_len, PBUF_RAM);
  if (p != NULL) {
    memcpy(p->payload, cs->send_data, cs->send_data_len);
    lwip_begin();
    err_t err = udp_sendto(cs->pcb, p, ip, req->port);
    lwip_end();
    pbuf_free(p);

    if (err == ERR_OK) {
      cs->state = NET_UDP_STATE_WAITING;
    } else {
      cs->state = NET_UDP_STATE_ERROR;
      picorb_warn("Failed to send UDP packet, error: %d\n", err);
    }
  } else {
    cs->state = NET_UDP_STATE_ERROR;
    picorb_warn("Failed to allocate pbuf for send data\n");
  }

  return cs;
}

static int
UDPClient_poll_impl(udp_connection_state **pcs)
{
  if (*pcs == NULL) {
    return 0;
  }

  udp_connection_state *cs = *pcs;

  switch(cs->state)
  {
    case NET_UDP_STATE_NONE:
    case NET_UDP_STATE_WAITING:
      break;
    case NET_UDP_STATE_RECEIVED:
      cs->state = NET_UDP_STATE_FINISHED;
      return 0;
    case NET_UDP_STATE_ERROR:
      MRB;
      return 0;
  }
  return cs->state;
}

static void
UDPClient_close(udp_connection_state *cs)
{
  if (cs == NULL) return;
  mrb_state *mrb = cs->mrb;
  lwip_begin();
  udp_remove(cs->pcb);
  lwip_end();
  picorb_free(mrb, cs);
}

bool
UDPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  bool ret = false;
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  Net_get_ip(req->host, &ip);

  udp_connection_state *cs = NULL;

  if (!ip4_addr_isloopback(&ip)) {
    cs = UDPClient_send_impl(mrb, &ip, req, res);
    if (cs) {
      int max_wait = 50;
      while (UDPClient_poll_impl(&cs) && 0 < max_wait--) {
        Net_sleep_ms(100);
      }
      if (max_wait <= 0) {
        picorb_warn("UDPClient_send: timeout\n");
      } else {
        res->recv_data = cs->recv_data;
        res->recv_data_len = cs->recv_data_len;
      }
      if (cs->state == NET_UDP_STATE_FINISHED) {
        ret = true;
      }
      UDPClient_close(cs);
    }
  }
  return ret;
}
