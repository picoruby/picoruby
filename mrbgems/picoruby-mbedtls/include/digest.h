#ifndef DIGEST_DEFINED_H_
#define DIGEST_DEFINED_H_

#include <stdbool.h>
#include <string.h>
#include <stddef.h>

enum {
  DIGEST_SUCCESS = 0,
  DIGEST_BAD_INPUT_DATA,
  DIGEST_FAILED_TO_SETUP,
  DIGEST_FAILED_TO_START,
};

int MbedTLS_digest_algorithm_name(const char * name);
int MbedTLS_digest_instance_size(void);
int MbedTLS_digest_new(unsigned char *data, int alg);
int MbedTLS_digest_update(unsigned char *data, const unsigned char *input, size_t ilen);
size_t MbedTLS_digest_get_size(unsigned char *data);
int MbedTLS_digest_finish(unsigned char *data, unsigned char *output);
void MbedTLS_digest_free(unsigned char *data);

#endif /* DIGEST_DEFINED_H_ */
