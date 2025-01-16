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

  class HTTPClientBase
    def initialize(host)
      @host = host
    end

    def get(path)
      make_request("GET", path)
    end

    def get_with_headers(path, headers)
      make_request("GET", path, headers)
    end

    def post(path, headers, body)
      make_request("POST", path, headers, body)
    end

    def put(path, headers, body)
      make_request("PUT", path, headers, body)
    end

    private

    def build_request(method, path, headers = {}, body = nil)
      req = "#{method} #{path} HTTP/1.1\r\n"
      req += "Host: #{@host}\r\n"

      unless headers.keys.any?{|k| k.downcase == "connection" }
        req += "Connection: close\r\n"
      end

      headers.each do |k, v|
        req += "#{k}: #{v}\r\n"
      end

      req += "\r\n"
      req += body if body

      req
    end

    def make_request(method, path, headers = {}, body = nil)
      request = build_request(method, path, headers, body)
      response = TCPClient.request(@host, port, request, use_tls)
      Net::HTTPUtil.format_response(response)
    end

    def port
      raise NotImplementedError, "Subclass must implement port method"
    end

    def use_tls
      raise NotImplementedError, "Subclass must implement use_tls method"
    end

  end

  class HTTPClient < HTTPClientBase
    private

    def port
      80
    end

    def use_tls
      false
    end
  end

  class HTTPSClient < HTTPClientBase
    private

    def port
      443
    end

    def use_tls
      true
    end
  end

  class MQTTClient
    def initialize(host, port = 1883, client_id = "picoruby_mqtt")
      @host = host
      @port = port
      @client_id = client_id
      $_mqtt_singleton = self
    end

    def connect
      _connect_impl(@host, @port, @client_id, false)
    end

    def publish(topic, payload)
      _publish_impl(payload, topic)
    end

    def subscribe(topic)
      _subscribe_impl(topic)
    end

    def instance
      $_mqtt_singleton
    end

    # You can override this method
    def callback
      blink_led
    end

    def blink_led
      @led ||= CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
      @led_on ||= false
      @led&.write((@led_on = !@led_on) ? 1 : 0)
    end

    def disconnect
      _disconnect_impl
    end
  end

end
