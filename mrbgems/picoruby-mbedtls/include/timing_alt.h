#ifndef TIMING_ALT_DEFINED_H_
#define TIMING_ALT_DEFINED_H_

void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms);
int mbedtls_timing_get_delay(void *data);

#endif /* TIMING_ALT_DEFINED_H_ */
