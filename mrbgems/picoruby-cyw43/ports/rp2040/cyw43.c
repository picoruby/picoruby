#include "../../include/cyw43.h"

#include "pico/cyw43_arch.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"

int
CYW43_CONST_link_down(void)
{
  return CYW43_LINK_DOWN;
}

int
CYW43_CONST_link_join(void)
{
  return CYW43_LINK_JOIN;
}

int
CYW43_CONST_link_noip(void)
{
  return CYW43_LINK_NOIP;
}

int
CYW43_CONST_link_up(void)
{
  return CYW43_LINK_UP;
}

int
CYW43_CONST_link_fail(void)
{
  return CYW43_LINK_FAIL;
}

int
CYW43_CONST_link_nonet(void)
{
  return CYW43_LINK_NONET;
}

int
CYW43_CONST_link_badauth(void)
{
  return CYW43_LINK_BADAUTH;
}

int
CYW43_tcpip_link_status(void)
{
  return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
}

bool
CYW43_dhcp_supplied(void)
{
  struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
  return dhcp_supplied_address(netif);
}

int
CYW43_arch_init_with_country(const uint8_t *country)
{
  int res;
  if (country == NULL) {
    res = cyw43_arch_init();
  } else {
    res = cyw43_arch_init_with_country(CYW43_COUNTRY(country[0], country[1], 0));
  }
  if (res != 0) {
    return res; // error
  }
  // Confirm whether the LED connecting to CYW43 works
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  sleep_ms(5);
  if (cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN) == 1) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    return 0;
  }
  return -1; // Maybe No CYW43 module on the board
}

void
CYW43_arch_enable_sta_mode(void)
{
  cyw43_arch_enable_sta_mode();
}

void
CYW43_arch_disable_sta_mode(void)
{
  cyw43_arch_disable_sta_mode();
}

int
CYW43_wifi_connect_with_dhcp(const char *ssid, const char *pw, uint32_t auth, uint32_t timeout_ms)
{
  int result = cyw43_arch_wifi_connect_timeout_ms(ssid, pw, auth, timeout_ms);
  if (result == 0) {
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    if (!dhcp_supplied_address(netif)) {
      dhcp_set_struct(netif, &cyw43_state.dhcp_client);
      dhcp_start(netif);
    }
  }
  return result;
}

int
CYW43_wifi_disconnect(void)
{
  struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
  dhcp_release_and_stop(netif);
  int result = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  return result;
}

void
CYW43_GPIO_write(uint8_t pin, uint8_t val)
{
  cyw43_arch_gpio_put(pin, val == 1);
}

uint8_t
CYW43_GPIO_read(uint8_t pin)
{
  return(cyw43_arch_gpio_get(pin) ? 1 : 0);
}
