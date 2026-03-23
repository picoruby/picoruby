class DataTest < Picotest::Test

  def setup
    require 'data'
  end

  # ---- Data.define ----

  def test_define_returns_object
    klass = Data.define(:x, :y)
    assert_not_nil klass
  end

  def test_define_with_string_members
    klass = Data.define("x", "y")
    obj = klass.new(1, 2)
    assert_equal 1, obj.x
    assert_equal 2, obj.y
  end

  # ---- .new (positional) ----

  def test_new_positional
    _Point = Data.define(:x, :y)
    p = _Point.new(3, 4)
    assert_equal 3, p.x
    assert_equal 4, p.y
  end

  def test_new_wrong_arity_raises
    klass = Data.define(:a, :b)
    assert_raise(ArgumentError) { klass.new(1) }
  end

  # ---- member accessors ----

  def test_member_access
    _Color = Data.define(:r, :g, :b)
    c = _Color.new(255, 128, 0)
    assert_equal 255, c.r
    assert_equal 128, c.g
    assert_equal 0,   c.b
  end

  def test_unknown_member_raises
    klass = Data.define(:x)
    obj = klass.new(1)
    assert_raise(NoMethodError) { obj.z }
  end

  # ---- .members (class method) ----

  def test_class_members
    klass = Data.define(:name, :age)
    m = klass.members
    assert_equal 2,     m.size
    assert_equal :name, m[0]
    assert_equal :age,  m[1]
  end

  # ---- #members (instance method) ----

  def test_instance_members
    klass = Data.define(:x, :y)
    obj = klass.new(10, 20)
    m = obj.members
    assert_equal 2,  m.size
    assert_equal :x, m[0]
    assert_equal :y, m[1]
  end

  # ---- #to_h ----

  def test_to_h
    klass = Data.define(:name, :score)
    obj = klass.new("Alice", 99)
    h = obj.to_h
    assert_equal "Alice", h[:name]
    assert_equal 99,      h[:score]
  end

  # ---- #is_a? ----

  def test_is_a_data
    klass = Data.define(:x)
    obj = klass.new(1)
    assert_true obj.is_a?(Data)
  end

  # ---- multiple independent subclasses ----

  def test_two_subclasses_are_independent
    _A = Data.define(:val)
    _B = Data.define(:val)
    a = _A.new(1)
    b = _B.new(2)
    assert_equal 1, a.val
    assert_equal 2, b.val
  end

#  # ---- Data.new raises ----
#
#  def test_data_new_raises
#    assert_raise(NoMethodError) { Data.new }
#  end

  # ---- Data.members raises ----

  def test_data_members_raises
    assert_raise(NoMethodError) { Data.members }
  end

  # ---- values of various types ----

  def test_member_with_string_value
    klass = Data.define(:label)
    obj = klass.new("hello")
    assert_equal "hello", obj.label
  end

  def test_member_with_nil_value
    klass = Data.define(:optional)
    obj = klass.new(nil)
    assert_nil obj.optional
  end

  def test_member_with_symbol_value
    klass = Data.define(:kind)
    obj = klass.new(:foo)
    assert_equal :foo, obj.kind
  end

end
