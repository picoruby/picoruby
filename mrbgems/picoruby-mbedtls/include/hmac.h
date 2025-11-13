#ifndef HMAC_DEFINED_H_
#define HMAC_DEFINED_H_

enum {
  HMAC_SUCCESS = 0,
  HMAC_UNSUPPORTED_HASH,
  HMAC_SETUP_FAILED,
  HMAC_STARTS_FAILED,
  HMAC_UPDATE_FAILED,
  HMAC_RESET_FAILED,
  HMAC_FINISH_FAILED,
  HMAC_ALREADY_FINISHED,
};

int MbedTLS_hmac_instance_size();
int MbedTLS_hmac_init(void *data, const char *algorithm, const unsigned char *key, int key_len);
int MbedTLS_hmac_update(void *data, const unsigned char *input, int input_len);
int MbedTLS_hmac_reset(void *data);
int MbedTLS_hmac_finish(void *data, unsigned char *output);
void MbedTLS_hmac_free(void *data);
int MbedTLS_hmac_check_finished(void *data);

#endif /* HMAC_DEFINED_H_ */
