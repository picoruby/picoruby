class MQTTClient
  def initialize(host, port = nil, client_id = "picoruby_mqtt")
    @host = host
    @port = port || 1883
    @client_id = client_id
    @connected = false
    @led_state = false
    on_message
  end

  attr_accessor :led
  def connect
    @port ||= 1883
    @connected = _connect_impl(@host, @port, @client_id)
  end

  def publish(topic, payload)
    return false unless @connected
    _publish_impl(payload, topic)
  end

  def subscribe(topic)
    return false unless @connected
    _subscribe_impl(topic)
  end

  def disconnect
    return false unless @connected
    @connected = false
    _disconnect_impl
  end

  def process_messages
    return unless @connected
    packet = _pop_packet_impl
    if packet
      result = _parse_packet_impl(packet)
      if result
        topic, payload = result
        @message_callback&.call(topic, payload) if @message_callback
      end
    end
  end

  # You can override by passing block
  def on_message(&block)
    if block_given?
      @message_callback = block
    else
      # default callback
      @message_callback = Proc.new do |topic, payload|
        puts "Received message on topic '#{topic}': #{payload}"
        @led_state = !@led_state
        @led&.write(@led_state ? 1 : 0)
      end
    end
  end

  def wait_for_messages
    puts "Waiting for messages... (Press reset button to stop)"
    loop do
      process_messages
      sleep_ms(100)
    end
  end
end
