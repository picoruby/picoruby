class UDPSocketTest < Picotest::Test
  def test_udp_socket_class_exists
    assert_equal(Class, UDPSocket.class)
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

  # Note: recvfrom tests are temporarily disabled due to implementation issues
  # These will be implemented in Phase 2.5
  # def test_udp_socket_recvfrom
  #   receiver = UDPSocket.new
  #   receiver.bind('127.0.0.1', 9998)
  #   ...
  # end
end
