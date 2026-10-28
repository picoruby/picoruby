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

    if picoruby?
      masked = client.__send__(:mask_data, data, mask_key)
      unmasked = client.__send__(:mask_data, masked, mask_key)
    else
      masked = client.mask_data(data, mask_key)
      unmasked = client.mask_data(masked, mask_key)
    end

    assert_equal(data, unmasked)
  end

  def test_mask_data_empty
    client = Net::WebSocket::Client.new("ws://example.com")
    data = ""
    mask_key = "\x01\x02\x03\x04"

    if picoruby?
      masked = client.__send__(:mask_data, data, mask_key)
    else
      masked = client.mask_data(data, mask_key)
    end
    assert_equal("", masked)
  end

  def test_mask_data_longer_than_key
    client = Net::WebSocket::Client.new("ws://example.com")
    data = "Hello, World!"
    mask_key = "\xAB\xCD\xEF\x12"

    if picoruby?
      masked = client.__send__(:mask_data, data, mask_key)
      unmasked = client.__send__(:mask_data, masked, mask_key)
    else
      masked = client.mask_data(data, mask_key)
      unmasked = client.mask_data(masked, mask_key)
    end

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
    if picoruby?
      client.__send__(:handle_close_frame, payload)
    else
      client.handle_close_frame(payload)
    end
    assert(true)
  end

  def test_handle_close_frame_with_code_only
    client = Net::WebSocket::Client.new("ws://example.com")
    payload = [1001].pack("n")
    if picoruby?
      client.__send__(:handle_close_frame, payload)
    else
      client.handle_close_frame(payload)
    end
    assert(true)
  end

  def test_handle_close_frame_empty
    client = Net::WebSocket::Client.new("ws://example.com")
    payload = ""
    if picoruby?
      client.__send__(:handle_close_frame, payload)
    else
      client.handle_close_frame(payload)
    end
    assert(true)
  end

  def test_mask_xor_property
    client = Net::WebSocket::Client.new("ws://example.com")
    data = "Test data 123!"
    mask = "abcd"

    # Masking twice should return original
    if picoruby?
      once = client.__send__(:mask_data, data, mask)
      twice = client.__send__(:mask_data, once, mask)
    else
      once = client.mask_data(data, mask)
      twice = client.mask_data(once, mask)
    end;

    assert_equal(data, twice)
  end

  def test_mask_each_byte_different_key_byte
    client = Net::WebSocket::Client.new("ws://example.com")
    # Test that each byte uses correct mask key byte
    data = "\x00\x00\x00\x00\x00"
    mask = "\x01\x02\x03\x04"

    if picoruby?
      masked = client.__send__(:mask_data, data, mask)
    else
      masked = client.mask_data(data, mask)
    end

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

  # A socket double for the handshake. It answers every readpartial from a
  # canned HTTP response and records close.
  class FakeSocket
    attr_reader :written, :closed_count

    def initialize(response)
      @response = response
      @pos = 0
      @written = ""
      @closed_count = 0
    end

    def write(data)
      @written << data
      data.size
    end

    def readpartial(n)
      raise EOFError if @response.size <= @pos
      chunk = @response[@pos, n] || ""
      @pos += chunk.size
      chunk
    end

    def close
      @closed_count += 1
    end

    def closed?
      0 < @closed_count
    end
  end

  # Prepended onto the singleton classes of TCPSocket and SSLSocket. It
  # raises $ws_test_open_error when that is set and calls the original
  # method otherwise. A stub cannot raise (picotest evaluates the block
  # once, at stub time), and remove_method on the singleton class would
  # delete the C-defined SSLSocket.open itself, so the module stays in.
  module RaisingOpen
    def new(*args)
      e = $ws_test_open_error
      raise e if e
      super
    end

    def open(*args)
      e = $ws_test_open_error
      raise e if e
      super
    end
  end

  def install_raising_open
    return if $ws_test_raising_open_installed
    class << TCPSocket
      prepend RaisingOpen
    end
    class << SSLSocket
      prepend RaisingOpen
    end
    $ws_test_raising_open_installed = true
  end

  def connect_error_with_open_raising(client, error)
    install_raising_open
    $ws_test_open_error = error
    begin
      client.connect
    rescue Net::WebSocket::WebSocketError => e
      return e
    ensure
      $ws_test_open_error = nil
    end
    nil
  end

  # On mruby/c, stub() on a C-defined class method overwrites the C entry in
  # the class method table and clear_doubles removes it. Module#prepend is
  # not available there either. These tests replace TCPSocket.new and
  # SSLSocket.open, so they run on PicoRuby only.
  def skip_unless_class_stub_is_safe
    skip "stub of a C-defined class method is not safe on mruby/c" unless picoruby?
  end

  def test_connect_wraps_tcp_error_in_websocket_error
    skip_unless_class_stub_is_safe
    client = Net::WebSocket::Client.new("ws://example.com:8080/socket")
    error = connect_error_with_open_raising(client, SocketError.new("getaddrinfo failed"))
    assert(error.is_a?(Net::WebSocket::WebSocketError))
    assert(error.message.include?("example.com:8080"))
    assert(error.message.include?("getaddrinfo failed"))
    assert_false(client.connected?)
  end

  def test_connect_wraps_ssl_error_in_websocket_error
    skip_unless_class_stub_is_safe
    client = Net::WebSocket::Client.new("wss://secure.example.com")
    client.ssl_context = "mock_context"
    error = connect_error_with_open_raising(client, RuntimeError.new("SSL connection failed"))
    assert(error.is_a?(Net::WebSocket::WebSocketError))
    assert(error.message.include?("secure.example.com:443"))
    assert(error.message.include?("SSL connection failed"))
    assert_false(client.connected?)
  end

  def test_connect_does_not_call_connect_after_ssl_open
    skip_unless_class_stub_is_safe
    fake = FakeSocket.new("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n")
    stub(SSLSocket).open { fake }
    client = Net::WebSocket::Client.new("wss://secure.example.com/ws")
    client.ssl_context = "mock_context"
    # FakeSocket has no connect method, so a connect call would raise
    assert(client.connect)
    assert(client.connected?)
    assert(fake.written.start_with?("GET /ws HTTP/1.1\r\n"))
  end

  def test_connect_closes_socket_when_handshake_fails
    skip_unless_class_stub_is_safe
    fake = FakeSocket.new("HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
    stub(TCPSocket).new { fake }
    client = Net::WebSocket::Client.new("ws://example.com/ws")
    assert_raise(Net::WebSocket::HandshakeError) do
      client.connect
    end
    assert_equal(1, fake.closed_count)
    assert_false(client.connected?)
    assert_raise(Net::WebSocket::WebSocketError) do
      client.send_text("test")
    end
  end

  def test_connect_closes_socket_when_server_closes_during_handshake
    skip_unless_class_stub_is_safe
    fake = FakeSocket.new("HTTP/1.1 101")
    stub(TCPSocket).new { fake }
    client = Net::WebSocket::Client.new("ws://example.com/ws")
    assert_raise(Net::WebSocket::ConnectionClosed) do
      client.connect
    end
    assert_equal(1, fake.closed_count)
    assert_false(client.connected?)
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
