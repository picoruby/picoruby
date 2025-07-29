#include <stdbool.h>
#include <math.h>
#include "../../include/pitchdetector.h"

#include "picoruby/debug.h"

static uint16_t volume_threshold;

// YIN algorithm constants
#define YIN_THRESHOLD_BASE 0.15f
#define YIN_THRESHOLD_MIN 0.10f
#define YIN_THRESHOLD_MAX 0.25f
#define MIN_PERIOD (SAMPLE_RATE / 800)  // 800Hz max
#define MAX_PERIOD (BUFFER_SIZE / 2)    // Nyquist limit
#define LOW_FREQ_THRESHOLD 120.0f       // Below this use extended analysis
#define POWER_THRESHOLD 0.001f

// Adaptive filtering constants
#define SIGNAL_HISTORY_SIZE 8
#define SNR_THRESHOLD 3.0f

// Harmonics detection constants
#define MAX_HARMONICS 4
#define HARMONIC_TOLERANCE 0.02f  // 2% tolerance for harmonic detection
#define FUNDAMENTAL_CONFIDENCE_THRESHOLD 0.7f

// Pre-computed buffer for YIN difference function
static float yin_buffer[BUFFER_SIZE / 2];

// Signal history for adaptive processing
static float signal_history[SIGNAL_HISTORY_SIZE];
static float noise_estimate = 0;
static int history_index = 0;
static bool history_filled = false;

// Enhanced signal power calculation with DC offset removal
static float
calculate_signal_power(uint16_t *buffer, int size, float *dc_offset)
{
  float sum = 0;
  float mean = 0;

  // Calculate DC offset (mean value)
  for (int i = 0; i < size; i++) {
    mean += buffer[i];
  }
  mean /= size;
  *dc_offset = mean;

  // Calculate signal power (variance)
  for (int i = 0; i < size; i++) {
    float diff = buffer[i] - mean;
    sum += diff * diff;
  }

  return sum / size;
}

// Update noise estimate using signal history
static void
update_noise_estimate(float signal_power)
{
  signal_history[history_index] = signal_power;
  history_index = (history_index + 1) % SIGNAL_HISTORY_SIZE;

  if (!history_filled && history_index == 0) {
    history_filled = true;
  }

  // Calculate noise estimate as minimum from recent history
  if (history_filled) {
    float min_power = signal_history[0];
    for (int i = 1; i < SIGNAL_HISTORY_SIZE; i++) {
      if (signal_history[i] < min_power) {
        min_power = signal_history[i];
      }
    }
    noise_estimate = min_power;
  }
}

// Calculate adaptive YIN threshold based on signal quality
static float
calculate_adaptive_threshold(float signal_power)
{
  if (!history_filled || noise_estimate <= 0) {
    return YIN_THRESHOLD_BASE;
  }

  float snr = signal_power / noise_estimate;

  // Lower threshold for high SNR signals (more sensitive)
  // Higher threshold for low SNR signals (less sensitive, avoid false positives)
  if (snr > SNR_THRESHOLD * 2) {
    return YIN_THRESHOLD_MIN;
  } else if (snr < SNR_THRESHOLD) {
    return YIN_THRESHOLD_MAX;
  } else {
    // Linear interpolation between min and max
    float ratio = (snr - SNR_THRESHOLD) / SNR_THRESHOLD;
    return YIN_THRESHOLD_MAX - ratio * (YIN_THRESHOLD_MAX - YIN_THRESHOLD_MIN);
  }
}

// Simple high-pass filter to remove DC and low-frequency noise
static void
apply_highpass_filter(uint16_t *buffer, float dc_offset)
{
  static float prev_input = 0;
  static float prev_output = 0;
  const float alpha = 0.95f;  // High-pass filter coefficient

  for (int i = 0; i < BUFFER_SIZE; i++) {
    float input = buffer[i] - dc_offset;
    float output = alpha * (prev_output + input - prev_input);
    buffer[i] = (uint16_t)(output + dc_offset);
    prev_input = input;
    prev_output = output;
  }
}

// YIN difference function - core of the YIN algorithm
static void
yin_difference_function(uint16_t *buffer, float dc_offset)
{
  // Calculate difference function
  for (int tau = 0; tau < BUFFER_SIZE / 2; tau++) {
    float sum = 0;
    for (int j = 0; j < BUFFER_SIZE / 2; j++) {
      float delta = (buffer[j] - dc_offset) - (buffer[j + tau] - dc_offset);
      sum += delta * delta;
    }
    yin_buffer[tau] = sum;
  }

  // Apply cumulative mean normalized difference function
  yin_buffer[0] = 1.0f;
  float running_sum = 0;

  for (int tau = 1; tau < BUFFER_SIZE / 2; tau++) {
    running_sum += yin_buffer[tau];
    yin_buffer[tau] *= tau / running_sum;
  }
}

// Parabolic interpolation for sub-sample precision
static float
parabolic_interpolation(int tau)
{
  if (tau < 1 || tau >= BUFFER_SIZE / 2 - 1) {
    return tau;
  }

  float s0 = yin_buffer[tau - 1];
  float s1 = yin_buffer[tau];
  float s2 = yin_buffer[tau + 1];

  float a = (s0 - 2 * s1 + s2) / 2;
  if (fabsf(a) < 1e-10f) {
    return tau;
  }

  float b = (s2 - s0) / 2;
  float x0 = -b / (2 * a);

  return tau + x0;
}

// Check if a period is likely a harmonic of a lower fundamental
//static bool
//is_harmonic(int period, int fundamental_period)
//{
//  if (fundamental_period == 0) return false;
//
//  for (int h = 2; h <= MAX_HARMONICS; h++) {
//    float expected_period = (float)fundamental_period / h;
//    float tolerance = expected_period * HARMONIC_TOLERANCE;
//
//    if (fabsf(period - expected_period) <= tolerance) {
//      return true;
//    }
//  }
//  return false;
//}
static bool
is_harmonic(float p1, float p2, float tolerance, float strength1, float strength2) {
  // freq1がfreq2の2倍音または3倍音かどうか判定
  float freq1 = (float)SAMPLE_RATE / p1;
  float freq2 = (float)SAMPLE_RATE / p2;
  float ratio = freq1 / freq2;
  if (fabsf(ratio - 2.0f) < tolerance && strength1 < strength2) {
    return true;
  }
  if (fabsf(ratio - 3.0f) < tolerance && strength1 < strength2) {
   return true;
  }
  return false;
}

// Find multiple period candidates and verify fundamental frequency
static int
find_fundamental_period(float adaptive_threshold)
{
  typedef struct {
    int period;
    float strength;
  } PeriodCandidate;

  PeriodCandidate candidates[5];
  int candidate_count = 0;

  // Find top candidates
  for (int tau = MIN_PERIOD; tau < MAX_PERIOD && candidate_count < 5; tau++) {
    if (yin_buffer[tau] < adaptive_threshold) {
      // Check if it's a local minimum
      bool is_local_min = true;
      for (int k = -2; k <= 2; k++) {
        if (tau + k >= MIN_PERIOD && tau + k < MAX_PERIOD) {
          if (yin_buffer[tau + k] < yin_buffer[tau]) {
            is_local_min = false;
            break;
          }
        }
      }

      if (is_local_min) {
        candidates[candidate_count].period = tau;
        candidates[candidate_count].strength = 1.0f - yin_buffer[tau];
        candidate_count++;
      }
    }
  }

  if (candidate_count == 0) return 0;

  // Sort candidates by strength (descending)
  for (int i = 0; i < candidate_count - 1; i++) {
    for (int j = i + 1; j < candidate_count; j++) {
      if (candidates[j].strength > candidates[i].strength) {
        PeriodCandidate temp = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = temp;
      }
    }
  }

  // Check for fundamental frequency among candidates
  for (int i = 0; i < candidate_count; i++) {
    int current_period = candidates[i].period;
    float fundamental_confidence = candidates[i].strength;

    // Check if other candidates are harmonics of this one
    for (int j = 0; j < candidate_count; j++) {
      //if (i != j && is_harmonic(candidates[j].period, current_period)) {
      if (i != j && is_harmonic(candidates[i].period, candidates[j].period, HARMONIC_TOLERANCE, candidates[i].strength, candidates[j].strength)) {
        fundamental_confidence += candidates[j].strength * 0.2f;  // Bonus for harmonic support
      }
    }

    if (fundamental_confidence > FUNDAMENTAL_CONFIDENCE_THRESHOLD) {
      return current_period;
    }
  }

  // If no clear fundamental found, return the strongest candidate
  return candidates[0].period;
}

// Enhanced analysis for low frequencies using longer correlation window
static float
analyze_low_frequency(uint16_t *buffer, float dc_offset, int estimated_period)
{
  if (estimated_period == 0) return 0.0f;

  // Use multiple periods for better low-frequency analysis
  int analysis_window = estimated_period * 3;  // Analyze 3 periods
  if (analysis_window > BUFFER_SIZE - estimated_period) {
    analysis_window = BUFFER_SIZE - estimated_period;
  }

  float best_correlation = 0;
  int best_offset = 0;

  // Search around the estimated period for better precision
  int search_range = estimated_period / 20;  // ±5% search range

  for (int offset = -search_range; offset <= search_range; offset++) {
    int test_period = estimated_period + offset;
    if (test_period <= 0 || test_period >= BUFFER_SIZE / 2) continue;

    float correlation = 0;
    float norm_a = 0, norm_b = 0;

    // Calculate correlation over the analysis window
    for (int i = 0; i < analysis_window; i++) {
      float a = buffer[i] - dc_offset;
      float b = buffer[i + test_period] - dc_offset;
      correlation += a * b;
      norm_a += a * a;
      norm_b += b * b;
    }

    if (norm_a * norm_b > 0) {
      correlation /= sqrt(norm_a * norm_b);

      if (correlation > best_correlation) {
        best_correlation = correlation;
        best_offset = offset;
      }
    }
  }

  // Only return refined estimate if correlation is strong enough
  if (best_correlation > 0.7f) {
    return (float)SAMPLE_RATE / (estimated_period + best_offset);
  }

  return (float)SAMPLE_RATE / estimated_period;
}

// Enhanced YIN-based pitch detection with adaptive processing
float
detect_pitch_core(uint16_t *buffer)
{
  float dc_offset;
  float signal_power = calculate_signal_power(buffer, BUFFER_SIZE, &dc_offset);

  // Update noise estimate for adaptive processing
  update_noise_estimate(signal_power);

  // Check if signal is strong enough
  if (signal_power < volume_threshold * volume_threshold) {
    return 0.0f;  // Signal too weak
  }

  // Additional power-based filtering for better noise rejection
  float normalized_power = signal_power / (4096.0f * 4096.0f);  // 12-bit ADC normalization
//  D("Normalized power: %f\n", normalized_power);
  if (normalized_power < POWER_THRESHOLD) {
    return 0.0f;
  }

  // Apply high-pass filtering to reduce noise
  apply_highpass_filter(buffer, dc_offset);

  // Recalculate DC offset after filtering
  float filtered_power = calculate_signal_power(buffer, BUFFER_SIZE, &dc_offset);

  // Apply YIN difference function
  yin_difference_function(buffer, dc_offset);

  // Calculate adaptive threshold based on signal quality
  float adaptive_threshold = calculate_adaptive_threshold(filtered_power);

  // Find the fundamental period using harmonic analysis
  int period = find_fundamental_period(adaptive_threshold);
  if (period == 0) {
    return 0.0f;  // No clear pitch found
  }

  // Apply parabolic interpolation for sub-sample precision
  float precise_period = parabolic_interpolation(period);

  // Calculate initial frequency
  float frequency = (float)SAMPLE_RATE / precise_period;

  // For low frequencies, use enhanced analysis for better precision
  if (frequency < LOW_FREQ_THRESHOLD) {
    frequency = analyze_low_frequency(buffer, dc_offset, period);
  }

  // Final frequency validation
  if (frequency < 75.0f || frequency > 850.0f) {
    return 0.0f;  // Outside reasonable musical range
  }

  return frequency;
}

void
PITCHDETECTOR_set_volume_threshold(uint16_t value)
{
  volume_threshold = value;
}
