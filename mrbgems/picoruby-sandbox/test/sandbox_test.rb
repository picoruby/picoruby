class SandboxTest < Picotest::Test
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
