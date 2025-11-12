# Net::HTTP - HTTP client library for PicoRuby
# CRuby-compatible interface for making HTTP requests
#
# This file consolidates all Net::HTTP components into a single file
# to avoid PicoRuby build system issues with subdirectories.

require 'socket'

# ============================================================================
# URI Module - Simplified URI parsing for HTTP/HTTPS URLs
# ============================================================================
module URI
  class URI
    attr_reader :scheme, :host, :port, :path, :query, :fragment

    def initialize(scheme, host, port, path, query = nil, fragment = nil)
      @scheme = scheme
      @host = host
      @port = port
      @path = path
      @query = query
      @fragment = fragment
    end

    def to_s
      url = "#{@scheme}://#{@host}"
      url += ":#{@port}" if @port && @port != default_port
      url += @path if @path && !@path.empty?
      url += "?#{@query}" if @query
      url += "##{@fragment}" if @fragment
      url
    end

    def default_port
      case @scheme
      when 'http'
        80
      when 'https'
        443
      else
        nil
      end
    end

    def request_uri
      uri = @path && !@path.empty? ? @path : '/'
      uri += "?#{@query}" if @query
      uri
    end
  end

  # Parse a URI string
  def self.parse(uri_string)
    return nil unless uri_string

    # Extract scheme
    scheme = nil
    rest = uri_string

    if uri_string.start_with?('http://')
      scheme = 'http'
      rest = uri_string[7..-1]  # Remove 'http://'
    elsif uri_string.start_with?('https://')
      scheme = 'https'
      rest = uri_string[8..-1]  # Remove 'https://'
    else
      raise ArgumentError, "URI scheme must be http or https"
    end

    # Extract fragment
    fragment = nil
    fragment_idx = rest.index('#')
    if fragment_idx
      fragment = rest[(fragment_idx + 1)..-1]
      rest = rest[0..(fragment_idx - 1)]
    end

    # Extract query
    query = nil
    query_idx = rest.index('?')
    if query_idx
      query = rest[(query_idx + 1)..-1]
      rest = rest[0..(query_idx - 1)]
    end

    # Extract path
    path = '/'
    slash_idx = rest.index('/')
    if slash_idx
      host_port = rest[0..(slash_idx - 1)]
      path = rest[slash_idx..-1]
    else
      host_port = rest
    end

    # Extract host and port
    colon_idx = host_port.index(':')
    if colon_idx
      host = host_port[0..(colon_idx - 1)]
      port_str = host_port[(colon_idx + 1)..-1]
      # Convert port string to integer manually
      port = 0
      port_str.each_char do |c|
        if c >= '0' && c <= '9'
          port = port * 10 + (c.ord - '0'.ord)
        end
      end
    else
      host = host_port
      port = scheme == 'https' ? 443 : 80
    end

    URI.new(scheme, host, port, path, query, fragment)
  end

  # Encode URI component (simple version)
  def self.encode_www_form_component(str)
    result = ''
    str.to_s.each_char do |char|
      # Check if char is alphanumeric or in safe set: - _ . ~
      if (char >= 'a' && char <= 'z') ||
         (char >= 'A' && char <= 'Z') ||
         (char >= '0' && char <= '9') ||
         char == '-' || char == '_' || char == '.' || char == '~'
        result += char
      else
        # Percent-encode the character
        byte = char.ord
        hex = byte.to_s(16).upcase
        hex = '0' + hex if hex.length == 1
        result += '%' + hex
      end
    end
    result
  end

  # Encode form data
  def self.encode_www_form(params)
    params.map { |k, v|
      "#{encode_www_form_component(k)}=#{encode_www_form_component(v)}"
    }.join('&')
  end
end

# ============================================================================
# Net::HTTP Module
# ============================================================================
module Net
  # Exception for bad HTTP response
  class HTTPBadResponse < StandardError; end

  # --------------------------------------------------------------------------
  # HTTPResponse - HTTP response parsing and handling
  # --------------------------------------------------------------------------
  class HTTPResponse
    attr_reader :code, :message, :http_version
    attr_accessor :header, :body

    def initialize(code = nil, message = nil, http_version = nil)
      @code = code
      @message = message
      @http_version = http_version
      @header = {}
      @body = nil
    end

    # Parse raw HTTP response
    def self.parse(response_string)
      return nil unless response_string && !response_string.empty?

      lines = response_string.split("\r\n")
      return nil if lines.empty?

      # Parse status line
      status_line = lines.shift
      # Expected format: "HTTP/1.1 200 OK"
      if status_line && status_line.start_with?('HTTP/')
        parts = status_line.split(' ', 3)
        if parts.length >= 3
          http_version = parts[0][5..-1]  # Remove "HTTP/" prefix
          code = parts[1]
          message = parts[2]
        else
          raise HTTPBadResponse, "Invalid HTTP response status line"
        end
      else
        raise HTTPBadResponse, "Invalid HTTP response status line"
      end

      response = new(code, message, http_version)

      # Parse headers
      while !lines.empty? && (line = lines.shift) && line != ""
        colon_idx = line.index(':')
        if colon_idx
          key = line[0..(colon_idx - 1)].strip
          value = line[(colon_idx + 1)..-1].strip
          response.header[key.downcase] = value
        end
      end

      # Remaining lines are body
      response.body = lines.join("\r\n") if !lines.empty?

      response
    end

    # Get header value (case-insensitive)
    def [](key)
      @header[key.downcase]
    end

    # Set header value (case-insensitive)
    def []=(key, value)
      @header[key.downcase] = value
    end

    # Get header value (case-insensitive, alias for [])
    def get_fields(key)
      [self[key]].compact
    end

    # Read body (for compatibility)
    def read_body(&block)
      if block
        yield @body if @body
      end
      @body
    end

    # Check if response is successful (2xx)
    def success?
      @code && @code.to_i >= 200 && @code.to_i < 300
    end

    # Check if response is a redirect (3xx)
    def redirect?
      @code && @code.to_i >= 300 && @code.to_i < 400
    end

    # Check if response is a client error (4xx)
    def client_error?
      @code && @code.to_i >= 400 && @code.to_i < 500
    end

    # Check if response is a server error (5xx)
    def server_error?
      @code && @code.to_i >= 500 && @code.to_i < 600
    end

    # Check if response is an error (4xx or 5xx)
    def error?
      client_error? || server_error?
    end

    # Get response code type
    def code_type
      return nil unless @code
      case @code.to_i / 100
      when 1
        HTTPInformation
      when 2
        HTTPSuccess
      when 3
        HTTPRedirection
      when 4
        HTTPClientError
      when 5
        HTTPServerError
      else
        HTTPUnknownResponse
      end
    end

    # Convert to string
    def to_s
      "#{@http_version} #{@code} #{@message}"
    end
  end

  # Response code type classes
  class HTTPInformation < HTTPResponse; end
  class HTTPSuccess < HTTPResponse; end
  class HTTPRedirection < HTTPResponse; end
  class HTTPClientError < HTTPResponse; end
  class HTTPServerError < HTTPResponse; end
  class HTTPUnknownResponse < HTTPResponse; end

  # Specific response classes
  class HTTPOK < HTTPSuccess; end
  class HTTPCreated < HTTPSuccess; end
  class HTTPAccepted < HTTPSuccess; end
  class HTTPNoContent < HTTPSuccess; end
  class HTTPMovedPermanently < HTTPRedirection; end
  class HTTPFound < HTTPRedirection; end
  class HTTPSeeOther < HTTPRedirection; end
  class HTTPNotModified < HTTPRedirection; end
  class HTTPTemporaryRedirect < HTTPRedirection; end
  class HTTPBadRequest < HTTPClientError; end
  class HTTPUnauthorized < HTTPClientError; end
  class HTTPForbidden < HTTPClientError; end
  class HTTPNotFound < HTTPClientError; end
  class HTTPMethodNotAllowed < HTTPClientError; end
  class HTTPInternalServerError < HTTPServerError; end
  class HTTPNotImplemented < HTTPServerError; end
  class HTTPBadGateway < HTTPServerError; end
  class HTTPServiceUnavailable < HTTPServerError; end

  # --------------------------------------------------------------------------
  # HTTPGenericRequest - HTTP request generation
  # --------------------------------------------------------------------------
  class HTTPGenericRequest
    attr_reader :method, :path
    attr_accessor :body

    def initialize(method, path, initheader = nil)
      @method = method.to_s.upcase
      @path = path
      @header = {}
      @body = nil

      # Initialize headers
      if initheader
        initheader.each do |key, value|
          self[key] = value
        end
      end
    end

    # Get header value (case-insensitive)
    def [](key)
      @header[key.downcase]
    end

    # Set header value (case-insensitive)
    def []=(key, value)
      @header[key.downcase] = value
    end

    # Delete header (case-insensitive)
    def delete(key)
      @header.delete(key.downcase)
    end

    # Get all header keys
    def each_header
      @header.each do |key, value|
        yield key, value
      end
    end

    # Set default headers
    def set_default_headers(host, port)
      # Host header
      if port && (port != 80 && port != 443)
        self['host'] = "#{host}:#{port}"
      else
        self['host'] = host
      end

      # User-Agent
      self['user-agent'] ||= 'PicoRuby-Net-HTTP/1.0'

      # Content-Length for requests with body
      if @body
        self['content-length'] = @body.bytesize.to_s
      end

      # Connection
      self['connection'] ||= 'close'
    end

    # Convert request to HTTP wire format
    def to_s
      lines = []

      # Request line
      lines << "#{@method} #{@path} HTTP/1.1"

      # Headers
      @header.each do |key, value|
        # Capitalize header names properly (manually since capitalize is not in mruby/c)
        parts = key.split('-')
        formatted_parts = []
        parts.each do |word|
          if word.length > 0
            # Capitalize first char, lowercase rest
            capitalized = word[0].upcase + (word.length > 1 ? word[1..-1].downcase : '')
            formatted_parts << capitalized
          else
            formatted_parts << word
          end
        end
        formatted_key = formatted_parts.join('-')
        lines << "#{formatted_key}: #{value}"
      end

      # Empty line between headers and body
      lines << ""

      # Body (if present)
      lines << @body if @body

      lines.join("\r\n")
    end

    # Check if request expects a response body
    def response_body_permitted?
      @method != 'HEAD'
    end

    # Check if request has a body
    def request_body_permitted?
      %w[POST PUT PATCH].include?(@method)
    end
  end

  # Specific request type classes
  class HTTPRequest < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('GET', path, initheader)
    end
  end

  class Get < HTTPRequest
    def initialize(path, initheader = nil)
      super(path, initheader)
    end
  end

  class Head < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('HEAD', path, initheader)
    end
  end

  class Post < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('POST', path, initheader)
    end
  end

  class Put < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('PUT', path, initheader)
    end
  end

  class Delete < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('DELETE', path, initheader)
    end
  end

  class Patch < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('PATCH', path, initheader)
    end
  end

  class Options < HTTPGenericRequest
    def initialize(path, initheader = nil)
      super('OPTIONS', path, initheader)
    end
  end

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
        @socket = TCPSocket.new(@address, @port)
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
    def get(path, initheader = nil, dest = nil, &block)
      request(Get.new(path, initheader), &block)
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
      raise IOError, "HTTP session not started" unless @started
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
    def self.get(uri_or_host, path = nil, port = nil)
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

      start(host, port) do |http|
        http.use_ssl = use_ssl if use_ssl
        response = http.get(path)
        return response.body
      end
    end

    # Class method: Get response object
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

      start(host, port) do |http|
        http.use_ssl = use_ssl if use_ssl
        http.get(path)
      end
    end

    # Class method: POST form data
    def self.post_form(url, params)
      uri = URI.parse(url)
      req = Post.new(uri.request_uri)
      req['content-type'] = 'application/x-www-form-urlencoded'
      req.body = URI.encode_www_form(params)

      start(uri.host, uri.port) do |http|
        http.use_ssl = (uri.scheme == 'https')
        http.request(req)
      end
    end

    # Class method: Start HTTP session
    def self.start(address, port = nil, &block)
      http = new(address, port)
      http.start(&block)
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
      header_end = response.index("\r\n\r\n")
      headers_part = response[0..header_end + 3]

      # Check for Content-Length (case-insensitive search)
      headers_lower = headers_part.downcase
      cl_idx = headers_lower.index('content-length:')
      if cl_idx
        # Extract the value after "content-length:"
        start_idx = cl_idx + 15  # length of "content-length:"
        line_end = headers_part.index("\r\n", start_idx)
        if line_end
          value_str = headers_part[start_idx..(line_end - 1)].strip
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
