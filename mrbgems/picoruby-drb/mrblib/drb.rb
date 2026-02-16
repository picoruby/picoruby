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

  class << self
    attr_accessor :primary_server

    # Protocol extension point: create a socket for the given URI
    # Override this method to support custom protocols
    def create_socket(uri)
      raise DRbBadURI, "unsupported protocol: #{uri}"
    end

    # Protocol extension point: create a server for the given URI
    # Override this method to support custom protocols
    def create_server(uri, front, config)
      raise DRbBadURI, "unsupported protocol: #{uri}"
    end

    # Start a DRb server
    # If uri is nil, start in client-only mode (no server)
    def start_service(uri = nil, front = nil, config = {})
      if uri
        @primary_server = create_server(uri, front, config)
        @primary_server&.start
        @uri = uri
      end
      # Client-only mode: no server started
    end

    # Stop the primary DRb server
    def stop_service
      @primary_server&.stop
      @primary_server = nil
      @uri = nil

      # Terminate the server task if running
      server_thread = Task.get("DRb server")
      server_thread&.terminate
    end

    # Get the URI of the primary server
    def uri
      @uri
    end

    # Get the front object
    def front
      @primary_server&.front
    end

    # Create a reference to a remote object
    def ref_object(uri)
      DRbObject.new(uri)
    end

    # Send a message to a remote object
    def send_message(uri, ref, msg_id, args, block = nil)
      # Connect to server using protocol handler
      socket = create_socket(uri)
      msg = DRbMessage.new(socket)

      begin
        # Send request
        msg.send_request(ref, msg_id, args, block)

        # Receive reply
        success, result = msg.recv_reply

        if success
          result
        else
          raise result.to_s
        end
      ensure
        socket.close
      end
    end

    # Run the main loop (for servers)
    # Returns a Task object (compatible with CRuby's Thread#join)
    def thread
      server = @primary_server
      Task.new(name: "DRb server") do
        server&.run
      end
    end
  end
end
