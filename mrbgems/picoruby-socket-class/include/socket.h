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

/* Socket structure for LwIP (rp2040 and other microcontrollers) */
#ifndef PICORB_PLATFORM_POSIX

/* Forward declarations for LwIP types */
struct altcp_pcb;
struct ip_addr;

/* Socket states */
#define SOCKET_STATE_NONE        0
#define SOCKET_STATE_CONNECTING  1
#define SOCKET_STATE_CONNECTED   2
#define SOCKET_STATE_CLOSING     3
#define SOCKET_STATE_CLOSED      4
#define SOCKET_STATE_ERROR       99

typedef struct {
  struct altcp_pcb *pcb;     /* LwIP control block */
  char *recv_buf;            /* Receive buffer */
  size_t recv_len;           /* Current data in buffer */
  size_t recv_capacity;      /* Buffer capacity */
  int state;                 /* Connection state */
  char remote_host[256];     /* Remote hostname */
  int remote_port;           /* Remote port */
  int socktype;              /* SOCK_STREAM or SOCK_DGRAM */
  bool connected;
  bool closed;
} picorb_socket_t;

/* LwIP helper functions - implemented in ports/rp2040/ */
#ifndef PICORB_NO_LWIP_HELPERS
extern void lwip_begin(void);
extern void lwip_end(void);
extern void Net_sleep_ms(int ms);
/* Note: ip parameter type depends on LwIP configuration (ip_addr_t in implementation) */
extern int Net_get_ip(const char *name, void *ip);
#endif

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

/* SSL Context API */
typedef struct picorb_ssl_context picorb_ssl_context_t;

#define SSL_VERIFY_NONE 0
#define SSL_VERIFY_PEER 1

picorb_ssl_context_t* SSLContext_create(void);
bool SSLContext_set_ca_file(picorb_ssl_context_t *ctx, const char *ca_file);
bool SSLContext_set_verify_mode(picorb_ssl_context_t *ctx, int mode);
int SSLContext_get_verify_mode(picorb_ssl_context_t *ctx);
void SSLContext_free(picorb_ssl_context_t *ctx);

/* SSL Socket API */
typedef struct picorb_ssl_socket picorb_ssl_socket_t;

picorb_ssl_socket_t* SSLSocket_create(picorb_socket_t *tcp_socket, picorb_ssl_context_t *ssl_ctx);
bool SSLSocket_set_hostname(picorb_ssl_socket_t *ssl_sock, const char *hostname);
bool SSLSocket_connect(picorb_ssl_socket_t *ssl_sock);
ssize_t SSLSocket_send(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len);
ssize_t SSLSocket_recv(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len);
bool SSLSocket_close(picorb_ssl_socket_t *ssl_sock);
bool SSLSocket_closed(picorb_ssl_socket_t *ssl_sock);
const char* SSLSocket_remote_host(picorb_ssl_socket_t *ssl_sock);
int SSLSocket_remote_port(picorb_ssl_socket_t *ssl_sock);

/* Address resolution */
bool resolve_address(const char *host, char *ip, size_t ip_len);

#endif /* PICORB_SOCKET_H */
