# Issue #545: a file loaded with require suspended on JS::Promise#await, and
# require returned before the rest of the file ran. require and load wait
# through Sandbox#wait, so the test is on Sandbox.
class SandboxAwaitTest < Picotest::Test
  def test_wait_keeps_waiting_while_the_task_awaits_a_promise
    $sandbox_await_log = []
    sandbox = Sandbox.new
    assert sandbox.compile <<~RUBY
      $sandbox_await_log << :first
      JS.eval('new Promise((r) => setTimeout(r, 10))').await
      $sandbox_await_log << :second
    RUBY
    sandbox.execute
    assert_equal true, sandbox.wait(timeout: 2000)
    assert_equal :DORMANT, sandbox.state
    assert_equal [:first, :second], $sandbox_await_log
  ensure
    sandbox.terminate if sandbox
    $sandbox_await_log = nil
  end
end
