#include "version.h"

#define TOSTRING(x) #x
#define EXPAND(x) TOSTRING(x)

const char* picorb_version(void) {
  return(
    "picoruby "
    PICORUBY_VERSION
#if defined(PICORUBY_COMMIT_TIMESTAMP) && defined(PICORUBY_COMMIT_BRANCH) && defined(PICORUBY_COMMIT_HASH)
    " (" EXPAND(PICORUBY_COMMIT_TIMESTAMP) " " EXPAND(PICORUBY_COMMIT_BRANCH) " " EXPAND(PICORUBY_COMMIT_HASH) ") "
#endif
  );
}
