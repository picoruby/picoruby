class SignalTest < Picotest::Test
  # Kernel#trap sets the same handler as Signal.trap and returns the old one
  def test_kernel_trap
    handler = Proc.new { }
    old = trap(:CONT, handler)
    assert_equal handler.object_id, Signal.trap(:CONT, old).object_id
    old = trap("CONT") { }
    assert_true Signal.trap(:CONT, old).is_a?(Proc)
  end
end
