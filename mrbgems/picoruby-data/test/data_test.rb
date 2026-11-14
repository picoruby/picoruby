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

  # ---- Data.define argument check ----

  def test_define_with_invalid_member_raises
    assert_raise(TypeError) { Data.define(:x, 1) }
  end

  def test_define_keeps_name_of_released_string
    # No symbol of this name exists before Data.define makes it.
    name = "runtime_" + "member"
    klass = Data.define(name)
    name = nil
    garbage = []
    8.times { |i| garbage << "garbage_string_#{i}" }
    assert_equal "runtime_member", klass.members[0].to_s
  end

  # ---- returned collections are copies ----

  def test_to_h_returns_copy
    klass = Data.define(:name)
    obj = klass.new("Alice")
    h = obj.to_h
    h[:name] = "Bob"
    assert_equal "Alice", obj.name
    assert_equal "Alice", obj.to_h[:name]
  end

  def test_class_members_returns_copy
    klass = Data.define(:x)
    klass.members << :y
    assert_equal [:x], klass.members
    assert_equal 1, klass.new(1).x
  end

  # ---- reference counting ----

  def test_to_h_remains_valid_after_returned_hashes_are_released
    klass = Data.define(:name, :score)
    obj = klass.new("Alice", 99)
    3.times do
      h = obj.to_h
      assert_equal "Alice", h[:name]
      assert_equal 99, h[:score]
      h = nil
    end
    assert_equal "Alice", obj.name
    assert_equal 99, obj.score
  end

  def test_class_members_remain_valid_after_returned_arrays_are_released
    klass = Data.define(:name, :score)
    3.times do
      members = klass.members
      assert_equal [:name, :score], members
      members = nil
    end
    obj = klass.new("Alice", 99)
    assert_equal "Alice", obj.name
    assert_equal 99, obj.score
  end

  def test_instances_release_members
    klass = Data.define(:value)
    discard_instances(klass)
    before = used_memory
    discard_instances(klass)
    assert_equal before, used_memory
  end

  def test_returned_collections_release_memory
    klass = Data.define(:value)
    obj = klass.new('payload')
    discard_collections(klass, obj)
    before = used_memory
    discard_collections(klass, obj)
    assert_equal before, used_memory
  end

  def test_definitions_release_member_keys
    small = [:value]
    large = []
    32.times { |i| large << "member_#{i}".to_sym }
    discard_definitions(small)
    discard_definitions(large)
    before = used_memory
    discard_definitions(small)
    small_growth = used_memory - before
    before = used_memory
    discard_definitions(large)
    large_growth = used_memory - before
    # A class registration keeps memory that does not depend on the member
    # count. A leaked key array does: 20 arrays of 32 values exceed 1 KiB.
    assert_true large_growth - small_growth < 1024
  end

  def used_memory
    require 'picorubyvm'
    PicoRubyVM.memory_statistics[:used]
  end

  def discard_instances(klass)
    100.times do
      obj = klass.new('payload')
      obj = nil
    end
    nil
  end

  def discard_collections(klass, obj)
    100.times do
      obj.to_h
      klass.members
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
