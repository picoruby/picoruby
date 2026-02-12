require 'socket'

module Net
  # --------------------------------------------------------------------------
  # HTTP - Main HTTP client class
  # --------------------------------------------------------------------------
  class HTTP
    attr_accessor :address, :port, :open_timeout, :read_timeout
    attr_accessor :use_ssl, :verify_mode, :ca_file, :ca_path
    attr_reader :started

    # Create new HTTP client
    def initialize(address, port = nil)
      @address = address
      @port = port || 80
      @socket = nil
      @started = false
      @use_ssl = false
      @verify_mode = nil
      @ca_file = nil
      @ca_path = nil
      @open_timeout = 60
      @read_timeout = 60
    end

    # Start HTTP session
    def start
      raise IOError, "HTTP session already started" if @started

      # Create socket connection
      begin
        # Wrap with SSLSocket if SSL is enabled
        if @use_ssl
          # Create SSL context
          ssl_ctx = SSLContext.new

          # Set CA file if provided
          if @ca_file
            ssl_ctx.ca_file = @ca_file # steep:ignore
          end

          # Set verify mode (default to VERIFY_PEER if not specified)
          ssl_ctx.verify_mode = @verify_mode || SSLContext::VERIFY_PEER

          # Connect directly with hostname and port
          # (avoids unnecessary plain TCP connection on platforms like RP2040
          # where SSLSocket creates its own TLS+TCP connection internally)
          @socket = SSLSocket.open(@address, @port, ssl_ctx)
        else
          @socket = TCPSocket.new(@address, @port)
        end
      rescue => e
        raise IOError, "Failed to connect to #{@address}:#{@port} - #{e.message}"
      end

      @started = true

      # If block given, yield self and ensure finish
      if block_given?
        begin
          yield self
        ensure
          finish
        end
      end

      self
    end

    # Finish HTTP session
    def finish
      if @socket && !@socket.closed?
        @socket.close
      end
      @socket = nil
      @started = false
    end

    # Check if session is active
    def active?
      @started && @socket && !@socket.closed?
    end

    # Send GET request
    if RUBY_ENGINE == 'mruby'
      def get(path, initheader = nil, dest = nil, &block)
        request(Get.new(path, initheader), &block)
      end
    else # mruby/c
      def get(path, initheader = nil, dest = nil, &block)
        if self.class? # picoruby-metaprog
          # @type var initheader: String
          res = Net::HTTP._get(path, initheader, dest)
          # @type var res: Net::HTTPResponse
          return res # Truth: It's a String
        end
        request(Get.new(path, initheader), &block)
      end
    end

    # Send HEAD request
    def head(path, initheader = nil)
      request(Head.new(path, initheader))
    end

    # Send POST request
    def post(path, data, initheader = nil, dest = nil, &block)
      req = Post.new(path, initheader)
      req.body = data
      request(req, &block)
    end

    # Send PUT request
    def put(path, data, initheader = nil, dest = nil, &block)
      req = Put.new(path, initheader)
      req.body = data
      request(req, &block)
    end

    # Send DELETE request
    def delete(path, initheader = nil, dest = nil, &block)
      request(Delete.new(path, initheader), &block)
    end

    # Send generic HTTP request
    def request(req, body = nil, &block)
      start unless @started
      raise ArgumentError, "Request must be an HTTPRequest" unless req.is_a?(HTTPGenericRequest)

      # Set body if provided
      req.body = body if body

      # Set default headers
      req.set_default_headers(@address, @port)

      # Send request
      request_string = req.to_s
      @socket.write(request_string)

      # Read response
      response_string = read_response

      # Parse response
      response = HTTPResponse.parse(response_string)

      # Call block with response body if given
      if block && response.body
        yield response.body
      end

      response
    end

    # Class method: Simple GET request
    # Note: Renamed from 'get' to avoid mruby/c limitation where class methods
    # and instance methods cannot have the same name
    def self._get(host, path, port = nil)
      if path.nil?
        raise ArgumentError, "Path cannot be nil"
      end
      if host.start_with?('http')
        # Parse URI
        uri = URI.parse(host)
        host = uri.host
        port ||= uri.port
        use_ssl = uri.scheme == 'https'
      else
        port ||= 80
        use_ssl = false
      end

      http = new(host, port)
      http.use_ssl = use_ssl if use_ssl
      body = nil
      http.start do |h|
        response = h.get(path)
        body = response.body
      end
      body
    end

    if RUBY_ENGINE == 'mruby'
      class << self
        alias get _get
      end
    end

    # Class method: Get response object
    # Note: Safe from mruby/c limitation as no instance method with same name exists
    def self.get_response(uri_or_host, path = nil, port = nil)
      if uri_or_host.is_a?(String) && uri_or_host.start_with?('http')
        # Parse URI
        uri = URI.parse(uri_or_host)
        host = uri.host
        path = uri.request_uri
        port = uri.port
        use_ssl = uri.scheme == 'https'
      else
        host = uri_or_host
        path ||= '/'
        port ||= 80
        use_ssl = false
      end

      http = new(host, port)
      http.use_ssl = use_ssl if use_ssl
      http.start do |h|
        h.get(path)
      end
    end

    # Class method: POST form data
    # Note: Safe from mruby/c limitation as no instance method with same name exists
    def self.post_form(url, params)
      uri = URI.parse(url)
      req = Post.new(uri.request_uri)
      req['content-type'] = 'application/x-www-form-urlencoded'
      req.body = URI.encode_www_form(params)

      http = new(uri.host, uri.port)
      http.use_ssl = (uri.scheme == 'https')
      http.start do |h|
        h.request(req)
      end
    end

    private

    # Read full HTTP response from socket
    def read_response
      response = ""
      headers_done = false
      content_length = nil
      chunked = false

      # Read status line and headers
      loop do
        line = @socket.read(8192)
        break unless line

        response += line

        # Check if we have complete headers
        if response.include?("\r\n\r\n")
          headers_done = true
          break
        end
      end

      unless headers_done
        raise HTTPBadResponse, "Incomplete HTTP headers"
      end

      # Parse headers to determine body reading strategy
      header_end = response.index("\r\n\r\n") || 0
      headers_part = response[0..header_end + 3] || ''

      # Check for Content-Length (case-insensitive search)
      headers_lower = headers_part.downcase
      cl_idx = headers_lower.index('content-length:')
      if cl_idx
        # Extract the value after "content-length:"
        start_idx = cl_idx + 15  # length of "content-length:"
        line_end = headers_part.index("\r\n", start_idx)
        if line_end
          value_str = headers_part[start_idx..(line_end - 1)]&.strip || ''
          # Parse integer from string
          content_length = 0
          value_str.each_char do |c|
            if c >= '0' && c <= '9'
              content_length = content_length * 10 + (c.ord - '0'.ord)
            else
              break if content_length > 0  # Stop at first non-digit after digits
            end
          end
        end
      end

      # Check for Transfer-Encoding: chunked (case-insensitive)
      if headers_lower.index('transfer-encoding:') &&
         headers_lower.index('chunked')
        chunked = true
      end

      # Read body based on headers
      if content_length
        # Read exact content length
        body_start = response.length
        remaining = content_length - (response.length - header_end - 4)
        if remaining > 0
          body_part = @socket.read(remaining)
          response += body_part if body_part
        end
      elsif chunked
        # Read chunked encoding (simplified)
        loop do
          chunk = @socket.read(8192)
          break unless chunk
          response += chunk
          # Simple check for end of chunks
          break if response.end_with?("\r\n0\r\n\r\n")
        end
      else
        # Read until connection closes
        loop do
          chunk = @socket.read(8192)
          break unless chunk
          response += chunk
        end
      end

      response
    end

    # Check if using SSL
    def use_ssl?
      @use_ssl
    end
  end
end
