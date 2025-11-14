# Net::HTTP - HTTP client library for PicoRuby
# CRuby-compatible interface for making HTTP requests
#
# This file consolidates all Net::HTTP components into a single file
# to avoid PicoRuby build system issues with subdirectories.


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

      # Join headers with CRLF
      result = lines.join("\r\n")

      # Add CRLF after headers
      result += "\r\n"

      # Add empty line to separate headers from body
      result += "\r\n"

      # Body (if present)
      result += @body if @body

      result
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
end
