# Median filter
# https://en.wikipedia.org/wiki/Median_filter
#
# Rejects outlier samples from noisy sensor readings. Unlike linear
# filters (moving average, IIR), a median filter removes single-sample
# spikes completely instead of smearing them into the output.
#
#   filter = MedianFilter.new           # window: 3
#   filter.update(20)  # => 20
#   filter.update(20)  # => 20
#   filter.update(95)  # => 20  (spike rejected)
#   filter.update(21)  # => 21
#
# A window of N samples tolerates up to (N - 1) / 2 consecutive
# outliers and delays a genuine change by the same number of samples.
class MedianFilter
  def initialize(window: 3)
    @samples = []
    self.window = window
  end

  attr_reader :window

  # Changes the window size on the fly. Shrinking the window discards
  # the oldest retained samples immediately so the next median is
  # computed over at most `window` samples.
  def window=(value)
    unless value.is_a?(Integer) && 0 < value && value % 2 == 1
      raise ArgumentError, "window must be a positive odd Integer"
    end
    @window = value
    @samples.shift while value < @samples.size
    value
  end

  # Feeds a new sample and returns the median of the retained samples.
  # Until the window fills up, the median of the samples seen so far is
  # returned (the lower middle one while the count is even), so the
  # first sample passes through as is.
  def update(value)
    @samples << value
    @samples.shift if @window < @samples.size
    sorted = @samples.sort
    median = sorted[(sorted.size - 1) / 2]
    # @type var median: Integer | Float
    median
  end

  # Forgets all retained samples. Call this when the signal source
  # restarts (e.g. a distance sensor coming back into range) so that
  # stale samples do not distort the next medians.
  def reset
    @samples.clear
    nil
  end

  # Number of currently retained samples (0 to window).
  def size
    @samples.size
  end
end
