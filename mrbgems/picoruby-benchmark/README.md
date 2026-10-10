# picoruby-benchmark

A `Benchmark` module for PicoRuby. The API follows the CRuby Benchmark library, so a benchmark script written for CRuby runs on a board with few changes.

## Usage

```ruby
require 'benchmark'

Benchmark.realtime { work }            # => 0.012345 (seconds)
Benchmark.realtime_us { work }         # => 12345 (microseconds)

tms = Benchmark.measure("work") { work }
tms.real      # => 0.012345
tms.real_us   # => 12345
tms.memory    # => 1024 (bytes of VM heap in use after the run minus before)

Benchmark.bm(12) do |x|
  x.report("getbyte")  { by_getbyte }
  x.report("bit_get")  { by_bit_get }
end
```

The output of `bm` is:

```
                    real       memory
getbyte         0.054012 s        0 B
bit_get         1.219340 s        0 B
```

`bmbm` runs every block once as a rehearsal, prints the rehearsal, and then runs the blocks again for the report. Use it when the first run pays for a cold cache or for heap growth.

`bmbm` is not available on mruby/c (FemtoRuby). It raises `NotImplementedError`. The rehearsal stores the report blocks and calls them after the outer block returned. mruby/c has no closure environment, so a stored block cannot reach the variables of that outer block. Use `bm` with `repeat:` instead.

## Differences from CRuby

- The clock is `Machine.uptime_us`. The rows show the wall-clock time only. There are no user, system and total columns.
- `measure`, `bm` and `bmbm` take `repeat:`. The block runs that many times, and the row shows the median run. A single run on a board is disturbed by timer interrupts and by GC, so use `repeat: 5` for a number that goes into a report.
- Each `Tms` records `memory`, the change of the VM heap in use from `PicoRubyVM.memory_statistics`. It is `nil` when the allocator does not report statistics.
- `Tms#real` is a Float. On a build without Float, use `Tms#real_us`.
- `Benchmark.output = io` sends the rows to any object that responds to `print`, for example a logger or a String collector in a test. The default, `nil`, prints to the console.
