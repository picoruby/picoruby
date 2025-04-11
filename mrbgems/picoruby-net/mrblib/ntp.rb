class Net
  class NTP
    NTP_SERVER = "pool.ntp.org"
    NTP_PORT = 123
    # Seconds from 1900-01-01 to 1970-01-01
    UNIX_TIME_BASE = 2_208_988_800 # (70*365+17)*24*60*60
                                   #         ^^ leap years

    def self.get(ntp_server = NTP_SERVER, ntp_port = NTP_PORT)
      # Create a NTP packet
      ntp_packet = "\e" + "\0" * 47

      r = UDPClient.send(ntp_server, ntp_port, ntp_packet, false)

      if r&.size == ntp_packet.size
        # steep:ignore:start
        # NTP timestamp
        ntp_t = r[40].ord << 24 | r[41].ord << 16 | r[42].ord << 8 | r[43].ord
        # NTP fraction (nanoseconds)
        ntp_f = r[44].ord << 24 | r[45].ord << 16 | r[46].ord << 8 | r[47].ord
        # steep:ignore:end
        # Convert NTP timestamp to UNIX timestamp
        timestamp = ntp_t - UNIX_TIME_BASE
        # Calculate nanoseconds
        nanoseconds = (ntp_f * 1_000_000_000) >> 32
        return [timestamp, nanoseconds]
      else
        raise "Failed to retrieve time from NTP server."
      end
    end

  end
end
