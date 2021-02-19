#include <stdio.h>
#include <stdlib.h>

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
// C99 or newer
static const char format[] = "echo '#define PTR_SIZE %zu' > ptr_size.h";
#define put_ptr_size(type)  sprintf(command, format, sizeof(type))

#elif defined(_MSC_VER)
// Visual C/C++
static const char format[] = "echo '#define PTR_SIZE %Iu' > ptr_size.h";
#define put_ptr_size(type)  sprintf(command, format, sizeof(type))

#else
static const char format[] = "echo '#define PTR_SIZE %lu' > ptr_size.h";
#define put_ptr_size(type) sprintf(command, format, (unsigned long)sizeof(type))
#endif

int main(void)
{
  char command[100];
  put_ptr_size(void *);
  return system(command);
}
