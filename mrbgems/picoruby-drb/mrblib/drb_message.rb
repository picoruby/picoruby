require 'marshal'

module DRb
  class DRbMessage
    def initialize(socket)
      @socket = socket
    end

    # Send a request: [ref, msg_id, args, block]
    def send_request(ref, msg_id, args, block = nil)
      data = Marshal.dump([ref, msg_id, args, block])
      send_message(data)
    end

    # Receive a request: returns [ref, msg_id, args, block]
    def recv_request
      data = recv_message
      Marshal.load(data)
    end

    # Send a reply: [success, result]
    def send_reply(success, result)
      data = Marshal.dump([success, result])
      send_message(data)
    end

    # Receive a reply: returns [success, result]
    def recv_reply
      data = recv_message
      Marshal.load(data)
    end

    private

    def send_message(data)
      # Send 4-byte header with message size (big-endian)
      size = data.size
      header = [size].pack('N')
      @socket.write(header)
      @socket.write(data)
    end

    def recv_message
      # Read 4-byte header
      header = @socket.read(4)
      raise DRbConnError, "connection closed" if header.nil? || header.size < 4

      # Parse size (big-endian)
      size = header.unpack('N')[0]
      raise DRbConnError, "invalid message size: #{size}" if size <= 0 || size > 10_000_000

      # Read message data
      data = @socket.read(size)
      raise DRbConnError, "connection closed" if data.nil? || data.size < size

      data
    end
  end
end
