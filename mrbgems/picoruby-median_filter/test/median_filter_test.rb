class MedianFilterTest < Picotest::Test
  def test_default_window_is_three
    filter = MedianFilter.new
    assert_equal(3, filter.window)
  end

  def test_first_sample_passes_through
    filter = MedianFilter.new
    assert_equal(20, filter.update(20))
  end

  def test_single_spike_is_rejected
    filter = MedianFilter.new
    filter.update(20)
    filter.update(20)
    assert_equal(20, filter.update(95))
    assert_equal(21, filter.update(21))
  end

  def test_low_spike_is_rejected
    filter = MedianFilter.new
    filter.update(20)
    filter.update(20)
    assert_equal(20, filter.update(2))
  end

  def test_follows_monotonic_change_with_one_sample_lag
    filter = MedianFilter.new
    filter.update(10)
    filter.update(20)
    assert_equal(20, filter.update(30))
    assert_equal(30, filter.update(40))
  end

  def test_accepts_floats
    filter = MedianFilter.new
    filter.update(1.5)
    filter.update(1.6)
    assert_equal(1.6, filter.update(99.9))
  end

  def test_window_five_rejects_two_consecutive_spikes
    filter = MedianFilter.new(window: 5)
    filter.update(10)
    filter.update(10)
    filter.update(10)
    assert_equal(10, filter.update(90))
    assert_equal(10, filter.update(90))
  end

  def test_reset_forgets_history
    filter = MedianFilter.new
    filter.update(10)
    filter.update(10)
    filter.reset
    assert_equal(0, filter.size)
    assert_equal(50, filter.update(50))
  end

  def test_size_is_capped_at_window
    filter = MedianFilter.new
    5.times { |i| filter.update(i) }
    assert_equal(3, filter.size)
  end

  def test_even_window_is_rejected
    assert_raise(ArgumentError) do
      MedianFilter.new(window: 4)
    end
  end

  def test_zero_window_is_rejected
    assert_raise(ArgumentError) do
      MedianFilter.new(window: 0)
    end
  end

  def test_negative_window_is_rejected
    assert_raise(ArgumentError) do
      MedianFilter.new(window: -3)
    end
  end

  def test_window_can_grow_at_runtime
    filter = MedianFilter.new
    filter.window = 5
    assert_equal(5, filter.window)
    filter.update(10)
    filter.update(10)
    filter.update(10)
    assert_equal(10, filter.update(90))
    assert_equal(10, filter.update(90))
  end

  def test_shrinking_window_discards_oldest_samples
    filter = MedianFilter.new(window: 5)
    filter.update(10)
    filter.update(20)
    filter.update(30)
    filter.update(40)
    filter.update(50)
    filter.window = 3
    assert_equal(3, filter.size)
    # Retained samples are the newest three (30, 40, 50); feeding 90
    # rolls them to (40, 50, 90) whose median is 50
    assert_equal(50, filter.update(90))
  end

  def test_window_writer_rejects_even_value
    filter = MedianFilter.new
    assert_raise(ArgumentError) do
      filter.window = 2
    end
  end
end
