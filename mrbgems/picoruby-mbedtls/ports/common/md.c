#include <mbedtls/md.h>

void
MbedTLS_md_free(void *p)
{
  mbedtls_md_free(p);
}
