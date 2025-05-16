class MQTTClient
  def initialize(host, port = nil, client_id = "picoruby_mqtt")
    @host = host
    @port = port || 1883
    @client_id = client_id
    @connected = false
    @message_callback = nil
  end

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
        puts "Received message on topic '#{topic}': #{payload}"
        @message_callback&.call(topic, payload) if @message_callback
      end
    end
  end

  def on_message(&block)
    @message_callback = block if block_given?
  end

  def wait_for_messages
    puts "Waiting for messages... (Press reset button to stop)"
    loop do
      process_messages
      sleep_ms(100) 
    end
  end
end
