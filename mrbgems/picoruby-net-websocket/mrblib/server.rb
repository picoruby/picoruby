require 'socket'
require 'base64'
require 'rng'
require 'mbedtls'
require 'pack'

module Net
  module WebSocket
    class Server
      attr_reader :host, :port

      def initialize(host: '0.0.0.0', port: 8080)
        @host = host
        @port = port
        @server = nil
      end

      def start
        @server = TCPServer.new(@host, @port)
        true
      end

      def accept
        raise WebSocketError.new("Server not started") unless @server

        client_socket = @server.accept
        Connection.new(client_socket)
      end

      def accept_loop(&block)
        loop do
          conn = accept
          block.call(conn)
        end
      end

      def close
        @server&.close
        @server = nil
      end

      private

      class Connection
        def initialize(socket)
          @socket = socket
          @fragments = []
          perform_handshake
        end

        def send_text(text)
          send_frame(OPCODE_TEXT, text, masked: false)
        end

        def send_binary(data)
          send_frame(OPCODE_BINARY, data, masked: false)
        end

        def send(data, type: :text)
          opcode = type == :binary ? OPCODE_BINARY : OPCODE_TEXT
          send_frame(opcode, data, masked: false)
        end

        def receive(timeout: nil)
          deadline = timeout ? Time.now.to_f + timeout : nil

          loop do
            if deadline && Time.now.to_f > deadline
              return nil
            end

            if @socket.ready?
              opcode, payload = receive_frame

              case opcode
              when OPCODE_TEXT, OPCODE_BINARY
                return payload
              when OPCODE_CLOSE
                handle_close_frame(payload)
                raise ConnectionClosed.new("Connection closed by client")
              when OPCODE_PING
                send_frame(OPCODE_PONG, payload, masked: false)
              when OPCODE_PONG
                # Ignore pong
              end
            else
              sleep_ms 100
            end
          end
          return nil
        end

        def ping(payload = "")
          send_frame(OPCODE_PING, payload, masked: false)
        end

        def close(code = CLOSE_NORMAL, reason = "")
          return unless @socket && !@socket.closed?

          payload = [code].pack("n") + reason
          send_frame(OPCODE_CLOSE, payload, masked: false)

          begin
            max_wait = 10
            while max_wait > 0 && !@socket.ready?
              sleep_ms 100
              max_wait -= 1
            end

            if @socket.ready?
              opcode, response = receive_frame
            end
          rescue
            # Ignore errors during close
          end

          @socket.close
        end

        def closed?
          @socket.nil? || @socket.closed?
        end

        private

        def perform_handshake
          request = read_http_request
          headers = parse_http_headers(request)

          unless headers['upgrade']&.downcase == 'websocket'
            raise HandshakeError.new("Not a WebSocket upgrade request")
          end

          sec_key = headers['sec-websocket-key']
          unless sec_key
            raise HandshakeError.new("Missing Sec-WebSocket-Key")
          end

          accept_key = compute_accept_key(sec_key)

          response = "HTTP/1.1 101 Switching Protocols\r\n"
          response += "Upgrade: websocket\r\n"
          response += "Connection: Upgrade\r\n"
          response += "Sec-WebSocket-Accept: #{accept_key}\r\n"
          response += "\r\n"

          @socket.write(response)
        end

        def read_http_request
          request = ""
          loop do
            line = read_line
            request += line
            break if line == "\r\n"
          end
          request
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

        def parse_http_headers(request)
          headers = {}
          lines = request.split("\r\n")

          lines.each do |line|
            colon_pos = line.index(':')
            if colon_pos
              key = line[0, colon_pos]&.downcase
              if key.nil? || key.empty?
                raise HandshakeError.new("Invalid header line: #{line}")
              end
              value = line[colon_pos + 1..-1]
              if value.nil?
                raise HandshakeError.new("Invalid header line: #{line}")
              end
              # Strip leading whitespace
              while value.length > 0 && (value[0] == ' ' || value[0] == "\t")
                value = value[1..-1]
              end
              headers[key] = value
            end
          end

          headers
        end

        def compute_accept_key(sec_key)
          digest = MbedTLS::Digest.new(:sha1)
          digest.update(sec_key + WEBSOCKET_GUID)
          hash = digest.finish
          Base64.encode64(hash)
        end

        def send_frame(opcode, payload, masked: false)
          frame = ""

          fin = 0x80
          frame += [fin | opcode].pack("C")

          mask_bit = masked ? 0x80 : 0x00
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

          if masked
            mask_key = ""
            4.times do
              mask_key += (RNG.random_int % 256).chr
            end
            frame += mask_key
            masked_payload = mask_data(payload, mask_key)
            frame += masked_payload
          else
            frame += payload
          end

          @socket.write(frame)
        end

        def receive_frame
          byte0 = @socket.read(1)
          raise ConnectionClosed.new("Connection closed") if byte0.nil? || byte0.empty?

          byte0_val = byte0[0]&.ord || 0
          fin = (byte0_val & 0x80) != 0
          opcode = byte0_val & 0x0F

          byte1 = @socket.read(1)
          raise ConnectionClosed.new("Connection closed") if byte1.nil? || byte1.empty?

          byte1_val = byte1[0]&.ord || 0
          masked = (byte1_val & 0x80) != 0
          payload_len = byte1_val & 0x7F

          if payload_len == 126
            len_bytes = @socket.read(2)
            if len_bytes.nil? || len_bytes.length < 2
              raise ConnectionClosed.new("Incomplete frame")
            end
            payload_len = len_bytes.unpack("n")[0]
          elsif payload_len == 127
            len_bytes = @socket.read(8)
            if len_bytes.nil? || len_bytes.length < 8
              raise ConnectionClosed.new("Incomplete frame")
            end
            high, low = len_bytes.unpack("NN")
            payload_len = ((high || 0) << 32) | (low || 0)
          end

          mask_key = nil
          if masked
            mask_key = @socket.read(4)
          end

          payload = ""
          if payload_len > 0
            payload = @socket.read(payload_len)
            if payload.nil? || payload.length < payload_len
              raise ConnectionClosed.new("Incomplete frame")
            end
          end

          if masked && mask_key
            payload = mask_data(payload, mask_key)
          end

          if !fin
            @fragments << {opcode: opcode, payload: payload}
            return receive_frame
          elsif opcode == OPCODE_CONTINUATION
            first_fragment = @fragments.shift
            unless first_fragment
              raise ProtocolError.new("Unexpected continuation frame")
            end
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
            byte = (data[i]&.ord || 0)
            masked_byte = byte ^ (mask_key[i % 4]&.ord || 0)
            result += masked_byte.chr
          end
          result
        end

        def handle_close_frame(payload)
          if payload.length >= 2
            code = payload[0, 2].unpack("n")[0] # steep:ignore
            reason = payload.length > 2 ? payload[2..-1] : ""
          end
          nil
        end
      end
    end
  end
end
