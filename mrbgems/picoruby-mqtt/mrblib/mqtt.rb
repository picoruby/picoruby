module MQTT
  class Client
    def initialize(host:, port: 1883, client_id: nil)
      @host = host
      @port = port
      @client_id = client_id || "picoruby-#{Time.now.to_i}"
      @connected = false
    end

    def connect(host = nil, port = nil, client_id = nil)
      @host = host if host
      @port = port if port
      @client_id = client_id if client_id

      @connected = _connect_impl(@host, @port, @client_id)
    end

    def disconnect
      return unless @connected
      _disconnect_impl
      @connected = false
    end

    def connected?
      @connected
    end

    def publish(topic, payload)
      return false unless @connected
      _publish_impl(topic, payload.to_s)
    end

    def subscribe(topic)
      return false unless @connected
      _subscribe_impl(topic)
    end

    def get(&block)
      return unless @connected && block_given?

      loop do
        message = _get_message_impl
        if message
          block.call(message[0], message[1])
        end
        sleep_ms 10
      end
    end
  end
end
