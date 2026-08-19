/*
 * Minimal DHCP server for PicoRuby CYW43 AP mode.
 *
 * Adapted from TinyUSB's networking helpers.
 * Copyright (c) 2015 Sergey Fetisov <fsenok@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "../../include/cyw43.h"
#include "ap_dhcp_server.h"

#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include <stddef.h>
#include <string.h>

#define AP_DHCP_SERVER_PORT 67
#define AP_DHCP_CLIENT_PORT 68
#define AP_DHCP_MAX_LEASES 4
#define AP_DHCP_FIRST_HOST 2
#define AP_DHCP_OFFER_HOLD_SEC 60
#define AP_DHCP_LEASE_TIME_SEC (24 * 60 * 60)

enum ap_dhcp_message_type {
  AP_DHCP_DISCOVER = 1,
  AP_DHCP_OFFER = 2,
  AP_DHCP_REQUEST = 3,
  AP_DHCP_ACK = 5,
  AP_DHCP_RELEASE = 7,
};

enum ap_dhcp_option {
  AP_DHCP_OPT_PAD = 0,
  AP_DHCP_OPT_SUBNET_MASK = 1,
  AP_DHCP_OPT_ROUTER = 3,
  AP_DHCP_OPT_IP_ADDRESS = 50,
  AP_DHCP_OPT_LEASE_TIME = 51,
  AP_DHCP_OPT_MESSAGE_TYPE = 53,
  AP_DHCP_OPT_SERVER_ID = 54,
  AP_DHCP_OPT_END = 255,
};

typedef struct {
  uint8_t mac[6];
  ip4_addr_t addr;
  uint32_t lease_sec;
  uint64_t expires_at_ms;
} ap_dhcp_lease_t;

typedef struct {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint8_t ciaddr[4];
  uint8_t yiaddr[4];
  uint8_t siaddr[4];
  uint8_t giaddr[4];
  uint8_t chaddr[16];
  uint8_t legacy[192];
  uint8_t magic[4];
  uint8_t options[275];
} ap_dhcp_packet_t;

static struct udp_pcb *s_dhcp_pcb = NULL;
static ap_dhcp_lease_t s_leases[AP_DHCP_MAX_LEASES];
static ip4_addr_t s_router;

static const uint8_t s_magic_cookie[4] = {0x63, 0x82, 0x53, 0x63};

static uint64_t
ap_dhcp_now_ms(void)
{
  return to_us_since_boot(get_absolute_time()) / 1000;
}

static void
ap_dhcp_set_ip(uint8_t *dest, ip4_addr_t addr)
{
  memcpy(dest, &addr.addr, sizeof(addr.addr));
}

static ip4_addr_t
ap_dhcp_get_ip(const uint8_t *src)
{
  ip4_addr_t addr;
  memcpy(&addr.addr, src, sizeof(addr.addr));
  return addr;
}

static void
ap_dhcp_release_lease(ap_dhcp_lease_t *lease)
{
  memset(lease->mac, 0, sizeof(lease->mac));
  lease->expires_at_ms = 0;
}

static bool
ap_dhcp_lease_vacant(ap_dhcp_lease_t *lease)
{
  static const uint8_t zero_mac[6] = {0};

  if (memcmp(lease->mac, zero_mac, sizeof(zero_mac)) == 0) {
    return true;
  }
  if (lease->expires_at_ms <= ap_dhcp_now_ms()) {
    ap_dhcp_release_lease(lease);
    return true;
  }
  return false;
}

static ap_dhcp_lease_t *
ap_dhcp_find_lease_by_mac(const uint8_t *mac)
{
  for (size_t i = 0; i < AP_DHCP_MAX_LEASES; i++) {
    if (ap_dhcp_lease_vacant(&s_leases[i])) {
      continue;
    }
    if (memcmp(s_leases[i].mac, mac, 6) == 0) {
      return &s_leases[i];
    }
  }
  return NULL;
}

static ap_dhcp_lease_t *
ap_dhcp_find_lease_by_ip(ip4_addr_t ip)
{
  for (size_t i = 0; i < AP_DHCP_MAX_LEASES; i++) {
    if (s_leases[i].addr.addr == ip.addr) {
      return &s_leases[i];
    }
  }
  return NULL;
}

static ap_dhcp_lease_t *
ap_dhcp_find_vacant_lease(void)
{
  for (size_t i = 0; i < AP_DHCP_MAX_LEASES; i++) {
    if (ap_dhcp_lease_vacant(&s_leases[i])) {
      return &s_leases[i];
    }
  }
  return NULL;
}

static uint8_t *
ap_dhcp_find_option(uint8_t *options, size_t options_len, uint8_t code)
{
  size_t i = 0;

  while (i < options_len) {
    uint8_t opt = options[i];
    if (opt == AP_DHCP_OPT_END) {
      break;
    }
    if (opt == AP_DHCP_OPT_PAD) {
      i++;
      continue;
    }
    if (i + 1 >= options_len) {
      break;
    }

    uint8_t opt_len = options[i + 1];
    if (i + 2 + opt_len > options_len) {
      break;
    }
    if (opt == code) {
      return &options[i];
    }
    i += 2 + opt_len;
  }

  return NULL;
}

static size_t
ap_dhcp_fill_options(
  uint8_t *dest,
  uint8_t msg_type,
  uint32_t lease_sec,
  ip4_addr_t server_id,
  ip4_addr_t router,
  ip4_addr_t subnet_mask
)
{
  uint8_t *ptr = dest;

  *ptr++ = AP_DHCP_OPT_MESSAGE_TYPE;
  *ptr++ = 1;
  *ptr++ = msg_type;

  *ptr++ = AP_DHCP_OPT_SERVER_ID;
  *ptr++ = 4;
  ap_dhcp_set_ip(ptr, server_id);
  ptr += 4;

  *ptr++ = AP_DHCP_OPT_LEASE_TIME;
  *ptr++ = 4;
  *ptr++ = (lease_sec >> 24) & 0xff;
  *ptr++ = (lease_sec >> 16) & 0xff;
  *ptr++ = (lease_sec >> 8) & 0xff;
  *ptr++ = lease_sec & 0xff;

  *ptr++ = AP_DHCP_OPT_SUBNET_MASK;
  *ptr++ = 4;
  ap_dhcp_set_ip(ptr, subnet_mask);
  ptr += 4;

  *ptr++ = AP_DHCP_OPT_ROUTER;
  *ptr++ = 4;
  ap_dhcp_set_ip(ptr, router);
  ptr += 4;

  *ptr++ = AP_DHCP_OPT_END;

  return (size_t)(ptr - dest);
}

static err_t
ap_dhcp_send_reply(
  struct udp_pcb *pcb,
  struct netif *ap_netif,
  ap_dhcp_packet_t *packet,
  uint8_t msg_type,
  ap_dhcp_lease_t *lease
)
{
  size_t options_len;
  size_t payload_len;
  struct pbuf *pbuf;

  packet->op = 2; /* BOOTREPLY */
  packet->secs = 0;
  packet->flags = 0;
  memset(packet->ciaddr, 0, sizeof(packet->ciaddr));
  ap_dhcp_set_ip(packet->yiaddr, lease->addr);
  ap_dhcp_set_ip(packet->siaddr, *netif_ip4_addr(ap_netif));
  memcpy(packet->magic, s_magic_cookie, sizeof(s_magic_cookie));
  memset(packet->options, 0, sizeof(packet->options));

  options_len = ap_dhcp_fill_options(
    packet->options,
    msg_type,
    lease->lease_sec,
    *netif_ip4_addr(ap_netif),
    s_router,
    *netif_ip4_netmask(ap_netif)
  );

  payload_len = offsetof(ap_dhcp_packet_t, options) + options_len;
  pbuf = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
  if (!pbuf) {
    return ERR_MEM;
  }

  memcpy(pbuf->payload, packet, payload_len);
  err_t err = udp_sendto_if_src(
    pcb,
    pbuf,
    IP_ADDR_BROADCAST,
    AP_DHCP_CLIENT_PORT,
    ap_netif,
    netif_ip_addr4(ap_netif)
  );
  pbuf_free(pbuf);
  return err;
}

static void
ap_dhcp_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *addr, u16_t port)
{
  ap_dhcp_packet_t packet;
  ap_dhcp_lease_t *lease = NULL;
  struct netif *ap_netif;
  uint8_t *msg_type_opt;
  size_t packet_len;
  size_t options_len;
  const size_t options_offset = offsetof(ap_dhcp_packet_t, options);

  (void)addr;

  if (!pbuf || port != AP_DHCP_CLIENT_PORT || pbuf->tot_len < options_offset) {
    if (pbuf) {
      pbuf_free(pbuf);
    }
    return;
  }

  ap_netif = (struct netif *)arg;
  if (!ap_netif || ap_netif != &cyw43_state.netif[CYW43_ITF_AP]) {
    pbuf_free(pbuf);
    return;
  }

  memset(&packet, 0, sizeof(packet));
  packet_len = pbuf_copy_partial(pbuf, &packet, LWIP_MIN((u16_t)sizeof(packet), pbuf->tot_len), 0);
  if (packet_len < options_offset) {
    pbuf_free(pbuf);
    return;
  }
  options_len = packet_len - options_offset;

  if (packet.op != 1 || packet.htype != 1 || packet.hlen != 6 ||
      memcmp(packet.magic, s_magic_cookie, sizeof(s_magic_cookie)) != 0) {
    pbuf_free(pbuf);
    return;
  }

  msg_type_opt = ap_dhcp_find_option(packet.options, options_len, AP_DHCP_OPT_MESSAGE_TYPE);
  if (!msg_type_opt || msg_type_opt[1] != 1) {
    pbuf_free(pbuf);
    return;
  }

  switch (msg_type_opt[2]) {
    case AP_DHCP_DISCOVER:
      lease = ap_dhcp_find_lease_by_mac(packet.chaddr);
      if (!lease) {
        lease = ap_dhcp_find_vacant_lease();
        if (lease) {
          memcpy(lease->mac, packet.chaddr, 6);
          lease->expires_at_ms = ap_dhcp_now_ms() + (uint64_t)AP_DHCP_OFFER_HOLD_SEC * 1000;
        }
      }
      if (lease) {
        ap_dhcp_send_reply(pcb, ap_netif, &packet, AP_DHCP_OFFER, lease);
      }
      break;

    case AP_DHCP_REQUEST: {
      ap_dhcp_lease_t *current_lease = ap_dhcp_find_lease_by_mac(packet.chaddr);
      uint8_t *server_id_opt = ap_dhcp_find_option(packet.options, options_len, AP_DHCP_OPT_SERVER_ID);
      uint8_t *requested_ip_opt = ap_dhcp_find_option(packet.options, options_len, AP_DHCP_OPT_IP_ADDRESS);
      ip4_addr_t requested_ip = ap_dhcp_get_ip(packet.ciaddr);

      if (server_id_opt &&
          (server_id_opt[1] != 4 ||
           ap_dhcp_get_ip(server_id_opt + 2).addr != netif_ip4_addr(ap_netif)->addr)) {
        break;
      }
      if (requested_ip_opt) {
        if (requested_ip_opt[1] != 4) {
          break;
        }
        requested_ip = ap_dhcp_get_ip(requested_ip_opt + 2);
      }
      if (requested_ip.addr == 0) {
        break;
      }

      lease = ap_dhcp_find_lease_by_ip(requested_ip);
      if (!lease || (lease != current_lease && !ap_dhcp_lease_vacant(lease))) {
        break;
      }
      if (current_lease && current_lease != lease) {
        ap_dhcp_release_lease(current_lease);
      }
      memcpy(lease->mac, packet.chaddr, 6);
      lease->expires_at_ms = ap_dhcp_now_ms() + (uint64_t)lease->lease_sec * 1000;
      ap_dhcp_send_reply(pcb, ap_netif, &packet, AP_DHCP_ACK, lease);
      break;
    }

    case AP_DHCP_RELEASE:
      lease = ap_dhcp_find_lease_by_mac(packet.chaddr);
      if (lease) {
        ap_dhcp_release_lease(lease);
      }
      break;

    default:
      break;
  }

  pbuf_free(pbuf);
}

bool
picoruby_ap_dhcp_server_start(void)
{
  struct netif *ap_netif = &cyw43_state.netif[CYW43_ITF_AP];
  const ip4_addr_t *ap_ip;
  const ip4_addr_t *ap_mask;
  uint32_t network;

  lwip_begin();

  if ((cyw43_state.itf_state & (1 << CYW43_ITF_AP)) == 0) {
    lwip_end();
    return false;
  }
  ap_ip = netif_ip4_addr(ap_netif);
  ap_mask = netif_ip4_netmask(ap_netif);
  if (!ap_ip || !ap_mask || ap_ip->addr == 0 || ap_mask->addr == 0) {
    lwip_end();
    return false;
  }

  if (s_dhcp_pcb) {
    udp_remove(s_dhcp_pcb);
    s_dhcp_pcb = NULL;
  }

  memset(s_leases, 0, sizeof(s_leases));
  s_router = *ap_ip;
  network = lwip_ntohl(ap_ip->addr) & lwip_ntohl(ap_mask->addr);

  for (size_t i = 0; i < AP_DHCP_MAX_LEASES; i++) {
    s_leases[i].addr.addr = lwip_htonl(network | (AP_DHCP_FIRST_HOST + (uint32_t)i));
    s_leases[i].lease_sec = AP_DHCP_LEASE_TIME_SEC;
  }

  s_dhcp_pcb = udp_new();
  if (!s_dhcp_pcb) {
    lwip_end();
    return false;
  }

  ip_set_option(s_dhcp_pcb, SOF_BROADCAST);
  if (udp_bind(s_dhcp_pcb, IP_ADDR_ANY, AP_DHCP_SERVER_PORT) != ERR_OK) {
    udp_remove(s_dhcp_pcb);
    s_dhcp_pcb = NULL;
    lwip_end();
    return false;
  }

  udp_bind_netif(s_dhcp_pcb, ap_netif);
  udp_recv(s_dhcp_pcb, ap_dhcp_udp_recv, ap_netif);

  lwip_end();
  return true;
}

void
picoruby_ap_dhcp_server_stop(void)
{
  lwip_begin();
  if (s_dhcp_pcb) {
    udp_remove(s_dhcp_pcb);
    s_dhcp_pcb = NULL;
  }
  memset(s_leases, 0, sizeof(s_leases));
  lwip_end();
}
