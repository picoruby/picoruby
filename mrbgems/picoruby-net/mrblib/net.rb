module Net
  class HTTPUtil
    def self.format_response(raw_response)
      if raw_response.is_a?(String)
        lines = raw_response.split("\r\n")
        line_number = 0
        status = 0
        headers = {}
        body = ""
        body_started = false
        while(true)
          if lines[line_number] == nil
            break
          elsif line_number == 0
            status = lines[0].split(' ')[1].to_i
          elsif body_started == false
            if lines[line_number].length == 0
              body_started = true
            else
              fields = lines[line_number].split(':')
              headers[fields[0]] = fields[1]&.strip
            end
          else
            body += lines[line_number]
            body += "\r\n" unless lines[line_number + 1].nil?
          end
          line_number += 1
        end
        return {
          status: status,
          headers: headers,
          body: body
        }
      else
        return nil
      end
    end
  end

  class DNS
  end

  class UDPClient
    def self.send(host, port, content, is_dtls)
      # ip = DNS.resolve(host, false)
      _send_impl(host, port, content, is_dtls)
    end
  end

  class TCPClient
    def self.request(host, port, content, is_tls)
      # ip = DNS.resolve(host, true)
      _request_impl(host, port, content, is_tls)
    end
  end

  class HTTPClient
    def initialize(host)
      @host = host
    end

    def get(path)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      req += "Connection: close\r\n"
      req += "\r\n"

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 80, req, false))
    end

    def get_with_headers(path, headers)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 80, req, false))
    end

    def post(path, headers, body)
      req =  "POST #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"
      req += body


      return Net::HTTPUtil.format_response(TCPClient.request(@host, 80, req, false))
    end

    def put(path, headers, body)
      req =  "PUT #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"
      req += body

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 80, req, false))
    end
  end

  class HTTPSClient
    def initialize(host)
      @host = host
    end

    def get(path)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      req += "Connection: close\r\n"
      req += "\r\n"

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 443, req, true))
    end

    def get_with_headers(path, headers)
      req =  "GET #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 443, req, true))
    end

    def post(path, headers, body)
      req =  "POST #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"
      req += body

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 443, req, true))
    end

    def put(path, headers, body)
      req =  "PUT #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"
      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end
      req += "\r\n"
      req += body

      return Net::HTTPUtil.format_response(TCPClient.request(@host, 443, req, true))
    end
  end

end
