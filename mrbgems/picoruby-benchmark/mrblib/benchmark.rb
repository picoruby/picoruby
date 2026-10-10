# Benchmark for PicoRuby.
#
# The API follows the CRuby Benchmark library: realtime, measure, bm and
# bmbm. The clock is Machine.uptime_us, so the times are wall-clock
# microseconds. Two additions serve microcontrollers:
#
# - `repeat:` runs a block several times and keeps the median, because a
#   single run on a board is disturbed by timer interrupts and GC.
# - Each measurement records the change of the VM heap in use, from
#   PicoRubyVM.memory_statistics, so an allocation-heavy approach shows up
#   next to its time.
require 'machine'
require 'picorubyvm'

module Benchmark
  CAPTION = "        real       memory\n"

  # Where the rows go. nil means Kernel#print. A test or a logger sets an
  # object that responds to print.
  OPTIONS = {output: nil}

  def self.output=(io)
    OPTIONS[:output] = io
  end

  def self.output
    OPTIONS[:output]
  end

  def self.emit(str)
    io = OPTIONS[:output]
    if io
      io.print(str)
    else
      print str
    end
    nil
  end

  # The result of one measurement.
  class Tms
    attr_reader :label, :real_us, :memory

    def initialize(real_us, memory, label = "")
      @real_us = real_us
      @memory = memory
      @label = label
    end

    # Wall-clock time in seconds.
    def real
      @real_us.to_f / 1_000_000
    end

    # One row of a bm report: the time in seconds and the memory in bytes.
    def format(label_width = 0)
      memory_s = @memory ? @memory.to_s : "-"
      @label.ljust(label_width) + sprintf("%4d.%06d s", @real_us / 1_000_000, @real_us % 1_000_000) + memory_s.rjust(9) + " B\n"
    end

    def to_s
      format
    end
  end

  # Collects the blocks of a bm or bmbm call and prints one row per block.
  class Report
    attr_reader :list

    def initialize(label_width, repeat)
      @label_width = label_width
      @repeat = repeat
      @list = []
    end

    def report(label = "", repeat: nil, &block)
      tms = Benchmark.measure(label, repeat: repeat || @repeat, &block)
      Benchmark.emit(tms.format(@label_width))
      @list << tms
      tms
    end

    def item(label = "", repeat: nil, &block)
      report(label, repeat: repeat, &block)
    end
  end

  # Collects the blocks of a bmbm call for the rehearsal.
  class Job
    attr_reader :list

    def initialize
      @list = []
    end

    def report(label = "", &block)
      @list << [label, block]
      self
    end

    def item(label = "", &block)
      report(label, &block)
    end
  end

  # Heap in use, or nil when the allocator does not report it.
  def self.memory_used
    PicoRubyVM.memory_statistics[:used]
  rescue
    nil
  end

  # Runs the block once and returns the wall-clock microseconds.
  def self.realtime_us(&block)
    t0 = Machine.uptime_us
    block.call
    Machine.uptime_us - t0
  end

  # Runs the block once and returns the wall-clock seconds.
  def self.realtime(&block)
    realtime_us(&block).to_f / 1_000_000
  end

  # Runs the block `repeat` times and returns the Tms of the median run.
  # With repeat: 1, it is one run. An even repeat takes the upper median.
  def self.measure(label = "", repeat: 1, &block)
    raise ArgumentError, "repeat must be positive" if repeat < 1
    times = [] #: Array[Integer]
    memories = [] #: Array[Integer?]
    i = 0
    while i < repeat
      before = memory_used
      us = realtime_us(&block)
      after = memory_used
      times << us
      memories << ((before && after) ? after - before : nil)
      i += 1
    end
    median = times.sort[repeat / 2]
    Tms.new(median, memories[times.index(median) || 0], label)
  end

  # Prints a caption, then one row per report call. Returns the Array of Tms.
  def self.bm(label_width = 0, repeat: 1)
    emit(" " * label_width + CAPTION)
    report = Report.new(label_width, repeat)
    yield report
    report.list
  end

  # Like bm, but runs every block once as a rehearsal first. The rehearsal
  # warms the caches and lets the heap settle. Then the blocks run again
  # and the rows of that run are returned.
  #
  # Not available on mruby/c. The rehearsal stores the report blocks and
  # calls them after the yield returned. A block written inside the bmbm
  # block resolves its upvars through the callinfo of that outer block,
  # and mruby/c has freed it by then (no closure environment).
  def self.bmbm(label_width = 0, repeat: 1)
    if RUBY_ENGINE == "mruby/c"
      raise NotImplementedError, "Benchmark.bmbm is not available on mruby/c. Use Benchmark.bm"
    end
    job = Job.new
    yield job
    list = job.list
    size = list.size
    width = label_width
    i = 0
    while i < size
      label = list[i][0]
      width = label.size if width < label.size
      i += 1
    end

    emit("Rehearsal " + "-" * (width + CAPTION.size - 11) + "\n")
    i = 0
    while i < size
      emit(measure(list[i][0], &list[i][1]).format(width))
      i += 1
    end
    emit("-" * (width + CAPTION.size - 1) + "\n\n")

    emit(" " * width + CAPTION)
    results = [] #: Array[Tms]
    i = 0
    while i < size
      tms = measure(list[i][0], repeat: repeat, &list[i][1])
      emit(tms.format(width))
      results << tms
      i += 1
    end
    results
  end
end
