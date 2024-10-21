#include <stdlib.h>
#include <mrubyc.h>
#include "../include/io-console.h"

static void
c_read_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
    * read_nonblock(maxlen, outbuf = nil, exeption: true) -> String | Symbol | nil
    * only supports `exception: false` mode
    */
  int maxlen = GET_INT_ARG(1);
  char buf[maxlen + 1];
  mrbc_value outbuf;
  int len;
  c_raw_bang(vm, v, 1); // TODO fixme: argc=1 means NONBLOCK mode (temporary)
  int c;
  for (len = 0; len < maxlen; len++) {
    // TODO no use hal_getchar. The same way as picoruby-io instead.
    c = hal_getchar();
    if (c < 0) {
      break;
    } else {
      buf[len] = c;
    }
  }
  buf[len] = '\0';
  c_cooked_bang(vm, v, 0);
  if (GET_ARG(2).tt == MRBC_TT_STRING) {
    outbuf = GET_ARG(2);
    uint8_t *str = outbuf.string->data;
    int orig_len = outbuf.string->size;
    if (orig_len != len) {
      str = mrbc_realloc(vm, str, len + 1);
      if (!str) {
        mrbc_raise(vm, MRBC_CLASS(RuntimeError), "no memory");
        return;
      }
    }
    memcpy(str, buf, len);
    str[len] = '\0';
    outbuf.string->data = str;
    outbuf.string->size = len;
    SET_RETURN(outbuf);
  } else if (len < 1) {
    SET_NIL_RETURN();
  } else {
    outbuf = mrbc_string_new_cstr(vm, buf);
    SET_RETURN(outbuf);
  }
}

void
mrbc_io_console_init(mrbc_vm *vm)
{
  mrbc_class *class_IO = mrbc_define_class(vm, "IO", mrbc_class_object);
  mrbc_define_method(vm, class_IO, "read_nonblock", c_read_nonblock);

  io_console_port_init(vm, class_IO);
}
