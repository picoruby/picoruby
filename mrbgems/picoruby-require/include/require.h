#ifndef PICORUBY_REQUIRE_H
#define PICORUBY_REQUIRE_H

#include <stdbool.h>
#include <stdint.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

bool picoruby_load_model(const uint8_t *mrb);
bool picoruby_load_model_by_name(const char *gem);
void picoruby_init_require(mrbc_vm *vm);

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_REQUIRE_H */
