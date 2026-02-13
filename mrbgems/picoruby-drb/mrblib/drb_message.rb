require 'marshal'

module DRb
  class DRbMessage
    def initialize(socket)
      @socket = socket
    end

    # Send a request (CRuby-compatible: each field individually)
    def send_request(ref, msg_id, args, block = nil)
      send_message(Marshal.dump(ref))
      send_message(Marshal.dump(msg_id.to_s))
      send_message(Marshal.dump(args.length))
      args.each { |a| send_message(Marshal.dump(a)) }
      send_message(Marshal.dump(block))
    end

    # Receive a request (CRuby-compatible: each field individually)
    def recv_request
      ref    = Marshal.load(recv_message)
      # @type var msg_id_str: String
      msg_id_str = Marshal.load(recv_message)
      msg_id = msg_id_str.to_sym
      # @type var argc: Integer
      argc   = Marshal.load(recv_message)
      args   = Array.new(argc) { Marshal.load(recv_message) } # steep:ignore
      # @type var block: Proc?
      block  = Marshal.load(recv_message)
      [ref, msg_id, args, block]
    end

    # Send a reply (CRuby-compatible: success + result individually)
    def send_reply(success, result)
      send_message(Marshal.dump(success))
      send_message(Marshal.dump(result))
    end

    # Receive a reply (CRuby-compatible: success + result individually)
    def recv_reply
      # @type var success: bool
      success = Marshal.load(recv_message)
      result  = Marshal.load(recv_message)
      [success, result]
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
