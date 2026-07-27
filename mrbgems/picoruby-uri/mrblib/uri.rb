# ============================================================================
# URI Module - Simplified URI parsing and form encoding for HTTP/HTTPS URLs
#
# Extracted from picoruby-net-http so that microcontroller targets and
# picoruby.wasm share one implementation. The encoding side follows CRuby's
# URI.encode_www_form_component: bytes outside [A-Za-z0-9*-._] are
# percent-encoded byte-wise (so multibyte UTF-8 works), and a space becomes
# "+".
# ============================================================================
module URI
  class URIClass
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
    unless uri_string
      raise ArgumentError, "URI string cannot be nil"
    end

    # Extract scheme
    scheme = nil
    rest = uri_string

    if uri_string.start_with?('http://')
      scheme = 'http'
      rest = uri_string.byteslice(7..-1)  # Remove 'http://'
    elsif uri_string.start_with?('https://')
      scheme = 'https'
      rest = uri_string.byteslice(8..-1)  # Remove 'https://'
    else
      raise ArgumentError, "URI scheme must be http or https"
    end

    # Extract fragment
    fragment = nil
    fragment_idx = rest&.index('#')
    if fragment_idx && rest
      fragment = rest.byteslice((fragment_idx + 1)..-1)
      rest = rest.byteslice(0..(fragment_idx - 1))
    end

    # Extract query
    query = nil
    query_idx = rest&.index('?')
    if query_idx && rest
      query = rest.byteslice((query_idx + 1)..-1)
      rest = rest.byteslice(0..(query_idx - 1))
    end

    # Extract path
    path = '/'
    slash_idx = rest&.index('/')
    if slash_idx && rest
      host_port = rest.byteslice(0..(slash_idx - 1))
      path = rest.byteslice(slash_idx..-1)
    else
      host_port = rest
    end

    # Extract host and port
    colon_idx = host_port&.index(':')
    if colon_idx && host_port
      host = host_port.byteslice(0..(colon_idx - 1))
      port_str = host_port.byteslice((colon_idx + 1)..-1)
      if port_str && !port_str.empty?
        port = 0
        port_bytes = port_str.bytes
        port_size = port_bytes.size
        i = 0
        while i < port_size
          b = port_bytes[i]
          if 48 <= b && b <= 57
            port = port * 10 + (b - 48)
          else
            raise ArgumentError, "URI port must be numeric"
          end
          i += 1
        end
      else
        port = scheme == 'https' ? 443 : 80
      end
    else
      host = host_port
      port = scheme == 'https' ? 443 : 80
    end

    URIClass.new(scheme, host || '', port, path || '', query, fragment)
  end

  # Characters CRuby's encode_www_form_component leaves untouched. Note the
  # tilde IS encoded (%7E) by CRuby's form encoding, unlike RFC 3986
  # unreserved.
  # No .freeze here: mruby/c (FemtoRuby) has no String#freeze, and a raise
  # in the module body would silently skip every definition below it.
  FORM_SAFE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*-._"
  HEX_UPPER = "0123456789ABCDEF"

  # Encode one form component the way CRuby does: byte-wise %XX for
  # anything outside FORM_SAFE, space as "+". Non-strings are to_s'd first.
  def self.encode_www_form_component(str)
    s = str.to_s
    result = ''
    bytes = s.bytes
    size = bytes.size
    i = 0
    while i < size
      b = bytes[i]
      if b == 32
        result << '+'
      else
        c = b.chr
        if FORM_SAFE.include?(c)
          result << c
        else
          result << '%' << HEX_UPPER[b >> 4].to_s << HEX_UPPER[b & 15].to_s
        end
      end
      i += 1
    end
    result
  end

  # Encode form data. Accepts a Hash or an Array of [key, value] pairs.
  def self.encode_www_form(params)
    pairs = [] #: Array[String]
    if params.is_a?(Hash)
      keys = params.keys
      size = keys.size
      i = 0
      while i < size
        key = keys[i]
        value = params[key]
        encoded_key = encode_www_form_component(key)
        if value.nil?
          pairs << encoded_key
        elsif value.is_a?(Array)
          value_size = value.size
          j = 0
          while j < value_size
            item = value[j]
            if item.nil?
              pairs << ''
            else
              pairs << "#{encoded_key}=#{encode_www_form_component(item)}"
            end
            j += 1
          end
        else
          pairs << "#{encoded_key}=#{encode_www_form_component(value)}"
        end
        i += 1
      end
    else
      size = params.size
      i = 0
      while i < size
        pair = params[i]
        encoded_key = encode_www_form_component(pair[0])
        value = pair[1]
        if value.nil?
          pairs << encoded_key
        elsif value.is_a?(Array)
          value_size = value.size
          j = 0
          while j < value_size
            item = value[j]
            if item.nil?
              pairs << ''
            else
              pairs << "#{encoded_key}=#{encode_www_form_component(item)}"
            end
            j += 1
          end
        else
          pairs << "#{encoded_key}=#{encode_www_form_component(value)}"
        end
        i += 1
      end
    end
    pairs.join('&')
  end
end
