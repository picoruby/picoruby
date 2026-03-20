class WebSocketServerTest < Picotest::Test
  def test_server_initialization
    server = Net::WebSocket::Server.new(host: '127.0.0.1', port: 9999)
    assert_equal('127.0.0.1', server.host)
    assert_equal(9999, server.port)
  end

  def test_server_default_initialization
    server = Net::WebSocket::Server.new
    assert_equal('0.0.0.0', server.host)
    assert_equal(8080, server.port)
  end

  def test_opcodes
    assert_equal(0x0, Net::WebSocket::OPCODE_CONTINUATION)
    assert_equal(0x1, Net::WebSocket::OPCODE_TEXT)
    assert_equal(0x2, Net::WebSocket::OPCODE_BINARY)
    assert_equal(0x8, Net::WebSocket::OPCODE_CLOSE)
    assert_equal(0x9, Net::WebSocket::OPCODE_PING)
    assert_equal(0xA, Net::WebSocket::OPCODE_PONG)
  end

  def test_close_codes
    assert_equal(1000, Net::WebSocket::CLOSE_NORMAL)
    assert_equal(1001, Net::WebSocket::CLOSE_GOING_AWAY)
    assert_equal(1002, Net::WebSocket::CLOSE_PROTOCOL_ERROR)
    assert_equal(1003, Net::WebSocket::CLOSE_UNSUPPORTED_DATA)
    assert_equal(1006, Net::WebSocket::CLOSE_ABNORMAL)
  end

  def test_websocket_guid
    assert_equal("258EAFA5-E914-47DA-95CA-C5AB0DC85B11", Net::WebSocket::WEBSOCKET_GUID)
  end

  def test_error_when_accepting_without_start
    server = Net::WebSocket::Server.new
    assert_raise(Net::WebSocket::WebSocketError) do
      server.accept
    end
  end
end
