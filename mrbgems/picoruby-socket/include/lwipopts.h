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
# include "picoruby/debug.h"
# define LWIP_PLATFORM_DIAG(x) do { debug_printf x; } while(0)
#endif

// デバッグ設定を最優先で定義
#define LWIP_DEBUG 1
#define LWIP_DBG_MIN_LEVEL     LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON      LWIP_DBG_ON

// TCP層の詳細デバッグ
#define TCP_DEBUG               LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG        LWIP_DBG_ON
#define TCP_INPUT_DEBUG         LWIP_DBG_ON

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
#define SYS_STATS 1
#define MEMP_STATS 1
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

// Support MQTT
#define LWIP_MQTT 1
#define MQTT_REQ_MAX_IN_FLIGHT 5
// MQTT requires additional timer for keep-alive
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)

// Enable MQTT debugging
#define MQTT_DEBUG LWIP_DBG_ON

// Enable CYW43 debugging to see flow control issues
#define CYW43_DEBUG 1

// Enable ALTCP debug to see connection issues
#define ALTCP_DEBUG LWIP_DBG_ON

// Enable TCP input debugging to see packet reception
#define TCP_INPUT_DEBUG LWIP_DBG_ON

// Enable timer debugging to see timeout processing
#define TIMERS_DEBUG LWIP_DBG_ON

#endif /* __LWIPOPTS_H__ */
