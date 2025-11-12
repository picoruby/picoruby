# TCPServer Tests
#
# Note: These tests focus on basic TCPServer functionality.
# Full client-server integration tests require threading or
# non-blocking I/O which may not be available in all environments.

class TCPServerTest < Picotest::Test
  # Test 1: TCPServer class exists
  def test_tcp_server_class_exists
    server = TCPServer.new(18080)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 2: TCPServer.new creates a server
  def test_tcp_server_new
    server = TCPServer.new(18081)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 3: TCPServer.new with backlog parameter
  def test_tcp_server_new_with_backlog
    server = TCPServer.new(18082, 10)
    assert_true server.is_a?(TCPServer)
    server.close
  end

  # Test 4: TCPServer.close works
  def test_tcp_server_close
    server = TCPServer.new(18083)
    server.close
    # After close, server should not be usable
    # (testing this would require attempting operations which might hang)
  end

  # Test 5: TCPServer responds to accept
  def test_tcp_server_responds_to_accept
    server = TCPServer.new(18084)
    assert_true server.respond_to?(:accept)
    server.close
  end

  # Test 6: TCPServer responds to accept_loop
  def test_tcp_server_responds_to_accept_loop
    server = TCPServer.new(18085)
    assert_true server.respond_to?(:accept_loop)
    server.close
  end
end
