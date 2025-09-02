#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "netif/ethernet.h"
#include "lwip/etharp.h"

#include "../../include/net.h"

static struct netif netif;

static err_t
dummy_output(struct netif *netif, struct pbuf *p)
{
  LWIP_UNUSED_ARG(netif);
  LWIP_UNUSED_ARG(p);
  return ERR_OK;
}

static err_t
tapif_init(struct netif *netif)
{
  netif->hwaddr_len = 6;
  netif->hwaddr[0] = 0x02;
  netif->hwaddr[1] = 0x00;
  netif->hwaddr[2] = 0x00;
  netif->hwaddr[3] = 0x00;
  netif->hwaddr[4] = 0x00;
  netif->hwaddr[5] = 0x01;
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  netif->output = etharp_output;
  netif->linkoutput = dummy_output;
  return ERR_OK;
}

void
Net_sleep_ms(int ms)
{
  usleep(ms * 1000);
}

void
lwip_begin(void)
{
  lwip_init();
  ip4_addr_t ipaddr, netmask, gw;
  IP4_ADDR(&ipaddr, 192, 168, 1, 100);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 192, 168, 1, 1);
  netif_add(&netif, &ipaddr, &netmask, &gw, NULL, tapif_init, ethernet_input);
  netif_set_default(&netif);
  netif_set_up(&netif);
  netif_set_link_up(&netif);

    ip4_addr_t dns_server;
    IP4_ADDR(&dns_server, 8, 8, 8, 8);  // Google DNS
    dns_setserver(0, &dns_server);
}

void
lwip_end(void)
{
  // no-op
}

int
mbedtls_hardware_poll(void *data __attribute__((unused)), unsigned char *output, size_t len, size_t *olen)
{
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    return -1;  // Error opening /dev/urandom
  }

  ssize_t bytes_read = read(fd, output, len);
  if (bytes_read < 0) {
    close(fd);
    return -1;  // Error reading from /dev/urandom
  }

  close(fd);

  // Set the number of bytes written to the output
  if (olen != NULL) {
    *olen = bytes_read;
  }

  return 0;  // Success
}
