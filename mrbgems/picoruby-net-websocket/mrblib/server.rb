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
        while true
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
          @close_sent = false
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

          while true
            if deadline && Time.now.to_f > deadline
              return nil
            end

            begin
              byte0 = @socket.read_nonblock(1)
            rescue EOFError
              raise ConnectionClosed.new("Connection closed by client")
            end
            if byte0
              opcode, payload = receive_frame(byte0)

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
              sleep_ms 100 unless deadline && Time.now.to_f >= deadline
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
          @close_sent || @socket.nil? || @socket.closed?
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
          response << "Upgrade: websocket\r\n"
          response << "Connection: Upgrade\r\n"
          response << "Sec-WebSocket-Accept: #{accept_key}\r\n"
          response << "\r\n"

          @socket.write(response)
        end

        def read_http_request
          request = ""
          while true
            line = read_line
            request << line
            break if line == "\r\n"
          end
          request
        end

        def read_line
          line = ""
          begin
            while true
              char = @socket.readpartial(1)
              line << char
              break if line.end_with?("\r\n")
            end
          rescue EOFError
            raise ConnectionClosed.new("Connection closed during handshake")
          end
          line
        end

        def parse_http_headers(request)
          headers = {} #: Hash[String, String]
          lines = request.split("\r\n")

          li = 0
          range = 1..-1
          while li < lines.size
            line = lines[li]
            colon_pos = line.index(':')
            if colon_pos
              key = line.byteslice(0, colon_pos)&.downcase
              if key.nil? || key.empty?
                raise HandshakeError.new("Invalid header line: #{line}")
              end
              value = line.byteslice(colon_pos + 1..-1)
              if value.nil?
                raise HandshakeError.new("Invalid header line: #{line}")
              end
              # Strip leading whitespace
              while value.length > 0 && (byte = value.getbyte(0)) && (byte == 32 || byte == 9) # " " or "\t"
                value = value.byteslice(range) || ""
              end
              headers[key] = value
            end
            li += 1
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
          frame << [fin | opcode].pack("C")

          mask_bit = masked ? 0x80 : 0x00
          length = payload.bytesize

          if length < 126
            frame << [mask_bit | length].pack("C")
          elsif length < 65536
            frame << [mask_bit | 126].pack("C")
            frame << [length].pack("n")
          else
            frame << [mask_bit | 127].pack("C")
            high = length >> 32
            low = length & 0xffffffff
            frame << [high, low].pack("NN")
          end

          if masked
            mask_key = ""
            mi = 0
            while mi < 4
              mask_key << [RNG.random_int % 256].pack("C")
              mi += 1
            end
            frame << mask_key
            masked_payload = mask_data(payload, mask_key)
            frame << masked_payload
          else
            frame << payload
          end

          @socket.write(frame)
        end

        def receive_frame(first_byte = nil)
          while true
            if first_byte
              byte0 = first_byte
              first_byte = nil
            else
              begin
                byte0 = @socket.readpartial(1)
              rescue EOFError
                raise ConnectionClosed.new("Connection closed")
              end
            end

            byte0_val = byte0.getbyte(0) || 0
            fin = (byte0_val & 0x80) != 0
            opcode = byte0_val & 0x0F

            begin
              byte1 = @socket.readpartial(1)
            rescue EOFError
              raise ConnectionClosed.new("Connection closed")
            end

            byte1_val = byte1.getbyte(0) || 0
            masked = (byte1_val & 0x80) != 0
            payload_len = byte1_val & 0x7F

            if payload_len == 126
              len_bytes = @socket.read(2)
              if len_bytes.nil? || len_bytes.bytesize < 2
                raise ConnectionClosed.new("Incomplete frame")
              end
              payload_len = len_bytes.unpack("n")[0]
            elsif payload_len == 127
              len_bytes = @socket.read(8)
              if len_bytes.nil? || len_bytes.bytesize < 8
                raise ConnectionClosed.new("Incomplete frame")
              end
              high, low = len_bytes.unpack("NN")
              payload_len = ((high || 0) << 32) | (low || 0) # steep:ignore NoMethod
            end

            mask_key = nil
            if masked
              mask_key = @socket.read(4)
            end

            payload = ""
            if payload_len > 0
              payload = @socket.read(payload_len)
              if payload.nil? || payload.bytesize < payload_len
                raise ConnectionClosed.new("Incomplete frame")
              end
            end

            if masked && mask_key
              payload = mask_data(payload, mask_key)
            end

            if !fin
              @fragments << {opcode: opcode, payload: payload}
              # Continue loop to read next fragment
            elsif opcode == OPCODE_CONTINUATION
              first_fragment = @fragments.shift
              unless first_fragment
                raise ProtocolError.new("Unexpected continuation frame")
              end
              opcode = first_fragment[:opcode]
              full_payload = first_fragment[:payload]

              fi = 0
              len = @fragments.length
              while fi < len
                full_payload << @fragments[fi][:payload]
                fi += 1
              end
              full_payload << payload

              @fragments.clear
              return [opcode, full_payload]
            else
              return [opcode, payload]
            end
          end
          # Should never reach here. Just for steep check
          raise ConnectionClosed.new("Connection closed. Should never reach here")
        end

        def mask_data(data, mask_key)
          result = ""
          i = 0
          len = data.bytesize
          while i < len
            byte = (data.getbyte(i) || 0)
            masked_byte = byte ^ (mask_key.getbyte(i % 4) || 0)
            result << [masked_byte].pack("C")
            i += 1
          end
          result
        end

        def handle_close_frame(payload)
          if payload.bytesize >= 2
            code = payload.byteslice(0, 2).unpack("n")[0] # steep:ignore
            reason = payload.bytesize > 2 ? payload.byteslice(2..-1) : ""
          else
            code = CLOSE_NORMAL
            reason = ""
          end
          begin
            send_frame(OPCODE_CLOSE, [code].pack("n") + reason) # steep:ignore
          ensure
            @close_sent = true
            @socket&.close
          end
          nil
        end
      end
    end
  end
end
