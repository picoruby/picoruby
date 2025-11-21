#ifndef CYW43_DEFINED_H_
#define CYW43_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int CYW43_arch_init_with_country(const uint8_t *);
#ifdef USE_WIFI
void CYW43_arch_enable_sta_mode(void);
void CYW43_arch_disable_sta_mode(void);
int CYW43_wifi_connect_with_dhcp(const char *ssid, const char *pw, uint32_t auth, uint32_t timeout_ms);
int CYW43_wifi_disconnect(void);
int CYW43_tcpip_link_status(void);
bool CYW43_dhcp_supplied(void);
int CYW43_CONST_link_down(void);
int CYW43_CONST_link_join(void);
int CYW43_CONST_link_noip(void);
int CYW43_CONST_link_up(void);
int CYW43_CONST_link_fail(void);
int CYW43_CONST_link_nonet(void);
int CYW43_CONST_link_badauth(void);
extern void lwip_begin(void);
extern void lwip_end(void);
const char *CYW43_ipv4_address(char *buf, size_t buflen);
#endif
void CYW43_GPIO_write(uint8_t, uint8_t);
uint8_t CYW43_GPIO_read(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* CYW43_DEFINED_H_ */

