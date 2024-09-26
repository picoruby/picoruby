class PicoRubyVM
  def self.profile_alloc
    unless block_given?
      raise ArgumentError, "block not given"
    end
    start_alloc_profiling
    yield
    stop_alloc_profiling
    alloc_profiling_result
  end
end
