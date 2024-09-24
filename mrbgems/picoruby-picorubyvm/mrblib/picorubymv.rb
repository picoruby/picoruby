class PicoRubyVM
  def self.profile_alloc
    unless block_given?
      raise ArgumentError, "block not given"
    end
    start_alloc_profiling
    yield
    stop_alloc_profiling
    get_alloc_peak
  end
end
