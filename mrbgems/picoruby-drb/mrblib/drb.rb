require 'socket'
require 'marshal'

module DRb
  # Errors
  class DRbError < StandardError; end
  class DRbConnError < DRbError; end
  class DRbBadURI < DRbError; end
  class DRbUnknownError < DRbError
    def initialize(err, buf)
      @err = err
      @buf = buf
      super("unknown object: #{err}")
    end

    attr_reader :err, :buf
  end

  @primary_server = nil
  @uri = nil

  class << self
    attr_accessor :primary_server

    # Start a DRb server
    def start_service(uri, front, config = {})
      @primary_server = DRbServer.new(uri, front, config)
      @primary_server.start
      @uri = uri
    end

    # Stop the primary DRb server
    def stop_service
      @primary_server.stop if @primary_server
      @primary_server = nil
      @uri = nil
    end

    # Get the URI of the primary server
    def uri
      @uri
    end

    # Get the front object
    def front
      @primary_server ? @primary_server.front : nil
    end

    # Create a reference to a remote object
    def ref_object(uri)
      DRbObject.new(uri)
    end

    # Send a message to a remote object
    def send_message(uri, ref, msg_id, args, block = nil)
      # Parse URI
      if uri =~ /druby:\/\/([^:]+):(\d+)/
        host = $1
        port = $2.to_i
      else
        raise DRbBadURI, "invalid URI: #{uri}"
      end

      # Connect to server
      socket = TCPSocket.new(host, port)
      msg = DRbMessage.new(socket)

      begin
        # Send request
        msg.send_request(ref, msg_id, args, block)

        # Receive reply
        success, result = msg.recv_reply

        if success
          result
        else
          raise result
        end
      ensure
        socket.close
      end
    end

    # Run the main loop (for servers)
    def thread
      return nil unless @primary_server
      @primary_server.run
    end
  end
end
