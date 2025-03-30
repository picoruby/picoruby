static void
mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void) level);
  console_printf("%s:%04d: %s", file, line, str);
}
