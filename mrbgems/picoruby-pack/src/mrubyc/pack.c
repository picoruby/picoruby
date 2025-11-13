#include <mrubyc.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "../../include/pack.h"

#define MAX_STACK_BUFFER_SIZE 256

/* Helper macros for byte order conversion */
#define PACK_UINT16_BE(val) \
  (((val) >> 8) & 0xFF), ((val) & 0xFF)

#define PACK_UINT16_LE(val) \
  ((val) & 0xFF), (((val) >> 8) & 0xFF)

#define PACK_UINT32_BE(val) \
  (((val) >> 24) & 0xFF), (((val) >> 16) & 0xFF), \
  (((val) >> 8) & 0xFF), ((val) & 0xFF)

#define PACK_UINT32_LE(val) \
  ((val) & 0xFF), (((val) >> 8) & 0xFF), \
  (((val) >> 16) & 0xFF), (((val) >> 24) & 0xFF)

#define UNPACK_UINT16_BE(ptr) \
  ((uint16_t)(((uint8_t*)(ptr))[0]) << 8 | \
   (uint16_t)(((uint8_t*)(ptr))[1]))

#define UNPACK_UINT16_LE(ptr) \
  ((uint16_t)(((uint8_t*)(ptr))[0]) | \
   (uint16_t)(((uint8_t*)(ptr))[1]) << 8)

#define UNPACK_UINT32_BE(ptr) \
  ((uint32_t)(((uint8_t*)(ptr))[0]) << 24 | \
   (uint32_t)(((uint8_t*)(ptr))[1]) << 16 | \
   (uint32_t)(((uint8_t*)(ptr))[2]) << 8 | \
   (uint32_t)(((uint8_t*)(ptr))[3]))

#define UNPACK_UINT32_LE(ptr) \
  ((uint32_t)(((uint8_t*)(ptr))[0]) | \
   (uint32_t)(((uint8_t*)(ptr))[1]) << 8 | \
   (uint32_t)(((uint8_t*)(ptr))[2]) << 16 | \
   (uint32_t)(((uint8_t*)(ptr))[3]) << 24)

/* Parse count from format string (e.g., "C3" -> 3) */
static int
parse_count(const char *format, int *index, int max_count)
{
  int count = 0;
  bool has_count = false;

  while (*index < strlen(format) && format[*index] >= '0' && format[*index] <= '9') {
    count = count * 10 + (format[*index] - '0');
    (*index)++;
    has_count = true;
  }

  if (!has_count) {
    return 1; /* Default count is 1 */
  }

  if (count > max_count) {
    return max_count;
  }

  return count;
}

/* PackHelper.pack_array(array, format) */
static void
c_pack_helper_pack_array(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Check if first argument is an array */
  if (v[1].tt != MRBC_TT_ARRAY) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "first argument must be an Array");
    return;
  }

  /* Check if second argument is a string */
  if (v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "second argument must be a String");
    return;
  }

  const char *format = (const char *)v[2].string->data;
  int format_len = v[2].string->size;
  mrbc_value *array = &v[1];
  int array_len = mrbc_array_size(array);

  /* Allocate buffer */
  uint8_t stack_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t *buffer = NULL;
  int buffer_size = 0;
  int buffer_capacity = MAX_STACK_BUFFER_SIZE;
  bool needs_free = false;

  /* Estimate buffer size needed */
  int estimated_size = 0;
  for (int i = 0; i < format_len; i++) {
    switch (format[i]) {
      case 'C': case 'c':
        estimated_size += 1;
        break;
      case 'S': case 's': case 'n': case 'v':
        estimated_size += 2;
        break;
      case 'L': case 'l': case 'N': case 'V':
        estimated_size += 4;
        break;
      case '*':
        estimated_size += array_len * 4; /* Rough estimate */
        break;
      case ' ': case '\t': case '\r': case '\n':
        /* Skip whitespace */
        break;
      default:
        if (format[i] >= '0' && format[i] <= '9') {
          /* Skip count digits */
          break;
        }
        mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "unsupported format character: %c", format[i]);
        return;
    }
  }

  /* Allocate buffer */
  if (estimated_size < MAX_STACK_BUFFER_SIZE) {
    buffer = stack_buffer;
  } else {
    buffer = (uint8_t *)mrbc_alloc(vm, estimated_size);
    if (!buffer) {
      mrbc_raise(vm, MRBC_CLASS(StandardError), "failed to allocate memory");
      return;
    }
    buffer_capacity = estimated_size;
    needs_free = true;
  }

  /* Pack loop */
  int format_index = 0;
  int args_index = 0;

  while (format_index < format_len) {
    char directive = format[format_index];
    format_index++;

    /* Skip whitespace */
    if (directive == ' ' || directive == '\t' || directive == '\r' || directive == '\n') {
      continue;
    }

    /* Parse count */
    int count = parse_count(format, &format_index, array_len - args_index);
    bool use_all = false;

    /* Check for '*' modifier */
    if (format_index < format_len && format[format_index] == '*') {
      use_all = true;
      count = array_len - args_index;
      format_index++;
    }

    /* Process directive */
    for (int i = 0; i < count; i++) {
      if (args_index >= array_len) {
        if (!use_all) {
          mrbc_raise(vm, MRBC_CLASS(ArgumentError), "too few arguments");
          if (needs_free) mrbc_free(vm, buffer);
          return;
        }
        break;
      }

      mrbc_value arg = mrbc_array_get(array, args_index);
      args_index++;

      /* Ensure we have enough space */
      if (buffer_size + 4 > buffer_capacity) {
        int new_capacity = buffer_capacity * 2;
        uint8_t *new_buffer = (uint8_t *)mrbc_alloc(vm, new_capacity);
        if (!new_buffer) {
          mrbc_raise(vm, MRBC_CLASS(StandardError), "failed to allocate memory");
          if (needs_free) mrbc_free(vm, buffer);
          return;
        }
        memcpy(new_buffer, buffer, buffer_size);
        if (needs_free) mrbc_free(vm, buffer);
        buffer = new_buffer;
        buffer_capacity = new_capacity;
        needs_free = true;
      }

      switch (directive) {
        case 'C': /* Unsigned 8-bit */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          buffer[buffer_size++] = (uint8_t)(arg.i & 0xFF);
          break;

        case 'c': /* Signed 8-bit */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          buffer[buffer_size++] = (int8_t)(arg.i & 0xFF);
          break;

        case 'S': /* Unsigned 16-bit, native endian */
        case 's': /* Signed 16-bit, native endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint16_t val = (uint16_t)(arg.i & 0xFFFF);
            memcpy(&buffer[buffer_size], &val, 2);
            buffer_size += 2;
          }
          break;

        case 'n': /* Unsigned 16-bit, big-endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint16_t val = (uint16_t)(arg.i & 0xFFFF);
            buffer[buffer_size++] = (val >> 8) & 0xFF;
            buffer[buffer_size++] = val & 0xFF;
          }
          break;

        case 'v': /* Unsigned 16-bit, little-endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint16_t val = (uint16_t)(arg.i & 0xFFFF);
            buffer[buffer_size++] = val & 0xFF;
            buffer[buffer_size++] = (val >> 8) & 0xFF;
          }
          break;

        case 'L': /* Unsigned 32-bit, native endian */
        case 'l': /* Signed 32-bit, native endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint32_t val = (uint32_t)arg.i;
            memcpy(&buffer[buffer_size], &val, 4);
            buffer_size += 4;
          }
          break;

        case 'N': /* Unsigned 32-bit, big-endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint32_t val = (uint32_t)arg.i;
            buffer[buffer_size++] = (val >> 24) & 0xFF;
            buffer[buffer_size++] = (val >> 16) & 0xFF;
            buffer[buffer_size++] = (val >> 8) & 0xFF;
            buffer[buffer_size++] = val & 0xFF;
          }
          break;

        case 'V': /* Unsigned 32-bit, little-endian */
          if (arg.tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer");
            if (needs_free) mrbc_free(vm, buffer);
            return;
          }
          {
            uint32_t val = (uint32_t)arg.i;
            buffer[buffer_size++] = val & 0xFF;
            buffer[buffer_size++] = (val >> 8) & 0xFF;
            buffer[buffer_size++] = (val >> 16) & 0xFF;
            buffer[buffer_size++] = (val >> 24) & 0xFF;
          }
          break;

        default:
          mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "unsupported format character: %c", directive);
          if (needs_free) mrbc_free(vm, buffer);
          return;
      }
    }
  }

  /* Create result string */
  mrbc_value result = mrbc_string_new(vm, buffer, buffer_size);

  /* Free buffer if allocated */
  if (needs_free) {
    mrbc_free(vm, buffer);
  }

  SET_RETURN(result);
}

/* PackHelper.unpack_string(string, format) */
static void
c_pack_helper_unpack_string(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Check if first argument is a string */
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "first argument must be a String");
    return;
  }

  /* Check if second argument is a string */
  if (v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "second argument must be a String");
    return;
  }

  const uint8_t *data = (const uint8_t *)v[1].string->data;
  int data_len = v[1].string->size;
  const char *format = (const char *)v[2].string->data;
  int format_len = v[2].string->size;

  /* Create result array */
  mrbc_value result = mrbc_array_new(vm, 0);

  /* Unpack loop */
  int format_index = 0;
  int data_index = 0;

  while (format_index < format_len && data_index < data_len) {
    char directive = format[format_index];
    format_index++;

    /* Skip whitespace */
    if (directive == ' ' || directive == '\t' || directive == '\r' || directive == '\n') {
      continue;
    }

    /* Parse count */
    int count = parse_count(format, &format_index, (data_len - data_index));
    bool use_all = false;

    /* Check for '*' modifier */
    if (format_index < format_len && format[format_index] == '*') {
      use_all = true;
      format_index++;

      /* Calculate count based on directive size */
      switch (directive) {
        case 'C': case 'c':
          count = data_len - data_index;
          break;
        case 'S': case 's': case 'n': case 'v':
          count = (data_len - data_index) / 2;
          break;
        case 'L': case 'l': case 'N': case 'V':
          count = (data_len - data_index) / 4;
          break;
      }
    }

    /* Process directive */
    for (int i = 0; i < count; i++) {
      if (data_index >= data_len) {
        break;
      }

      switch (directive) {
        case 'C': /* Unsigned 8-bit */
          if (data_index + 1 > data_len) goto unpack_end;
          {
            mrbc_value val = mrbc_integer_value(data[data_index]);
            mrbc_array_push(&result, &val);
            data_index += 1;
          }
          break;

        case 'c': /* Signed 8-bit */
          if (data_index + 1 > data_len) goto unpack_end;
          {
            mrbc_value val = mrbc_integer_value((int8_t)data[data_index]);
            mrbc_array_push(&result, &val);
            data_index += 1;
          }
          break;

        case 'S': /* Unsigned 16-bit, native endian */
          if (data_index + 2 > data_len) goto unpack_end;
          {
            uint16_t uval;
            memcpy(&uval, &data[data_index], 2);
            mrbc_value val = mrbc_integer_value(uval);
            mrbc_array_push(&result, &val);
            data_index += 2;
          }
          break;

        case 's': /* Signed 16-bit, native endian */
          if (data_index + 2 > data_len) goto unpack_end;
          {
            int16_t sval;
            memcpy(&sval, &data[data_index], 2);
            mrbc_value val = mrbc_integer_value(sval);
            mrbc_array_push(&result, &val);
            data_index += 2;
          }
          break;

        case 'n': /* Unsigned 16-bit, big-endian */
          if (data_index + 2 > data_len) goto unpack_end;
          {
            uint16_t uval = UNPACK_UINT16_BE(&data[data_index]);
            mrbc_value val = mrbc_integer_value(uval);
            mrbc_array_push(&result, &val);
            data_index += 2;
          }
          break;

        case 'v': /* Unsigned 16-bit, little-endian */
          if (data_index + 2 > data_len) goto unpack_end;
          {
            uint16_t uval = UNPACK_UINT16_LE(&data[data_index]);
            mrbc_value val = mrbc_integer_value(uval);
            mrbc_array_push(&result, &val);
            data_index += 2;
          }
          break;

        case 'L': /* Unsigned 32-bit, native endian */
          if (data_index + 4 > data_len) goto unpack_end;
          {
            uint32_t uval;
            memcpy(&uval, &data[data_index], 4);
            mrbc_value val = mrbc_integer_value((mrbc_int_t)uval);
            mrbc_array_push(&result, &val);
            data_index += 4;
          }
          break;

        case 'l': /* Signed 32-bit, native endian */
          if (data_index + 4 > data_len) goto unpack_end;
          {
            int32_t sval;
            memcpy(&sval, &data[data_index], 4);
            mrbc_value val = mrbc_integer_value((mrbc_int_t)sval);
            mrbc_array_push(&result, &val);
            data_index += 4;
          }
          break;

        case 'N': /* Unsigned 32-bit, big-endian */
          if (data_index + 4 > data_len) goto unpack_end;
          {
            uint32_t uval = UNPACK_UINT32_BE(&data[data_index]);
            mrbc_value val = mrbc_integer_value((mrbc_int_t)uval);
            mrbc_array_push(&result, &val);
            data_index += 4;
          }
          break;

        case 'V': /* Unsigned 32-bit, little-endian */
          if (data_index + 4 > data_len) goto unpack_end;
          {
            uint32_t uval = UNPACK_UINT32_LE(&data[data_index]);
            mrbc_value val = mrbc_integer_value((mrbc_int_t)uval);
            mrbc_array_push(&result, &val);
            data_index += 4;
          }
          break;

        default:
          mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "unsupported format character: %c", directive);
          return;
      }
    }
  }

unpack_end:
  SET_RETURN(result);
}

/* Initialize pack module */
void
mrbc_pack_init(mrbc_vm *vm)
{
  /* Define PackHelper class */
  mrbc_class *class_PackHelper = mrbc_define_class(vm, "PackHelper", mrbc_class_object);

  /* Define class methods */
  mrbc_define_method(vm, class_PackHelper, "pack_array", c_pack_helper_pack_array);
  mrbc_define_method(vm, class_PackHelper, "unpack_string", c_pack_helper_unpack_string);
}
