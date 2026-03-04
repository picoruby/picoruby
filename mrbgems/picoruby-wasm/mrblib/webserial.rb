module JS
  class WebSerial
    # Returns true if Web Serial API is available (Chrome / Edge).
    def self.supported?
      nav = JS.global[:navigator] # steep:ignore
      nav && (serial = nav[:serial]) && serial.type != "undefined"
    end

    # Request a serial port via the browser picker.
    # Must be called from a user gesture handler (click, etc.).
    # Yields a JS::WebSerial instance (or nil if user cancelled) to the block.
    def self.request_port(filters: [], &block)
      raw_port = JS::WebSerial._request_port.await
      port_obj = raw_port ? new(raw_port) : nil
      block.call(port_obj) if block
      port_obj
    end

    # js_port is a JS::Object wrapping a SerialPort.
    def initialize(js_port)
      @js_port = js_port
      @opened = false
    end

    # Open the serial port with given parameters.
    # Yields self to the block when the port is successfully opened.
    # Uses JS::WebSerial._open_port (C method) because Kernel#open is private
    # and would shadow method_missing on JS::Object.
    def open(baud_rate: 115200, data_bits: 8, stop_bits: 1, parity: "none", &block)
      options = JS.global.create_object
      options[:baudRate] = baud_rate
      options[:dataBits] = data_bits
      options[:stopBits] = stop_bits
      options[:parity] = parity
      JS::WebSerial._open_port(@js_port, options).await
      @opened = true
      block.call(self) if block
      self
    end

    # Write a binary String to the serial port.
    def write(str)
      JS::WebSerial._write(@js_port, str)
    end

    # Register a block to be called with each received binary String chunk.
    def on_receive(&block)
      callback_id = block.object_id
      JS::Object::CALLBACKS[callback_id] = block
      JS::WebSerial._start_reading(@js_port, callback_id)
    end

    # Register a block to be called when the port disconnects.
    def on_disconnect(&block)
      callback_id = block.object_id
      JS::Object::CALLBACKS[callback_id] = block
      JS::WebSerial._set_on_disconnect(@js_port, callback_id)
    end

    # Close the serial port.
    def close
      @opened = false
      JS::WebSerial._close_port(@js_port)
    end

    # Wait for pending writes to flush (returns a Promise).
    def drain
      JS::WebSerial._drain(@js_port)
    end

    # Pipe decoded serial text directly to an xterm.js terminal.
    def start_terminal_read(terminal)
      JS::WebSerial._read_from_port(@js_port, terminal)
    end

    # Start capturing serial output.
    def capture_start
      JS::WebSerial._capture_start(@js_port)
    end

    # Peek at captured output without clearing it.
    def capture_peek
      JS::WebSerial._capture_peek(@js_port).to_s
    end

    # Stop capturing and return the captured output.
    def capture_stop
      JS::WebSerial._capture_stop(@js_port).to_s
    end

    def opened?
      @opened
    end
  end
end
