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
        # Initiate non-blocking connection
        result = _connect_impl(@host, @port, @client_id)
        puts "[Ruby] _connect_impl returned: #{result}"
        return false unless result

        puts "[Ruby] Starting connection check loop"

        # Short test loop (1 second timeout)
        10.times do |i|
          puts "[Ruby] Check #{i}"

          if _is_connected_impl
            puts "[Ruby] Connected!"
            @connected = true
            return true
          end

          puts "[Ruby] About to sleep"
          sleep(0.1)
          puts "[Ruby] Woke up from sleep"
        end

        puts "[Ruby] Connection timeout"
        @connected = false
        false
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
