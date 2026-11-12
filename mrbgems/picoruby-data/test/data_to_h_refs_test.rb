class DataToHReferencesTest < Picotest::Test
  def setup
    require 'data'
  end

  def test_to_h_remains_valid_after_returned_hashes_are_released
    klass = Data.define(:name, :score)
    obj = klass.new("Alice", 99)

    # Releasing each result must not release the instance's own hash.
    3.times do
      h = obj.to_h
      assert_equal "Alice", h[:name]
      assert_equal 99, h[:score]
      h = nil
    end

    assert_equal "Alice", obj.name
    assert_equal 99, obj.score
  end
end
