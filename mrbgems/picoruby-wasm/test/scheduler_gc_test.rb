class WasmSchedulerGCTest < Picotest::Test
  def test_scheduler_driven_gc_can_be_enabled_without_breaking_tasks
    origin_gen = GC.generational_mode
    begin
      GC.scheduler_driven = true

      assert_true GC.scheduler_driven
      assert_false GC.generational_mode

      ran = false
      task = Task.new do
        ran = true
        nil
      end

      20.times do
        break if task.status == :DORMANT
        sleep_ms 1
      end

      assert_true ran
      assert_equal :DORMANT, task.status
    ensure
      GC.scheduler_driven = false
      GC.generational_mode = origin_gen
      GC.start
    end
  end

  def test_scheduler_driven_gc_allows_allocations_across_wasm_idle_points
    origin_gen = GC.generational_mode
    origin_interval = GC.interval_ratio
    begin
      GC.generational_mode = false
      GC.start

      GC.step_limit = 256
      GC.interval_ratio = 100
      GC.scheduler_driven = true

      count = 0
      30.times do
        400.times { "x" * 8 }
        count += 1
        sleep_ms 1
      end

      assert_equal 30, count
    ensure
      GC.scheduler_driven = false
      GC.step_limit = 0
      GC.interval_ratio = origin_interval
      GC.generational_mode = origin_gen
      GC.start
    end
  end
end
