# bitbench - benchmark String#bit_index and String#bit_field_get
#
# The same comparisons as the CRuby benchmark of [Feature #22399],
# scaled to the RAM of a microcontroller: 8 KiB buffers (65,536 bits).
# Each row is the median of 5 runs. The memory column is the change of
# the VM heap in use across the run.

require 'benchmark'

SIZE = 8 * 1024
BITS = SIZE * 8
REPEAT = 5

sparse = "\x00" * SIZE                     # 64 set bits
i = 0
while i < 64
  sparse.bit_set(i * 1024 + 3)
  i += 1
end
dense = "\xAA" * SIZE                      # every other bit set
runs = ""                                  # 8 runs of 8,192 bits
i = 0
while i < 8
  runs << ((i % 2 == 0) ? "\x00" : "\xFF") * 1024
  i += 1
end

# --- offsets of the set bits: a bit_set? loop against a bit_index loop ---
# The offsets are summed, not collected, so that the row measures the
# search and not an Array of 32,768 Integers.

def offsets_by_loop(s)
  sum = 0
  i = 0
  while i < BITS
    sum += i if s.bit_set?(i)
    i += 1
  end
  sum
end

def offsets_by_bit_index(s)
  sum = 0
  i = -1
  while (i = s.bit_index(1, i + 1))
    sum += i
  end
  sum
end

# C iterator prototype (not part of the proposal)
def offsets_by_each_bit_index(s)
  sum = 0
  s.each_bit_index(1) { |i| sum += i }
  sum
end

# --- run length and all runs: a bit_get loop against bit_index ---

def run_length_by_loop(s, offset)
  bit = s.bit_get(offset)
  i = offset + 1
  i += 1 while i < BITS && s.bit_get(i) == bit
  i - offset
end

def run_length_by_bit_index(s, offset)
  bit = s.bit_get(offset)
  (s.bit_index(1 - bit, offset) || BITS) - offset
end

def runs_by_loop(s)
  count = 0
  i = 0
  while i < BITS
    bit = s.bit_get(i)
    j = i + 1
    j += 1 while j < BITS && s.bit_get(j) == bit
    count += 1
    i = j
  end
  count
end

def runs_by_bit_index(s)
  count = 0
  i = 0
  while i < BITS
    bit = s.bit_get(i)
    j = s.bit_index(1 - bit, i) || BITS
    count += 1
    i = j
  end
  count
end

# C iterator prototype (not part of the proposal)
def runs_by_each_bit_run(s)
  count = 0
  s.each_bit_run { |bit, off, len| count += 1 }
  count
end

# --- field reads: a bit_get fold, getbyte masks, bit_field_get ---

def field_by_bit_get(s, off, len, lsb_first)
  v = 0
  i = 0
  while i < len
    bit = s.bit_get(off + i, lsb_first: lsb_first)
    v |= bit << (lsb_first ? i : len - 1 - i)
    i += 1
  end
  v
end

def frames_by_bit_get(s, n)
  sum = 0
  i = 0
  while i < n
    off = i * 36
    sum += field_by_bit_get(s, off, 12, true) + field_by_bit_get(s, off + 12, 10, true) + field_by_bit_get(s, off + 22, 14, true)
    i += 1
  end
  sum
end

def frames_by_getbyte(s, n)
  sum = 0
  i = 0
  while i < n
    b = i * 36 >> 3
    if i % 2 == 0
      temp =  s.getbyte(b)         | ((s.getbyte(b+1) & 0x0F) << 8)
      hum  = (s.getbyte(b+1) >> 4) | ((s.getbyte(b+2) & 0x3F) << 4)
      co2  = (s.getbyte(b+2) >> 6) |  (s.getbyte(b+3) << 2) | ((s.getbyte(b+4) & 0x0F) << 10)
    else
      temp = (s.getbyte(b) >> 4)   |  (s.getbyte(b+1) << 4)
      hum  =  s.getbyte(b+2)       | ((s.getbyte(b+3) & 0x03) << 8)
      co2  = (s.getbyte(b+3) >> 2) |  (s.getbyte(b+4) << 6)
    end
    sum += temp + hum + co2
    i += 1
  end
  sum
end

def frames_by_bit_field_get(s, n)
  sum = 0
  i = 0
  while i < n
    off = i * 36
    sum += s.bit_field_get(off, 12) + s.bit_field_get(off + 12, 10) + s.bit_field_get(off + 22, 14)
    i += 1
  end
  sum
end

puts "bitbench: #{SIZE} byte buffers, median of #{REPEAT} runs"
puts

puts "== offsets of the set bits (bit_offsets equivalent) =="
raise "sparse mismatch" unless offsets_by_loop(sparse) == offsets_by_bit_index(sparse)
raise "dense mismatch" unless offsets_by_loop(dense) == offsets_by_bit_index(dense)
raise "each_bit_index mismatch" unless offsets_by_each_bit_index(dense) == offsets_by_bit_index(dense)
Benchmark.bm(28, repeat: REPEAT) do |x|
  x.report("sparse: bit_set? loop") { offsets_by_loop(sparse) }
  x.report("sparse: bit_index loop") { offsets_by_bit_index(sparse) }
  x.report("sparse: each_bit_index (C)") { offsets_by_each_bit_index(sparse) }
  x.report("dense: bit_set? loop") { offsets_by_loop(dense) }
  x.report("dense: bit_index loop") { offsets_by_bit_index(dense) }
  x.report("dense: each_bit_index (C)") { offsets_by_each_bit_index(dense) }
end
puts

puts "== run length at offset 0 (bit_run_length equivalent, x10) =="
raise "run length mismatch" unless run_length_by_loop(runs, 0) == 8192 && run_length_by_bit_index(runs, 0) == 8192
Benchmark.bm(28, repeat: REPEAT) do |x|
  x.report("bit_get loop") { i = 0; while i < 10; run_length_by_loop(runs, 0); i += 1; end }
  x.report("bit_index") { i = 0; while i < 10; run_length_by_bit_index(runs, 0); i += 1; end }
end
puts

puts "== all 8 runs (bit_runs equivalent) =="
raise "runs mismatch" unless runs_by_loop(runs) == 8 && runs_by_bit_index(runs) == 8
raise "each_bit_run mismatch" unless runs_by_each_bit_run(runs) == 8
Benchmark.bm(28, repeat: REPEAT) do |x|
  x.report("bit_get loop") { runs_by_loop(runs) }
  x.report("bit_index loop") { runs_by_bit_index(runs) }
  x.report("each_bit_run (C)") { runs_by_each_bit_run(runs) }
end
puts

puts "== all #{BITS} runs of 1 bit (bit_runs equivalent, short runs) =="
raise "short runs mismatch" unless runs_by_loop(dense) == BITS && runs_by_bit_index(dense) == BITS
raise "each_bit_run mismatch" unless runs_by_each_bit_run(dense) == BITS
Benchmark.bm(28, repeat: REPEAT) do |x|
  x.report("bit_get loop") { runs_by_loop(dense) }
  x.report("bit_index loop") { runs_by_bit_index(dense) }
  x.report("each_bit_run (C)") { runs_by_each_bit_run(dense) }
end
puts

puts "== one 16-bit field at bit offset 4 of a 4-byte header (x1000) =="
hdr = "\x12\x34\x81\x80"
[true, false].each do |order|
  raise "field mismatch" unless hdr.bit_field_get(4, 16, lsb_first: order) == field_by_bit_get(hdr, 4, 16, order)
  label = order ? "lsb_first" : "msb_first"
  Benchmark.bm(28, repeat: REPEAT) do |x|
    x.report("#{label}: bit_get fold") { i = 0; while i < 1000; field_by_bit_get(hdr, 4, 16, order); i += 1; end }
    x.report("#{label}: bit_field_get") { i = 0; while i < 1000; hdr.bit_field_get(4, 16, lsb_first: order); i += 1; end }
  end
end
puts

frames = BITS / 36
puts "== unaligned 12/10/14-bit sensor frames (#{frames} frames) =="
raise "frames mismatch" unless frames_by_bit_get(dense, frames) == frames_by_bit_field_get(dense, frames)
raise "frames mismatch" unless frames_by_getbyte(dense, frames) == frames_by_bit_field_get(dense, frames)
Benchmark.bm(28, repeat: REPEAT) do |x|
  x.report("bit_get fold loop") { frames_by_bit_get(dense, frames) }
  x.report("getbyte with masks") { frames_by_getbyte(dense, frames) }
  x.report("bit_field_get") { frames_by_bit_field_get(dense, frames) }
end
