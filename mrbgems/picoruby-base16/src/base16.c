
const char base16chars[] = "0123456789abcdef";

static unsigned char
base16_convert(unsigned char n)
{
  if (n >= 0x30 && n <= 0x39) {
    return n - 0x30;
  } else if (n >= 0x41 && n <= 0x46) {
    return n - 55; // 0x41(A) = 65 should return 10
  } else if (n >= 0x61 && n <= 0x66) {
    return n - 87; // 0x61(a) = 97 should return 10
  } else {
    return 0;
  }
}


#if defined(PICORB_VM_MRUBY)

#include "mruby/base16.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/base16.c"

#endif
