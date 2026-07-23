class JSBasicObjectTest < Picotest::Test
  def test_superclass_is_basic_object
    assert_equal(BasicObject, JS::Object.superclass)
  end

  def test_predicates
    obj = JS.global.create_object
    assert_equal(false, obj.nil?)
    assert_equal(true, obj.is_a?(JS::Object))
    assert_equal(true, obj.kind_of?(JS::Object))
    assert_equal(true, obj.instance_of?(JS::Object))
    assert_equal(false, obj.is_a?(JS::Element))
  end

  def test_respond_to_uses_method_table_only
    promise = JS.eval('Promise.resolve(1)')
    assert_equal(true, promise.respond_to?(:await))
    obj = JS.global.create_object
    assert_equal(false, obj.respond_to?(:preventDefault))
  end

  def test_unknown_predicate_raises_no_method_error
    obj = JS.global.create_object
    error = begin
      obj.definitely_missing?
      nil
    rescue NoMethodError => e
      e
    end
    assert_equal(NoMethodError, error.class)
  end

  def test_id_and_case_equality
    obj = JS.global.create_object
    assert_equal(Integer, obj.__id__.class)
    assert_equal(true, JS::Object === obj)
  end

  def test_kernel_names_forward_to_js
    obj = JS.global.create_object
    obj[:send] = 42
    assert_equal(42, obj.send)
    obj[:hash] = 'fragment'
    assert_equal('fragment', obj.hash)
  end

  def test_enumerable_on_js_array
    arr = JS.global.create_array
    arr.push(1)
    arr.push(2)
    arr.push(3)
    assert_equal([2, 4, 6], arr.map { |v| v.to_i * 2 })
    assert_equal([2], arr.select { |v| v.to_i % 2 == 0 })
    assert_equal(true, arr.include?(2))
  end

  def test_string_interpolation_uses_to_s
    obj = JS.global.create_object
    assert_equal(true, "#{obj}".start_with?('#<JS::Object'))
  end
end
