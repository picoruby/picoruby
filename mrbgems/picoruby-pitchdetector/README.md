# PicoRuby Pitch Detector

A high-precision pitch detection library for embedded systems, designed specifically for PicoRuby and Raspberry Pi Pico.

## Features

- **YIN Algorithm**: Advanced pitch detection using the YIN (Yet another pitch detector) algorithm
- **Adaptive Processing**: Dynamic threshold adjustment based on signal quality
- **Harmonic Analysis**: Accurate fundamental frequency detection with harmonic verification
- **Low-frequency Optimization**: Enhanced analysis for bass frequencies (75-120Hz)
- **Noise Filtering**: Built-in high-pass filtering and noise rejection
- **Real-time Processing**: DMA-based continuous ADC sampling at 8kHz

## Usage

```ruby
require 'pitchdetector'

# Initialize with ADC pin and volume threshold
pd = PitchDetector.new(26, volume_threshold: 50)

# Start pitch detection
pd.start

# Detect pitch in a loop
while true
  freq = pd.detect_pitch
  if freq
    puts "Detected frequency: #{freq} Hz"
  end
  sleep_ms 200
end

# Stop detection
pd.stop
```

## How the YIN Algorithm Works

The YIN algorithm, developed in 2002, provides superior pitch detection accuracy compared to traditional autocorrelation methods.

Refer to http://audition.ens.fr/adc/pdf/2002_JASA_YIN.pdf

### Core Principles

**1. Difference Function**
Instead of measuring similarity (autocorrelation), YIN measures the "difference" between signal segments:

```c
// Calculate squared difference for each time lag τ
for (int tau = 0; tau < BUFFER_SIZE / 2; tau++) {
  float sum = 0;
  for (int j = 0; j < BUFFER_SIZE / 2; j++) {
    float delta = (buffer[j] - dc_offset) - (buffer[j + tau] - dc_offset);
    sum += delta * delta;
  }
  yin_buffer[tau] = sum;
}
```

**2. Cumulative Mean Normalized Difference Function (CMNDF)**
Normalization that favors smaller periods (higher frequencies):

```c
yin_buffer[0] = 1.0f;
float running_sum = 0;

for (int tau = 1; tau < BUFFER_SIZE / 2; tau++) {
  running_sum += yin_buffer[tau];
  yin_buffer[tau] *= tau / running_sum;  // Normalization
}
```

### Advantages Over Autocorrelation

**Problems with Traditional Autocorrelation:**
- Sensitive to harmonics, often detects overtones instead of fundamental
- Poor noise immunity
- Difficult threshold setting

**YIN Solutions:**
1. **Difference-based**: Measures dissimilarity instead of similarity
2. **Smart Normalization**: τ-proportional normalization favors smaller periods
3. **Minimum Search**: Looks for difference minima instead of correlation maxima

### Implementation Enhancements

**1. Adaptive Thresholding**
```c
// Dynamic threshold adjustment based on signal quality
float adaptive_threshold = calculate_adaptive_threshold(filtered_power);
```

**2. Harmonic Verification**
```c
// Analyze multiple candidates to verify fundamental frequency
if (fundamental_confidence > FUNDAMENTAL_CONFIDENCE_THRESHOLD) {
  return current_period;
}
```

**3. Low-frequency Specialization**
```c
// Extended analysis window for frequencies below 120Hz
int analysis_window = estimated_period * 3;
```

### Algorithm Workflow

1. **Preprocessing**: DC offset removal, noise filtering
2. **Difference Calculation**: Compute self-difference for each τ
3. **Normalization**: Apply CMNDF to favor smaller periods
4. **Threshold Detection**: Find first local minimum below adaptive threshold
5. **Harmonic Verification**: Identify fundamental from multiple candidates
6. **Refinement**: Parabolic interpolation for sub-sample precision

## Technical Specifications

- **Sampling Rate**: 8 kHz
- **Buffer Size**: 1024 samples
- **Frequency Range**: 75-850 Hz (musical instrument range)
- **Precision**: Sub-sample accuracy with parabolic interpolation
- **Latency**: ~128ms (1 buffer duration)

## API Reference

### PitchDetector Class

#### `new(pin, volume_threshold: 50)`
Creates a new pitch detector instance.
- `pin`: ADC input pin number (eg: 26-28 for RP2040)
- `volume_threshold`: Minimum signal level for detection

#### `start`
Begins continuous pitch detection with DMA-based sampling.

#### `stop`
Stops pitch detection and releases resources.

#### `detect_pitch`
Returns the detected frequency in Hz, or `0.0` if no pitch detected.

#### `volume_threshold = value`
Sets the volume threshold for pitch detection.

## Performance Characteristics

The enhanced YIN implementation provides:

- **Fundamental Accuracy**: Correctly identifies the fundamental frequency, avoiding harmonic confusion
- **Noise Immunity**: Robust performance in noisy environments
- **Bass Precision**: Improved accuracy for low frequencies (75-120Hz)
- **Stability**: Consistent detection across varying signal conditions
