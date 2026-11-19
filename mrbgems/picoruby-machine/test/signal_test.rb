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
    skip "only the mruby VM catches SIGINT for a trap" if femtoruby?
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

  # trap(:INT) and the dispatch work on every build without respond_to?
  # (mruby/c has it only from picoruby-metaprog); with nothing caught the
  # handler does not run
  def test_trap_int_and_dispatch_on_every_build
    called = false
    old = Signal.trap(:INT) { called = true }
    begin
      Signal.__dispatch_pending
      assert_false called
    ensure
      Signal.trap(:INT, old || "DEFAULT")
    end
  end

  # A signal only the host lists can be trapped, and gets its handler back;
  # one nobody knows raises ArgumentError
  def test_trap_a_signal_only_the_host_lists
    names = Signal.list.keys
    host_only = nil
    i = 0
    while i < names.size
      host_only ||= names[i] if Signal::LIST[names[i]].nil?
      i += 1
    end
    if host_only
      handler = Proc.new { }
      old = Signal.trap(host_only, handler)
      assert_equal handler.object_id, Signal.trap(host_only, old).object_id
    end
    assert_raise(ArgumentError) { Signal.trap("NOSUCHSIGNAL") { } }
  end
end
