class MQTTTest < Picotest::Test
  def test_client_initialization
    client = MQTT::Client.new("test.example.com", 1883)
    assert_equal("test.example.com", client.host)
    assert_equal(1883, client.port)
    assert_equal(60, client.keep_alive)
    assert_equal(true, client.clean_session)
  end

  def test_client_initialization_default_port
    client = MQTT::Client.new("test.example.com")
    assert_equal(1883, client.port)
  end

  def test_encode_length_small
    client = MQTT::Client.new("test.example.com")
    encoded = client.send(:encode_length, 0)
    assert_equal("\x00", encoded)

    encoded = client.send(:encode_length, 127)
    assert_equal("\x7F", encoded)
  end

  def test_encode_length_medium
    client = MQTT::Client.new("test.example.com")
    encoded = client.send(:encode_length, 128)
    assert_equal("\x80\x01", encoded)

    encoded = client.send(:encode_length, 16383)
    assert_equal("\xFF\x7F", encoded)
  end

  def test_encode_length_large
    client = MQTT::Client.new("test.example.com")
    encoded = client.send(:encode_length, 16384)
    assert_equal("\x80\x80\x01", encoded)
  end

  def test_decode_length_small
    client = MQTT::Client.new("test.example.com")
    value, offset = client.send(:decode_length, "\x00")
    assert_equal(0, value)
    assert_equal(1, offset)

    value, offset = client.send(:decode_length, "\x7F")
    assert_equal(127, value)
    assert_equal(1, offset)
  end

  def test_decode_length_medium
    client = MQTT::Client.new("test.example.com")
    value, offset = client.send(:decode_length, "\x80\x01")
    assert_equal(128, value)
    assert_equal(2, offset)

    value, offset = client.send(:decode_length, "\xFF\x7F")
    assert_equal(16383, value)
    assert_equal(2, offset)
  end

  def test_decode_length_large
    client = MQTT::Client.new("test.example.com")
    value, offset = client.send(:decode_length, "\x80\x80\x01")
    assert_equal(16384, value)
    assert_equal(3, offset)
  end

  def test_encode_string
    client = MQTT::Client.new("test.example.com")
    encoded = client.send(:encode_string, "hello")
    assert_equal("\x00\x05hello", encoded)
  end

  def test_encode_string_empty
    client = MQTT::Client.new("test.example.com")
    encoded = client.send(:encode_string, "")
    assert_equal("\x00\x00", encoded)
  end

  def test_encode_string_long
    client = MQTT::Client.new("test.example.com")
    long_str = "a" * 300
    encoded = client.send(:encode_string, long_str)
    assert_equal([300].pack("n") + long_str, encoded)
  end

  def test_next_packet_id
    client = MQTT::Client.new("test.example.com")
    id1 = client.send(:next_packet_id)
    id2 = client.send(:next_packet_id)
    assert_equal(1, id1)
    assert_equal(2, id2)
  end

  def test_next_packet_id_wraparound
    client = MQTT::Client.new("test.example.com")
    # Set packet_id to near max
    client.instance_variable_set(:@packet_id, 65534)
    id1 = client.send(:next_packet_id)
    id2 = client.send(:next_packet_id)
    id3 = client.send(:next_packet_id)
    assert_equal(65535, id1)
    assert_equal(1, id2)  # Should wrap to 1, not 0
    assert_equal(2, id3)
  end

  def test_connected_returns_false_initially
    client = MQTT::Client.new("test.example.com")
    assert_equal(false, client.connected?)
  end

  def test_client_id_generation
    client = MQTT::Client.new("test.example.com")
    client.client_id = "test-client-123"
    assert_equal("test-client-123", client.client_id)
  end

  def test_keep_alive_setting
    client = MQTT::Client.new("test.example.com")
    client.keep_alive = 30
    assert_equal(30, client.keep_alive)
  end

  def test_clean_session_setting
    client = MQTT::Client.new("test.example.com")
    client.clean_session = false
    assert_equal(false, client.clean_session)
  end

  def test_username_password_setting
    client = MQTT::Client.new("test.example.com")
    client.username = "user123"
    client.password = "pass456"
    assert_equal("user123", client.username)
    assert_equal("pass456", client.password)
  end

  def test_mqtt_packet_types
    assert_equal(1, MQTT::CONNECT)
    assert_equal(2, MQTT::CONNACK)
    assert_equal(3, MQTT::PUBLISH)
    assert_equal(8, MQTT::SUBSCRIBE)
    assert_equal(9, MQTT::SUBACK)
    assert_equal(10, MQTT::UNSUBSCRIBE)
    assert_equal(11, MQTT::UNSUBACK)
    assert_equal(12, MQTT::PINGREQ)
    assert_equal(13, MQTT::PINGRESP)
    assert_equal(14, MQTT::DISCONNECT)
  end

  def test_connack_return_codes
    assert_equal(0x00, MQTT::CONNACK_ACCEPTED)
    assert_equal(0x01, MQTT::CONNACK_REFUSED_PROTOCOL)
    assert_equal(0x02, MQTT::CONNACK_REFUSED_IDENTIFIER)
    assert_equal(0x03, MQTT::CONNACK_REFUSED_SERVER)
    assert_equal(0x04, MQTT::CONNACK_REFUSED_CREDENTIALS)
    assert_equal(0x05, MQTT::CONNACK_REFUSED_AUTHORIZED)
  end

  def test_parse_publish_qos0
    client = MQTT::Client.new("test.example.com")

    # Construct a PUBLISH packet for QoS 0
    # Topic: "test/topic" (10 bytes)
    # Payload: "hello"
    data = "\x00\x0Atest/topic" + "hello"

    topic, payload = client.send(:parse_publish, 0, data)
    assert_equal("test/topic", topic)
    assert_equal("hello", payload)
  end

  def test_parse_publish_empty_payload
    client = MQTT::Client.new("test.example.com")

    data = "\x00\x05topic"

    topic, payload = client.send(:parse_publish, 0, data)
    assert_equal("topic", topic)
    assert_equal("", payload)
  end

  def test_raise_error_when_not_connected
    client = MQTT::Client.new("test.example.com")

    assert_raise(MQTT::MQTTError) do
      client.publish("test", "data")
    end
  end

  def test_raise_error_on_unsupported_qos
    client = MQTT::Client.new("test.example.com")

    assert_raise(MQTT::MQTTError) do
      # This should fail even before connecting
      # because QoS check happens first
      begin
        client.publish("test", "data", qos: 1)
      rescue MQTT::MQTTError => e
        raise e if e.message.include?("QoS")
        # If error is "Not connected", we need to skip the QoS check
        # In actual implementation, QoS check should happen first
      end
    end
  end
end

# Note: Integration tests that require actual MQTT broker connection
# should be run separately in a testing environment with broker access
#
# Example integration test (commented out):
#
# class MQTTIntegrationTest < Picotest::Test
#   def test_connect_to_broker
#     client = MQTT::Client.new("test.mosquitto.org", 1883)
#     client.connect(client_id: "picoruby-test")
#     assert(client.connected?)
#     client.disconnect
#   end
#
#   def test_publish_and_subscribe
#     client = MQTT::Client.new("test.mosquitto.org", 1883)
#     client.connect(client_id: "picoruby-test-pubsub")
#
#     client.subscribe("picoruby/test")
#     client.publish("picoruby/test", "Hello MQTT!")
#
#     topic, message = client.receive(timeout: 5)
#     assert_equal("picoruby/test", topic)
#     assert_equal("Hello MQTT!", message)
#
#     client.disconnect
#   end
# end
