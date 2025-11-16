class CoAPTest < Picotest::Test
  def test_message_encode_decode_simple
    msg = CoAP::Message.new
    msg.version = 1
    msg.mtype = CoAP::TYPE_CON
    msg.code = CoAP::CODE_GET
    msg.message_id = 12345

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(1, decoded.version)
    assert_equal(CoAP::TYPE_CON, decoded.mtype)
    assert_equal(CoAP::CODE_GET, decoded.code)
    assert_equal(12345, decoded.message_id)
  end

  def test_message_with_token
    msg = CoAP::Message.new
    msg.message_id = 100
    msg.token = "abcd"

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(100, decoded.message_id)
    assert_equal("abcd", decoded.token)
    assert_equal(4, decoded.token_length)
  end

  def test_message_with_payload
    msg = CoAP::Message.new
    msg.message_id = 200
    msg.code = CoAP::CODE_CONTENT
    msg.payload = "Hello, CoAP!"

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(200, decoded.message_id)
    assert_equal(CoAP::CODE_CONTENT, decoded.code)
    assert_equal("Hello, CoAP!", decoded.payload)
  end

  def test_message_with_simple_option
    msg = CoAP::Message.new
    msg.message_id = 300
    msg.add_option(CoAP::OPTION_URI_PATH, "temperature")

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(300, decoded.message_id)
    assert_equal(1, decoded.options.length)
    assert_equal(CoAP::OPTION_URI_PATH, decoded.options[0][:number])
    assert_equal("temperature", decoded.options[0][:value])
  end

  def test_message_with_multiple_options
    msg = CoAP::Message.new
    msg.message_id = 400
    msg.add_option(CoAP::OPTION_URI_PATH, "api")
    msg.add_option(CoAP::OPTION_URI_PATH, "temperature")
    msg.add_option(CoAP::OPTION_CONTENT_FORMAT, CoAP::FORMAT_TEXT_PLAIN)

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(400, decoded.message_id)
    assert_equal(3, decoded.options.length)
  end

  def test_get_option
    msg = CoAP::Message.new
    msg.add_option(CoAP::OPTION_URI_PATH, "test")
    msg.add_option(CoAP::OPTION_CONTENT_FORMAT, CoAP::FORMAT_JSON)

    uri_path = msg.get_option(CoAP::OPTION_URI_PATH)
    assert_equal("test", uri_path)

    content_format = msg.get_option(CoAP::OPTION_CONTENT_FORMAT)
    assert(content_format != nil)
  end

  def test_request_uri_path
    req = CoAP::Request.new(CoAP::CODE_GET)
    req.uri_path = "/api/sensor/temperature"

    assert_equal("/api/sensor/temperature", req.uri_path)
    assert_equal(3, req.options.length)
  end

  def test_request_encode_decode
    req = CoAP::Request.new(CoAP::CODE_POST)
    req.message_id = 500
    req.token = "xyz"
    req.uri_path = "/actuator/led"
    req.payload = "on"
    req.add_option(CoAP::OPTION_CONTENT_FORMAT, CoAP::FORMAT_TEXT_PLAIN)

    encoded = req.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(CoAP::CODE_POST, decoded.code)
    assert_equal(500, decoded.message_id)
    assert_equal("xyz", decoded.token)
    assert_equal("on", decoded.payload)
  end

  def test_response_creation
    resp = CoAP::Response.new(code: CoAP::CODE_CONTENT, payload: "25.5")

    assert_equal(CoAP::CODE_CONTENT, resp.code)
    assert_equal("25.5", resp.payload)
    assert_equal(CoAP::TYPE_ACK, resp.mtype)
  end

  def test_response_encode_decode
    resp = CoAP::Response.new(code: CoAP::CODE_CHANGED, payload: "OK")
    resp.message_id = 600
    resp.token = "tk"

    encoded = resp.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(CoAP::CODE_CHANGED, decoded.code)
    assert_equal(600, decoded.message_id)
    assert_equal("tk", decoded.token)
    assert_equal("OK", decoded.payload)
  end

  def test_message_types
    con_msg = CoAP::Message.new
    con_msg.mtype = CoAP::TYPE_CON
    encoded = con_msg.encode
    decoded = CoAP::Message.decode(encoded)
    assert_equal(CoAP::TYPE_CON, decoded.mtype)

    non_msg = CoAP::Message.new
    non_msg.mtype = CoAP::TYPE_NON
    encoded = non_msg.encode
    decoded = CoAP::Message.decode(encoded)
    assert_equal(CoAP::TYPE_NON, decoded.mtype)

    ack_msg = CoAP::Message.new
    ack_msg.mtype = CoAP::TYPE_ACK
    encoded = ack_msg.encode
    decoded = CoAP::Message.decode(encoded)
    assert_equal(CoAP::TYPE_ACK, decoded.mtype)
  end

  def test_method_codes
    get_msg = CoAP::Message.new
    get_msg.code = CoAP::CODE_GET
    assert_equal("GET", get_msg.method_name)

    post_msg = CoAP::Message.new
    post_msg.code = CoAP::CODE_POST
    assert_equal("POST", post_msg.method_name)

    put_msg = CoAP::Message.new
    put_msg.code = CoAP::CODE_PUT
    assert_equal("PUT", put_msg.method_name)

    delete_msg = CoAP::Message.new
    delete_msg.code = CoAP::CODE_DELETE
    assert_equal("DELETE", delete_msg.method_name)
  end

  def test_response_codes
    created = CoAP::Response.new(code: CoAP::CODE_CREATED)
    assert_equal(CoAP::CODE_CREATED, created.code)

    content = CoAP::Response.new(code: CoAP::CODE_CONTENT)
    assert_equal(CoAP::CODE_CONTENT, content.code)

    not_found = CoAP::Response.new(code: CoAP::CODE_NOT_FOUND)
    assert_equal(CoAP::CODE_NOT_FOUND, not_found.code)
  end

  def test_response_code_string
    msg = CoAP::Message.new
    msg.code = CoAP::CODE_CONTENT  # 2.05
    assert_equal("2.05", msg.response_code_string)

    msg.code = CoAP::CODE_NOT_FOUND  # 4.04
    assert_equal("4.04", msg.response_code_string)

    msg.code = CoAP::CODE_CREATED  # 2.01
    assert_equal("2.01", msg.response_code_string)
  end

  def test_empty_message
    msg = CoAP::Message.new
    msg.code = CoAP::CODE_EMPTY
    msg.message_id = 0

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(CoAP::CODE_EMPTY, decoded.code)
    assert_equal(0, decoded.message_id)
    assert_equal("", decoded.payload)
  end

  def test_message_with_empty_token
    msg = CoAP::Message.new
    msg.message_id = 700
    msg.token = ""

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(0, decoded.token_length)
    assert_equal("", decoded.token)
  end

  def test_option_with_empty_value
    msg = CoAP::Message.new
    msg.message_id = 800
    msg.add_option(CoAP::OPTION_IF_NONE_MATCH, "")

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(1, decoded.options.length)
    assert_equal("", decoded.options[0][:value])
  end

  def test_large_message_id
    msg = CoAP::Message.new
    msg.message_id = 65535  # Max 16-bit value

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(65535, decoded.message_id)
  end

  def test_uri_path_with_slashes
    req = CoAP::Request.new(CoAP::CODE_GET)
    req.uri_path = "/a/b/c/d"

    assert_equal("/a/b/c/d", req.uri_path)
    assert_equal(4, req.options.select { |opt| opt[:number] == CoAP::OPTION_URI_PATH }.length)
  end

  def test_uri_path_root
    req = CoAP::Request.new(CoAP::CODE_GET)
    req.uri_path = "/"

    assert_equal("/", req.uri_path)
  end

  def test_option_ordering
    msg = CoAP::Message.new
    # Add options in random order
    msg.add_option(CoAP::OPTION_CONTENT_FORMAT, CoAP::FORMAT_TEXT_PLAIN)
    msg.add_option(CoAP::OPTION_URI_PATH, "test")
    msg.add_option(CoAP::OPTION_URI_HOST, "localhost")

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    # Options should be sorted by number
    assert_equal(3, decoded.options.length)
  end

  def test_multiple_uri_path_segments
    msg = CoAP::Message.new
    msg.add_option(CoAP::OPTION_URI_PATH, "api")
    msg.add_option(CoAP::OPTION_URI_PATH, "v1")
    msg.add_option(CoAP::OPTION_URI_PATH, "sensors")

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    uri_paths = decoded.options.select { |opt| opt[:number] == CoAP::OPTION_URI_PATH }
    assert_equal(3, uri_paths.length)
    assert_equal("api", uri_paths[0][:value])
    assert_equal("v1", uri_paths[1][:value])
    assert_equal("sensors", uri_paths[2][:value])
  end

  def test_payload_only
    msg = CoAP::Message.new
    msg.message_id = 900
    msg.payload = "test payload"

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal("test payload", decoded.payload)
    assert_equal(0, decoded.options.length)
  end

  def test_long_payload
    msg = CoAP::Message.new
    msg.message_id = 1000
    msg.payload = "a" * 500

    encoded = msg.encode
    decoded = CoAP::Message.decode(encoded)

    assert_equal(500, decoded.payload.length)
  end
end
