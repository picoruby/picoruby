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
  mrbc_value *send_data;
  mrbc_value *recv_data;
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
  lwip_begin();
  altcp_arg(cs->pcb, NULL);
  altcp_recv(cs->pcb, NULL);
  altcp_sent(cs->pcb, NULL);
  altcp_err(cs->pcb, NULL);
  altcp_poll(cs->pcb, NULL, 0);
  err = altcp_close(cs->pcb);
  if (err != ERR_OK) {
    console_printf("altcp_close failed: %d\n", err);
    altcp_abort(cs->pcb);
    err = ERR_ABRT;
  }
  lwip_end();
  TCPClient_free_tls_config(cs);
  mrbc_raw_free(cs);
  return err;
}

static err_t
TCPClient_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  if (err != ERR_OK) {
    console_printf("TCPClient_recv_cb: err=%d\n", err);
    cs->state = NET_TCP_STATE_ERROR;
    return TCPClient_close(cs);
  }
  if (pbuf != NULL) {
    char *tmpbuf = mrbc_raw_alloc(pbuf->tot_len + 1);
    struct pbuf *current_pbuf = pbuf;
    int offset = 0;
    while (current_pbuf != NULL) {
      pbuf_copy_partial(current_pbuf, tmpbuf + offset, current_pbuf->len, 0);
      offset += current_pbuf->len;
      current_pbuf = current_pbuf->next;
    }
    tmpbuf[pbuf->tot_len] = '\0';
    mrbc_string_append_cbuf(cs->recv_data, tmpbuf, pbuf->tot_len);
    mrbc_raw_free(tmpbuf);
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
  if (err != ERR_OK) {
    console_printf("TCPClient_connected_cb: err=%d\n", err);
    return TCPClient_close((tcp_connection_state *)arg);
  }
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  cs->state = NET_TCP_STATE_CONNECTED;
  return ERR_OK;
}

static err_t
TCPClient_poll_cb(void *arg, struct altcp_pcb *pcb)
{
  console_printf("TCPClient_poll_cb (timeout)\n");
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  cs->state = NET_TCP_STATE_TIMEOUT;
  return TCPClient_close(cs);
}

static void
TCPClient_err_cb(void *arg, err_t err)
{
  tcp_connection_state *cs = (tcp_connection_state *)arg;
  console_printf("Error with: %d\n", err);
  cs->state = NET_TCP_STATE_ERROR;
}

static tcp_connection_state *
TCPClient_new_connection(mrbc_value *send_data, mrbc_value *recv_data)
{
  tcp_connection_state *cs = (tcp_connection_state *)mrbc_raw_alloc(sizeof(tcp_connection_state));
  cs->tls_config = NULL;
  cs->state = NET_TCP_STATE_NONE;
  cs->pcb = altcp_new(NULL);
  altcp_recv(cs->pcb, TCPClient_recv_cb);
  altcp_sent(cs->pcb, TCPClient_sent_cb);
  altcp_err(cs->pcb, TCPClient_err_cb);
  altcp_poll(cs->pcb, TCPClient_poll_cb, 10);
  altcp_arg(cs->pcb, cs);
  cs->send_data = send_data;
  cs->recv_data = recv_data;
  return cs;
}

static tcp_connection_state *
TCPClient_new_tls_connection(const char *host, mrbc_value *send_data, mrbc_value *recv_data)
{
  tcp_connection_state *cs = (tcp_connection_state *)mrbc_raw_alloc(sizeof(tcp_connection_state));
  cs->state = NET_TCP_STATE_NONE;

  struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
  cs->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
  if (!cs->pcb) {
    console_printf("altcp_tls_new failed\n");
    mrbc_raw_free(cs);
    return NULL;
  }
  cs->tls_config = tls_config;
  mbedtls_ssl_set_hostname(altcp_tls_context(cs->pcb), host);
  altcp_recv(cs->pcb, TCPClient_recv_cb);
  altcp_sent(cs->pcb, TCPClient_sent_cb);
  altcp_err(cs->pcb, TCPClient_err_cb);
  altcp_poll(cs->pcb, TCPClient_poll_cb, 10);
  altcp_arg(cs->pcb, cs);
  cs->send_data = send_data;
  cs->recv_data = recv_data;
  return cs;
}

static tcp_connection_state *
TCPClient_connect_impl(ip_addr_t *ip, const char *host, int port, mrbc_value *send_data, mrbc_value *recv_data, bool is_tls)
{
  err_t err;
  tcp_connection_state *cs;
  if (is_tls) {
    cs = TCPClient_new_tls_connection(host, send_data, recv_data);
  } else {
    cs = TCPClient_new_connection(send_data, recv_data);
  }
  if (cs) {
    lwip_begin();
    err = altcp_connect(cs->pcb, ip, port, TCPClient_connected_cb);
    if (err != ERR_OK) {
      console_printf("altcp_connect failed: %d\n", err);
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
      err = altcp_write(cs->pcb, cs->send_data->string->data, cs->send_data->string->size, 0);
      if (err != ERR_OK) {
        console_printf("altcp_write failed: %d\n", err);
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
      TCPClient_close(cs);
      *pcs = NULL;
      return 0;
    case NET_TCP_STATE_ERROR:
      TCPClient_close(cs);
      return 0;
    case NET_TCP_STATE_TIMEOUT:
      return 0;
  }
  return cs->state;
}

mrbc_value
TCPClient_send(const char *host, int port, mrbc_value *send_data, bool is_tls)
{
  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  mrbc_value ret;
  Net_get_ip(host, &ip);
  if(!ip4_addr_isloopback(&ip)) {
    char ip_str[16];
    ipaddr_ntoa_r(&ip, ip_str, 16);
    mrbc_value recv_data = mrbc_string_new(NULL, NULL, 0);
    tcp_connection_state *cs = TCPClient_connect_impl(&ip, host, port, send_data, &recv_data, is_tls);
    if (!cs) {
      ret = mrbc_nil_value();
    } else {
      while(TCPClient_poll_impl(&cs))
      {
        sleep_ms(200);
      }
      // recv_data is ready after connection is complete
      ret = recv_data;
    }
  } else {
    ret = mrbc_nil_value();
  }
  return ret;
}
