# The values are computed, not written: a literal past mrb_int would not
# compile on an MRB_INT32 build. 1 << 30 is the last 31-bit fixnum
# of a word-boxing build; 1 << 31 is past mrb_int on MRB_INT32, where
# the irep makes a Bignum of it.
class AOTProbeTest < Picotest::Test
  def test_add_past_the_fixnum
    assert_equal(1 << 31, AOTProbe.add(1 << 30, 1 << 30))
  end

  def test_add_past_mrb_int
    assert_equal(1 << 32, AOTProbe.add(1 << 31, 1 << 31))
  end

  def test_add_negative_past_mrb_int
    assert_equal(-(1 << 32), AOTProbe.add(-(1 << 31), -(1 << 31)))
  end

  def test_mul_past_mrb_int
    assert_equal(10_000_000_000, AOTProbe.mul(100_000, 100_000))
  end

  def test_sub_of_two_bignums_is_small
    assert_equal(7, AOTProbe.sub((1 << 40) + 7, 1 << 40))
  end

  def test_div_and_mod_of_a_bignum
    assert_equal(1 << 20, AOTProbe.div(1 << 40, 1 << 20))
    assert_equal(5, AOTProbe.mod((1 << 40) + 5, 1 << 20))
  end

  def test_shift_into_a_bignum
    assert_equal(1 << 40, AOTProbe.shl(1, 40))
  end

  def test_neg_of_a_bignum
    assert_equal(-(1 << 40), AOTProbe.neg(1 << 40))
  end

  def test_a_bignum_passes_through
    assert_equal(1 << 40, AOTProbe.same(1 << 40))
  end

  def test_float_to_i_past_mrb_int
    assert_equal(4_000_000_000_000, AOTProbe.to_int(4.0e12))
  end

  def test_bignum_to_f
    assert_in_delta(1.099511627776e12, AOTProbe.to_float(1 << 40), 1.0)
  end

  def test_compare_past_mrb_int
    assert(AOTProbe.later?(1 << 31, (1 << 31) + 1))
    assert_false(AOTProbe.later?((1 << 31) + 1, 1 << 31))
  end

  def test_elapsed_microseconds_past_36_minutes
    now = (1 << 31) + 500
    assert_equal(1500, AOTProbe.elapsed(now, now - 1500))
    assert_equal(0, AOTProbe.elapsed(now - 1, now))
  end

  def test_keyword_parameters
    assert_equal(6, AOTProbe.scale(2, by: 3))
    assert_equal(7, AOTProbe.scale(2, by: 3, offset: 1))
    assert_equal(7, AOTProbe.scale(2, offset: 1, by: 3))
  end

  def test_keyword_parameters_past_mrb_int
    assert_equal((1 << 40) + 5, AOTProbe.scale(1 << 20, by: 1 << 20, offset: 5))
  end

  def test_missing_required_keyword_raises
    assert_raise(ArgumentError) { AOTProbe.scale(2) }
  end

  def test_unknown_keyword_raises
    assert_raise(ArgumentError) { AOTProbe.scale(2, by: 3, extra: 1) }
  end

  def test_keyword_type_error_at_entry
    assert_raise(TypeError) { AOTProbe.scale(2, by: "3") }
  end

  def test_direct_call_with_keywords
    assert_equal(19, AOTProbe.scaled_twice(2, 3))
  end

  class KwCallee
    def call(v, by:)
      v * by
    end
  end

  def test_call_with_keywords_through_the_vm
    assert_equal(12, AOTProbe.call_with_keywords(KwCallee.new, 4))
  end

  def test_block_parameter_called_in_a_loop
    seen = []
    assert_equal(3, AOTProbe.each_index_of([5, 6, 7]) { |v, i| seen << v * 10 + i })
    assert_equal([50, 61, 72], seen)
  end

  def test_block_writes_an_outer_local
    sum = 0
    AOTProbe.each_index_of([1, 2, 3]) { |v, _| sum += v }
    assert_equal(6, sum)
  end

  def test_break_with_a_value_leaves_the_compiled_def
    r = AOTProbe.each_index_of([1, 2, 3]) { |v, _| break v * 100 if v == 2 }
    assert_equal(200, r)
  end

  def test_next_in_the_block
    seen = []
    AOTProbe.each_index_of([1, 2, 3]) { |v, _| next if v == 2; seen << v }
    assert_equal([1, 3], seen)
  end

  def helper_with_a_return_in_the_block
    AOTProbe.each_index_of([1, 2, 3]) { |v, _| return v if v == 2 }
    :not_reached
  end

  def test_return_from_the_block_leaves_the_method
    assert_equal(2, helper_with_a_return_in_the_block)
  end

  def test_an_exception_in_the_block_propagates
    assert_raise(RuntimeError) { AOTProbe.each_index_of([1]) { |_, _| raise "boom" } }
  end

  def test_yield_twice
    assert_equal(12, AOTProbe.twice(3) { |v| v * 2 })
  end

  def test_yield_result_past_mrb_int
    assert_equal(1 << 41, AOTProbe.twice(1) { |_| 1 << 40 })
  end

  def test_yield_result_of_another_class
    assert_equal("abab", AOTProbe.twice(1) { |_| "ab" })
  end

  def test_yield_without_a_block_raises
    assert_raise(LocalJumpError) { AOTProbe.twice(1) }
  end

  def test_block_given
    assert_equal(5, AOTProbe.maybe(5))
    assert_equal(6, AOTProbe.maybe(5) { |v| v + 1 })
    assert_equal(5, AOTProbe.without_block(5))
  end

  def test_block_forwarded_to_a_compiled_def
    seen = []
    assert_equal(2, AOTProbe.forward([8, 9]) { |v, i| seen << [v, i] })
    assert_equal([[8, 0], [9, 1]], seen)
  end

  def test_symbol_forwarded_to_a_compiled_def
    r = AOTProbe.call_or_value(7, &:to_s)
    assert_equal("7", r)
  end

  def test_block_forwarded_to_a_core_method
    assert_equal([2, 4], AOTProbe.forward_to_vm([1, 2]) { |v| v * 2 })
    assert_equal(["1", "2"], AOTProbe.forward_to_vm([1, 2], &:to_s))
  end

  class KwBlockCallee
    def call(v, by:)
      yield v * by
    end
  end

  def test_block_forwarded_with_keywords
    assert_equal(13, AOTProbe.forward_with_keywords(KwBlockCallee.new, 4) { |v| v + 1 })
  end

  def test_block_parameter_absent
    assert_equal(7, AOTProbe.call_or_value(7))
    assert_equal(8, AOTProbe.call_or_value(7) { |v| v + 1 })
  end

  def test_join_on_an_array_is_not_a_task_join
    assert_equal("1,2", AOTProbe.joined([1, 2, 3]))
  end

  def test_equality_beside_a_define_method_block
    assert_true(AOTProbe::Tagged.new == AOTProbe::Tagged.new)
    assert_false(AOTProbe::Tagged.new == 1)
    assert_true(AOTProbe::Tagged.new.one(1))
    assert_false(AOTProbe::Tagged.new.two(2))
  end

  def test_included_hook_runs_while_the_irep_loads
    assert_equal(3, AOTProbe::Counted.count_of([1, 2, 3]))
  end

  def test_class_body_call_names_the_singleton_only
    assert_equal(2, AOTProbe::DEFINED)
    assert_equal(6, AOTProbe::Builder.new.define(3))
  end

  def test_clock_keeps_a_bignum_timestamp
    clock = AOTProbe::Clock.new
    clock.observe((1 << 31) - 100)
    assert_equal(200, clock.observe((1 << 31) + 100))
    assert_equal((1 << 31) + 100, clock.last_us)
  end
end
