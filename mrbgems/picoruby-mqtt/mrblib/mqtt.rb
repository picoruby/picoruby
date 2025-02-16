class MQTTClient
  def initialize(host, port = 1883, client_id = "picoruby_mqtt")
    @host = host
    @port = port || 1883
    @client_id = client_id
    @ssl = false
    @ca_cert = nil
    $_mqtt_singleton = self
  end

  def connect(opts = {})
    @ssl = !!opts[:ssl]
    @ca_cert = opts[:ca_cert] if opts[:ca_cert]
    _connect_impl(@host, @port, @client_id, false)
  end

  def ssl=(value)
    @ssl = value
    @port = 8883 if @ssl && @port == 1883
  end

  def ssl?
    @ssl
  end

  def ca_cert=(cert)
    @ca_cert = cert
  end

  def ca_cert
    @ca_cert
  end

  def publish(topic, payload)
    _publish_impl(payload, topic)
  end

  def subscribe(topic)
    _subscribe_impl(topic)
  end

  def instance
    $_mqtt_singleton
  end

  # You can override this method
  def callback
    blink_led
  end

  def blink_led
    @led ||= CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on ||= false
    @led&.write((@led_on = !@led_on) ? 1 : 0)
  end

  def disconnect
    _disconnect_impl
  end
end
