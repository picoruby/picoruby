#ifndef PICORB_RANDOM_H_
#define PICORB_RANDOM_H_

#include <stddef.h>

/*
 * Fills `len` bytes at `buf` from the source of random bytes of the
 * platform. Returns 0, or -1 when the source cannot give them.
 * Each port (ports/<platform>/random.c) gives this function. It does not
 * depend on a VM, because the R2P2 build compiles every port for both VMs.
 */
int picorb_random_bytes(void *buf, size_t len);

#endif /* PICORB_RANDOM_H_ */
