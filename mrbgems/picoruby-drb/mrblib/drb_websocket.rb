module DRb
  IS_WASM = begin ::JS::WebSocket
              true
            rescue NameError
              false
            end

  module WebSocket

    if !IS_WASM
      # Microcontroller implementation
      # WebSocket adapter wrapping Net::WebSocket
      # Simple protocol: WebSocket binary frames contain DRb messages directly
      class Adapter
        def initialize(ws)
          @ws = ws
          @buffer = ""
        end

        def write(data)
          @ws.send(data, type: :binary)
        end

        def read(n)
          while @buffer.length < n
            msg = @ws.receive(timeout: 10)
            raise DRbConnError, "connection timeout" unless msg
            @buffer += msg
          end

          result = @buffer[0, n]
          @buffer = @buffer[n..-1]
          result
        end

        def close
          # No-op: keep connection alive for reuse
          # DRb calls close after each RPC, but WebSocket
          # reconnection is expensive
        end

        def closed?
          @ws.respond_to?(:closed?) ? @ws.closed? : false
        end
      end

      # WebSocket server for DRb
      class Server
        def initialize(uri, front, config = {})
          @uri = uri
          @front = front
          @config = config
          @running = false

          # Parse ws://host:port
          host, port = DRb.parse_ws_uri(uri)
          @host = host
          @port = port
        end

        attr_reader :uri, :front

        def start
          @running = true
          @server = Net::WebSocket::Server.new(host: @host, port: @port)
          @server.start
          puts "DRb WebSocket server started on #{@uri}"
          self
        end

        def stop
          @running = false
          @server&.close
        end

        def alive?
          @running
        end

        def run
          while @running
            ws_conn = @server.accept
            socket = Adapter.new(ws_conn)
            handle_client(socket)
          end
        rescue => e
          puts "Server error: #{e.message}"
          puts e.backtrace if e.respond_to?(:backtrace)
        ensure
          stop
        end

        def accept
          return nil unless @running
          ws_conn = @server.accept
          socket = Adapter.new(ws_conn)
          handle_client(socket)
        end

        private

        def handle_client(socket)
          loop do
            msg = DRbMessage.new(socket)

            begin
              ref, msg_id, args, block = msg.recv_request
              obj = (ref.nil? || ref == @front) ? @front : ref

              if msg_id.is_a?(Symbol)
                result = obj.send(msg_id, *(args || []), &block)
              else
                raise DRbError, "invalid message ID"
              end

              msg.send_reply(true, result)
            rescue DRbConnError
              break
            rescue => e
              error_msg = "#{e.class}: #{e.message}"
              begin
                msg.send_reply(false, error_msg)
              rescue
                break
              end
            end
          end
        ensure
          begin
            socket.close
          rescue
            # Ignore errors during close (connection may already be closed)
          end
        end
      end

      def self.connect(uri)
        client = Net::WebSocket::Client.connect(uri)
        Adapter.new(client)
      end

    elsif IS_WASM
      # WASM/Browser implementation
      # Browser WebSocket adapter
      class BrowserSocket
        def initialize(url)
          @ws = ::JS::WebSocket.new(url)
          @ws.binaryType = 'arraybuffer'
          @queue = []
          @buffer = ""
          @ready = false
          @error = nil

          @ws.onopen { @ready = true }
          @ws.onmessage { |data|
            @queue << data
          }
          @ws.onerror { |event| @error = event }
          @ws.onclose { @ready = false }

          timeout = 100
          until @ready || @error
            sleep_ms 100
            timeout -= 1
            if timeout <= 0
              @ws.close
              raise DRbConnError, "connection timeout"
            end
          end

          if @error
            @ws.close
            raise DRbConnError, "connection error: #{@error}"
          end
        end

        def write(data)
          unless @ready
            raise DRbConnError, "connection not ready"
          end
          @ws.send_binary(data)
        end

        def read(n)
          while @buffer.length < n
            timeout = 1000
            while @queue.empty?
              unless @ready
                raise DRbConnError, "connection closed"
              end
              sleep_ms 10
              timeout -= 1
              if timeout <= 0
                raise DRbConnError, "read timeout"
              end
            end

            @buffer += @queue.shift
          end

          result = @buffer[0, n]
          @buffer = @buffer[n..-1]
          result
        end

        def close
          # No-op: keep connection alive for reuse.
          # DRb calls close after each RPC, but WebSocket
          # reconnection is too slow for per-call usage.
        end

        def closed?
          !@ready
        end

        def real_close
          @ws.close unless @ws.closed?
        end
      end

      @connections = {}

      def self.connect(uri)
        conn = @connections[uri]
        if conn && !conn.closed?
          conn
        else
          conn = BrowserSocket.new(uri)
          @connections[uri] = conn
          conn
        end
      end
    end
  end
end

# Register WebSocket protocol with DRb
module DRb
  class << self
    # Parse ws:// or wss:// URI
    def parse_ws_uri(uri)
      unless uri.start_with?("ws://") || uri.start_with?("wss://")
        raise DRbBadURI, "not a ws:// URI: #{uri}"
      end

      if IS_WASM
        # Browser WebSocket API handles the full URI
        uri
      else
        # Microcontroller: parse host:port
        domain = if uri.start_with?("wss://")
          uri[6..-1]
        else
          uri[5..-1]
        end

        if domain.nil? || domain.empty?
          raise DRbBadURI, "invalid URI: #{uri}"
        end

        port_index = domain.index(':')
        if port_index
          host = domain[0..port_index - 1]
          port_str = domain[(port_index + 1)..-1]
          unless host && port_str
            raise DRbBadURI, "invalid URI: #{uri}"
          end
          if slash_index = port_str.index('/')
            port_str = port_str[0..slash_index - 1]
          end
          port = port_str.to_i
          [host, port]
        else
          raise DRbBadURI, "invalid URI: #{uri}"
        end
      end
    end

    # Override create_socket to support ws://
    alias_method :_ws_base_create_socket, :create_socket

    def create_socket(uri)
      uri = uri.to_s if uri.respond_to?(:to_s)
      if uri.start_with?("ws://") || uri.start_with?("wss://")
        # Cache connections for reuse
        @ws_connections ||= {}
        socket = @ws_connections[uri]
        if socket.nil? || socket.closed?
          socket = WebSocket.connect(uri)
          @ws_connections[uri] = socket
        end
        socket
      else
        _ws_base_create_socket(uri)
      end
    end

    # Override create_server to support ws://
    alias_method :_ws_base_create_server, :create_server

    def create_server(uri, front, config)
      uri = uri.to_s if uri.respond_to?(:to_s)
      if uri.start_with?("ws://")
        if IS_WASM
          raise DRbBadURI, "WebSocket server not supported in browser environment"
        else
          WebSocket::Server.new(uri, front, config)
        end
      elsif uri.start_with?("wss://")
        raise DRbBadURI, "wss:// server not yet supported"
      else
        _ws_base_create_server(uri, front, config)
      end
    end
  end
end
