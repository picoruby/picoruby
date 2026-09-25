require 'socket'

# Stand-in for the event queue recvfrom blocks on: instead of waiting it
# runs the given block once, as if another task had acted meanwhile.
class UDPSocketTestEventQueue
  def initialize(&block)
    @block = block
  end

  def pop
    @block.call
  end
end

class UDPSocketTest < Picotest::Test
  def test_udp_socket_class_exists
    # Just try to instantiate it - if class doesn't exist, this will fail
    socket = UDPSocket.new
    assert_true socket.is_a?(UDPSocket)
    socket.close
  end

  def test_udp_socket_new
    sock = UDPSocket.new
    assert_true(sock.is_a?(UDPSocket))
    assert_true(sock.is_a?(BasicSocket))
    sock.close
  end

  def test_udp_socket_bind
    sock = UDPSocket.new
    sock.bind('127.0.0.1', 9998)
    assert_false(sock.closed?)
    sock.close
    assert_true(sock.closed?)
  end

  def test_udp_socket_connect
    sock = UDPSocket.new
    sock.connect('127.0.0.1', 9999)
    assert_false(sock.closed?)
    sock.close
  end

  def test_udp_socket_send_after_connect
    sender = UDPSocket.new
    sender.connect('127.0.0.1', 10000)

    # Send data
    sent = sender.send('Hello UDP', 0)
    assert_equal(9, sent)

    sender.close
  end

  def test_udp_socket_sendto_without_connect
    sender = UDPSocket.new

    # Send data with explicit destination
    sent = sender.send('Test', 0, '127.0.0.1', 10001)
    assert_equal(4, sent)

    sender.close
  end

  def test_udp_socket_closed
    sock = UDPSocket.new
    assert_false(sock.closed?)

    sock.close
    assert_true(sock.closed?)
  end

  def test_udp_socket_eof
    sock = UDPSocket.new
    assert_false(sock.eof?)

    sock.close
    assert_true(sock.eof?)
  end

  def test_socket_error_class_exists
    assert_true(Object.const_defined?(:SocketError))
    assert_true(SocketError.ancestors.include?(StandardError))
  end

  # DNS-dependent tests (invalid host) are not run here because they
  # require network access.  The C layer is tested on microcontroller
  # builds where hardfault prevention matters most.

  def test_udp_socket_recvfrom
    receiver = UDPSocket.new
    receiver.bind('127.0.0.1', 19005)

    sender = UDPSocket.new
    test_data = "Test message"
    sender.send(test_data, 0, '127.0.0.1', 19005)

    data, addr = receiver.recvfrom(100)
    assert_equal(test_data, data)
    assert_true(addr.is_a?(Array))

    sender.close
    receiver.close
  end

  def test_udp_socket_recvfrom_interrupted_by_sigint
    previous = Signal.trap(:INT, "DEFAULT")
    sock = UDPSocket.new
    sock.bind('127.0.0.1', 19006)
    sock.instance_variable_set(:@event_queue, UDPSocketTestEventQueue.new { Signal.raise(:INT) })
    assert_raise(Interrupt) { sock.recvfrom(100) }
    assert_true sock.closed?
    assert_equal "DEFAULT", Signal.trap(:INT, "DEFAULT")
  ensure
    Signal.trap(:INT, previous || "DEFAULT")
  end

  def test_udp_socket_recvfrom_closed_by_another_task
    previous = Signal.trap(:INT, "DEFAULT")
    sock = UDPSocket.new
    sock.bind('127.0.0.1', 19007)
    sock.instance_variable_set(:@event_queue, UDPSocketTestEventQueue.new { sock.close })
    assert_raise(IOError) { sock.recvfrom(100) }
    assert_equal "DEFAULT", Signal.trap(:INT, "DEFAULT")
  ensure
    Signal.trap(:INT, previous || "DEFAULT")
  end
end
