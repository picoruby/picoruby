class SandboxTest < Picotest::Test
  def test_compile_with_filename
    sandbox = Sandbox.new
    sandbox.compile("__FILE__", filename: "/myscript.rb")
    sandbox.execute
    sandbox.wait(timeout: nil)
    assert_equal "/myscript.rb", sandbox.result
  end

  class FileDouble
    def initialize(content)
      @content = content
    end

    def read
      r = @content
      @content = nil
      r
    end

    def close
    end
  end

  def test_load_file
    stub(File).open { FileDouble.new("__FILE__") }
    sandbox = Sandbox.new
    sandbox.load_file("/myscript.rb")
    assert_nil sandbox.error
    assert_equal "/myscript.rb", sandbox.result
  end

  # Regression test for sandbox re-running the previous script
  # when its task was suspended inside a method call.
  def test_execute_runs_new_script_after_suspend_in_method
    sandbox = Sandbox.new

    assert sandbox.compile <<~RUBY
      def suspend_in_method
        Task.current.suspend
      end
      suspend_in_method
      :old
    RUBY
    sandbox.execute
    sandbox.wait(timeout: nil)

    assert sandbox.compile <<~RUBY
      :new
    RUBY
    sandbox.execute
    sandbox.wait(timeout: nil)

    assert_equal :new, sandbox.result
  end
end
