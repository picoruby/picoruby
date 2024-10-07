#include "../../include/net.h"
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"

#include "include/common.h"

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
  mrbc_value *send_data;
  mrbc_value *recv_data;
  mrbc_vm *vm;
  ip_addr_t remote_ip;
  u16_t remote_port;
} udp_connection_state;

/* end of platform-dependent definitions */

static void
UDPClient_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  udp_connection_state *cs = (udp_connection_state *)arg;
  if (p != NULL) {
    char *tmpbuf = mrbc_alloc(cs->vm, p->tot_len + 1);
    pbuf_copy_partial(p, tmpbuf, p->tot_len, 0);
    tmpbuf[p->tot_len] = '\0';
    mrbc_string_append_cbuf(cs->recv_data, tmpbuf, p->tot_len);
    mrbc_free(cs->vm, tmpbuf);
    cs->state = NET_UDP_STATE_RECEIVED;
    pbuf_free(p);
  } else {
    cs->state = NET_UDP_STATE_ERROR;
  }
}

static udp_connection_state *
UDPClient_new_connection(mrbc_value *send_data, mrbc_value *recv_data, mrbc_vm *vm)
{
  udp_connection_state *cs = (udp_connection_state *)mrbc_raw_alloc(sizeof(udp_connection_state));
  cs->state = NET_UDP_STATE_NONE;
  cs->pcb = udp_new();
  udp_recv(cs->pcb, UDPClient_recv_cb, cs);
  cs->send_data = send_data;
  cs->recv_data = recv_data;
  cs->vm = vm;
  return cs;
}

static udp_connection_state *
UDPClient_send_impl(ip_addr_t *ip, int port, mrbc_value *send_data, mrbc_value *recv_data, mrbc_vm *vm, bool is_dtls)
{
  (void)is_dtls;
  udp_connection_state *cs = UDPClient_new_connection(send_data, recv_data, vm);
  cs->remote_ip = *ip;
  cs->remote_port = port;

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, cs->send_data->string->size, PBUF_RAM);
  if (p != NULL) {
    memcpy(p->payload, cs->send_data->string->data, cs->send_data->string->size);
    cyw43_arch_lwip_begin();
    err_t err = udp_sendto(cs->pcb, p, ip, port);
    cyw43_arch_lwip_end();
    pbuf_free(p);

    if (err == ERR_OK) {
      cs->state = NET_UDP_STATE_WAITING;
    } else {
      cs->state = NET_UDP_STATE_ERROR;
    }
  } else {
    cs->state = NET_UDP_STATE_ERROR;
  }

  return cs;
}

static int
UDPClient_poll_impl(udp_connection_state **pcs)
{
  if (*pcs == NULL)
    return 0;
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
      mrbc_free(cs->vm, cs);
      *pcs = NULL;
      return 0;
    case NET_UDP_STATE_ERROR:
      cyw43_arch_lwip_begin();
      udp_remove(cs->pcb);
      cyw43_arch_lwip_end();
      mrbc_free(cs->vm, cs);
      *pcs = NULL;
      return 0;
  }
  return cs->state;
}

mrbc_value
UDPClient_send(const char *host, int port, mrbc_vm *vm, mrbc_value *send_data, bool is_dtls)
{
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  mrbc_value ret;
  Net_get_ip(host, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    mrbc_value recv_data = mrbc_string_new(vm, NULL, 0);
    udp_connection_state *cs = UDPClient_send_impl(&ip, port, send_data, &recv_data, vm, is_dtls);
    while(UDPClient_poll_impl(&cs))
    {
      sleep_ms(200);
    }
    ret = recv_data;
  } else {
    ret = mrbc_nil_value();
  }
  return ret;
}
