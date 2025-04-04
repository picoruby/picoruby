const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char
base64_convert(unsigned char n)
{
  if (n >= 0x30 && n <= 0x39) {
    return n + 4;  // 0x30(0) = 48 should return 52
  } else if (n >= 0x41 && n <= 0x5a) {
    return n - 65; // 0x41(A) = 65 should return 0
  } else if (n >= 0x61 && n <= 0x7a) {
    return n - 71; // 0x61(a) = 97 should return 26
  } else if (n == 0x2b) {
    return 62; // +
  } else if (n == 0x2f) {
    return 63; // slash
  } else {
    return 0;
  }
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/base64.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/base64.c"

#endif
