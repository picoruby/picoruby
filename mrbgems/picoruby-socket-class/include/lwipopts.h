#ifndef _LWIPOPTS_EXAMPLE_COMMONH_H
#define _LWIPOPTS_EXAMPLE_COMMONH_H

// Common settings used in most of the pico_w examples
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html for details)

// allow override in some examples
#ifndef NO_SYS
#define NO_SYS 1
#endif

// allow override in some examples
#ifndef LWIP_SOCKET
#define LWIP_SOCKET 0
#endif

#if PICO_CYW43_ARCH_POLL
#define MEM_LIBC_MALLOC 1
#else
// MEM_LIBC_MALLOC is incompatible with non polling versions
#define MEM_LIBC_MALLOC 0
#endif

#if defined(PICORB_VM_MRUBYC) && !defined(LWIP_PLATFORM_DIAG)
# include "mrubyc.h"
# define LWIP_PLATFORM_DIAG(x) do { console_printf x; } while(0)
#endif

#if defined(PICORB_VM_MRUBY) && !defined(LWIP_PLATFORM_DIAG)
# include "mruby.h"
# define LWIP_PLATFORM_DIAG(x) do { mrb_p(mrb, x); } while(0)
#endif

// You can show stats by `stats_display();`
#define LWIP_STATS_DISPLAY 1
#if !defined(S16_F)
#define S16_F "d"
#endif
#if !defined(U16_F)
#define U16_F "u"
#endif
#if !defined(U32_F)
#define U32_F "ul"
#endif
#if !defined(S32_F)
#define S32_F "ld"
#endif
#if !defined(X16_F)
#define X16_F "04x"
#endif
#if !defined(X32_F)
#define X32_F "08x"
#endif

#define MEM_ALIGNMENT 4
#define MEM_SIZE 4000
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 24
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define TCP_WND 16384
#define TCP_MSS 1460
#define TCP_SND_BUF (8 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETCONN 0
#define MEM_STATS 1
#define SYS_STATS 0
#define MEMP_STATS 0
#define LINK_STATS 0
#define LWIP_CHKSUM_ALGORITHM 3
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_DNS 1
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

#define LWIP_ALTCP 1
#define LWIP_ALTCP_TLS 1
#define LWIP_ALTCP_TLS_MBEDTLS 1

#ifndef LWIP_DEBUG
#define LWIP_DEBUG 1
#endif

#endif /* __LWIPOPTS_H__ */
