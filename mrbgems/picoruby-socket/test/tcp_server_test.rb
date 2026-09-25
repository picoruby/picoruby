# TCPServer Tests
#
# Note: These tests focus on basic TCPServer functionality.
# Full client-server integration tests require threading or
# non-blocking I/O which may not be available in all environments.

require 'socket'

# Stand-in for the event queue accept blocks on: instead of waiting it
# runs the given block once, as if another task had acted meanwhile.
class TCPServerTestEventQueue
  def initialize(&block)
    @block = block
  end

  def pop
    @block.call
  end
end

# Serves one fake client straight away and nothing afterwards
class TCPServerTestOneClientServer < TCPServer
  def accept_nonblock
    return nil if @served
    @served = true
    Object.new
  end
end

class TCPServerTest < Picotest::Test
  # Test 1: TCPServer.new with nil host and service (CRuby compatible)
  def test_tcp_server_new_with_nil_host
    server = TCPServer.new(nil, 18080)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 2: TCPServer.new with host and service (CRuby compatible)
  def test_tcp_server_new_with_host_and_service
    server = TCPServer.new("127.0.0.1", 18081)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 3: TCPServer.new with host, service, and backlog
  def test_tcp_server_new_with_all_params
    server = TCPServer.new("127.0.0.1", 18082, 15)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 4: TCPServer.new with nil host, service, and backlog
  def test_tcp_server_new_with_nil_host_and_backlog
    server = TCPServer.new(nil, 18083, 10)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 5: TCPServer.close works
  def test_tcp_server_close
    server = TCPServer.new(nil, 18084)
    server.close
    # After close, server should not be usable
    # (testing this would require attempting operations which might hang)
  end

  # Test 6: TCPServer responds to accept
  def test_tcp_server_responds_to_accept
    server = TCPServer.new(nil, 18085)
    assert_true server.respond_to?(:accept)
    server.close
  end

  # Test 7: TCPServer responds to accept_loop
  def test_tcp_server_responds_to_accept_loop
    server = TCPServer.new(nil, 18086)
    assert_true server.respond_to?(:accept_loop)
    server.close
  end

  def test_tcp_server_closed_p
    server = TCPServer.new(nil, 18087)
    assert_false server.closed?
    server.close
    assert_true server.closed?
    # Closing an already closed server is a no-op, as in CRuby
    server.close
    assert_true server.closed?
  end

  def with_int_handler(handler)
    previous = Signal.trap(:INT, handler)
    yield
  ensure
    Signal.trap(:INT, previous || "DEFAULT")
  end

  def test_accept_interrupted_by_sigint
    with_int_handler("DEFAULT") do
      server = TCPServer.new(nil, 18088)
      server.instance_variable_set(:@event_queue, TCPServerTestEventQueue.new { Signal.raise(:INT) })
      assert_raise(Interrupt) { server.accept }
      assert_true server.closed?
      assert_equal "DEFAULT", Signal.trap(:INT, "DEFAULT")
    end
  end

  def test_accept_interrupted_by_sigint_restores_previous_handler
    marker = Proc.new { }
    with_int_handler(marker) do
      server = TCPServer.new(nil, 18089)
      server.instance_variable_set(:@event_queue, TCPServerTestEventQueue.new { Signal.raise(:INT) })
      assert_raise(Interrupt) { server.accept }
      assert_equal marker.object_id, Signal.trap(:INT, "DEFAULT").object_id
    end
  end

  def test_accept_closed_by_another_task
    with_int_handler("DEFAULT") do
      server = TCPServer.new(nil, 18090)
      server.instance_variable_set(:@event_queue, TCPServerTestEventQueue.new { server.close })
      assert_raise(IOError) { server.accept }
      assert_true server.closed?
      assert_equal "DEFAULT", Signal.trap(:INT, "DEFAULT")
    end
  end

  def test_accept_on_closed_server
    server = TCPServer.new(nil, 18091)
    server.close
    assert_raise(IOError) { server.accept }
  end

  # A handler installed by someone else while accept was waiting must
  # survive accept's cleanup.
  def test_accept_keeps_handler_installed_meanwhile
    other = Proc.new { }
    with_int_handler("DEFAULT") do
      server = TCPServer.new(nil, 18092)
      queue = TCPServerTestEventQueue.new do
        Signal.trap(:INT, other)
        server.close
      end
      server.instance_variable_set(:@event_queue, queue)
      assert_raise(IOError) { server.accept }
      assert_equal other.object_id, Signal.trap(:INT, "DEFAULT").object_id
    end
  end

  # accept_loop owns the handler; Ctrl-C while the block runs closes the
  # server and the next accept reports Interrupt.
  def test_accept_loop_interrupted_while_handling_client
    with_int_handler("DEFAULT") do
      server = TCPServerTestOneClientServer.new(nil, 18093)
      handled = 0
      assert_raise(Interrupt) do
        server.accept_loop do |_client|
          handled += 1
          Signal.raise(:INT)
        end
      end
      assert_equal 1, handled
      assert_true server.closed?
      assert_equal "DEFAULT", Signal.trap(:INT, "DEFAULT")
    end
  end
end
