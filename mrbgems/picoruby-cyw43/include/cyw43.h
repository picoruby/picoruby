#ifndef CYW43_DEFINED_H_
#define CYW43_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int CYW43_arch_init_with_country(const uint8_t *);
void CYW43_GPIO_write(uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* CYW43_DEFINED_H_ */

