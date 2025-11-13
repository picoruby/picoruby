/*
 * TCP Server implementation for rp2040 using LwIP
 */

#include "../../include/socket.h"
#include "picoruby.h"
#include <string.h>

/* LwIP includes */
#define PICORB_NO_LWIP_HELPERS
#include "lwip/altcp.h"

/* TCP Server structure */
struct picorb_tcp_server {
  struct altcp_pcb *listen_pcb;
  struct altcp_pcb *accepted_pcb;
  int port;
  int state;
};

/* Accept callback */
static err_t
tcp_accept_callback(void *arg, struct altcp_pcb *newpcb, err_t err)
{
  picorb_tcp_server_t *server = (picorb_tcp_server_t *)arg;
  if (!server || err != ERR_OK) {
    if (newpcb) altcp_abort(newpcb);
    return ERR_ABRT;
  }

  /* Store accepted connection */
  server->accepted_pcb = newpcb;
  server->state = 1; /* has connection */

  return ERR_OK;
}

/* Create TCP server */
picorb_tcp_server_t*
TCPServer_create(int port, int backlog)
{
  if (port <= 0 || port > 65535) {
    return NULL;
  }

  picorb_tcp_server_t *server = (picorb_tcp_server_t *)picorb_alloc(NULL, sizeof(picorb_tcp_server_t));
  if (!server) {
    return NULL;
  }

  memset(server, 0, sizeof(picorb_tcp_server_t));
  server->port = port;

  lwip_begin();
  server->listen_pcb = altcp_new(NULL);
  if (!server->listen_pcb) {
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  err_t err = altcp_bind(server->listen_pcb, IP_ADDR_ANY, port);
  if (err != ERR_OK) {
    altcp_close(server->listen_pcb);
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  server->listen_pcb = altcp_listen(server->listen_pcb);
  if (!server->listen_pcb) {
    lwip_end();
    picorb_free(NULL, server);
    return NULL;
  }

  altcp_arg(server->listen_pcb, server);
  altcp_accept(server->listen_pcb, tcp_accept_callback);
  lwip_end();

  return server;
}

/* Accept connection */
picorb_socket_t*
TCPServer_accept(picorb_tcp_server_t *server)
{
  if (!server) {
    return NULL;
  }

  /* Wait for connection */
  int max_wait = 1000; /* 100 seconds */
  while (!server->accepted_pcb && max_wait-- > 0) {
    Net_sleep_ms(100);
  }

  if (!server->accepted_pcb) {
    return NULL; /* Timeout */
  }

  /* Create socket for accepted connection */
  picorb_socket_t *sock = (picorb_socket_t *)picorb_alloc(NULL, sizeof(picorb_socket_t));
  if (!sock) {
    lwip_begin();
    altcp_abort(server->accepted_pcb);
    lwip_end();
    server->accepted_pcb = NULL;
    return NULL;
  }

  memset(sock, 0, sizeof(picorb_socket_t));
  sock->pcb = server->accepted_pcb;
  sock->state = SOCKET_STATE_CONNECTED;
  sock->socktype = 1; /* SOCK_STREAM */

  server->accepted_pcb = NULL;
  server->state = 0;

  return sock;
}

/* Close server */
bool
TCPServer_close(picorb_tcp_server_t *server)
{
  if (!server) {
    return false;
  }

  if (server->listen_pcb) {
    lwip_begin();
    altcp_close(server->listen_pcb);
    lwip_end();
  }

  picorb_free(NULL, server);
  return true;
}
