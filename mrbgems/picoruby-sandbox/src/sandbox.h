#ifndef _SANDBOX_H
#define _SANDBOX_H

#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

void mrbc_sandbox_init(mrbc_vm *vm);
void create_sandbox(void);

#ifdef __cplusplus
}
#endif

#endif /* _SANDBOX_H */


