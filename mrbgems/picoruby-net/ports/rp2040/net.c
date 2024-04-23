#include "../../include/net.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

/* platform-dependent definitions */

/* state values for TCP connection */
#define NET_TCP_STATE_NONE               0
#define NET_TCP_STATE_CONNECTION_STARTED 1
#define NET_TCP_STATE_CONNECTED          2
#define NET_TCP_STATE_WAITING_PACKET     3
#define NET_TCP_STATE_PACKET_RECVED      4
#define NET_TCP_STATE_FINISHED           6

/* TCP connection struct */
typedef struct tcp_connection_state_str
{
  int state;
  struct altcp_pcb *pcb;
  mrbc_value *send_data;
  mrbc_value *recv_data;
} tcp_connection_state;

/* end of platform-dependent definitions */

void dns_found(const char *name, const ip_addr_t *ip, void *arg)
{
  ip_addr_t *result = (ip_addr_t *)arg;
  if (ip) {
    ip4_addr_copy(*result, *ip);
  } else {
    ip4_addr_set_loopback(result);
  }
  return;
}

err_t get_ip_impl(const char *name, ip_addr_t *ip)
{
  cyw43_arch_lwip_begin();
  err_t err = dns_gethostbyname(name, ip, dns_found, ip);
  cyw43_arch_lwip_end();
  return err;
}

err_t get_ip(const char *name, ip_addr_t *ip)
{
  get_ip_impl(name, ip);
  while (!ip_addr_get_ip4_u32(ip)) {
    sleep_ms(50);
  }
  return ERR_OK;
}

mrbc_value DNS_resolve(mrbc_vm *vm, const char *name)
{
  ip_addr_t ip;
  mrbc_value ret;
  ip4_addr_set_zero(&ip);
  get_ip(name, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    char buf[16];
    ipaddr_ntoa_r(&ip, buf, 16);
    ret = mrbc_string_new(vm, buf, strlen(buf));
  } else {
    ret = mrbc_nil_value();
  }
  return ret;
}

err_t TCP_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  // do nothing as of now.
  cs->state = NET_TCP_STATE_FINISHED;
  return ERR_OK;
}

err_t TCP_sent_cb(void *arg, struct altcp_pcb *pcb, u16_t len)
{
  // do nothing as of now
  return ERR_OK;
}

static err_t TCP_connected_cb(void *arg, struct altcp_pcb *pcb, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  cs->state = NET_TCP_STATE_CONNECTED;
  return ERR_OK;
}

err_t TCP_poll_cb(void *arg, struct altcp_pcb *pcb)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  cs->state = NET_TCP_STATE_FINISHED;
  return ERR_OK;
}

void TCP_err_cb(void *arg, err_t err)
{
  // do nothing as of now
}

tcp_connection_state *TCP_new_connection(mrbc_value *send_data, mrbc_value *recv_data)
{
  tcp_connection_state *cs = (tcp_connection_state *)mrbc_raw_alloc(sizeof(tcp_connection_state));
  cs->state = NET_TCP_STATE_NONE;
  cs->pcb = altcp_new(NULL);
  altcp_recv(cs->pcb, TCP_recv_cb);
  altcp_sent(cs->pcb, TCP_sent_cb);
  altcp_err(cs->pcb, TCP_err_cb);
  altcp_poll(cs->pcb, TCP_poll_cb, 10);
  altcp_arg(cs->pcb, cs);
  cs->send_data = send_data;
  cs->recv_data = recv_data;
  return cs;
}

tcp_connection_state *TCP_connect_impl(ip_addr_t *ip, int port, mrbc_value *send_data, mrbc_value *recv_data)
{
  tcp_connection_state *cs = TCP_new_connection(send_data, recv_data);
  cyw43_arch_lwip_begin();
  altcp_connect(cs->pcb, ip, port, TCP_connected_cb);
  cyw43_arch_lwip_end();
  cs->state = NET_TCP_STATE_CONNECTION_STARTED;
  return cs;
}

int TCP_poll_impl(tcp_connection_state **pcs)
{
  if (*pcs == NULL)
    return 0;
  tcp_connection_state *cs = *pcs;
  switch(cs->state)
  {
    case NET_TCP_STATE_NONE:
    case NET_TCP_STATE_CONNECTION_STARTED:
    case NET_TCP_STATE_WAITING_PACKET:
      break;
    case NET_TCP_STATE_CONNECTED:
      cs->state = NET_TCP_STATE_WAITING_PACKET;
      cyw43_arch_lwip_begin();
      altcp_write(cs->pcb, cs->send_data->string->data, cs->send_data->string->size, 0);
      altcp_output(cs->pcb);
      cyw43_arch_lwip_end();
      break;
    case NET_TCP_STATE_PACKET_RECVED:
      cs->state = NET_TCP_STATE_WAITING_PACKET;
      break;
    case NET_TCP_STATE_FINISHED:
      cyw43_arch_lwip_begin();
      altcp_close(cs->pcb);
      cyw43_arch_lwip_end();
      free(cs);
      *pcs = NULL;
      return 0;
  }
  return cs->state;
}

mrbc_value TCP_send(ip_addr_t *ip, int port, mrbc_vm *vm, mrbc_value *send_data)
{
  mrbc_value recv_data = mrbc_string_new(vm, NULL, 0);
  tcp_connection_state *cs = TCP_connect_impl(ip, port, send_data, &recv_data);
  while(TCP_poll_impl(&cs))
  {
    sleep_ms(200);
  }
  // recv_data is ready after connection is complete
  return recv_data;
}
