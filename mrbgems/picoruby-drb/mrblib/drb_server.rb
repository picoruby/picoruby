require 'socket'

module DRb
  class DRbServer
    def initialize(uri, front, config = {})
      @uri = uri
      @front = front
      @config = config
      @running = false

      # Parse URI to get host and port
      if uri.start_with?("druby://")
        if domain = uri[7..-1]
          port_index = domain.index(':')
          if port_index
            @host = domain[0..port_index - 1]
            @port = domain[(port_index + 1)..-1]&.to_i
          end
        end
      end
      if @host.nil? || @port.nil?
        raise DRbBadURI, "invalid URI: #{uri}"
      end
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
        # Send error reply
        msg.send_reply(false, e)
      ensure
        client.close
      end
    end
  end
end
