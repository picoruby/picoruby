class WebSocketTest < Picotest::Test
  def test_client_initialization
    client = Net::WebSocket::Client.new("ws://example.com:8080/socket")
    assert_equal("ws://example.com:8080/socket", client.url)
    assert_equal("example.com", client.host)
    assert_equal(8080, client.port)
    assert_equal("/socket", client.path)
  end

  def test_parse_url_with_default_port
    client = Net::WebSocket::Client.new("ws://example.com/path")
    assert_equal("example.com", client.host)
    assert_equal(80, client.port)
    assert_equal("/path", client.path)
  end

  def test_parse_url_with_root_path
    client = Net::WebSocket::Client.new("ws://example.com")
    assert_equal("/", client.path)
  end

  def test_parse_url_without_ws_prefix
    client = Net::WebSocket::Client.new("example.com:9000/ws")
    assert_equal("example.com", client.host)
    assert_equal(9000, client.port)
    assert_equal("/ws", client.path)
  end

  def test_wss_url_parsing
    client = Net::WebSocket::Client.new("wss://secure.example.com")
    assert_equal("secure.example.com", client.host)
    assert_equal(443, client.port)
    assert_equal("/", client.path)
  end

  def test_wss_url_with_port
    client = Net::WebSocket::Client.new("wss://secure.example.com:8443/socket")
    assert_equal("secure.example.com", client.host)
    assert_equal(8443, client.port)
    assert_equal("/socket", client.path)
  end

  def test_wss_url_with_path
    client = Net::WebSocket::Client.new("wss://api.example.com/v1/stream")
    assert_equal("api.example.com", client.host)
    assert_equal(443, client.port)
    assert_equal("/v1/stream", client.path)
  end

  def test_connected_returns_false_initially
    client = Net::WebSocket::Client.new("ws://example.com")
    assert_equal(false, client.connected?)
  end

  def test_add_header
    client = Net::WebSocket::Client.new("ws://example.com")
    client.add_header("Authorization", "Bearer token123")
    client.add_header("X-Custom", "value")
    # Headers are stored internally
    assert(true)
  end

  def test_ssl_context_accessor
    client = Net::WebSocket::Client.new("wss://example.com")
    assert_equal(nil, client.ssl_context)

    # Can set custom SSL context
    # (In real usage, would create SSLContext.new)
    client.ssl_context = "mock_context"
    assert_equal("mock_context", client.ssl_context)
  end

  def test_mask_data
    client = Net::WebSocket::Client.new("ws://example.com")
    data = "Hello"
    mask_key = "\x01\x02\x03\x04"

    masked = client.__send__(:mask_data, data, mask_key)
    unmasked = client.__send__(:mask_data, masked, mask_key)

    assert_equal(data, unmasked)
  end

  def test_mask_data_empty
    client = Net::WebSocket::Client.new("ws://example.com")
    data = ""
    mask_key = "\x01\x02\x03\x04"

    masked = client.__send__(:mask_data, data, mask_key)
    assert_equal("", masked)
  end

  def test_mask_data_longer_than_key
    client = Net::WebSocket::Client.new("ws://example.com")
    data = "Hello, World!"
    mask_key = "\xAB\xCD\xEF\x12"

    masked = client.__send__(:mask_data, data, mask_key)
    unmasked = client.__send__(:mask_data, masked, mask_key)

    assert_equal(data, unmasked)
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

  def test_error_when_sending_without_connection
    client = Net::WebSocket::Client.new("ws://example.com")
    assert_raise(Net::WebSocket::WebSocketError) do
      client.send_text("test")
    end
  end

  def test_error_when_receiving_without_connection
    client = Net::WebSocket::Client.new("ws://example.com")
    assert_raise(Net::WebSocket::WebSocketError) do
      client.receive
    end
  end

  def test_parse_url_with_nested_path
    client = Net::WebSocket::Client.new("ws://example.com/api/v1/websocket")
    assert_equal("/api/v1/websocket", client.path)
  end

  def test_parse_url_with_query_string
    client = Net::WebSocket::Client.new("ws://example.com/socket?token=abc123")
    assert_equal("/socket?token=abc123", client.path)
  end

  def test_handle_close_frame_with_code_and_reason
    client = Net::WebSocket::Client.new("ws://example.com")
    # Close frame with code 1000 and reason "Normal"
    payload = [1000].pack("n") + "Normal"
    # This should not raise error
    client.__send__(:handle_close_frame, payload)
    assert(true)
  end

  def test_handle_close_frame_with_code_only
    client = Net::WebSocket::Client.new("ws://example.com")
    payload = [1001].pack("n")
    client.__send__(:handle_close_frame, payload)
    assert(true)
  end

  def test_handle_close_frame_empty
    client = Net::WebSocket::Client.new("ws://example.com")
    payload = ""
    client.__send__(:handle_close_frame, payload)
    assert(true)
  end

  def test_mask_xor_property
    client = Net::WebSocket::Client.new("ws://example.com")
    data = "Test data 123!"
    mask = "abcd"

    # Masking twice should return original
    once = client.__send__(:mask_data, data, mask)
    twice = client.__send__(:mask_data, once, mask)

    assert_equal(data, twice)
  end

  def test_mask_each_byte_different_key_byte
    client = Net::WebSocket::Client.new("ws://example.com")
    # Test that each byte uses correct mask key byte
    data = "\x00\x00\x00\x00\x00"
    mask = "\x01\x02\x03\x04"

    masked = client.__send__(:mask_data, data, mask)

    # Each byte should be XORed with corresponding mask byte
    assert_equal("\x01", masked[0])
    assert_equal("\x02", masked[1])
    assert_equal("\x03", masked[2])
    assert_equal("\x04", masked[3])
    assert_equal("\x01", masked[4])  # Wraps around
  end

  def test_url_variations
    # Test various URL formats
    urls = [
      ["ws://host", "host", 80, "/"],
      ["ws://host/", "host", 80, "/"],
      ["ws://host:1234", "host", 1234, "/"],
      ["ws://host:1234/path", "host", 1234, "/path"],
      ["wss://host", "host", 443, "/"],
      ["wss://host:8443", "host", 8443, "/"],
      ["wss://host/secure", "host", 443, "/secure"],
      ["host:9000", "host", 9000, "/"]
    ]

    urls.each do |url, expected_host, expected_port, expected_path|
      client = Net::WebSocket::Client.new(url)
      assert_equal(expected_host, client.host)
      assert_equal(expected_port, client.port)
      assert_equal(expected_path, client.path)
    end
  end
end

# Note: Integration tests that require actual WebSocket server
# should be run separately in a testing environment
#
# Example integration test (commented out):
#
# class WebSocketIntegrationTest < Picotest::Test
#   def test_connect_to_echo_server
#     ws = Net::WebSocket::Client.new("ws://echo.websocket.org")
#     ws.connect
#     assert(ws.connected?)
#
#     ws.send_text("Hello!")
#     response = ws.receive(timeout: 5)
#     assert_equal("Hello!", response)
#
#     ws.close
#   end
#
#   def test_ping_pong
#     ws = Net::WebSocket::Client.new("ws://echo.websocket.org")
#     ws.connect
#
#     ws.ping("test")
#     # Pong should be received automatically
#
#     ws.close
#   end
# end
