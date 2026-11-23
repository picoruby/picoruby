# Kernel#require searches $LOAD_PATH for the file. These tests check the
# search itself, so there are no real files: File.file? says yes to every
# path, and __require_load_file (the method __require_file hands the found
# path to) records the path instead of running the file.
class RequireTest < Picotest::Test
  LOAD_PATH = "/picotest/lib"

  # The globals change in place, as code that sets the load path does, so
  # that $: and $" keep naming the same Arrays.
  def setup
    @loaded = []
    @load_path = $LOAD_PATH.dup
    @loaded_features = $LOADED_FEATURES.dup
    __refill($LOAD_PATH, [LOAD_PATH])
  end

  def teardown
    __refill($LOAD_PATH, @load_path)
    __refill($LOADED_FEATURES, @loaded_features)
  end

  def __refill(ary, items)
    ary.clear
    items.each { |item| ary << item }
  end

  # Replaces Kernel#__require_load_file for the test object, which is the
  # receiver of require. With @nested, the "file" requires that name in
  # turn, and @nested_result has what that require returned; with @broken,
  # it raises.
  def __require_load_file(path, _cwd = false)
    @loaded << path
    if @broken
      @broken = nil
      raise "broken"
    end
    if (nested = @nested)
      @nested = nil
      @nested_result = require(nested)
    end
    true
  end

  # Names a class may define for itself, as Rails' RoutesReloader does
  # load_paths: require and load must not call them.
  def load_paths
    raise "the receiver's own load_paths was called"
  end

  def required?
    raise "the receiver's own required? was called"
  end

  def test_receiver_methods_of_the_same_name_are_not_called
    stub(File).file? { true }
    assert_equal true, require("bar")
    assert_equal ["#{LOAD_PATH}/bar.mrb"], @loaded
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

  # ./ and ../ are relative to the working directory, not the load path
  def test_dot_slash_name_is_looked_for_in_the_working_directory
    stub(File).file? { true }
    cwd = File.expand_path(".")
    assert_equal true, require("./foo")
    assert_equal ["#{cwd}/foo.mrb"], @loaded
  end

  # load looks in the working directory after the load path; require does not
  def test_load_goes_on_to_the_working_directory
    cwd = File.expand_path(".")
    assert_equal [LOAD_PATH, cwd], __require_load_paths("db/schema.rb", true)
    assert_equal [LOAD_PATH], __require_load_paths("db/schema.rb")
    assert_equal [cwd], __require_load_paths("../x.rb")
    assert_equal [""], __require_load_paths("/x.rb", true)
  end

  def test_loaded_path_is_not_loaded_again
    stub(File).file? { true }
    $LOADED_FEATURES << "#{LOAD_PATH}/foo.rb"
    assert_equal false, require("foo.rb")
    assert_equal [], @loaded
  end

  def test_file_still_loading_is_not_loaded_again
    stub(File).file? { true }
    @nested = "foo.rb"
    assert_equal true, require("foo.rb")
    assert_equal false, @nested_result
    assert_equal ["#{LOAD_PATH}/foo.rb"], @loaded
  end

  def test_file_is_loadable_again_once_its_load_raised
    stub(File).file? { true }
    @broken = true
    assert_raise(RuntimeError) { require("bar.rb") }
    assert_equal true, require("bar.rb")
  end

  # The picotest runner wraps Kernel#require and rescues LoadError from it,
  # so this test calls __require_file, the method that raises.
  def test_missing_file_raises_load_error_with_the_name_as_given
    stub(File).file? { false }
    assert_raise(LoadError, "cannot load such file -- nope.rb") do
      __require_file "nope.rb"
    end
    assert_equal [], @loaded
  end

  def test_short_names_of_the_load_path_and_loaded_features
    $: << "/picotest/other"
    assert_equal [LOAD_PATH, "/picotest/other"], $LOAD_PATH
    $LOADED_FEATURES << "picotest_feature"
    assert_true $".include?("picotest_feature")
  end

  def test_load_error_path_is_the_name_as_given
    return unless RUBY_ENGINE == "mruby" # mruby/c has no LoadError#path
    stub(File).file? { false }
    error = nil
    begin
      __require_file "nope"
    rescue LoadError => e
      error = e
    end
    assert_equal "nope", error&.path
  end

  # __dir__ and require_relative read the caller's file from the debug
  # info of its code, which mruby/c does not have.

  # The real path, as CRuby's: /tmp is a link to /private/tmp on macOS
  def test_dir_is_the_real_directory_of_the_caller_s_file
    return unless RUBY_ENGINE == "mruby"
    assert_equal File.dirname(File.realpath(__FILE__)), __dir__
  end

  def test_require_relative_looks_beside_the_caller_s_file
    return unless RUBY_ENGINE == "mruby"
    stub(File).file? { true }
    dir = File.dirname(File.realpath(__FILE__))
    assert_equal true, require_relative("foo")
    assert_equal true, require_relative("../bar.rb")
    # expand_path, not "#{File.dirname(dir)}/bar.rb": that is "//bar.rb"
    # when the file is in /tmp
    assert_equal ["#{dir}/foo.mrb", File.expand_path("../bar.rb", dir)], @loaded
  end

  # The File of picoruby-vfs has no realpath: the name as given
  def test_dir_without_file_realpath
    return unless RUBY_ENGINE == "mruby"
    sc = File.singleton_class
    sc.send(:alias_method, :__realpath_saved, :realpath)
    sc.send(:remove_method, :realpath)
    begin
      assert_equal File.dirname(__FILE__), __dir__
    ensure
      sc.send(:alias_method, :realpath, :__realpath_saved)
      sc.send(:remove_method, :__realpath_saved)
    end
  end

  # An eval given no file has no directory; given one that does not exist,
  # its directory is that name's
  def test_dir_and_require_relative_in_eval
    return unless RUBY_ENGINE == "mruby"
    assert_nil eval("__dir__")
    assert_equal "lib/sub", eval("__dir__", nil, "lib/sub/x.rb")
    assert_raise(LoadError, "cannot infer basepath") do
      eval("require_relative 'foo'")
    end
  end
end
