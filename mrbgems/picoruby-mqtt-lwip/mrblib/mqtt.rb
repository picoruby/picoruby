module Net
  module MQTT
    class Client
      attr_reader :host, :port
      attr_accessor :client_id, :keep_alive, :clean_session

      def initialize(host, port = 1883, **options)
        @host = host
        @port = port
        @client_id = options[:client_id] || "picoruby-#{Time.now.to_i}"
        @keep_alive = options[:keep_alive] || 60
        @clean_session = options[:clean_session] || true
        @connected = false
      end

      def connect
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

    def publish(topic, payload, retain: false, qos: 0)
      return false unless @connected
      _publish_impl(topic, payload.to_s)
    end

    def subscribe(topic)
      return false unless @connected
      _subscribe_impl(topic)
    end

    def get(&block)
      return unless @connected

      if block_given?
        # Non-Blocking
        loop do
          message = _get_message_impl
          if message
            yield(message[0], message[1])
          end
          sleep_ms 10
        end
      else
        # Blocking
        _get_message_impl
      end
    end
    end
  end
end
