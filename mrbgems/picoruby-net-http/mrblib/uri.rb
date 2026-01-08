# ============================================================================
# URI Module - Simplified URI parsing for HTTP/HTTPS URLs
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
      rest = uri_string[7..-1]  # Remove 'http://'
    elsif uri_string.start_with?('https://')
      scheme = 'https'
      rest = uri_string[8..-1]  # Remove 'https://'
    else
      raise ArgumentError, "URI scheme must be http or https"
    end

    # Extract fragment
    fragment = nil
    fragment_idx = rest&.index('#')
    if fragment_idx && rest
      fragment = rest[(fragment_idx + 1)..-1]
      rest = rest[0..(fragment_idx - 1)]
    end

    # Extract query
    query = nil
    query_idx = rest&.index('?')
    if query_idx && rest
      query = rest[(query_idx + 1)..-1]
      rest = rest[0..(query_idx - 1)]
    end

    # Extract path
    path = '/'
    slash_idx = rest&.index('/')
    if slash_idx && rest
      host_port = rest[0..(slash_idx - 1)]
      path = rest[slash_idx..-1]
    else
      host_port = rest
    end

    # Extract host and port
    colon_idx = host_port&.index(':')
    if colon_idx && host_port
      host = host_port[0..(colon_idx - 1)]
      port_str = host_port[(colon_idx + 1)..-1]
      # Convert port string to integer manually
      port = 0
      port_str&.each_char do |c|
        if c >= '0' && c <= '9'
          port = port * 10 + (c.ord - '0'.ord)
        end
      end
    else
      host = host_port
      port = scheme == 'https' ? 443 : 80
    end

    URIClass.new(scheme, host || '', port, path || '', query, fragment)
  end

  # Encode URI component (simple version)
  def self.encode_www_form_component(str)
    result = ''
    str.each_char do |char|
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
