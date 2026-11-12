class DataMembersReferencesTest < Picotest::Test
  def setup
    require 'data'
  end

  def test_class_members_remain_valid_after_returned_arrays_are_released
    klass = Data.define(:name, :score)

    # Releasing each result must not release the definition's member keys.
    3.times do
      members = klass.members
      assert_equal [:name, :score], members
      members = nil
    end

    obj = klass.new("Alice", 99)
    assert_equal "Alice", obj.name
    assert_equal 99, obj.score
  end
end
