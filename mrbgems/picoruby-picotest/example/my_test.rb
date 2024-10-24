class MyTest < Picotest::Test
  def test_truth
    assert(true)
  end
  def test_equal
    assert_equal(1, 2)
  end
  def test_false
    assert(false)
  end

  def test_raise
    assert_raise(ArgumentError) do |e|
      raise NoMethodError.new("NoMethodError")
    end
  end
end

