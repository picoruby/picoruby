require 'pack'
require 'time'

class Net
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

  class NTP
    class Response
      def initialize(year, month, day, hour, min, sec)
        @year = year
        @month = month
        @day = day
        @hour = hour
        @min = min
        @sec = sec
      end

      def time
        Time.new(@year, @month, @day, @hour, @min, @sec)
      end
    end

    NTP_SERVER = "pool.ntp.org"
    NTP_PORT = 123
    NTP_PACKET_SIZE = 48

    def self.get(ntp_server = NTP_SERVER)
      # Create a NTP packet
      ntp_packet = String.pack('C', 0x1b) + ("\0" * 47)

      response = UDPClient.send(ntp_server, NTP_PORT, ntp_packet, false)

      if response && response.size == NTP_PACKET_SIZE
        # Extract NTP timestamp (seconds from 1900-01-01)
        seconds = (response[40].ord << 24) | (response[41].ord << 16) | (response[42].ord << 8) | response[43].ord
        fraction = (response[44].ord << 24) | (response[45].ord << 16) | (response[46].ord << 8) | response[47].ord

        # Convert NTP timestamp to UNIX timestamp
        unix_time = seconds - 2_208_988_800 + (fraction / 2.0**32)

        # Convert UNIX timestamp to human-readable time
        year, month, day, hour, min, sec = time_components(unix_time)
        Response.new(year, month, day, hour, min, sec)
      else
        raise "Failed to retrieve time from NTP server."
      end
    end

    def self.time_components(unix_time)
      sec = unix_time.to_i
      min = sec / 60
      sec = sec % 60
      hour = min / 60
      min = min % 60
      day = hour / 24
      hour = hour % 24

      # For simplicity, this implementation does not consider leap years
      year = 1970
      while 365 <= day
        days_in_year = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365
        if days_in_year <= day
          day -= days_in_year
          year += 1
        else
          break
        end
      end

      month = 1
      [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31].each do |days_in_month|
        if year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) && month == 2
          days_in_month = 29
        end
        break if day < days_in_month
        day -= days_in_month
        month += 1
      end

      [year, month, day + 1, hour, min, sec]
    end
  end
end
