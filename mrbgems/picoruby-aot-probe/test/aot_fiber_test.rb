# Enumerator and Fiber around compiled defs (docs/architecture.md, 4.3).
# A block under a compiled method cannot Fiber.yield: external iteration
# (next, peek) of an Enumerator of a compiled method raises FiberError
# where the irep answers. The twin runs these tests on the irep and on
# the AOT builds and wants the same summary, so each test asks which one
# it runs on and checks that build's answer. Internal iteration is the
# same on both.
class AOTFiberTest < Picotest::Test
  CROSS = "FiberError: can't cross C function boundary"
  PASS = "RuntimeError: can't pass across C function boundary"

  def outcome
    yield
  rescue Exception => e
    "#{e.class}: #{e.message}"
  end

  # true when AOTProbe's defs are compiled: one external step tells
  def compiled?
    outcome { AOTProbe.enum_for(:each_index_of, [1]).next } == CROSS
  end

  def test_enum_for_compiled_each_to_a
    assert_equal([[5, 0], [6, 1]], AOTProbe.enum_for(:each_index_of, [5, 6]).to_a)
  end

  def test_lazy_over_compiled_each
    got = AOTProbe.enum_for(:each_index_of, [1, 2, 3, 4]).lazy.map { |v, _| v * 2 }.select { |v| 4 < v }.to_a
    assert_equal([6, 8], got)
  end

  def test_next_inside_compiled_block
    e = [1, 2].each
    assert_equal(3, AOTProbe.twice(0) { |_| e.next })
  end

  def test_enum_for_compiled_each_next
    got = outcome do
      e = AOTProbe.enum_for(:each_index_of, [5, 6])
      [e.next, e.next]
    end
    assert_equal(compiled? ? CROSS : [[5, 0], [6, 1]], got)
  end

  def test_enum_for_compiled_each_peek
    got = outcome do
      e = AOTProbe.enum_for(:each_index_of, [5, 6])
      [e.peek, e.next]
    end
    assert_equal(compiled? ? CROSS : [[5, 0], [5, 0]], got)
  end

  def test_generator_over_compiled_each_next
    got = outcome do
      g = Enumerator.new { |y| AOTProbe.each_index_of([7, 8]) { |v, _| y << v } }
      [g.next, g.next]
    end
    assert_equal(compiled? ? CROSS : [7, 8], got)
  end

  def test_generator_over_compiled_yield_next
    got = outcome do
      g = Enumerator.new { |y| AOTProbe.twice(3) { |x| y << x; x } }
      [g.next, g.next]
    end
    assert_equal(compiled? ? CROSS : [3, 3], got)
  end

  def test_next_inside_compiled_block_with_task_pass
    got = nil
    me = self # a task block runs with self = main
    t = Task.new do
      got = me.outcome do
        g = Enumerator.new { |y| Task.pass; y << 1; Task.pass; y << 2 }
        AOTProbe.twice(0) { |_| g.next }
      end
    end
    t.join
    assert_equal(compiled? ? PASS : 3, got)
  end

  def test_compiled_each_next_in_tasks
    got = []
    me = self # a task block runs with self = main
    tasks = [0, 1].map do |k|
      Task.new do
        got[k] = me.outcome do
          e = AOTProbe.enum_for(:each_index_of, [k, k + 10])
          [e.next, e.next]
        end
      end
    end
    tasks.each(&:join)
    want = if compiled?
      [CROSS, CROSS]
    else
      [[[0, 0], [10, 1]], [[1, 0], [11, 1]]]
    end
    assert_equal(want, got)
  end
end
