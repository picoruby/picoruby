module MQTT
  class Client
    def initialize(host:, port: 1883)
      @host = host
      @port = port
      @connected = false
    end

    def connect(client_id = nil)
      @client_id = client_id || "picoruby-#{Time.now.to_i}"

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

    def get
      return unless @connected

      # return topic, payload
      return _get_message_impl
    end
  end
end
