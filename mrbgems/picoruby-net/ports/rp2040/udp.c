#include "../../include/net.h"
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "include/common.h"

#include "mruby.h"
#include "mruby/string.h"

/* platform-dependent definitions */

/* state values for UDP connection */
#define NET_UDP_STATE_NONE          0
#define NET_UDP_STATE_SENDING       1
#define NET_UDP_STATE_WAITING       2
#define NET_UDP_STATE_RECEIVED      3
#define NET_UDP_STATE_ERROR         99

/* UDP connection struct */
typedef struct udp_connection_state_str
{
  int state;
  struct udp_pcb *pcb;
  mrb_value send_data;
  mrb_value recv_data;
  mrb_state *mrb;
  ip_addr_t remote_ip;
  u16_t remote_port;
} udp_connection_state;

/* end of platform-dependent definitions */

static void
UDPClient_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  udp_connection_state *cs = (udp_connection_state *)arg;

  if (p != NULL) {
    char *tmpbuf = mrb_malloc(cs->mrb, p->tot_len + 1);
    pbuf_copy_partial(p, tmpbuf, p->tot_len, 0);
    tmpbuf[p->tot_len] = '\0';
    mrb_str_cat(cs->mrb, cs->recv_data, tmpbuf, p->tot_len);
    mrb_free(cs->mrb, tmpbuf);
    cs->state = NET_UDP_STATE_RECEIVED;
    pbuf_free(p);
  } else {
    cs->state = NET_UDP_STATE_ERROR;
    mrb_warn(cs->mrb, "State changed to NET_UDP_STATE_ERROR\n");
  }
}

static udp_connection_state *
UDPClient_new_connection(mrb_value send_data, mrb_value recv_data, mrb_state *mrb)
{
  udp_connection_state *cs = (udp_connection_state *)mrb_malloc(mrb, sizeof(udp_connection_state));
  cs->state = NET_UDP_STATE_NONE;
  cs->pcb = udp_new();
  if (cs->pcb == NULL) {
    mrb_warn(cs->mrb, "Failed to create new UDP PCB\n");
    return NULL;
  }
  udp_recv(cs->pcb, UDPClient_recv_cb, cs);
  cs->send_data = send_data;
  cs->recv_data = recv_data;
  cs->mrb = mrb;
  return cs;
}

static udp_connection_state *
UDPClient_send_impl(ip_addr_t *ip, int port, mrb_value send_data, mrb_value recv_data, mrb_state *mrb, bool is_dtls)
{
  (void)is_dtls;
  udp_connection_state *cs = UDPClient_new_connection(send_data, recv_data, mrb);
  if (cs == NULL) {
    mrb_warn(cs->mrb, "Failed to create new connection\n");
    return NULL;
  }

  cs->remote_ip = *ip;
  cs->remote_port = port;

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, RSTRING_LEN(cs->send_data), PBUF_RAM);
  if (p != NULL) {
    memcpy(p->payload, RSTRING_PTR(cs->send_data), RSTRING_LEN(cs->send_data));
    cyw43_arch_lwip_begin();
    err_t err = udp_sendto(cs->pcb, p, ip, port);
    cyw43_arch_lwip_end();
    pbuf_free(p);

    if (err == ERR_OK) {
      cs->state = NET_UDP_STATE_WAITING;
    } else {
      cs->state = NET_UDP_STATE_ERROR;
      mrb_warn(cs->mrb, "Failed to send UDP packet, error: %d\n", err);
    }
  } else {
    cs->state = NET_UDP_STATE_ERROR;
    mrb_warn(cs->mrb, "Failed to allocate pbuf for send data\n");
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
      cs->state = NET_UDP_STATE_NONE;
      cyw43_arch_lwip_begin();
      udp_remove(cs->pcb);
      cyw43_arch_lwip_end();
      mrb_free(cs->mrb, cs);
      *pcs = NULL;
      return 0;
    case NET_UDP_STATE_ERROR:
      mrb_warn(cs->mrb, "Error occurred, cleaning up\n");
      cyw43_arch_lwip_begin();
      udp_remove(cs->pcb);
      cyw43_arch_lwip_end();
      mrb_free(cs->mrb, cs);
      *pcs = NULL;
      return 0;
  }
  return cs->state;
}

mrb_value
UDPClient_send(const char *host, int port, mrb_state *mrb, mrb_value send_data, bool is_dtls)
{
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  mrb_value ret;

  Net_get_ip(host, &ip);

  if(!ip4_addr_isloopback(&ip)) {
    mrb_value recv_data = mrb_str_new_capa(mrb, 0);
    udp_connection_state *cs = UDPClient_send_impl(&ip, port, send_data, recv_data, mrb, is_dtls);

    if (cs == NULL) {
      mrb_warn(cs->mrb, "Failed to create UDP connection\n");
      ret = mrb_nil_value();
    } else {
      int poll_count = 0;
      while(UDPClient_poll_impl(&cs))
      {
        poll_count++;
        sleep_ms(200);
        // Add a timeout mechanism to prevent infinite loop
        if (poll_count > 50) { // 10 seconds timeout (50 * 200ms)
          mrb_warn(cs->mrb, "Polling timeout reached\n");
          break;
        }
      }
      if (cs != NULL) {
        mrb_warn(cs->mrb, "Connection state not NULL after polling, cleaning up\n");
        cyw43_arch_lwip_begin();
        udp_remove(cs->pcb);
        cyw43_arch_lwip_end();
        mrb_free(cs->mrb, cs);
      }
      ret = recv_data;
    }
  } else {
    mrb_warn(mrb, "IP is loopback, not sending\n");
    ret = mrb_nil_value();
  }
  return ret;
}
