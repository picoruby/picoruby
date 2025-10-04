class TestAliasClass
  def original
    42
  end
end

class TestUndefClass1
  def foo
    1
  end

  def bar
    2
  end
end

class TestUndefClass2
  def baz
    3
  end
end

class TestUndefClass3
end

class MetaprogTest < Picotest::Test
  def test_alias_method
    TestAliasClass.alias_method(:aliased, :original)
    obj = TestAliasClass.new
    assert_equal(42, obj.original)
    assert_equal(42, obj.aliased)
  end

  def test_undef_method
    obj1 = TestUndefClass1.new
    assert_equal(1, obj1.foo)
    assert_equal(2, obj1.bar)

    TestUndefClass1.undef_method(:foo)

    obj2 = TestUndefClass1.new
    assert_equal(2, obj2.bar)

    error = false
    begin
      obj2.foo
    rescue NoMethodError
      error = true
    end
    assert_true(error)
  end

  def test_undef_method_with_string
    TestUndefClass2.undef_method("baz")

    obj = TestUndefClass2.new
    error = false
    begin
      obj.baz
    rescue NoMethodError
      error = true
    end
    assert_true(error)
  end

  def test_undef_nonexistent
    assert_raise(NameError) do
      TestUndefClass3.undef_method(:nonexistent)
    end
  end
end
