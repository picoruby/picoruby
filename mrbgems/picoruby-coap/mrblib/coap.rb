#
# CoAP library for PicoRuby
# This is a pure Ruby implementation of CoAP (Constrained Application Protocol).
# It provides both client and server functionality.
#
# Author: Hitoshi HASUMI
# License: MIT
#

require 'socket'

module CoAP
  class CoAPError < StandardError; end
  class ParseError < CoAPError; end
  class EncodeError < CoAPError; end

  DEFAULT_PORT = 5683

  # Message Types
  TYPE_CON = 0  # Confirmable
  TYPE_NON = 1  # Non-confirmable
  TYPE_ACK = 2  # Acknowledgement
  TYPE_RST = 3  # Reset

  # Method Codes (0.XX)
  CODE_EMPTY = 0
  CODE_GET = 1
  CODE_POST = 2
  CODE_PUT = 3
  CODE_DELETE = 4

  # Response Codes
  CODE_CREATED = 65      # 2.01
  CODE_DELETED = 66      # 2.02
  CODE_VALID = 67        # 2.03
  CODE_CHANGED = 68      # 2.04
  CODE_CONTENT = 69      # 2.05
  CODE_BAD_REQUEST = 128 # 4.00
  CODE_NOT_FOUND = 132   # 4.04
  CODE_INTERNAL_ERROR = 160 # 5.00

  # Option Numbers
  OPTION_IF_MATCH = 1
  OPTION_URI_HOST = 3
  OPTION_ETAG = 4
  OPTION_IF_NONE_MATCH = 5
  OPTION_URI_PORT = 7
  OPTION_LOCATION_PATH = 8
  OPTION_URI_PATH = 11
  OPTION_CONTENT_FORMAT = 12
  OPTION_MAX_AGE = 14
  OPTION_URI_QUERY = 15
  OPTION_ACCEPT = 17
  OPTION_LOCATION_QUERY = 20
  OPTION_PROXY_URI = 35
  OPTION_PROXY_SCHEME = 39
  OPTION_SIZE1 = 60

  # Content Formats
  FORMAT_TEXT_PLAIN = 0
  FORMAT_APP_LINK_FORMAT = 40
  FORMAT_APP_XML = 41
  FORMAT_APP_OCTET_STREAM = 42
  FORMAT_APP_EXI = 47
  FORMAT_APP_JSON = 50

  # CoAP Message class
  class Message
    attr_accessor :version, :mtype, :token_length, :code, :message_id
    attr_accessor :token, :options, :payload

    def initialize
      @version = 1
      @mtype = TYPE_CON
      @token_length = 0
      @code = CODE_EMPTY
      @message_id = 0
      @token = ""
      @options = []
      @payload = ""
    end

    def encode
      # CoAP header: 4 bytes
      # Ver(2) | T(2) | TKL(4) | Code(8) | Message ID(16)

      @token_length = @token.length

      byte0 = (@version << 6) | (@mtype << 4) | @token_length

      header = [byte0, @code, @message_id].pack("CCn")

      result = header + @token

      # Encode options
      unless @options.empty?
        sorted_options = @options.sort_by { |opt| opt[:number] }
        prev_number = 0

        sorted_options.each do |opt|
          delta = opt[:number] - prev_number
          value = opt[:value] || ""

          # Convert value to string if it's an integer
          if value.is_a?(Integer)
            # Encode as minimum bytes
            if value == 0
              value = ""
            elsif value <= 0xFF
              value = [value].pack("C")
            elsif value <= 0xFFFF
              value = [value].pack("n")
            else
              value = [value].pack("N")
            end
          end

          length = value.length

          # Option header
          delta_ext = 0
          length_ext = 0

          if delta >= 269
            delta_nibble = 14
            delta_ext = delta - 269
            delta_ext_bytes = [delta_ext].pack("n")
          elsif delta >= 13
            delta_nibble = 13
            delta_ext = delta - 13
            delta_ext_bytes = [delta_ext].pack("C")
          else
            delta_nibble = delta
            delta_ext_bytes = ""
          end

          if length >= 269
            length_nibble = 14
            length_ext = length - 269
            length_ext_bytes = [length_ext].pack("n")
          elsif length >= 13
            length_nibble = 13
            length_ext = length - 13
            length_ext_bytes = [length_ext].pack("C")
          else
            length_nibble = length
            length_ext_bytes = ""
          end

          option_header = [(delta_nibble << 4) | length_nibble].pack("C")
          result += option_header + delta_ext_bytes + length_ext_bytes + value

          prev_number = opt[:number]
        end
      end

      # Payload marker and payload
      unless @payload.empty?
        result += [0xFF].pack("C") + @payload
      end

      result
    end

    def self.decode(data)
      msg = Message.new

      if data.length < 4
        raise ParseError.new("Message too short")
      end

      # Decode header
      byte0 = data[0].ord
      msg.version = (byte0 >> 6) & 0x03
      msg.mtype = (byte0 >> 4) & 0x03
      msg.token_length = byte0 & 0x0F

      msg.code = data[1].ord
      msg.message_id = data[2, 2].unpack("n")[0]

      offset = 4

      # Decode token
      if msg.token_length > 0
        if offset + msg.token_length > data.length
          raise ParseError.new("Token extends beyond message")
        end
        msg.token = data[offset, msg.token_length]
        offset += msg.token_length
      end

      # Decode options
      option_number = 0
      while offset < data.length
        byte = data[offset].ord

        # Check for payload marker
        if byte == 0xFF
          offset += 1
          break
        end

        delta_nibble = (byte >> 4) & 0x0F
        length_nibble = byte & 0x0F
        offset += 1

        # Decode option delta
        if delta_nibble == 13
          delta_ext = data[offset].ord
          delta = delta_ext + 13
          offset += 1
        elsif delta_nibble == 14
          delta_ext = data[offset, 2].unpack("n")[0]
          delta = delta_ext + 269
          offset += 2
        elsif delta_nibble == 15
          raise ParseError.new("Reserved option delta value")
        else
          delta = delta_nibble
        end

        # Decode option length
        if length_nibble == 13
          length_ext = data[offset].ord
          length = length_ext + 13
          offset += 1
        elsif length_nibble == 14
          length_ext = data[offset, 2].unpack("n")[0]
          length = length_ext + 269
          offset += 2
        elsif length_nibble == 15
          raise ParseError.new("Reserved option length value")
        else
          length = length_nibble
        end

        option_number += delta

        # Read option value
        value = ""
        if length > 0
          if offset + length > data.length
            raise ParseError.new("Option value extends beyond message")
          end
          value = data[offset, length]
          offset += length
        end

        msg.options << {number: option_number, value: value}
      end

      # Decode payload
      if offset < data.length
        msg.payload = data[offset..-1]
      end

      msg
    end

    def get_option(option_number)
      option = @options.find { |opt| opt[:number] == option_number }
      option ? option[:value] : nil
    end

    def add_option(option_number, value)
      @options << {number: option_number, value: value}
    end

    def method_name
      case @code
      when CODE_GET then "GET"
      when CODE_POST then "POST"
      when CODE_PUT then "PUT"
      when CODE_DELETE then "DELETE"
      else "UNKNOWN"
      end
    end

    def response_code_string
      class_code = (@code >> 5) & 0x07
      detail = @code & 0x1F
      "#{class_code}.%02d" % detail
    end
  end

  # CoAP Request class
  class Request < Message
    def initialize(method_code)
      super()
      @code = method_code
      @mtype = TYPE_CON
    end

    def uri_path=(path)
      # Split path and add as URI_PATH options
      path.split('/').each do |segment|
        next if segment.empty?
        add_option(OPTION_URI_PATH, segment)
      end
    end

    def uri_path
      paths = @options.select { |opt| opt[:number] == OPTION_URI_PATH }
      "/" + paths.map { |opt| opt[:value] }.join('/')
    end
  end

  # CoAP Response class
  class Response < Message
    def initialize(code: CODE_CONTENT, payload: "")
      super()
      @code = code
      @payload = payload
      @mtype = TYPE_ACK
    end
  end

  # CoAP Client
  class Client
    def initialize
      @socket = UDPSocket.new
      @message_id = rand(0xFFFF)
      @token_counter = 0
    end

    def get(uri, timeout: 5)
      request(CODE_GET, uri, nil, timeout)
    end

    def post(uri, payload, timeout: 5)
      request(CODE_POST, uri, payload, timeout)
    end

    def put(uri, payload, timeout: 5)
      request(CODE_PUT, uri, payload, timeout)
    end

    def delete(uri, timeout: 5)
      request(CODE_DELETE, uri, nil, timeout)
    end

    def close
      @socket.close if @socket
    end

    private

    def request(method_code, uri, payload, timeout)
      # Parse URI
      host, port, path = parse_uri(uri)

      # Create request
      req = Request.new(method_code)
      req.message_id = next_message_id
      req.token = next_token
      req.uri_path = path

      if payload
        req.payload = payload
        req.add_option(OPTION_CONTENT_FORMAT, FORMAT_TEXT_PLAIN)
      end

      # Send request
      packet = req.encode
      @socket.send(packet, 0, host, port)

      # Wait for response
      deadline = Time.now.to_f + timeout

      begin
        while Time.now.to_f < deadline
          remaining = deadline - Time.now.to_f
          break if remaining <= 0

          ready = IO.select([@socket], nil, nil, remaining)
          if ready && ready[0]
            data, addr = @socket.recvfrom(4096)
            response = Message.decode(data)

            # Check if this is the response to our request
            if response.message_id == req.message_id && response.token == req.token
              return response
            end
          end
        end
      rescue => e
        raise CoAPError.new("Request failed: #{e.message}")
      end

      raise CoAPError.new("Request timeout")
    end

    def parse_uri(uri)
      # Simple URI parser for coap://host:port/path
      if uri.start_with?("coap://")
        uri = uri[7..-1]
      end

      # Split host:port and path
      parts = uri.split('/', 2)
      host_port = parts[0]
      path = parts[1] ? "/" + parts[1] : "/"

      # Split host and port
      if host_port.include?(':')
        host, port_str = host_port.split(':', 2)
        port = port_str.to_i
      else
        host = host_port
        port = DEFAULT_PORT
      end

      [host, port, path]
    end

    def next_message_id
      @message_id = (@message_id + 1) & 0xFFFF
    end

    def next_token
      @token_counter = (@token_counter + 1) & 0xFF
      [@token_counter].pack("C")
    end
  end

  # CoAP Server
  class Server
    def initialize(port: DEFAULT_PORT)
      @port = port
      @socket = UDPSocket.new
      @socket.bind("0.0.0.0", @port)
      @resources = {}
    end

    def add_resource(path, &handler)
      @resources[path] = handler
    end

    def run
      loop do
        begin
          data, addr = @socket.recvfrom(4096)
          handle_request(data, addr)
        rescue => e
          # Handle error
        end
      end
    end

    def close
      @socket.close if @socket
    end

    private

    def handle_request(data, addr)
      begin
        request = Message.decode(data)

        # Only handle requests (code class 0)
        return unless (request.code >> 5) == 0

        # Get URI path
        path_options = request.options.select { |opt| opt[:number] == OPTION_URI_PATH }
        path = "/" + path_options.map { |opt| opt[:value] }.join('/')

        # Find handler
        handler = @resources[path]

        response = nil
        if handler
          # Call handler
          response = handler.call(request)
        else
          # Not found
          response = Response.new(code: CODE_NOT_FOUND)
        end

        # Set response message ID and token to match request
        response.message_id = request.message_id
        response.token = request.token

        # Set message type based on request type
        if request.mtype == TYPE_CON
          response.mtype = TYPE_ACK
        else
          response.mtype = TYPE_NON
        end

        # Send response
        packet = response.encode
        @socket.send(packet, 0, addr[3], addr[1])
      rescue => e
        # Ignore malformed packets
      end
    end
  end
end
