class TestAliasClass
  def original
    42
  end
end

class MetaprogTest < Picotest::Test
  def test_alias_method
    TestAliasClass.alias_method(:aliased, :original)
    obj = TestAliasClass.new
    assert_equal(42, obj.original)
    assert_equal(42, obj.aliased)
  end
end
