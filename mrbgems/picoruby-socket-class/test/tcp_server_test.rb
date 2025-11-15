# TCPServer Tests
#
# Note: These tests focus on basic TCPServer functionality.
# Full client-server integration tests require threading or
# non-blocking I/O which may not be available in all environments.

require 'socket'

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
end
