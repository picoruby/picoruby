require 'socket'
require 'time'

module Net
  class NTP
    # NTP epoch is 1900-01-01 00:00:00
    # Unix epoch is 1970-01-01 00:00:00
    # Difference is 70 years + 17 leap days = 2208988800 seconds
    NTP_EPOCH_OFFSET = 2208988800

    # NTP timestamp format: 32-bit seconds + 32-bit fractional seconds
    # Fractional part represents 1/(2^32) of a second

    class Packet
      attr_reader :leap_indicator, :version, :mode, :stratum, :poll, :precision
      attr_reader :root_delay, :root_dispersion, :reference_identifier
      attr_reader :reference_timestamp_sec, :reference_timestamp_frac
      attr_reader :originate_timestamp_sec, :originate_timestamp_frac
      attr_reader :receive_timestamp_sec, :receive_timestamp_frac
      attr_reader :transmit_timestamp_sec, :transmit_timestamp_frac

      def initialize(data = nil)
        if data
          parse(data)
        else
          # Create client request packet (version 3, mode 3 = client)
          @leap_indicator = 0
          @version = 3
          @mode = 3
          @stratum = 0
          @poll = 0
          @precision = 0
          @root_delay = 0
          @root_dispersion = 0
          @reference_identifier = 0
          @reference_timestamp_sec = 0
          @reference_timestamp_frac = 0
          @originate_timestamp_sec = 0
          @originate_timestamp_frac = 0
          @receive_timestamp_sec = 0
          @receive_timestamp_frac = 0
          sec, frac = current_ntp_time
          @transmit_timestamp_sec = sec
          @transmit_timestamp_frac = frac
        end
      end

      def to_binary
        # Pack NTP packet into 48-byte binary format
        data = []

        # Byte 0: LI (2 bits) + VN (3 bits) + Mode (3 bits)
        data << ((@leap_indicator << 6) | (@version << 3) | @mode)

        # Bytes 1-3: Stratum, Poll, Precision
        data << @stratum
        data << @poll
        data << @precision

        # Bytes 4-7: Root Delay (32-bit)
        data += pack_u32(@root_delay)

        # Bytes 8-11: Root Dispersion (32-bit)
        data += pack_u32(@root_dispersion)

        # Bytes 12-15: Reference Identifier (32-bit)
        data += pack_u32(@reference_identifier)

        # Bytes 16-23: Reference Timestamp (64-bit)
        data += pack_u32(@reference_timestamp_sec)
        data += pack_u32(@reference_timestamp_frac)

        # Bytes 24-31: Originate Timestamp (64-bit)
        data += pack_u32(@originate_timestamp_sec)
        data += pack_u32(@originate_timestamp_frac)

        # Bytes 32-39: Receive Timestamp (64-bit)
        data += pack_u32(@receive_timestamp_sec)
        data += pack_u32(@receive_timestamp_frac)

        # Bytes 40-47: Transmit Timestamp (64-bit)
        data += pack_u32(@transmit_timestamp_sec)
        data += pack_u32(@transmit_timestamp_frac)

        # Convert array to string
        result = ""
        data.each { |byte| result << byte.chr }
        result
      end

      def parse(data)
        return unless data.length >= 48

        bytes = data.bytes

        # Byte 0
        byte0 = bytes[0]
        @leap_indicator = (byte0 >> 6) & 0x03
        @version = (byte0 >> 3) & 0x07
        @mode = byte0 & 0x07

        # Bytes 1-3
        @stratum = bytes[1]
        @poll = bytes[2]
        @precision = bytes[3]

        # Bytes 4-7: Root Delay
        @root_delay = unpack_u32(bytes, 4)

        # Bytes 8-11: Root Dispersion
        @root_dispersion = unpack_u32(bytes, 8)

        # Bytes 12-15: Reference Identifier
        @reference_identifier = unpack_u32(bytes, 12)

        # Bytes 16-23: Reference Timestamp
        @reference_timestamp_sec = unpack_u32(bytes, 16)
        @reference_timestamp_frac = unpack_u32(bytes, 20)

        # Bytes 24-31: Originate Timestamp
        @originate_timestamp_sec = unpack_u32(bytes, 24)
        @originate_timestamp_frac = unpack_u32(bytes, 28)

        # Bytes 32-39: Receive Timestamp
        @receive_timestamp_sec = unpack_u32(bytes, 32)
        @receive_timestamp_frac = unpack_u32(bytes, 36)

        # Bytes 40-47: Transmit Timestamp
        @transmit_timestamp_sec = unpack_u32(bytes, 40)
        @transmit_timestamp_frac = unpack_u32(bytes, 44)
      end

      # Convert NTP timestamp (64-bit) to Unix timestamp (seconds since 1970)
      def transmit_time
        return nil if @transmit_timestamp_sec == 0
        ntp_to_unix(@transmit_timestamp_sec)
      end

      def receive_time
        return nil if @receive_timestamp_sec == 0
        ntp_to_unix(@receive_timestamp_sec)
      end

      def reference_time
        return nil if @reference_timestamp_sec == 0
        ntp_to_unix(@reference_timestamp_sec)
      end

      private

      def current_ntp_time
        # Get current Unix timestamp and convert to NTP format
        # For embedded systems without RTC, this will be 0
        unix_time = Time.now.to_i
        unix_to_ntp(unix_time)
      end

      def unix_to_ntp(unix_time)
        # Convert Unix timestamp to NTP timestamp
        # NTP timestamp is 64-bit: 32-bit seconds + 32-bit fractional
        ntp_seconds = unix_time + NTP_EPOCH_OFFSET
        # Fractional part is 0 for integer timestamps
        # Return [seconds, fractional] as separate 32-bit values
        [ntp_seconds, 0]
      end

      def ntp_to_unix(ntp_seconds)
        # Convert NTP seconds (32-bit) to Unix timestamp
        # Handle unsigned 32-bit arithmetic properly
        if ntp_seconds < 0
          # Treat as unsigned 32-bit value
          ntp_seconds = ntp_seconds & 0xFFFFFFFF
        end
        ntp_seconds - NTP_EPOCH_OFFSET
      end

      def pack_u32(value)
        # Pack 32-bit unsigned integer (big-endian)
        [
          (value >> 24) & 0xFF,
          (value >> 16) & 0xFF,
          (value >> 8) & 0xFF,
          value & 0xFF
        ]
      end

      def unpack_u32(bytes, offset)
        # Unpack 32-bit unsigned integer (big-endian)
        result = (bytes[offset] << 24) |
                 (bytes[offset + 1] << 16) |
                 (bytes[offset + 2] << 8) |
                 bytes[offset + 3]
        # Handle sign extension for values >= 0x80000000
        # In PicoRuby, left shift can produce negative values for large numbers
        if result < 0
          # Convert to unsigned 32-bit by masking
          result & 0xFFFFFFFF
        else
          result
        end
      end
    end

    # Get time from NTP server
    # Returns Unix timestamp (seconds since 1970-01-01)
    def self.get(host = "pool.ntp.org", port = 123, timeout = 5)
      socket = UDPSocket.new
      socket.bind("0.0.0.0", 0)  # Bind to any local port

      request = Packet.new
      request_data = request.to_binary

      socket.send(request_data, 0, host, port)

      start_time = Time.now.to_i
      response_data = nil

      while true
        # Simple timeout check
        current_time = Time.now.to_i
        if 0 < timeout && timeout < (current_time - start_time)
          socket.close
          raise "NTP request timeout"
        end

        # Try to receive data
        # recvfrom returns [data, addr_info]
        begin
          # TODO: Sometimes this blocks forever
          result = socket.recvfrom(48)
          if result && result[0] && 48 <= result[0].length
            response_data = result[0]
            break
          end
        rescue
          # Receive failed, continue waiting
        end
        sleep_ms 100
      end

      socket.close

      # Parse response
      response = Packet.new(response_data)

      # Return transmit timestamp as Unix time
      response.transmit_time
    end
  end
end
