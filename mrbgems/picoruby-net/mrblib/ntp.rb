module Net
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

      if r&.bytesize == ntp_packet.bytesize
        # NTP timestamp
        ntp_t = r.getbyte(40) << 24 | r.getbyte(41) << 16 | r.getbyte(42) << 8 | r.getbyte(43)
        # NTP fraction (nanoseconds)
        ntp_f = r.getbyte(44) << 24 | r.getbyte(45) << 16 | r.getbyte(46) << 8 | r.getbyte(47)
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
