#ifndef PICORB_SOCKET_H
#define PICORB_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* Platform detection */
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  #define PICORB_PLATFORM_POSIX 1
#endif

/* Socket structure for POSIX */
#ifdef PICORB_PLATFORM_POSIX
typedef struct {
  int fd;                    /* File descriptor (-1 = invalid) */
  int family;                /* AF_INET, AF_INET6 */
  int socktype;              /* SOCK_STREAM, SOCK_DGRAM */
  int protocol;              /* IPPROTO_TCP, IPPROTO_UDP */
  bool connected;
  bool closed;
  char remote_host[256];
  int remote_port;
} picorb_socket_t;
#endif

/* Socket structure for LwIP (to be implemented in Phase 6) */
#ifndef PICORB_PLATFORM_POSIX
struct altcp_pcb;  /* Forward declaration */

typedef struct {
  struct altcp_pcb *pcb;     /* LwIP control block */
  char *recv_buf;
  size_t recv_len;
  size_t recv_capacity;
  enum {
    SOCKET_STATE_NONE,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSED
  } state;
  char remote_host[256];
  int remote_port;
} picorb_socket_t;
#endif

/* TCP Socket API */
bool TCPSocket_create(picorb_socket_t *sock);
bool TCPSocket_connect(picorb_socket_t *sock, const char *host, int port);
ssize_t TCPSocket_send(picorb_socket_t *sock, const void *data, size_t len);
ssize_t TCPSocket_recv(picorb_socket_t *sock, void *buf, size_t len);
bool TCPSocket_close(picorb_socket_t *sock);

/* Get socket info */
const char* TCPSocket_remote_host(picorb_socket_t *sock);
int TCPSocket_remote_port(picorb_socket_t *sock);
bool TCPSocket_closed(picorb_socket_t *sock);

/* UDP Socket API */
bool UDPSocket_create(picorb_socket_t *sock);
bool UDPSocket_bind(picorb_socket_t *sock, const char *host, int port);
bool UDPSocket_connect(picorb_socket_t *sock, const char *host, int port);
ssize_t UDPSocket_send(picorb_socket_t *sock, const void *data, size_t len);
ssize_t UDPSocket_sendto(picorb_socket_t *sock, const void *data, size_t len,
                          const char *host, int port);
ssize_t UDPSocket_recvfrom(picorb_socket_t *sock, void *buf, size_t len,
                            char *host, size_t host_len, int *port);
bool UDPSocket_close(picorb_socket_t *sock);
bool UDPSocket_closed(picorb_socket_t *sock);

/* TCP Server API (to be implemented in Phase 3) */
typedef struct picorb_tcp_server picorb_tcp_server_t;

picorb_tcp_server_t* TCPServer_create(int port, int backlog);
picorb_socket_t* TCPServer_accept(picorb_tcp_server_t *server);
bool TCPServer_close(picorb_tcp_server_t *server);

/* SSL Socket API (Phase 5) */
typedef struct picorb_ssl_socket picorb_ssl_socket_t;

picorb_ssl_socket_t* SSLSocket_create(picorb_socket_t *tcp_socket, const char *hostname);
bool SSLSocket_init(picorb_ssl_socket_t *ssl_sock);
ssize_t SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len);
ssize_t SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len);
bool SSLSocket_close(picorb_ssl_socket_t *ssl_sock);
bool SSLSocket_closed(picorb_ssl_socket_t *ssl_sock);
const char* SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock);
int SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock);

/* Address resolution */
bool resolve_address(const char *host, char *ip, size_t ip_len);

#endif /* PICORB_SOCKET_H */
