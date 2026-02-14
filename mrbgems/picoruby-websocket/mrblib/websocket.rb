#
# WebSocket library for PicoRuby
# This is a pure Ruby implementation of WebSocket client (RFC 6455).
# Designed for real-time communication in IoT devices.
#
# Author: Hitoshi HASUMI
# License: MIT
#

require 'socket'
require 'base64'

module WebSocket
  class WebSocketError < StandardError; end
  class HandshakeError < WebSocketError; end
  class ProtocolError < WebSocketError; end
  class ConnectionClosed < WebSocketError; end

  # Opcodes
  OPCODE_CONTINUATION = 0x0
  OPCODE_TEXT = 0x1
  OPCODE_BINARY = 0x2
  OPCODE_CLOSE = 0x8
  OPCODE_PING = 0x9
  OPCODE_PONG = 0xA

  # Close codes
  CLOSE_NORMAL = 1000
  CLOSE_GOING_AWAY = 1001
  CLOSE_PROTOCOL_ERROR = 1002
  CLOSE_UNSUPPORTED_DATA = 1003
  CLOSE_ABNORMAL = 1006

  # Magic GUID for handshake
  WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

  class Client
    attr_reader :url, :host, :port, :path
    attr_accessor :ssl_context

    def initialize(url)
      @url = url
      @socket = nil
      @headers = {}
      @fragments = []
      @use_ssl = false
      @ssl_context = nil
      parse_url(url)
    end

    def self.connect(url, &block)
      client = new(url)
      client.connect
      if block
        begin
          block.call(client)
        ensure
          client.close
        end
      else
        client
      end
    end

    def add_header(name, value)
      @headers[name] = value
    end

    def connect
      # Open TCP connection
      tcp_socket = TCPSocket.new(@host, @port)

      # Wrap with SSL if using wss://
      if @use_ssl
        # Create default SSL context if not provided
        unless @ssl_context
          @ssl_context = SSLContext.new
          @ssl_context.verify_mode = SSLContext::VERIFY_PEER
        end

        # Create SSL socket
        @socket = SSLSocket.new(tcp_socket, @ssl_context)
        @socket.hostname = @host  # For SNI
        @socket.connect  # Perform SSL handshake
      else
        @socket = tcp_socket
      end

      # Perform WebSocket handshake
      perform_handshake

      true
    end

    def connected?
      @socket ? !@socket.closed? : false
    end

    def send_text(text)
      send_frame(OPCODE_TEXT, text)
    end

    def send_binary(data)
      send_frame(OPCODE_BINARY, data)
    end

    def send(data, type: :text)
      opcode = type == :binary ? OPCODE_BINARY : OPCODE_TEXT
      send_frame(opcode, data)
    end

    def receive(timeout: nil)
      raise WebSocketError.new("Not connected") unless connected?

      deadline = timeout ? Time.now.to_f + timeout : nil

      loop do
        if deadline && Time.now.to_f > deadline
          return nil
        end

        # Check if data is available
        ready = IO.select([@socket], nil, nil, 0.1)
        if ready && ready[0]
          opcode, payload = receive_frame

          case opcode
          when OPCODE_TEXT, OPCODE_BINARY
            return payload
          when OPCODE_CLOSE
            # Server initiated close
            handle_close_frame(payload)
            raise ConnectionClosed.new("Connection closed by server")
          when OPCODE_PING
            # Respond with pong
            send_frame(OPCODE_PONG, payload)
          when OPCODE_PONG
            # Ignore pong (could store for latency measurement)
          end
        end
      end
    end

    def ping(payload = "")
      send_frame(OPCODE_PING, payload)
    end

    def close(code = CLOSE_NORMAL, reason = "")
      return unless connected?

      # Send close frame
      payload = [code].pack("n") + reason
      send_frame(OPCODE_CLOSE, payload)

      # Wait for close frame from server
      begin
        ready = IO.select([@socket], nil, nil, 1)
        if ready && ready[0]
          opcode, response = receive_frame
          if opcode == OPCODE_CLOSE
            # Close acknowledged
          end
        end
      rescue
        # Ignore errors during close
      end

      @socket.close
      @socket = nil
    end

    private

    def parse_url(url)
      # Simple URL parser for ws://host:port/path or wss://host:port/path
      if url.start_with?("wss://")
        @use_ssl = true
        url = url[6..-1]
        default_port = 443
      elsif url.start_with?("ws://")
        @use_ssl = false
        url = url[5..-1]
        default_port = 80
      else
        @use_ssl = false
        default_port = 80
      end

      # Split host:port and path
      parts = url.split('/', 2)
      host_port = parts[0]
      @path = parts[1] ? "/" + parts[1] : "/"

      # Split host and port
      if host_port.include?(':')
        @host, port_str = host_port.split(':', 2)
        @port = port_str.to_i
      else
        @host = host_port
        @port = default_port
      end
    end

    def perform_handshake
      # Generate Sec-WebSocket-Key
      key_bytes = ""
      16.times do
        key_bytes += rand(256).chr
      end
      sec_key = Base64.encode(key_bytes)

      # Build HTTP request
      request = "GET #{@path} HTTP/1.1\r\n"
      request += "Host: #{@host}\r\n"
      request += "Upgrade: websocket\r\n"
      request += "Connection: Upgrade\r\n"
      request += "Sec-WebSocket-Key: #{sec_key}\r\n"
      request += "Sec-WebSocket-Version: 13\r\n"

      # Add custom headers
      @headers.each do |name, value|
        request += "#{name}: #{value}\r\n"
      end

      request += "\r\n"

      # Send request
      @socket.write(request)

      # Read response
      response = read_http_response

      # Verify response
      if !response.include?("101") || !response.include?("Switching Protocols")
        raise HandshakeError.new("Invalid handshake response")
      end

      # For simplicity, we don't verify Sec-WebSocket-Accept
      # In production, you should verify it using SHA-1 hash

      true
    end

    def read_http_response
      response = ""
      loop do
        line = read_line
        response += line
        break if line == "\r\n"
      end
      response
    end

    def read_line
      line = ""
      loop do
        char = @socket.read(1)
        raise ConnectionClosed.new("Connection closed during handshake") if char.nil? || char.empty?
        line += char
        break if line.end_with?("\r\n")
      end
      line
    end

    def send_frame(opcode, payload)
      raise WebSocketError.new("Not connected") unless connected?

      # Build frame
      frame = ""

      # Byte 0: FIN + opcode
      fin = 0x80  # FIN bit set
      frame += [fin | opcode].pack("C")

      # Byte 1: MASK + payload length
      mask_bit = 0x80  # Client must mask
      length = payload.length

      if length < 126
        frame += [mask_bit | length].pack("C")
      elsif length < 65536
        frame += [mask_bit | 126].pack("C")
        frame += [length].pack("n")
      else
        frame += [mask_bit | 127].pack("C")
        high = length >> 32
        low = length & 0xffffffff
        frame += [high, low].pack("NN")
      end

      # Masking key (4 random bytes)
      mask_key = ""
      4.times do
        mask_key += rand(256).chr
      end
      frame += mask_key

      # Masked payload
      masked_payload = mask_data(payload, mask_key)
      frame += masked_payload

      @socket.write(frame)
    end

    def receive_frame
      # Read byte 0: FIN + opcode
      byte0 = @socket.read(1)
      raise ConnectionClosed.new("Connection closed") if byte0.nil? || byte0.empty?

      byte0_val = byte0[0].ord
      fin = (byte0_val & 0x80) != 0
      opcode = byte0_val & 0x0F

      # Read byte 1: MASK + payload length
      byte1 = @socket.read(1)
      raise ConnectionClosed.new("Connection closed") if byte1.nil? || byte1.empty?

      byte1_val = byte1[0].ord
      masked = (byte1_val & 0x80) != 0
      payload_len = byte1_val & 0x7F

      # Read extended payload length
      if payload_len == 126
        len_bytes = @socket.read(2)
        payload_len = len_bytes.unpack("n")[0]
      elsif payload_len == 127
        len_bytes = @socket.read(8)
        high, low = len_bytes.unpack("NN")
        payload_len = (high << 32) | low
      end

      # Read masking key (if present)
      mask_key = nil
      if masked
        mask_key = @socket.read(4)
      end

      # Read payload
      payload = ""
      if payload_len > 0
        payload = @socket.read(payload_len)
        if payload.nil? || payload.length < payload_len
          raise ConnectionClosed.new("Incomplete frame")
        end
      end

      # Unmask payload (if masked)
      if masked && mask_key
        payload = mask_data(payload, mask_key)
      end

      # Handle fragmentation
      if !fin
        # Fragmented message
        @fragments << {opcode: opcode, payload: payload}
        # Continue reading fragments
        return receive_frame
      elsif opcode == OPCODE_CONTINUATION
        # Final fragment
        first_fragment = @fragments.shift
        opcode = first_fragment[:opcode]
        full_payload = first_fragment[:payload]

        @fragments.each do |frag|
          full_payload += frag[:payload]
        end
        full_payload += payload

        @fragments = []
        payload = full_payload
      end

      [opcode, payload]
    end

    def mask_data(data, mask_key)
      result = ""
      data.length.times do |i|
        byte = data[i].ord
        masked_byte = byte ^ mask_key[i % 4].ord
        result += masked_byte.chr
      end
      result
    end

    def handle_close_frame(payload)
      if payload.length >= 2
        code = payload[0, 2].unpack("n")[0]
        reason = payload.length > 2 ? payload[2..-1] : ""
      end
    end
  end
end
