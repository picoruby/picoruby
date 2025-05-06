class MQTTClient
  def initialize(host, port = nil, client_id = "picoruby_mqtt")
    @host = host
    @port = port || 1883
    @client_id = client_id
    $_mqtt_singleton = self
  end

  def connect
    @port ||= 1883
    _connect_impl(@host, @port, @client_id)
  end

  def publish(topic, payload)
    _publish_impl(payload, topic)
  end

  def subscribe(topic)
    _subscribe_impl(topic)
  end

  def disconnect
    _disconnect_impl
  end

  def callback
    blink_led
  end

  def blink_led
    @led ||= CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on ||= false
    @led&.write((@led_on = !@led_on) ? 1 : 0)
  end
end
