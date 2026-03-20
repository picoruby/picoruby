module Net
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
        ih_keys = initheader.keys
        ihi = 0
        while ihi < ih_keys.size
          self[ih_keys[ihi]] = initheader[ih_keys[ihi]]
          ihi += 1
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
      hkeys = @header.keys
      hi = 0
      while hi < hkeys.size
        yield hkeys[hi], @header[hkeys[hi]]
        hi += 1
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
        self['content-length'] = @body&.bytesize.to_s
      end

      # Connection
      self['connection'] ||= 'close'
    end

    # Convert request to HTTP wire format
    def to_s
      lines = [] #: Array[String]

      # Request line
      lines << "#{@method} #{@path} HTTP/1.1"

      # Headers
      hkeys = @header.keys
      hi = 0
      while hi < hkeys.size
        key = hkeys[hi]
        value = @header[key]
        # Capitalize header names properly (manually since capitalize is not in mruby/c)
        parts = key.split('-')
<<<<<<< HEAD
        formatted_parts = [] #: Array[String]
        parts.each do |word|
          if word.length > 0
            # Capitalize first char, lowercase rest
            capitalized = (word[0] or raise).upcase + (word.length > 1 ? (word[1..-1] or raise).downcase : '')
=======
        formatted_parts = []
        pi = 0
        while pi < parts.size
          word = parts[pi]
          if 0 < word.length
            # Capitalize first char, lowercase rest
            capitalized = word[0].upcase + (1 < word.length ? word[1..-1].downcase : '')
>>>>>>> origin/master
            formatted_parts << capitalized
          else
            formatted_parts << word
          end
          pi += 1
        end
        formatted_key = formatted_parts.join('-')
        lines << "#{formatted_key}: #{value}"
        hi += 1
      end

      # Join headers with CRLF
      result = lines.join("\r\n")

      # Add CRLF after headers
      result += "\r\n"

      # Add empty line to separate headers from body
      result += "\r\n"

      # Body (if present)
      result += (@body || '')

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
