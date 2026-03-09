require 'socket'

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

  def test_udp_connect_invalid_host_raises_socket_error
    sock = UDPSocket.new
    assert_raise(SocketError) do
      sock.connect('this.hostname.is.invalid.example', 9999)
    end
    sock.close
  end

  def test_udp_connect_invalid_host_message_contains_hostname
    sock = UDPSocket.new
    error_message = nil
    begin
      sock.connect('no-such-host.invalid', 9999)
    rescue SocketError => e
      error_message = e.message
    end
    assert_true(!error_message.nil?)
    assert_true(error_message.include?('no-such-host.invalid'))
    sock.close
  end

  def test_udp_sendto_invalid_host_raises_socket_error
    sock = UDPSocket.new
    assert_raise(SocketError) do
      sock.send('hello', 0, 'this.hostname.is.invalid.example', 9999)
    end
    sock.close
  end

  # Note: recvfrom is not yet implemented in UDPSocket
  # Uncomment when recvfrom is implemented
  # def test_udp_socket_recvfrom
  #   receiver = UDPSocket.new
  #   receiver.bind('127.0.0.1', 19005)
  #
  #   sender = UDPSocket.new
  #   test_data = "Test message"
  #   sender.send(test_data, 0, '127.0.0.1', 19005)
  #
  #   # Receive data with recvfrom
  #   data, addr = receiver.recvfrom(100)
  #   assert_equal(test_data, data)
  #   assert_true(addr.is_a?(Array))
  #   assert_equal(2, addr.length)  # [host, port]
  #
  #   sender.close
  #   receiver.close
  # end
end
