#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include <string.h>

static int
mbedtls_digest_algorithm_name(const char * name)
{
  int ret;
  if (strcmp(name, "sha256") == 0) {
    ret = (int)MBEDTLS_MD_SHA256;
  } else if (strcmp(name, "none") == 0) {
    ret = (int)MBEDTLS_MD_NONE;
  } else {
    ret = -1;
  }
  return ret;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/digest.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/digest.c"

#endif

