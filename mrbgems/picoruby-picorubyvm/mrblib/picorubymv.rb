class PicoRubyVM
  def self.alloc_profile
    unless block_given?
      raise ArgumentError, "block not given"
    end
    alloc_start_profiling
    yield
    alloc_stop_profiling
    alloc_profiling_result
  end
end
