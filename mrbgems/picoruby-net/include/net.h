#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(sleep_ms)
int sleep_ms(long milliseconds);
#endif
void lwip_begin(void);
void lwip_end(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
