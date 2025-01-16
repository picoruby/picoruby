#ifndef NET_COMMON_DEFINED_H_
#define NET_COMMON_DEFINED_H_

#include "lwip/err.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t Net_get_ip(const char *name, ip_addr_t *ip);

#ifdef __cplusplus
}
#endif

#endif // NET_COMMON_DEFINED_H_
