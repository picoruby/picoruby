# Simplified URI module for PicoRuby
# Provides basic URI parsing for HTTP/HTTPS URLs
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
  # Supports: http://host:port/path?query#fragment
  def self.parse(uri_string)
    return nil unless uri_string

    # Extract scheme
    if uri_string =~ /^(https?):\/\//
      scheme = $1
      rest = uri_string.sub(/^#{scheme}:\/\//, '')
    else
      raise ArgumentError, "URI scheme must be http or https"
    end

    # Extract fragment
    fragment = nil
    if rest =~ /#(.+)$/
      fragment = $1
      rest = rest.sub(/#.*$/, '')
    end

    # Extract query
    query = nil
    if rest =~ /\?(.+)$/
      query = $1
      rest = rest.sub(/\?.*$/, '')
    end

    # Extract path
    path = '/'
    if rest =~ /\//
      parts = rest.split('/', 2)
      host_port = parts[0]
      path = '/' + parts[1] if parts[1]
    else
      host_port = rest
    end

    # Extract host and port
    if host_port =~ /^([^:]+):(\d+)$/
      host = $1
      port = $2.to_i
    else
      host = host_port
      port = scheme == 'https' ? 443 : 80
    end

    URI.new(scheme, host, port, path, query, fragment)
  end

  # Encode URI component (simple version)
  def self.encode_www_form_component(str)
    str.to_s.gsub(/[^a-zA-Z0-9\-_.~]/) do |char|
      '%' + char.unpack('H2')[0].upcase
    end
  end

  # Encode form data
  def self.encode_www_form(params)
    params.map { |k, v|
      "#{encode_www_form_component(k)}=#{encode_www_form_component(v)}"
    }.join('&')
  end
end
