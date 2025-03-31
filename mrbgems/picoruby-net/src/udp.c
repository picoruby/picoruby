#include "../include/net.h"

/* state values for UDP connection */
#define NET_UDP_STATE_NONE          0
#define NET_UDP_STATE_SENDING       1
#define NET_UDP_STATE_WAITING       2
#define NET_UDP_STATE_RECEIVED      3
#define NET_UDP_STATE_ERROR         99

#if defined(PICORB_VM_MRUBY)
# define CS_MRB_WARN CS_MRB
# define CS_MRB mrb_state *mrb = cs->mrb
#else
# define CS_MRB_WARN
# define CS_MRB mrbc_vm *mrb = cs->mrb
#endif

/* UDP connection struct */
typedef struct udp_connection_state_str
{
  int state;
  struct udp_pcb *pcb;
  const char *send_data;
  size_t send_data_len;
  recv_data_t *recv_data;
  ip_addr_t remote_ip;
  u16_t remote_port;
  mrb_state *mrb;
} udp_connection_state;

/* end of platform-dependent definitions */

static void
UDPClient_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *addr, u16_t port)
{
  udp_connection_state *cs = (udp_connection_state *)arg;
  CS_MRB;

  if (pbuf != NULL) {
    recv_data_t *recv_data = cs->recv_data;
    if (recv_data->data == NULL) {
      recv_data->data = (char *)picorb_alloc(mrb, pbuf->tot_len + 1);
    } else {
      recv_data->data = (char *)picorb_realloc(mrb, recv_data->data, pbuf->tot_len + 1);
    }
    size_t offset = recv_data->len;

    pbuf_copy_partial(pbuf, recv_data->data + offset, pbuf->tot_len, 0);
    recv_data->data[pbuf->tot_len] = '\0';
    recv_data->len += pbuf->tot_len;
    cs->state = NET_UDP_STATE_RECEIVED;
    pbuf_free(pbuf);
  } else {
    cs->state = NET_UDP_STATE_ERROR;
    picorb_warn("State changed to NET_UDP_STATE_ERROR\n");
  }
}

static udp_connection_state *
UDPClient_new_connection(void *mrb, const char *send_data, size_t send_data_len, recv_data_t *recv_data)
{
  udp_connection_state *cs = (udp_connection_state *)picorb_alloc(mrb, sizeof(udp_connection_state));
  cs->mrb = mrb;
  cs->state = NET_UDP_STATE_NONE;
  cs->pcb = udp_new();
  if (cs->pcb == NULL) {
    picorb_warn("Failed to create new UDP PCB\n");
    return NULL;
  }
  udp_recv(cs->pcb, UDPClient_recv_cb, cs);
  cs->send_data = send_data;
  cs->send_data_len = send_data_len;
  cs->recv_data = recv_data;
  return cs;
}

static err_t
UDPClient_send_impl(udp_connection_state *cs, ip_addr_t *ip, int port, bool is_dtls)
{
  (void)is_dtls;
  CS_MRB_WARN;
  cs->remote_ip = *ip;
  cs->remote_port = port;

  struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, cs->send_data_len, PBUF_RAM);
  if (pbuf != NULL) {
    memcpy(pbuf->payload, cs->send_data, cs->send_data_len);
    lwip_begin();
    err_t err = udp_sendto(cs->pcb, pbuf, ip, port);
    lwip_end();
    pbuf_free(pbuf);

    if (err == ERR_OK) {
      cs->state = NET_UDP_STATE_WAITING;
    } else {
      cs->state = NET_UDP_STATE_ERROR;
      picorb_warn("Failed to send UDP packet, error: %d\n", err);
      return err;
    }
  } else {
    cs->state = NET_UDP_STATE_ERROR;
    picorb_warn("Failed to allocate pbuf for send data\n");
    return ERR_MEM;
  }

  return ERR_OK;
}

static int
UDPClient_poll_impl(udp_connection_state **pcs)
{
  if (*pcs == NULL) {
    return 0;
  }

  udp_connection_state *cs = *pcs;
  CS_MRB;

  switch(cs->state)
  {
    case NET_UDP_STATE_NONE:
    case NET_UDP_STATE_WAITING:
      break;
    case NET_UDP_STATE_RECEIVED:
      cs->state = NET_UDP_STATE_NONE;
      lwip_begin();
      udp_remove(cs->pcb);
      lwip_end();
      picorb_free(mrb, cs);
      *pcs = NULL;
      return 0;
    case NET_UDP_STATE_ERROR:
      picorb_warn("Error occurred, cleaning up\n");
      lwip_begin();
      udp_remove(cs->pcb);
      lwip_end();
      picorb_free(mrb, cs);
      *pcs = NULL;
      return 0;
  }
  return cs->state;
}

void
UDPClient_send(void *mrb, const char *host, int port, const char *send_data, size_t send_data_len, bool is_dtls, recv_data_t *recv_data)
{
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  Net_get_ip(host, &ip);

  if(!ip4_addr_isloopback(&ip)) {
    udp_connection_state *cs = UDPClient_new_connection(mrb, send_data, send_data_len, recv_data);
    if (cs == NULL) {
      picorb_warn("Failed to create UDP connection\n");
      return;
    }

    err_t err = UDPClient_send_impl(cs, &ip, port, is_dtls);

    if (err == ERR_OK) {
      int poll_count = 0;
      while (UDPClient_poll_impl(&cs)) {
        poll_count++;
        sleep_ms(200);
        // Add a timeout mechanism to prevent infinite loop
        if (poll_count > 50) { // 10 seconds timeout (50 * 200ms)
          picorb_warn("Polling timeout reached\n");
          break;
        }
      }
      if (cs != NULL) {
        picorb_warn("Connection state not NULL after polling, cleaning up\n");
        lwip_begin();
        udp_remove(cs->pcb);
        lwip_end();
        picorb_free(mrb, cs);
      }
      return;
    }
  }
  picorb_warn("IP is loopback, not sending\n");
}
