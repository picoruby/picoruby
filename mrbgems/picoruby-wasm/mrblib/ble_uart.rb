module JS
  module BLE
    # High-level IO-compatible wrapper for BLE UART communication.
    # Defaults to Nordic UART Service (NUS) UUIDs, but accepts custom UUIDs.
    # TX = write direction (Central -> Peripheral)
    # RX = read/notify direction (Peripheral -> Central)
    class UART
      # Nordic UART Service UUIDs (defaults)
      NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
      NUS_TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
      NUS_RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

      DEFAULT_TIMEOUT = nil

      attr_reader :device

      # Connect to a BLE UART device.
      def initialize(service_uuid: NUS_SERVICE_UUID,
                     tx_uuid: NUS_TX_CHAR_UUID,
                     rx_uuid: NUS_RX_CHAR_UUID,
                     name: nil, name_prefix: nil)
        @device = GATT.request_device(
          name: name,
          name_prefix: name_prefix,
          optional_services: [service_uuid]
        )

        @buffer = ""
        @server = @device.connect # steep:ignore
        @service = @server.service(service_uuid) # steep:ignore
        @tx_char = @service.characteristic(tx_uuid) # steep:ignore

        if tx_uuid == rx_uuid
          @rx_char = @tx_char
        else
          @rx_char = @service.characteristic(rx_uuid) # steep:ignore
        end

        @rx_char.on_change do |data| # steep:ignore
          @buffer << data
        end
        @rx_char.start_notify # steep:ignore

        @device.on_disconnected do # steep:ignore
          @connected = false
        end
        @connected = true
      end

      # Write string data to the TX characteristic.
      def write(data)
        @tx_char.write(data, without_response: true) # steep:ignore
        data.bytesize
      end

      # Write string data with trailing newline.
      def puts(data)
        write(data + "\n")
        nil
      end

      # Read up to nbytes bytes. Blocks until enough data or timeout.
      def read(nbytes, timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        while @buffer.bytesize < nbytes
          return nil if deadline && Time.now.to_f >= deadline
          sleep 0.05
        end
        result = @buffer.byteslice(0, nbytes)
        @buffer = @buffer.byteslice(nbytes, @buffer.bytesize - nbytes) || ""
        result
      end

      # Read a line (blocks until newline or timeout).
      def gets(timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        while true
          idx = @buffer.index("\n")
          if idx
            line = @buffer.byteslice(0, idx + 1)
            @buffer = @buffer.byteslice(idx + 1, @buffer.bytesize - idx - 1) || ""
            return line
          end
          return nil if deadline && Time.now.to_f >= deadline
          sleep 0.05
        end
      end

      # Non-blocking read: returns up to nbytes from buffer, or nil if empty.
      def read_nonblock(nbytes)
        return nil if @buffer.empty?
        actual = nbytes < @buffer.bytesize ? nbytes : @buffer.bytesize
        result = @buffer.byteslice(0, actual)
        @buffer = @buffer.byteslice(actual, @buffer.bytesize - actual) || ""
        result
      end

      # Number of bytes available in the receive buffer.
      def available
        @buffer.bytesize
      end

      def available?
        0 < @buffer.bytesize
      end

      # Whether the device is currently connected.
      def connected?
        @connected && @device.connected? # steep:ignore
      end

      # Disconnect and clean up.
      def close
        @rx_char.stop_notify # steep:ignore
        @device.disconnect # steep:ignore
        @connected = false
        @buffer = ""
      end
    end
  end
end
