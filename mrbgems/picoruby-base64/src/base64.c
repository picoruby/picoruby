#include <stddef.h>
#include <stdint.h>

/*
 * The encoder and the decoder of both VMs. They follow CRuby's
 * pack("m") and unpack("m") / unpack("m0"), which Base64 is made of.
 */

static const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
    return 255; // not in the alphabet
  }
}

/* RFC 2045 lines: 60 characters, from 45 bytes */
#define BASE64_LINE_LEN 60

/* The size of the encoding of len bytes, without the terminating NUL.
 * line_len is 0 for one line with no newline (strict_encode64), or the
 * characters of each line, which ends with "\n", the last one too
 * (encode64). line_len is a multiple of 4. */
static size_t
base64_encoded_size(size_t len, size_t line_len)
{
  size_t chars = (len + 2) / 3 * 4;
  if (line_len == 0 || chars == 0) return chars;
  return chars + (chars + line_len - 1) / line_len;
}

/* Writes the encoding of in[0, len) and a NUL to out, which has room for
 * base64_encoded_size(len, line_len) + 1 bytes. Answers the size. */
static size_t
base64_encode(const uint8_t *in, size_t len, uint8_t *out, size_t line_len)
{
  size_t o = 0;
  size_t col = 0;
  size_t i;
  for (i = 0; i < len; i += 3) {
    uint32_t n = (uint32_t)in[i] << 16;
    if (i + 1 < len) n |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < len) n |= (uint32_t)in[i + 2];
    out[o++] = base64chars[(n >> 18) & 63];
    out[o++] = base64chars[(n >> 12) & 63];
    out[o++] = (i + 1 < len) ? base64chars[(n >> 6) & 63] : '=';
    out[o++] = (i + 2 < len) ? base64chars[n & 63] : '=';
    if (line_len) {
      col += 4;
      if (line_len <= col) {
        out[o++] = '\n';
        col = 0;
      }
    }
  }
  if (line_len && 0 < col) out[o++] = '\n';
  out[o] = '\0';
  return o;
}

/* The room the decoding of len characters needs, with the NUL */
static size_t
base64_decoded_capa(size_t len)
{
  return (len + 3) / 4 * 3 + 1;
}

/* Decodes in[0, len) to out, which has base64_decoded_capa(len) bytes,
 * and writes a NUL after the result. Answers 0 and the size in *out_len,
 * or -1 for input that strict does not take.
 *
 * Lenient (decode64), as CRuby's unpack("m"): a character outside the
 * alphabet is skipped, '=' ends the input where the third or the fourth
 * character of a group is due (and is skipped before that), and the 2
 * or 3 characters of an unfinished group give 1 or 2 bytes.
 *
 * Strict (strict_decode64), as unpack("m0"): the length is a multiple of
 * 4, every character is in the alphabet, '=' only ends the last group,
 * and the bits a padded group leaves unused are 0. */
static int
base64_decode(const uint8_t *in, size_t len, uint8_t *out, size_t *out_len, int strict)
{
  size_t o = 0;
  size_t s = 0;
  unsigned char a, b, c, d;

  if (strict) {
    if (len % 4 != 0) return -1;
    for (s = 0; s < len; s += 4) {
      a = base64_convert(in[s]);
      b = base64_convert(in[s + 1]);
      if (a == 255 || b == 255) return -1;
      out[o++] = (uint8_t)(a << 2 | b >> 4);
      if (in[s + 2] == '=') {
        if (s + 4 != len || in[s + 3] != '=' || (b & 0xf)) return -1;
        break;
      }
      c = base64_convert(in[s + 2]);
      if (c == 255) return -1;
      out[o++] = (uint8_t)(b << 4 | c >> 2);
      if (in[s + 3] == '=') {
        if (s + 4 != len || (c & 0x3)) return -1;
        break;
      }
      d = base64_convert(in[s + 3]);
      if (d == 255) return -1;
      out[o++] = (uint8_t)(c << 6 | d);
    }
  }
  else {
    a = b = c = 255;
    while (s < len) {
      a = b = c = d = 255;
      while (s < len && (a = base64_convert(in[s])) == 255) s++;
      if (len <= s) break;
      s++;
      while (s < len && (b = base64_convert(in[s])) == 255) s++;
      if (len <= s) break;
      s++;
      while (s < len && (c = base64_convert(in[s])) == 255) {
        if (in[s] == '=') break;
        s++;
      }
      if (len <= s || in[s] == '=') break;
      s++;
      while (s < len && (d = base64_convert(in[s])) == 255) {
        if (in[s] == '=') break;
        s++;
      }
      if (len <= s || in[s] == '=') break;
      s++;
      out[o++] = (uint8_t)(a << 2 | b >> 4);
      out[o++] = (uint8_t)(b << 4 | c >> 2);
      out[o++] = (uint8_t)(c << 6 | d);
      a = 255;
    }
    if (a != 255 && b != 255) {
      out[o++] = (uint8_t)(a << 2 | b >> 4);
      if (c != 255) out[o++] = (uint8_t)(b << 4 | c >> 2);
    }
  }
  out[o] = '\0';
  *out_len = o;
  return 0;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/base64.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/base64.c"

#endif
