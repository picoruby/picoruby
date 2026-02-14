# TCP protocol support for DRb
# This file is only loaded when socket is available

begin
  require 'socket'

  module DRb
    # TCP Server implementation
    class DRbTCPServer
      def initialize(uri, front, config = {})
        @uri = uri
        @front = front
        @config = config
        @running = false

        # Parse URI to get host and port
        host, port = DRb.parse_druby_uri(uri)
        @host = host
        @port = port
      end

      attr_reader :uri, :front

      def start
        @running = true
        @server = TCPServer.new(@host, @port)
        puts "DRb server started on #{@uri}"
        self
      end

      def stop
        @running = false
        @server.close if @server
      end

      def alive?
        @running
      end

      def run
        while @running
          client = @server.accept
          handle_client(client)
        end
      rescue => e
        puts "Server error: #{e.message}"
      ensure
        stop
      end

      def accept
        return nil unless @running
        client = @server.accept
        handle_client(client)
      end

      private

      def handle_client(client)
        msg = DRbMessage.new(client)

        begin
          # Receive request
          ref, msg_id, args, block = msg.recv_request

          # Determine the target object
          obj = (ref.nil? || ref == @front) ? @front : ref

          # Invoke the method
          if msg_id.is_a?(Symbol)
            result = obj.send(msg_id, *(args || []), &block)
          else
            raise DRbError, "invalid message ID"
          end

          # Send success reply
          msg.send_reply(true, result)

        rescue => e
          # Send error reply (convert exception to string for Marshal compatibility)
          error_msg = "#{e.class}: #{e.message}"
          msg.send_reply(false, error_msg)
        ensure
          client.close
        end
      end
    end

    # Alias for backward compatibility
    DRbServer = DRbTCPServer

    class << self
      # Parse druby:// URI
      def parse_druby_uri(uri)
        unless uri.start_with?("druby://")
          raise DRbBadURI, "not a druby:// URI: #{uri}"
        end

        domain = uri[8..-1]
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
          port = port_str.to_i
          [host, port]
        else
          raise DRbBadURI, "invalid URI: #{uri}"
        end
      end

      # Override create_socket to support druby://
      alias_method :_base_create_socket, :create_socket

      def create_socket(uri)
        if uri.start_with?("druby://")
          host, port = parse_druby_uri(uri)
          TCPSocket.new(host, port)
        else
          _base_create_socket(uri)
        end
      end

      # Override create_server to support druby://
      alias_method :_base_create_server, :create_server

      def create_server(uri, front, config)
        if uri.start_with?("druby://")
          DRbTCPServer.new(uri, front, config)
        else
          _base_create_server(uri, front, config)
        end
      end
    end
  end

rescue LoadError
  # Socket not available (e.g., WASM environment)
  # TCP protocol will not be registered
end
