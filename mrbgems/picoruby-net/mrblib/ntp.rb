class Net
  class NTP
    NTP_SERVER = "pool.ntp.org"
    NTP_PORT = 123
    NTP_PACKET_SIZE = 48

    def self.get(ntp_server = NTP_SERVER, ntp_port = NTP_PORT)
      # Create a NTP packet
      ntp_packet = "\e" + "\0" * 47

      r = UDPClient.send(ntp_server, ntp_port, ntp_packet, false)

      if r&.size == NTP_PACKET_SIZE
        # Extract NTP timestamp (seconds from 1900-01-01)
        # @type var r: String
        r40 = r[40]
        r41 = r[41]
        r42 = r[42]
        r43 = r[43]
        # @type var r40: String
        # @type var r41: String
        # @type var r42: String
        # @type var r43: String
        seconds = ( r40.ord << 24 | r41.ord << 16 | r42.ord << 8 | r43.ord ) - 2_208_988_800
        # Extract NTP fraction (nanoseconds)
        r44 = r[44]
        r45 = r[45]
        r46 = r[46]
        r47 = r[47]
        # @type var r44: String
        # @type var r45: String
        # @type var r46: String
        # @type var r47: String
        fraction = r44.ord << 24 | r45.ord << 16 | r46.ord << 8 | r47.ord
        # Calculate nanoseconds
        nanoseconds = (fraction * 1_000_000_000) >> 32
        return [seconds, nanoseconds]

      else
        raise "Failed to retrieve time from NTP server."
      end
    end

  end
end
