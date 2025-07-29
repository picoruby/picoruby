#ifndef PITCHDETECTOR_DEFINED_H_
#define PITCHDETECTOR_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_RATE 8000 // 8kHz
#define BUFFER_SIZE 1024

void PITCHDETECTOR_start(uint8_t input);
void PITCHDETECTOR_stop(void);
float PITCHDETECTOR_detect_pitch(void);
void PITCHDETECTOR_set_volume_threshold(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* PITCHDETECTOR_DEFINED_H_ */
