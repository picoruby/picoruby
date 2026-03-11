module Net
  module MQTT
    class MQTTError < StandardError; end
    class ConnectionError < MQTTError; end
    class ProtocolError < MQTTError; end

    CONNECT     = 1
    CONNACK     = 2
    PUBLISH     = 3
    PUBACK      = 4
    PUBREC      = 5
    PUBREL      = 6
    PUBCOMP     = 7
    SUBSCRIBE   = 8
    SUBACK      = 9
    UNSUBSCRIBE = 10
    UNSUBACK    = 11
    PINGREQ     = 12
    PINGRESP    = 13
    DISCONNECT  = 14

    class Client
      attr_reader :host, :port
      attr_accessor :client_id, :keep_alive, :clean_session
      attr_accessor :username, :password
      attr_accessor :ssl, :ca_file, :cert_file, :key_file

      def initialize(host, port = 1883, **options)
        @host = host
        @port = port
        @client_id = options[:client_id] || "picoruby-#{Time.now.to_i}"
        @keep_alive = options[:keep_alive] || 60
        @clean_session = options[:clean_session] || true
        @username = options[:username]
        @password = options[:password]
        @ssl = options[:ssl] || false
        @ca_file = options[:ca_file]
        @cert_file = options[:cert_file]
        @key_file = options[:key_file]
        @connected = false
      end

      def self.connect(host, port = 1883, **options, &block)
        client = new(host, port, **options)
        client.connect
        if block
          begin
            block.call(client)
          ensure
            client.disconnect
          end
        else
          client
        end
      end

      def connect
        if @ssl
          raise MQTTError.new("TLS not supported")
        end
        if @username || @password
          raise MQTTError.new("username/password not supported")
        end

        # Initiate non-blocking connection
        result = _connect_impl(@host, @port, @client_id)
        raise ConnectionError.new("Connection failed") unless result

        # Short test loop (~3 seconds timeout)
        300.times do |i|
          _poll_impl if respond_to?(:_poll_impl)
          if _is_connected_impl
            @connected = true
            return true
          end

          poll_sleep_ms(10)
        end

        @connected = false
        raise ConnectionError.new("Connection timeout")
      end

      def poll_sleep_ms(ms)
        if respond_to?(:_poll_sleep_impl)
          _poll_sleep_impl(ms)
        elsif respond_to?(:_poll_impl)
          ms.times do
            _poll_impl
            sleep_ms 1
          end
        else
          sleep_ms(ms)
        end
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
        raise MQTTError.new("Not connected") unless @connected
        raise MQTTError.new("QoS must be 0") if qos != 0
        raise MQTTError.new("Retain not supported") if retain
        _publish_impl(topic, payload.to_s)
      end

      def subscribe(*topics, qos: 0)
        raise MQTTError.new("Not connected") unless @connected
        raise MQTTError.new("QoS must be 0") if qos != 0
        raise MQTTError.new("Only one topic supported") if topics.length != 1
        _subscribe_impl(topics[0])
      end

      def unsubscribe(*topics)
        raise MQTTError.new("Not connected") unless @connected
        raise MQTTError.new("unsubscribe not supported")
      end

      def receive(timeout: nil)
        raise MQTTError.new("Not connected") unless @connected

        deadline = timeout ? Time.now.to_f + timeout : nil
        while true
          message = _get_message_impl
          return message if message
          return nil if deadline && Time.now.to_f > deadline
          sleep_ms 10
        end
      end

      def ping
        raise MQTTError.new("Not connected") unless @connected
        raise MQTTError.new("ping not supported")
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
