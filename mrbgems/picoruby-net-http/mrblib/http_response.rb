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
    private def self.parse(response_string)
      unless response_string && !response_string.empty?
        raise HTTPBadResponse, "Empty HTTP response"
      end

      lines = response_string.split("\r\n")
      if lines.empty?
        raise HTTPBadResponse, "Invalid HTTP response"
      end

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
          key = line[0..(colon_idx - 1)]&.strip
          value = line[(colon_idx + 1)..-1]&.strip
          response.header[key&.downcase] = value
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
      [self[key]]
    end

    # Read body (for compatibility)
    def read_body(&block)
      if block
        yield @body || ''
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
end
