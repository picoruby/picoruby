module MQTT
  class Client
    def self.connect(host_or_opts, port = nil, client_id = nil)
      opts = case host_or_opts
      when String
        {
          host: host_or_opts,
          port: port || 1883,
          client_id: client_id || "picoruby_mqtt"
        }
      when Hash
        host_or_opts
      else
        raise ArgumentError, "First argument must be a String or Hash"
      end

      client = new(opts)
      client.connect

      if block_given?
        begin
          yield client
          nil
        ensure
          client.disconnect
        end
      else
        client
      end
    end

    def initialize(host_or_opts, port = nil, client_id = nil)
      case host_or_opts
      when String
        @host = host_or_opts
        @port = port || 1883
        @client_id = client_id || "picoruby_mqtt"
      when Hash
        opts = assert_hash(host_or_opts)
        @host = (opts[:host] || opts["host"]) || raise(ArgumentError, "host is required")
        @port = (opts[:port] || opts["port"] || 1883).to_i
        @client_id = opts[:client_id] || opts["client_id"] || "picoruby_mqtt"
      else
        raise ArgumentError, "First argument must be a String or Hash"
      end

      @connected = false
      @message_callback = nil
      @last_message = nil
      $_mqtt_singleton = self
    end

    def connect
      @connected = _connect_impl(@host, @port, @client_id, false)
    end

    def publish(topic, payload, retain = false)
      return false unless @connected
      _publish_impl(payload, topic)
    end

    def subscribe(topic)
      return false unless @connected
      _subscribe_impl(topic)
    end

    def get(topic = nil, &block)
      subscribe(topic) if topic

      if block_given?
        @message_callback = block
        wait_for_messages
        nil
      else
        process_messages
        if @last_message
          result = @last_message
          @last_message = nil
          result
        end
      end
    end

    def process_messages
      packet = _pop_packet_impl
      if packet
        result = _parse_packet_impl(packet)
        if result
          topic, payload = result
          @last_message = [topic, payload]
          @message_callback&.call(topic, payload) if @message_callback
        end
      end
    end

    def wait_for_messages
      loop do
        process_messages
        sleep_ms(100)
      end
    end

    def on_message(&block)
      @message_callback = block if block_given?
    end

    def disconnect
      return false unless @connected
      @connected = false
      _disconnect_impl
    end

    def instance
      $_mqtt_singleton
    end

    def assert_hash(hash)
      raise ArgumentError, "Expected Hash" unless hash.is_a?(Hash)
      hash
    end
  end
end
