class BenchmarkTest < Picotest::Test
  class Sink
    attr_reader :text

    def initialize
      @text = ""
    end

    def print(str)
      @text << str
    end
  end

  def setup
    @sink = Sink.new
    Benchmark.output = @sink
  end

  def teardown
    Benchmark.output = nil
  end

  def busy
    i = 0
    while i < 1000
      i += 1
    end
  end

  def test_realtime_us_is_a_non_negative_integer
    us = Benchmark.realtime_us { busy }
    assert(us.is_a?(Integer))
    assert(us >= 0)
  end

  def test_realtime_is_seconds
    sec = Benchmark.realtime { busy }
    assert(sec.is_a?(Float))
    assert(sec >= 0.0)
    assert(sec < 1.0)
  end

  def test_measure_returns_tms_with_label
    tms = Benchmark.measure("busy") { busy }
    assert_equal("busy", tms.label)
    assert(tms.real_us >= 0)
    assert_equal(tms.real_us.to_f / 1_000_000, tms.real)
  end

  def test_measure_runs_the_block_repeat_times
    count = 0
    Benchmark.measure("count", repeat: 5) { count += 1 }
    assert_equal(5, count)
  end

  def test_measure_rejects_zero_repeat
    assert_raise(ArgumentError) do
      Benchmark.measure("x", repeat: 0) { }
    end
  end

  def test_memory_is_integer_or_nil
    tms = Benchmark.measure("alloc") { "x" * 64 }
    memory = tms.memory
    assert(memory.nil? || memory.is_a?(Integer))
  end

  def test_tms_format_has_label_time_and_memory
    row = Benchmark::Tms.new(1_234_567, 512, "label").format(8)
    assert_equal("label      1.234567 s      512 B\n", row)
  end

  def test_tms_format_without_memory
    row = Benchmark::Tms.new(7, nil, "x").format
    assert_equal("x   0.000007 s        - B\n", row)
  end

  def test_bm_returns_one_tms_per_report
    results = Benchmark.bm(6) do |x|
      x.report("one") { busy }
      x.item("two") { busy }
    end
    assert_equal(2, results.size)
    assert_equal("one", results[0].label)
    assert_equal("two", results[1].label)
    lines = @sink.text.split("\n")
    assert_equal(3, lines.size)
    assert_equal("      " + Benchmark::CAPTION.chomp, lines[0])
    assert_equal("one   ", lines[1][0, 6])
    assert_equal("two   ", lines[2][0, 6])
  end

  def test_output_nil_prints_to_stdout_by_default
    Benchmark.output = nil
    assert_nil(Benchmark.output)
  end

  def test_bmbm_raises_on_mrubyc
    skip "mruby/c only" unless RUBY_ENGINE == "mruby/c"
    assert_raise(NotImplementedError) do
      Benchmark.bmbm { |x| x.report("noop") { } }
    end
  end

  def test_bmbm_runs_each_block_twice_per_repeat
    skip "bmbm is not available on mruby/c" if RUBY_ENGINE == "mruby/c"
    count = 0
    results = Benchmark.bmbm(repeat: 2) do |x|
      x.report("count") { count += 1 }
    end
    # one rehearsal run plus two measured runs
    assert_equal(3, count)
    assert_equal(1, results.size)
    assert_equal("count", results[0].label)
    assert_equal("Rehearsal ", @sink.text[0, 10])
  end
end
