class TestAliasClass
  def original
    42
  end
end

METAPROG_TEST_CONST = 42

class MetaprogTest < Picotest::Test
  def test_alias_method
    TestAliasClass.alias_method(:aliased, :original)
    obj = TestAliasClass.new
    assert_equal(42, obj.original)
    assert_equal(42, obj.aliased)
  end

  def test_const_defined_q
    assert_equal(true, const_defined?(:METAPROG_TEST_CONST))
    assert_equal(false, const_defined?(:METAPROG_UNDEFINED_CONST))
    assert_equal(true, const_defined?("METAPROG_TEST_CONST"))
    assert_equal(false, const_defined?("METAPROG_UNDEFINED_CONST"))
  end
end
