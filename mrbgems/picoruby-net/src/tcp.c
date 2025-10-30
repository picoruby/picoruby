#include "../include/net.h"
#include "../include/mbedtls_debug.h"
#include "lwip/altcp_tls.h"

/* platform-dependent definitions */

/* state values for TCP connection */
#define NET_TCP_STATE_NONE               0
#define NET_TCP_STATE_CONNECTION_STARTED 1
#define NET_TCP_STATE_CONNECTED          2
#define NET_TCP_STATE_WAITING_PACKET     3
#define NET_TCP_STATE_PACKET_RECVED      4
#define NET_TCP_STATE_FINISHED           6
#define NET_TCP_STATE_ERROR              99
#define NET_TCP_STATE_TIMEOUT            100

/* TCP connection struct */
typedef struct tcp_connection_state_str
{
  int state;
  struct altcp_pcb *pcb;
  const char *send_data;
  size_t send_data_len;
  char *recv_data;
  size_t recv_data_len;
  mrb_state *mrb;
  struct altcp_tls_config *tls_config;
} tcp_connection_state;

/* end of platform-dependent definitions */

static void
TCPClient_free_tls_config(tcp_connection_state *cs)
{
  if (cs && cs->tls_config) {
    altcp_tls_free_config(cs->tls_config);
  }
}

static err_t
TCPClient_close(tcp_connection_state *cs)
{
  err_t err = ERR_OK;
  if (!cs || !cs->pcb) return ERR_ARG;
  mrb_state *mrb = cs->mrb;
  lwip_begin();
  altcp_arg(cs->pcb, NULL);
  altcp_recv(cs->pcb, NULL);
  altcp_sent(cs->pcb, NULL);
  altcp_err(cs->pcb, NULL);
  err = altcp_close(cs->pcb);
  if (err != ERR_OK) {
    picorb_warn("altcp_close failed: %d\n", err);
    altcp_abort(cs->pcb);
    cs->pcb = NULL;
    err = ERR_ABRT;
  }
  lwip_end();
  TCPClient_free_tls_config(cs);
  picorb_free(mrb, cs);
  return err;
}

static err_t
TCPClient_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  mrb_state *mrb = cs->mrb;
  if (err != ERR_OK) {
    picorb_warn("TCPClient_recv_cb: err=%d\n", err);
    cs->state = NET_TCP_STATE_ERROR;
    pbuf_free(pbuf);
    return TCPClient_close(cs);
  }
  if (pbuf != NULL) {
    char *tmpbuf = picorb_alloc(mrb, pbuf->tot_len + 1);
    if (tmpbuf == NULL) {
      picorb_warn("TCPClient_recv_cb: out of memory (tmpbuf allocation failed)\n");
      cs->state = NET_TCP_STATE_ERROR;
      pbuf_free(pbuf);
      return ERR_MEM;
    }
    struct pbuf *current_pbuf = pbuf;
    int offset = 0;
    while (current_pbuf != NULL) {
      pbuf_copy_partial(current_pbuf, tmpbuf + offset, current_pbuf->len, 0);
      offset += current_pbuf->len;
      current_pbuf = current_pbuf->next;
    }
    tmpbuf[pbuf->tot_len] = '\0';
    assert(cs->recv_data);
    char *new_recv_data = (char *)picorb_realloc(mrb, cs->recv_data, cs->recv_data_len + pbuf->tot_len + 1);
    if (new_recv_data == NULL) {
      picorb_warn("TCPClient_recv_cb: out of memory (recv_data reallocation failed)\n");
      picorb_free(mrb, tmpbuf);
      pbuf_free(pbuf);
      cs->state = NET_TCP_STATE_ERROR;
      return ERR_MEM;
    }
    cs->recv_data = new_recv_data;
    memcpy(cs->recv_data + cs->recv_data_len, tmpbuf, pbuf->tot_len);
    cs->recv_data_len += pbuf->tot_len;
    picorb_free(mrb, tmpbuf);
    altcp_recved(pcb, pbuf->tot_len);
    cs->state = NET_TCP_STATE_PACKET_RECVED;
    pbuf_free(pbuf);
  } else {
    cs->state = NET_TCP_STATE_FINISHED;
  }
  return ERR_OK;
}

static err_t
TCPClient_sent_cb(void *arg, struct altcp_pcb *pcb, u16_t len)
{
  // do nothing as of now
  return ERR_OK;
}

static err_t
TCPClient_connected_cb(void *arg, struct altcp_pcb *pcb, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  MRB;
  if (err != ERR_OK) {
    picorb_warn("TCPClient_connected_cb: err=%d\n", err);
    return TCPClient_close(cs);
  }
  cs->state = NET_TCP_STATE_CONNECTED;
  return ERR_OK;
}

static err_t
TCPClient_poll_cb(void *arg, struct altcp_pcb *pcb)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  MRB;
  picorb_warn("TCPClient_poll_cb (timeout)\n");
  cs->state = NET_TCP_STATE_TIMEOUT;
  return ERR_OK;
}

static void
TCPClient_err_cb(void *arg, err_t err)
{
  if (!arg) return;
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  MRB;
  picorb_warn("Error with: %d\n", err);
  cs->state = NET_TCP_STATE_ERROR;
}

static void
TCPClient_setup_callbacks(struct altcp_pcb *pcb, tcp_connection_state *cs, u8_t poll_interval)
{
  altcp_recv(pcb, TCPClient_recv_cb);
  altcp_sent(pcb, TCPClient_sent_cb);
  altcp_err(pcb, TCPClient_err_cb);
  altcp_poll(pcb, TCPClient_poll_cb, poll_interval);
  altcp_arg(pcb, cs);
}

static void
TCPClient_init_state(tcp_connection_state *cs, mrb_state *mrb,
                     const net_request_t *req, net_response_t *res)
{
  cs->state = NET_TCP_STATE_NONE;
  cs->send_data = req->send_data;
  cs->send_data_len = req->send_data_len;
  cs->recv_data = res->recv_data;
  cs->recv_data_len = res->recv_data_len;
  cs->mrb = mrb;
}

static tcp_connection_state *
TCPClient_new_connection(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  tcp_connection_state *cs = (tcp_connection_state *)picorb_alloc(mrb, sizeof(tcp_connection_state));
  cs->tls_config = NULL;
  cs->pcb = altcp_new(NULL);
  TCPClient_setup_callbacks(cs->pcb, cs, 30);
  TCPClient_init_state(cs, mrb, req, res);
  return cs;
}

static tcp_connection_state *
TCPClient_new_tls_connection(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  tcp_connection_state *cs = (tcp_connection_state *)picorb_alloc(mrb, sizeof(tcp_connection_state));

  struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
  cs->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
  if (!cs->pcb) {
    picorb_warn("altcp_tls_new failed\n");
    altcp_tls_free_config(tls_config);
    picorb_free(mrb, cs);
    return NULL;
  }
  cs->tls_config = tls_config;
  mbedtls_ssl_set_hostname(altcp_tls_context(cs->pcb), req->host);
  TCPClient_setup_callbacks(cs->pcb, cs, 10);
  TCPClient_init_state(cs, mrb, req, res);
  return cs;
}

static tcp_connection_state *
TCPClient_connect_impl(mrb_state *mrb, ip_addr_t *ip, const net_request_t *req, net_response_t *res)
{
  err_t err;
  tcp_connection_state *cs;
  if (req->is_tls) {
    cs = TCPClient_new_tls_connection(mrb, req, res);
  } else {
    cs = TCPClient_new_connection(mrb, req, res);
  }
  if (cs) {
    lwip_begin();
    err = altcp_connect(cs->pcb, ip, req->port, TCPClient_connected_cb);
    if (err != ERR_OK) {
      picorb_warn("altcp_connect failed: %d\n", err);
      cs->state = NET_TCP_STATE_ERROR;
      lwip_end();
      return cs;
    }
    lwip_end();
    cs->state = NET_TCP_STATE_CONNECTION_STARTED;
  }
  return cs;
}

static int
TCPClient_poll_impl(tcp_connection_state **pcs)
{
  err_t err;
  if (*pcs == NULL)
    return 0;
  tcp_connection_state *cs = *pcs;
  switch(cs->state)
  {
    case NET_TCP_STATE_NONE:
      break;
    case NET_TCP_STATE_CONNECTION_STARTED:
      break;
    case NET_TCP_STATE_WAITING_PACKET:
      break;
    case NET_TCP_STATE_CONNECTED:
      cs->state = NET_TCP_STATE_WAITING_PACKET;
      lwip_begin();
      err = altcp_write(cs->pcb, cs->send_data, cs->send_data_len, 0);
      if (err != ERR_OK) {
        MRB;
        picorb_warn("altcp_write failed: %d\n", err);
        cs->state = NET_TCP_STATE_ERROR;
        return 1;
      }
      altcp_output(cs->pcb);
      lwip_end();
      break;
    case NET_TCP_STATE_PACKET_RECVED:
      cs->state = NET_TCP_STATE_WAITING_PACKET;
      break;
    case NET_TCP_STATE_FINISHED:
    case NET_TCP_STATE_ERROR:
    case NET_TCP_STATE_TIMEOUT:
      return 0;
  }
  return cs->state;
}

bool
TCPClient_send(mrb_state *mrb, const net_request_t *req, net_response_t *res)
{
  bool ret = false;
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  Net_get_ip(req->host, &ip);
  if (!ip4_addr_isloopback(&ip)) {
    tcp_connection_state *cs = TCPClient_connect_impl(mrb, &ip, req, res);
    if (cs) {
      int max_wait = 200;
      while (TCPClient_poll_impl(&cs) && 0 < max_wait--) {
        // res->recv_data is ready after connection is complete
        Net_sleep_ms(100);
      }
      if (max_wait <= 0) {
        picorb_warn("TCPClient_send: timeout\n");
      } else {
        res->recv_data = cs->recv_data;
        res->recv_data_len = cs->recv_data_len;
      }
      if (cs->state == NET_TCP_STATE_FINISHED) {
        ret = true;
      }
      TCPClient_close(cs);
    }
  }
  return ret;
}

