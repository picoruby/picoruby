class SignalTest < Picotest::Test
  # Kernel#trap sets the same handler as Signal.trap and returns the old one
  def test_kernel_trap
    handler = Proc.new { }
    old = trap(:CONT, handler)
    assert_equal handler.object_id, Signal.trap(:CONT, old).object_id
    old = trap("CONT") { }
    assert_true Signal.trap(:CONT, old).is_a?(Proc)
  end

  # On POSIX the process catches SIGINT while a Proc is trapped for it, and
  # lets it go again for "DEFAULT"; with nothing caught, dispatching runs
  # no handler
  def test_sigint_is_caught_while_a_proc_is_trapped
    return unless Machine.respond_to?(:__trap_sigint)
    called = false
    old = Signal.trap(:INT) { called = true }
    begin
      assert_true Machine.__trap_sigint(true)
      assert_false Machine.__sigint_pending?
      Signal.__dispatch_pending
      assert_false called
    ensure
      Signal.trap(:INT, old || "DEFAULT")
    end
    Signal.trap(:INT, "DEFAULT")
    assert_false Machine.__trap_sigint(false)
  end
end
