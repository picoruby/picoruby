class MQTTClientTest < Picotest::Test
  def test_mqtt_new
    client = MQTT::Client.new('127.0.0.1', 1883)
    assert_equal('127.0.0.1', client.host)
    assert_equal(1883, client.port)
    assert_false(client.connected?)
  end

  # Note: The following tests require an external MQTT broker for connectivity.
  # They are currently commented out because:
  # 1. They depend on an external MQTT broker.
  # 2. Integration tests with network services often require threading/async support,
  #    which is not readily available in the current test environment.
  # Uncomment and ensure an MQTT broker is running before attempting to run these tests.

  # def test_mqtt_connect_disconnect
  #   client = MQTT::Client.new('127.0.0.1', 1883)
  #   client.connect(client_id:  "picoruby-mqtt-test-#{Time.now.to_i}")
  #   assert_true(client.connected?)

  #   client.disconnect
  #   assert_false(client.connected?)
  # end

  # def test_mqtt_publish
  #   client = MQTT::Client.new('127.0.0.1', 1883)
  #   client_id = "picoruby-mqtt-test-#{Time.now.to_i}"
  #   client.connect(client_id:  client_id)

  #   topic = "picoruby-test/topic-#{client_id}"
  #   message = "Hello from PicoRuby MQTT test!"
  #   client.publish(topic, message)

  #   client.disconnect
  # end

  # def test_mqtt_subscribe
  #   client = MQTT::Client.new('127.0.0.1', 1883)
  #   client_id = "picoruby-mqtt-test-#{Time.now.to_i}"
  #   client.connect(client_id:  client_id)

  #   topic = "picoruby-test/topic-#{client_id}"
  #   client.subscribe(topic)
  #   client.unsubscribe(topic)

  #   client.disconnect
  # end
end
