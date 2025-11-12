require 'socket'

class TCPSocketTest < Picotest::Test
  def test_basic_socket_methods_exist
    # Ruby-defined methods should be visible
    methods = BasicSocket.instance_methods
    assert_true(methods.include?(:puts))
    assert_true(methods.include?(:gets))
    assert_true(methods.include?(:print))
    assert_true(methods.include?(:send))
    assert_true(methods.include?(:recv))
    assert_true(methods.include?(:peeraddr))
  end

  def test_tcp_socket_ruby_methods_exist
    # Ruby-defined methods should be visible
    methods = TCPSocket.instance_methods
    assert_true(methods.include?(:connected?))
    assert_true(methods.include?(:addr))
  end

  # Note: The following tests require external network connectivity
  # They are commented out because:
  # 1. They depend on external services (example.com)
  # 2. Integration tests with TCPServer require threading/async support
  #    which is not available in the current test environment
  # Uncomment when running in an environment with network access

  # def test_tcp_socket_connect
  #   socket = TCPSocket.new('example.com', 80)
  #   assert_false(socket.closed?)
  #   assert_equal('example.com', socket.remote_host)
  #   assert_equal(80, socket.remote_port)
  #   socket.close
  #   assert_true(socket.closed?)
  # end

  # def test_tcp_socket_write_and_read
  #   socket = TCPSocket.new('example.com', 80)
  #
  #   # Send HTTP request
  #   request = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n"
  #   bytes_sent = socket.write(request)
  #   assert_true(bytes_sent > 0)
  #
  #   # Read response
  #   response = socket.read(100)
  #   assert_true(response.length > 0)
  #   assert_true(response.include?('HTTP'))
  #
  #   socket.close
  # end

  # def test_tcp_socket_open
  #   socket = TCPSocket.open('example.com', 80)
  #   assert_false(socket.closed?)
  #   socket.close
  # end

  # def test_tcp_socket_io_compatible_methods
  #   socket = TCPSocket.new('example.com', 80)
  #
  #   # Test puts
  #   socket.puts("GET / HTTP/1.1")
  #   socket.puts("Host: example.com")
  #   socket.puts("Connection: close")
  #   socket.puts("")
  #
  #   # Test gets
  #   line = socket.gets
  #   assert_true(line.include?('HTTP'))
  #
  #   socket.close
  # end
end
