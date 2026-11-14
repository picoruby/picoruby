class DataMemoryTest < Picotest::Test
  def setup
    skip 'mruby/c reference counting regression' unless femtoruby?
    require 'data'
    require 'picorubyvm'
  end

  def test_instances_release_members
    klass = Data.define(:value)
    discard_instances(klass)
    used_memory
    before = used_memory
    discard_instances(klass)
    after = used_memory
    assert_equal before, after
  end

  def test_definitions_release_member_keys
    small = [:value]
    large = []
    32.times { |i| large << "member_#{i}".to_sym }
    discard_definitions(small)
    discard_definitions(large)
    used_memory
    before = used_memory
    discard_definitions(small)
    small_growth = used_memory - before
    before = used_memory
    discard_definitions(large)
    large_growth = used_memory - before
    # Class registration retains memory independently of the member count.
    # Allow less than 1 KiB of allocator rounding/registration differences.
    # The regression retains 20 arrays with 31 extra values each.
    # mrbc_value size and allocator overhead differ on 32- and 64-bit builds.
    # Even at 4 bytes per value, 20 * 31 * 4 = 2480 bytes exceeds 1 KiB.
    # Compare growth within one build rather than assuming an exact byte size.
    # This detects that leak, not every possible small leak.
    growth_difference = large_growth - small_growth
    if growth_difference >= 1024
      puts "Data definition memory growth (bytes): small=#{small_growth}, large=#{large_growth}, difference=#{growth_difference}, expected difference < 1024"
    end
    assert_true growth_difference < 1024
  end

  def used_memory
    PicoRubyVM.memory_statistics[:used]
  end

  def discard_instances(klass)
    100.times do
      obj = klass.new('payload')
      obj = nil
    end
    nil
  end

  def discard_definitions(keys)
    20.times do
      klass = Data.define(*keys)
      klass = nil
    end
    nil
  end
end
