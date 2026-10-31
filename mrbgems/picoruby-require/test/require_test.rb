# Kernel#require searches $LOAD_PATH for the file. These tests check the
# search itself, so there are no real files: File.file? says yes to every
# path, and load_file (the method require_file hands the found path to)
# records the path instead of running the file.
class RequireTest < Picotest::Test
  LOAD_PATH = "/picotest/lib"

  def setup
    @loaded = []
    @load_path = $LOAD_PATH
    @loaded_features = $LOADED_FEATURES.dup
    $LOAD_PATH = [LOAD_PATH]
  end

  def teardown
    $LOAD_PATH = @load_path
    $LOADED_FEATURES = @loaded_features
  end

  # Replaces Kernel#load_file for the test object, which is the receiver
  # of require.
  def load_file(path)
    @loaded << path
    true
  end

  def test_name_without_extension_prefers_mrb
    stub(File).file? { true }
    assert_equal true, require("foo")
    assert_equal ["#{LOAD_PATH}/foo.mrb"], @loaded
  end

  # CRuby accepts "foo.rb" and looks for that file, not for "foo.rb.rb"
  def test_name_with_rb_extension_is_searched_as_is
    stub(File).file? { true }
    assert_equal true, require("foo.rb")
    assert_equal ["#{LOAD_PATH}/foo.rb"], @loaded
  end

  def test_name_with_mrb_extension_is_searched_as_is
    stub(File).file? { true }
    assert_equal true, require("foo.mrb")
    assert_equal ["#{LOAD_PATH}/foo.mrb"], @loaded
  end

  # Another extension is not a source extension: foo.json.mrb, foo.json.rb
  def test_name_with_another_extension_gets_the_source_extensions
    stub(File).file? { true }
    assert_equal true, require("foo.json")
    assert_equal ["#{LOAD_PATH}/foo.json.mrb"], @loaded
  end

  def test_loaded_path_is_not_loaded_again
    stub(File).file? { true }
    $LOADED_FEATURES << "#{LOAD_PATH}/foo.rb"
    assert_equal false, require("foo.rb")
    assert_equal [], @loaded
  end

  # The picotest runner wraps Kernel#require and rescues LoadError from it,
  # so this test calls require_file, the method that raises.
  def test_missing_file_raises_load_error_with_the_name_as_given
    stub(File).file? { false }
    assert_raise(LoadError, "cannot load such file -- nope.rb") do
      require_file "nope.rb"
    end
    assert_equal [], @loaded
  end
end
