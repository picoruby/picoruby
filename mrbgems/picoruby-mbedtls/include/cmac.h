#ifndef CMAC_DEFINED_H_
#define CMAC_DEFINED_H_

enum {
  CMAC_SUCCESS = 0,
  CMAC_BAD_INPUT_DATA,
  CMAC_FAILED_TO_SETUP,
  CMAC_STARTS_FAILED,
  CMAC_UPDATE_FAILED,
  CMAC_RESET_FAILED,
  CMAC_FINISH_FAILED,
};

int MbedTLS_cmac_instance_size();
int MbedTLS_cmac_init_aes(unsigned char *data, const unsigned char *key, int key_bitlen);
int MbedTLS_cmac_update(unsigned char *data, const unsigned char *input, int input_len);
int MbedTLS_cmac_reset(unsigned char *data);
int MbedTLS_cmac_finish(unsigned char *data, unsigned char *output);
void MbedTLS_cmac_free(unsigned char *data);

#endif /* CMAC_DEFINED_H_ */
