require 'pack'
require 'time'

class Net
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

    def self.get(ntp_server = NTP_SERVER, ntp_port = NTP_PORT)
      # Create a NTP packet
      ntp_packet = String.pack('C', 0x1b) + ("\0" * 47)

      response = UDPClient.send(ntp_server, ntp_port, ntp_packet, false)

      if response && response.size == NTP_PACKET_SIZE
        # Extract NTP timestamp (seconds from 1900-01-01)
        seconds = ((response[40]&.ord||0) << 24) | ((response[41]&.ord||0) << 16) | ((response[42]&.ord||0) << 8) | (response[43]&.ord||0)
        fraction = ((response[44]&.ord||0) << 24) | ((response[45]&.ord||0) << 16) | ((response[46]&.ord||0) << 8) | (response[47]&.ord||0)

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
