#ifndef MMRBC_COMPILE_H_
#define MMRBC_COMPILE_H_

#include <stdbool.h>

#include "scope.h"
#include "stream.h"

bool Compile(Scope *scope, StreamInterface *si);

#endif
