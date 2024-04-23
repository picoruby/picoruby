class Net
  class DNS
  end

  class TCPClient
    def self.request(host, port, content, is_tls)
      # ip = DNS.resolve(host)
      _request_impl(host, port, content, is_tls)
    end
  end

  class HTTPClient
    def initialize(host)
      @host = host
    end

    def get(path)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host:#{@host}\r\n"
      req += "\r\n"

      TCPClient.request(@host, 80, req, false)
    end
  end

  class HTTPSClient
    def initialize(host)
      @host = host
    end

    def get(path)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host:#{@host}\r\n"
      req += "\r\n"

      TCPClient.request(@host, 443, req, true)
    end
  end
end
