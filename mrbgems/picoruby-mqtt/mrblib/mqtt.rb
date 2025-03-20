class MQTTClient
  VERIFY_NONE = 0
  VERIFY_PEER = 1
  VERIFY_FAIL_IF_NO_PEER_CERT = 2

  TLS_V1_2 = 0
  TLS_V1_1 = 1
  TLS_V1 = 2

  def initialize(host, port = nil, client_id = "picoruby_mqtt", **args)
    @host = host
    @port = port
    @client_id = client_id
    @ssl = args[:ssl] || false
    @ssl_context = {}
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

    if opts[:ssl]
      @ssl = opts[:ssl]
      setup_ssl_context(opts) if @ssl
    end

    _connect_impl(
      @host,
      @port,
      @client_id,
      @ssl,
      @ssl_context[:ca_file],
      @ssl_context[:cert_file],
      @ssl_context[:key_file],
      @ssl_context[:version] || TLS_V1_2,
      @ssl_context[:verify_mode] || VERIFY_PEER,
      @ssl_context[:ca_cert],
      @ssl_context[:client_cert],
      @ssl_context[:client_key],
      @ssl_context[:verify_hostname].nil? ? true : @ssl_context[:verify_hostname],
      @ssl_context[:handshake_timeout] || 30_000
    )
  end

  def set_ssl_context(opts = {})
    setup_ssl_context(opts)
  end

  def setup_ssl_context(args)
    return unless @ssl

    verify_mode = case args[:verify_mode]
                 when :none then VERIFY_NONE
                 when :fail_if_no_peer_cert then VERIFY_FAIL_IF_NO_PEER_CERT
                 else VERIFY_PEER
                 end

    version = case args[:ssl_version]
             when :TLSv1_1 then TLS_V1_1
             when :TLSv1 then TLS_V1
             else TLS_V1_2
             end

    @ssl_context = {
      ca_file: args[:ca_file],
      cert_file: args[:cert_file],
      key_file: args[:key_file],
      ca_cert: args[:ca_cert],
      client_cert: args[:client_cert],
      client_key: args[:client_key],
      verify_mode: verify_mode,
      version: version,
      verify_hostname: args[:verify_hostname],
      handshake_timeout: args[:handshake_timeout]
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
