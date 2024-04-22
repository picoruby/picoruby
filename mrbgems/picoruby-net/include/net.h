#ifndef NET_DEFINED_H_
#define NET_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

mrbc_value DNS_resolve(mrbc_vm *vm, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NET_DEFINED_H_ */
