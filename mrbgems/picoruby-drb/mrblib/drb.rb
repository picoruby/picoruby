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

  # Initialize global variables for mruby/c compatibility
  $drb_primary_server = nil
  $drb_uri = nil

  class << self
    def primary_server
      $drb_primary_server
    end

    def primary_server=(server)
      $drb_primary_server = server
    end

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
    def start_service(uri, front, config = {})
      $drb_primary_server = create_server(uri, front, config)
      $drb_primary_server&.start
      $drb_uri = uri
    end

    # Stop the primary DRb server
    def stop_service
      $drb_primary_server&.stop
      $drb_primary_server = nil
      $drb_uri = nil
    end

    # Get the URI of the primary server
    def uri
      $drb_uri
    end

    # Get the front object
    def front
      $drb_primary_server&.front
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
    def thread
      $drb_primary_server&.run
    end
  end
end
