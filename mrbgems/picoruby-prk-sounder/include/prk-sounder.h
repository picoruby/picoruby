#ifndef PRK_SOUNDER_DEFINED_H_
#define PRK_SOUNDER_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOUNDER_DEFAULT_HZ 2000
#define SOUNDER_SM_CYCLES 128
#define SONG_MAX_LEN 128

extern float    Sounder_song_data[SONG_MAX_LEN];
extern uint16_t Sounder_song_len[SONG_MAX_LEN];
extern uint8_t  Sounder_song_data_index;
extern uint8_t  Sounder_song_data_len;

void Sounder_init(uint32_t pin);
void Sounder_replay(void);

#endif

