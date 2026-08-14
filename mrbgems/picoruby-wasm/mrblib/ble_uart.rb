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

      # Cap for the receive buffer. If the application stops reading,
      # oldest bytes are dropped first instead of growing forever.
      RX_BUFFER_LIMIT = 8192

      attr_reader :device

      # Connect to a BLE UART device.
      # By default the browser chooser only lists devices advertising
      # service_uuid; pass filter_by_service: false to list everything
      # (some peripherals do not advertise the service UUID).
      def initialize(service_uuid: NUS_SERVICE_UUID,
                     tx_uuid: NUS_TX_CHAR_UUID,
                     rx_uuid: NUS_RX_CHAR_UUID,
                     name: nil, name_prefix: nil,
                     filter_by_service: true)
        @service_uuid = service_uuid
        @tx_uuid = tx_uuid
        @rx_uuid = rx_uuid
        @on_disconnect = nil
        @rx_callback_id = nil
        @device = GATT.request_device(
          name: name,
          name_prefix: name_prefix,
          services: filter_by_service ? [service_uuid] : nil,
          optional_services: [service_uuid]
        )

        @buffer = ""
        @device.on_disconnected do # steep:ignore
          @connected = false
          @on_disconnect&.call
        end
        _connect
      end

      # Registers a block called when the connection is lost.
      def on_disconnect(&block)
        @on_disconnect = block
      end

      # Reconnect to the same device without showing the browser
      # chooser again (the permission granted by the first chooser is
      # remembered by Web Bluetooth). Use this to recover from a
      # dropped link or after #close.
      def reconnect
        return self if connected?
        @buffer = ""
        _connect
        self
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

      # Disconnect and clean up. The device reference is kept, so
      # #reconnect can re-establish the link without a new chooser.
      def close
        @rx_char.stop_notify # steep:ignore
        @device.disconnect # steep:ignore
        @connected = false
        @buffer = ""
        _close_rx_consumer
      end

      private

      def _connect
        @server = @device.connect # steep:ignore
        @service = @server.service(@service_uuid) # steep:ignore
        @tx_char = @service.characteristic(@tx_uuid) # steep:ignore

        if @tx_uuid == @rx_uuid
          @rx_char = @tx_char
        else
          @rx_char = @service.characteristic(@rx_uuid) # steep:ignore
        end

        # End the previous notification consumer before installing a
        # new one; otherwise every reconnect would leak a consumer task.
        # The JS-side listener uses replace semantics, so re-registering
        # on the same characteristic cannot stack handlers either.
        _close_rx_consumer
        @rx_callback_id = @rx_char.on_change do |data| # steep:ignore
          _push_rx(data)
        end
        @rx_char.start_notify # steep:ignore
        @connected = true
      end

      def _close_rx_consumer
        callback_id = @rx_callback_id
        if callback_id
          JS::Object._close_event_queue(callback_id)
          @rx_callback_id = nil
        end
      end

      def _push_rx(data)
        @buffer << data
        if RX_BUFFER_LIMIT < @buffer.bytesize
          @buffer = @buffer.byteslice(-RX_BUFFER_LIMIT, RX_BUFFER_LIMIT) || ""
        end
      end
    end
  end
end
