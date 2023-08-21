# Infinite Impulse Response (IIR) Filter
# https://en.wikipedia.org/wiki/Infinite_impulse_response
class IIRFilter
  COEFF_SHIFT = 4
  MAX_COEFF_SHIFT = 8
  FILTER_FEEDFORWARD = 100 * 256

  def initialize
    @filterd_value = 0
  end

  def filter(raw_value)
    # Left shift input by 256 to allow divisions up to 256
    raw_value <<= MAX_COEFF_SHIFT
    if raw_value < (@filterd_value - FILTER_FEEDFORWARD) || (@filterd_value + FILTER_FEEDFORWARD) < raw_value
      # Pass the filter input as it is for fast changes in input
      @filterd_value = raw_value
      puts "case 1 filterd_value: #{@filterd_value}"
    else
      # If not the first sample then based on filter coefficient, filter the input signal
      @filterd_value += (raw_value - @filterd_value) >> COEFF_SHIFT
      puts "case 2 filterd_value: #{@filterd_value}"
    end
    # Compensate filter result for left shift of 8 and round off
    return (@filterd_value >> MAX_COEFF_SHIFT) + ((@filterd_value >> MAX_COEFF_SHIFT - 1) & 1)
   end
end
