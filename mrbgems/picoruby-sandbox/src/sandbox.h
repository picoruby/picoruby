#ifndef _SANDBOX_H
#define _SANDBOX_H

#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

void mrbc_sandbox_init(void);
void create_sandbox(void);

#ifdef __cplusplus
}
#endif

#endif /* _SANDBOX_H */


