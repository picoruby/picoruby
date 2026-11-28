require 'socket'

class SocketTest < Picotest::Test
  def test_socket_is_a_basic_socket
    assert_true(Object.const_defined?(:Socket))
    assert_true(Socket.ancestors.include?(BasicSocket))
  end

  def test_constants_are_reachable_on_socket_and_socket_constants
    assert_equal(Socket::Constants::AF_INET, Socket::AF_INET)
    assert_equal(Socket::Constants::AF_INET6, Socket::AF_INET6)
    assert_equal(Socket::Constants::SOCK_STREAM, Socket::SOCK_STREAM)
  end

  def test_address_families
    assert_equal(0, Socket::AF_UNSPEC)
    assert_equal(2, Socket::AF_INET)
    # 10 on Linux and lwIP, 30 on macOS; never the same as AF_INET
    assert_true(0 < Socket::AF_INET6)
    assert_true(Socket::AF_INET != Socket::AF_INET6)
    assert_equal(Socket::AF_UNSPEC, Socket::PF_UNSPEC)
    assert_equal(Socket::AF_INET, Socket::PF_INET)
    assert_equal(Socket::AF_INET6, Socket::PF_INET6)
  end

  def test_socket_types_and_protocols
    assert_equal(1, Socket::SOCK_STREAM)
    assert_equal(2, Socket::SOCK_DGRAM)
    assert_equal(3, Socket::SOCK_RAW)
    assert_equal(0, Socket::IPPROTO_IP)
    assert_equal(6, Socket::IPPROTO_TCP)
    assert_equal(17, Socket::IPPROTO_UDP)
  end
end
