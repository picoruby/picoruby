class MQTTClient
  SSL_VERIFY_MODES = {
    none: 0,
    peer: 1
  }.freeze

  SSL_VERSIONS = {
    TLSv1_2: 0,
    TLSv1_1: 1,
    TLSv1: 2
  }.freeze

  def initialize(host, port = nil, client_id = "picoruby_mqtt", **args)
    @host = host
    @port = port
    @client_id = client_id
    @ssl = args[:ssl] || false
    @ssl_context = nil
    $_mqtt_singleton = self

    setup_ssl_context(args) if @ssl
  end

  def ssl=(value)
    @ssl = value
    @port = 8883 if @ssl && (@port.nil? || @port == 1883)
  end

  def ssl?
    @ssl
  end

  def connect(opts = {})
    @port ||= @ssl ? 8883 : 1883

    if opts.key?(:ssl)
      @ssl = opts[:ssl]
      setup_ssl_context(opts) if @ssl
    end

    _connect_impl(
      @host,
      @port,
      @client_id,
      @ssl,
      @ssl_context&.dig(:ca_file),
      @ssl_context&.dig(:cert_file),
      @ssl_context&.dig(:key_file),
      @ssl_context&.dig(:version),
      @ssl_context&.dig(:verify_mode)
    )
  end

  def set_ssl_context(opts = {})
    setup_ssl_context(opts)
  end

  def setup_ssl_context(args)
    return unless @ssl

    @ssl_context = {
      cert_file: args[:cert_file],
      key_file: args[:key_file],
      ca_file: args[:ca_file],
      verify_mode: SSL_VERIFY_MODES[args[:verify_mode] || :peer],
      version: SSL_VERSIONS[args[:ssl_version] || :TLSv1_2]
    }
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
