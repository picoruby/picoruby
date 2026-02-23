module JS
  module BLE
<<<<<<< HEAD
    # High-level IO-compatible wrapper for BLE UART communication.
    # Defaults to Nordic UART Service (NUS) UUIDs, but accepts custom UUIDs.
    # TX = write direction (Central -> Peripheral)
    # RX = read/notify direction (Peripheral -> Central)
    class UART
      # Nordic UART Service UUIDs (defaults)
      NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
      NUS_TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
      NUS_RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
=======
    # High-level IO-compatible wrapper for Nordic UART Service (NUS).
    # Provides blocking and non-blocking read/write with internal buffer.
    class UART
      # Nordic UART Service UUIDs
      SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
      TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
      RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
>>>>>>> f7cfe1d5 (WIP)

      DEFAULT_TIMEOUT = nil

      attr_reader :device

      # Connect to a BLE UART device.
<<<<<<< HEAD
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
=======
      # Accepts same filter options as GATT.request_device.
      # The NUS service UUID is automatically added to optional_services.
      def initialize(name: nil, name_prefix: nil,
                     services: nil, optional_services: nil)
        opt = optional_services ? optional_services.dup : []
        opt << SERVICE_UUID unless opt.include?(SERVICE_UUID)

        @device = GATT.request_device(
          name: name,
          name_prefix: name_prefix,
          services: services,
          optional_services: opt
        )

        @buffer = ""
        @server = @device.connect
        @service = @server.service(SERVICE_UUID)
        @tx_char = @service.characteristic(TX_CHAR_UUID)
        @rx_char = @service.characteristic(RX_CHAR_UUID)

        @rx_char.on_change do |data|
          @buffer << data
        end
        @rx_char.start_notify

        @device.on_disconnected do
>>>>>>> f7cfe1d5 (WIP)
          @connected = false
        end
        @connected = true
      end

<<<<<<< HEAD
      # Write string data to the TX characteristic.
      def write(data)
        @tx_char.write(data, without_response: true) # steep:ignore
        data.bytesize
      end

      # Write string data with trailing newline.
=======
      # Write string data to the UART TX characteristic.
      # @param data [String]
      # @return [Integer] number of bytes written
      def write(data)
        @tx_char.write(data, without_response: true)
        data.length
      end

      # Write string data with trailing newline.
      # @param data [String]
>>>>>>> f7cfe1d5 (WIP)
      def puts(data)
        write(data + "\n")
        nil
      end

      # Read up to nbytes bytes. Blocks until enough data or timeout.
<<<<<<< HEAD
      def read(nbytes, timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        while @buffer.bytesize < nbytes
=======
      # @param nbytes [Integer]
      # @param timeout [Integer, nil] timeout in seconds (nil = block forever)
      # @return [String, nil] nil on timeout
      def read(nbytes, timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        while @buffer.length < nbytes
>>>>>>> f7cfe1d5 (WIP)
          return nil if deadline && Time.now.to_f >= deadline
          sleep 0.05
        end
        result = @buffer.byteslice(0, nbytes)
<<<<<<< HEAD
        @buffer = @buffer.byteslice(nbytes, @buffer.bytesize - nbytes) || ""
=======
        @buffer = @buffer.byteslice(nbytes, @buffer.length - nbytes) || ""
>>>>>>> f7cfe1d5 (WIP)
        result
      end

      # Read a line (blocks until newline or timeout).
<<<<<<< HEAD
      def gets(timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        while true
          idx = @buffer.index("\n")
          if idx
            line = @buffer.byteslice(0, idx + 1)
            @buffer = @buffer.byteslice(idx + 1, @buffer.bytesize - idx - 1) || ""
=======
      # @param timeout [Integer, nil] timeout in seconds
      # @return [String, nil] nil on timeout
      def gets(timeout: DEFAULT_TIMEOUT)
        deadline = timeout ? Time.now.to_f + timeout : nil
        loop do
          idx = @buffer.index("\n")
          if idx
            line = @buffer.byteslice(0, idx + 1)
            @buffer = @buffer.byteslice(idx + 1, @buffer.length - idx - 1) || ""
>>>>>>> f7cfe1d5 (WIP)
            return line
          end
          return nil if deadline && Time.now.to_f >= deadline
          sleep 0.05
        end
      end

      # Non-blocking read: returns up to nbytes from buffer, or nil if empty.
<<<<<<< HEAD
      def read_nonblock(nbytes)
        return nil if @buffer.empty?
        actual = nbytes < @buffer.bytesize ? nbytes : @buffer.bytesize
        result = @buffer.byteslice(0, actual)
        @buffer = @buffer.byteslice(actual, @buffer.bytesize - actual) || ""
=======
      # @param nbytes [Integer]
      # @return [String, nil]
      def read_nonblock(nbytes)
        return nil if @buffer.empty?
        actual = nbytes < @buffer.length ? nbytes : @buffer.length
        result = @buffer.byteslice(0, actual)
        @buffer = @buffer.byteslice(actual, @buffer.length - actual) || ""
>>>>>>> f7cfe1d5 (WIP)
        result
      end

      # Number of bytes available in the receive buffer.
<<<<<<< HEAD
      def available
        @buffer.bytesize
      end

      def available?
        0 < @buffer.bytesize
      end

      # Whether the device is currently connected.
      def connected?
        @connected && @device.connected? # steep:ignore
=======
      # @return [Integer]
      def available
        @buffer.length
      end

      # Whether the device is currently connected.
      # @return [Boolean]
      def connected?
        @connected && @device.connected?
>>>>>>> f7cfe1d5 (WIP)
      end

      # Disconnect and clean up.
      def close
<<<<<<< HEAD
        @rx_char.stop_notify # steep:ignore
        @device.disconnect # steep:ignore
=======
        @rx_char.stop_notify
        @device.disconnect
>>>>>>> f7cfe1d5 (WIP)
        @connected = false
        @buffer = ""
      end
    end
  end
end
