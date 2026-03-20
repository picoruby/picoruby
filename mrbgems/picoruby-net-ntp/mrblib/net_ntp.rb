require 'socket'
require 'time'
require 'pack'

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
        # C4: LI/VN/Mode, Stratum, Poll, Precision (4 bytes)
        # N11: Root Delay, Root Dispersion, Ref ID,
        #      Ref/Orig/Recv/Xmit timestamps (sec+frac each) (44 bytes)
        [
          (@leap_indicator << 6) | (@version << 3) | @mode,
          @stratum, @poll, @precision,
          @root_delay, @root_dispersion, @reference_identifier,
          @reference_timestamp_sec, @reference_timestamp_frac,
          @originate_timestamp_sec, @originate_timestamp_frac,
          @receive_timestamp_sec, @receive_timestamp_frac,
          @transmit_timestamp_sec, @transmit_timestamp_frac
        ].pack("C4N11")
      end

      def parse(data)
        return unless 48 <= data.bytesize

        fields = data.unpack("C4N11")
        byte0 = fields[0]
        @leap_indicator = (byte0 >> 6) & 0x03
        @version = (byte0 >> 3) & 0x07
        @mode = byte0 & 0x07
        @stratum = fields[1]
        @poll = fields[2]
        @precision = fields[3]
        @root_delay = fields[4]
        @root_dispersion = fields[5]
        @reference_identifier = fields[6]
        @reference_timestamp_sec = fields[7]
        @reference_timestamp_frac = fields[8]
        @originate_timestamp_sec = fields[9]
        @originate_timestamp_frac = fields[10]
        @receive_timestamp_sec = fields[11]
        @receive_timestamp_frac = fields[12]
        @transmit_timestamp_sec = fields[13]
        @transmit_timestamp_frac = fields[14]
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

    end

    # Get time from NTP server
    # Returns Unix timestamp (seconds since 1970-01-01)
    def self.get(host = "pool.ntp.org", port = 123, timeout = 5)
      socket = UDPSocket.new
      socket.bind("0.0.0.0", 0)  # Bind to any local port

      request = Packet.new
      request_data = request.to_binary

      start_time = Time.now.to_i
      # Set last_send to one second before start so the first iteration sends immediately
      last_send = start_time - 1
      response_data = nil

      while true
        # Simple timeout check
        current_time = Time.now.to_i
        if 0 < timeout && timeout < (current_time - start_time)
          socket.close
          raise "NTP request timeout"
        end

        # Send (or resend) request every 1 second.
        # Done inside the loop so DNS resolution failures are retried within the timeout window.
        if current_time - last_send >= 1
          begin
            socket.send(request_data, 0, host, port)
            last_send = current_time
          rescue => e
            # DNS resolution or send failed; retry on next iteration
          end
        end

        # Try to receive data (non-blocking to allow timeout check)
        begin
          result = socket.recvfrom_nonblock(48)
          if result && result[0] && 48 <= result[0].bytesize
            response_data = result[0]
            break
          end
        rescue => e
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
