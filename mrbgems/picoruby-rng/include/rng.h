#ifndef RNG_DEFINED_H_
#define RNG_DEFINED_H_

#include <stddef.h>
#include <stdint.h>

/*
 * One byte of entropy. The return type leaves no room for a failure, so
 * a port must not use the value to report one: the POSIX port exits
 * rather than hand back a byte it did not get. A port whose source can
 * fail reports it through rng_random_bytes_impl() below instead.
 */
uint8_t rng_random_byte_impl(void);

/*
 * len bytes of entropy. 0 on success, negative on failure.
 *
 * Separate from the call above because a caller that must not proceed on
 * fabricated entropy has to be able to tell, and a byte-at-a-time
 * contract cannot say so. Ports that cannot fail get a generic
 * implementation for free (src/rng.c); one that can fail overrides it.
 */
int rng_random_bytes_impl(uint8_t *buf, size_t len);

#endif
