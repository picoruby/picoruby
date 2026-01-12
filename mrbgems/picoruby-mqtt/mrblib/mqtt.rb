#
# MQTT library for PicoRuby
# This is a pure Ruby implementation of MQTT 3.1.1 client.
# Designed for IoT devices and constrained environments.
#
# Author: Hitoshi HASUMI
# License: MIT
#

require 'socket'
require 'pack'

module MQTT
  class MQTTError < StandardError; end
  class ConnectionError < MQTTError; end
  class ProtocolError < MQTTError; end

  # MQTT Control Packet Types
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

  # Connection return codes
  CONNACK_ACCEPTED              = 0x00
  CONNACK_REFUSED_PROTOCOL      = 0x01
  CONNACK_REFUSED_IDENTIFIER    = 0x02
  CONNACK_REFUSED_SERVER        = 0x03
  CONNACK_REFUSED_CREDENTIALS   = 0x04
  CONNACK_REFUSED_AUTHORIZED    = 0x05

  class Client
    attr_reader :host, :port
    attr_accessor :client_id, :keep_alive, :clean_session
    attr_accessor :username, :password

    def initialize(host, port = 1883)
      @host = host
      @port = port
      @client_id = nil
      @keep_alive = 60
      @clean_session = true
      @username = nil
      @password = nil
      @socket = nil
      @packet_id = 0
      @last_ping = nil
    end

    def self.connect(host, port = 1883, **options, &block)
      client = new(host, port)
      client.connect(**options)
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

    def connect(**options)
      @client_id = options[:client_id] || "picoruby_#{Time.now.to_i}"
      @keep_alive = options[:keep_alive] || 60
      @clean_session = options[:clean_session] || true
      @username = options[:username]
      @password = options[:password]

      # Open TCP connection
      @socket = TCPSocket.new(@host, @port)

      # Send CONNECT packet
      send_connect

      # Receive CONNACK
      receive_connack

      @last_ping = Time.now
      true
    end

    def connected?
      @socket.nil? ? false : !(@socket.closed?)
    end

    def publish(topic, payload, retain: false, qos: 0)
      raise MQTTError.new("Not connected") unless connected?
      raise MQTTError.new("QoS must be 0") if qos != 0

      send_publish(topic, payload, retain: retain, qos: qos)
    end

    def subscribe(*topics, qos: 0)
      raise MQTTError.new("Not connected") unless connected?
      raise MQTTError.new("QoS must be 0") if qos != 0

      send_subscribe(topics, qos)
      receive_suback
    end

    def unsubscribe(*topics)
      raise MQTTError.new("Not connected") unless connected?

      send_unsubscribe(topics)
      receive_unsuback
    end

    def receive(timeout: nil)
      raise MQTTError.new("Not connected") unless connected?

      # Check if we need to send PING
      check_keepalive

      # Wait for PUBLISH packet
      deadline = timeout ? Time.now.to_f + timeout : nil

      loop do
        if deadline && Time.now.to_f > deadline
          return nil
        end

        # Non-blocking receive with short timeout
        packet_type, flags, data = receive_packet

        case packet_type
        when PUBLISH
          # Parse PUBLISH packet
          topic, payload = parse_publish(flags, data)
          return [topic, payload]
        when PINGRESP
          # Update last ping time
          @last_ping = Time.now
        else
          # Unexpected packet, ignore
        end

        # Check keepalive even while waiting
        check_keepalive
      end

      nil # never reached
    end

    def ping
      raise MQTTError.new("Not connected") unless connected?

      send_ping
      @last_ping = Time.now

      # Wait for PINGRESP
      packet_type, flags, data = receive_packet
      if packet_type != PINGRESP
        raise ProtocolError.new("Expected PINGRESP, got #{packet_type}")
      end

      true
    end

    def disconnect
      return unless connected?

      send_disconnect
      @socket.close
      @socket = nil
    end

    private

    def next_packet_id
      @packet_id = (@packet_id + 1) & 0xFFFF
      @packet_id = 1 if @packet_id == 0
      @packet_id
    end

    def check_keepalive
      return unless @keep_alive > 0

      elapsed = Time.now.to_f - @last_ping.to_f
      if elapsed >= @keep_alive * 0.8
        ping
      end
    end

    # Encode variable length integer
    def encode_length(length)
      result = ""
      loop do
        byte = length % 128
        length = length / 128
        if length > 0
          byte |= 0x80
        end
        result += [byte].pack("C")
        break if length == 0
      end
      result
    end

    # Decode variable length integer
    def decode_length(data, offset = 0)
      multiplier = 1
      value = 0
      loop do
        sliced_data = data[offset]
        break unless sliced_data.is_a?(String)

        byte = sliced_data.ord
        offset += 1
        value += (byte & 0x7F) * multiplier
        break if (byte & 0x80) == 0
        multiplier *= 128
        raise ProtocolError.new("Invalid remaining length") if multiplier > 128*128*128
      end
      [value, offset]
    end

    # Encode string with length prefix
    def encode_string(str)
      [str.length].pack("n") + str
    end

    def send_connect
      # Variable header
      protocol_name = encode_string("MQTT")
      protocol_level = [4].pack("C")  # MQTT 3.1.1

      # Connect flags
      flags = 0
      flags |= 0x02 if @clean_session  # Clean session
      if @username
        flags |= 0x80  # Username flag
        flags |= 0x40 if @password  # Password flag
      end

      connect_flags = [flags].pack("C")
      keep_alive = [@keep_alive].pack("n")

      variable_header = protocol_name + protocol_level + connect_flags + keep_alive

      # Payload
      payload = encode_string(@client_id)
      payload += encode_string(@username) if @username
      payload += encode_string(@password || "") if @password

      # Fixed header
      packet_type = (CONNECT << 4)
      remaining_length = variable_header.length + payload.length

      packet = [packet_type].pack("C") + encode_length(remaining_length) + variable_header + payload

      @socket.write(packet)
    end

    def receive_connack
      packet_type, flags, data = receive_packet

      if packet_type != CONNACK
        raise ProtocolError.new("Expected CONNACK, got #{packet_type}")
      end

      if data.length < 2
        raise ProtocolError.new("Invalid CONNACK packet")
      end

      return_code_data = data[1]
      return_code = return_code_data.is_a?(String) ? return_code_data.ord : -1

      if return_code != CONNACK_ACCEPTED
        error_messages = {
          CONNACK_REFUSED_PROTOCOL => "Unacceptable protocol version",
          CONNACK_REFUSED_IDENTIFIER => "Identifier rejected",
          CONNACK_REFUSED_SERVER => "Server unavailable",
          CONNACK_REFUSED_CREDENTIALS => "Bad username or password",
          CONNACK_REFUSED_AUTHORIZED => "Not authorized"
        }
        raise ConnectionError.new(error_messages[return_code] || "Connection refused: #{return_code}")
      end

      true
    end

    def send_publish(topic, payload, retain: false, qos: 0)
      # Variable header
      variable_header = encode_string(topic)

      # No packet identifier for QoS 0

      # Fixed header
      flags = 0
      flags |= 0x01 if retain
      flags |= (qos << 1)

      packet_type = (PUBLISH << 4) | flags
      remaining_length = variable_header.length + payload.length

      packet = [packet_type].pack("C") + encode_length(remaining_length) + variable_header + payload

      @socket.write(packet)
    end

    def send_subscribe(topics, qos)
      # Variable header
      packet_id = next_packet_id
      variable_header = [packet_id].pack("n")

      # Payload
      payload = ""
      topics.each do |topic|
        payload += encode_string(topic)
        payload += [qos].pack("C")
      end

      # Fixed header
      packet_type = (SUBSCRIBE << 4) | 0x02  # Reserved flags
      remaining_length = variable_header.length + payload.length

      packet = [packet_type].pack("C") + encode_length(remaining_length) + variable_header + payload

      @socket.write(packet)
    end

    def receive_suback
      packet_type, flags, data = receive_packet

      if packet_type != SUBACK
        raise ProtocolError.new("Expected SUBACK, got #{packet_type}")
      end

      # data[0..1] is packet ID
      # data[2..-1] are return codes for each subscription
      # For now, we just check that we got SUBACK
      true
    end

    def send_unsubscribe(topics)
      # Variable header
      packet_id = next_packet_id
      variable_header = [packet_id].pack("n")

      # Payload
      payload = ""
      topics.each do |topic|
        payload += encode_string(topic)
      end

      # Fixed header
      packet_type = (UNSUBSCRIBE << 4) | 0x02  # Reserved flags
      remaining_length = variable_header.length + payload.length

      packet = [packet_type].pack("C") + encode_length(remaining_length) + variable_header + payload

      @socket.write(packet)
    end

    def receive_unsuback
      packet_type, flags, data = receive_packet

      if packet_type != UNSUBACK
        raise ProtocolError.new("Expected UNSUBACK, got #{packet_type}")
      end

      true
    end

    def send_ping
      packet_type = (PINGREQ << 4)
      packet = [packet_type, 0].pack("CC")

      @socket.write(packet)
    end

    def send_disconnect
      packet_type = (DISCONNECT << 4)
      packet = [packet_type, 0].pack("CC")

      @socket.write(packet)
    end

    def receive_packet
      # Read fixed header
      byte1 = @socket.read(1)
      raise ConnectionError.new("Connection closed") if byte1.nil? || byte1.empty?

      packet_type = (byte1[0].ord >> 4) & 0x0F
      flags = byte1[0].ord & 0x0F

      # Read remaining length
      remaining_length, offset = read_remaining_length

      # Read data
      data = ""
      if remaining_length > 0
        data = @socket.read(remaining_length)
        if data.nil? || data.length < remaining_length
          raise ConnectionError.new("Incomplete packet")
        end
      end

      [packet_type, flags, data]
    end

    def read_remaining_length
      multiplier = 1
      value = 0
      loop do
        byte_str = @socket.read(1)
        raise ConnectionError.new("Connection closed") if byte_str.nil? || byte_str.empty?

        byte = byte_str[0].ord
        value += (byte & 0x7F) * multiplier
        break if (byte & 0x80) == 0
        multiplier *= 128
        raise ProtocolError.new("Invalid remaining length") if multiplier > 128*128*128
      end
      [value, 0]
    end

    def parse_publish(flags, data)
      offset = 0

      # Topic name
      topic_data = data[offset, 2] || ''
      topic_length = topic_data.unpack("n")[0]
      offset += 2
      topic = data[offset, topic_length] || ''
      offset += topic_length

      # QoS level
      qos = (flags >> 1) & 0x03

      # Packet ID (only for QoS > 0)
      if qos > 0
        packet_id_data = data[offset, 2] || ''
        packet_id = packet_id_data.unpack("n")[0]
        offset += 2
      end

      # Payload
      payload = data[offset..-1] || ''

      [topic, payload]
    end
  end
end
