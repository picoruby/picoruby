#ifndef PITCHDETECTOR_DEFINED_H_
#define PITCHDETECTOR_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_RATE 8000 // 8kHz
#define BUFFER_SIZE 1024

// YIN algorithm constants
#define YIN_THRESHOLD_BASE 0.15f
#define YIN_THRESHOLD_MIN 0.10f
#define YIN_THRESHOLD_MAX 0.25f
#define MIN_PERIOD (SAMPLE_RATE / 800)  // 800Hz max
#define MAX_PERIOD (BUFFER_SIZE / 2)    // Nyquist limit
#define LOW_FREQ_THRESHOLD 120.0f       // Below this use extended analysis
#define POWER_THRESHOLD 0.01f

// Adaptive filtering constants
#define SIGNAL_HISTORY_SIZE 8
#define SNR_THRESHOLD 3.0f

// Harmonics detection constants
#define MAX_HARMONICS 4
#define HARMONIC_TOLERANCE 0.02f  // 2% tolerance for harmonic detection
#define FUNDAMENTAL_CONFIDENCE_THRESHOLD 0.7f

// Platform-specific functions (DMA/hardware layer)
void PITCHDETECTOR_start(uint8_t input);
void PITCHDETECTOR_stop(void);
float PITCHDETECTOR_detect_pitch(void);

// Platform-independent functions (signal processing layer)
void PITCHDETECTOR_init_processing(void);
float PITCHDETECTOR_process_buffer(uint16_t *buffer, int size);
float PITCHDETECTOR_calculate_signal_power(uint16_t *buffer, int size, float *dc_offset);
void PITCHDETECTOR_set_volume_threshold(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* PITCHDETECTOR_DEFINED_H_ */
