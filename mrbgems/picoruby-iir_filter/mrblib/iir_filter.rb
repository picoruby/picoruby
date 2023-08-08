# Infinite Impulse Response (IIR) Filter
# https://en.wikipedia.org/wiki/Infinite_impulse_response
class IIRFilter
  COEFF_SHIFT = 4
  MAX_COEFF_SHIFT = 8

  def initialize(bit_depth)
    if bit_depth < 6
      raise ArgumentError, "Bit depth must be greater than 6"
    end
    @feedforward = 0b11 << bit_depth - 7
    @filterd_value = 0
  end

  def filter(raw_value)
    # Left shift input by 256 to allow divisions up to 256
    raw_value <<= MAX_COEFF_SHIFT
    if raw_value < (@filterd_value - @feedforward) || (@filterd_value + @feedforward) < raw_value
      # Pass the filter input as it is for fast changes in input
      @filterd_value = raw_value
    else
      # If not the first sample then based on filter coefficient, filter the input signal
      @filterd_value += (raw_value - @filterd_value) >> COEFF_SHIFT
    end
    # Compensate filter result for left shift of 8 and round off
    return (@filterd_value >> MAX_COEFF_SHIFT) + ((@filterd_value >> MAX_COEFF_SHIFT - 1) & 1)
   end
end
