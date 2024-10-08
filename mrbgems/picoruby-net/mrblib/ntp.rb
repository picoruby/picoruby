require 'pack'
require 'time'

class Net
  class NTP
    class Response
      def initialize(unix_time)
        @unixtime = unix_time
      end

      def time
        Time.at(@unixtime)
      end
    end

    NTP_SERVER = "pool.ntp.org"
    NTP_PORT = 123
    NTP_PACKET_SIZE = 48

    def self.set_hwclock
      Time.set_hwclock(Net::NTP.get.time)
    end

    def self.get(ntp_server = NTP_SERVER, ntp_port = NTP_PORT)
      # Create a NTP packet
      ntp_packet = String.pack('C', 0x1b) + ("\0" * 47)

      response = UDPClient.send(ntp_server, ntp_port, ntp_packet, false)

      if response && response.size == NTP_PACKET_SIZE
        # Extract NTP timestamp (seconds from 1900-01-01)
        seconds = ((response[40]&.ord||0) << 24) | ((response[41]&.ord||0) << 16) | ((response[42]&.ord||0) << 8) | (response[43]&.ord||0)
        fraction = ((response[44]&.ord||0) << 24) | ((response[45]&.ord||0) << 16) | ((response[46]&.ord||0) << 8) | (response[47]&.ord||0)

        # Convert NTP timestamp to UNIX timestamp
        unixtime = (seconds - 2_208_988_800 + (fraction / 2.0**32)).to_i

        # Convert UNIX timestamp to human-readable time
        Response.new(unixtime)
      else
        raise "Failed to retrieve time from NTP server."
      end
    end

  end
end
