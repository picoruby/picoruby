module JS
  class WebSerial
    # Returns true if Web Serial API is available (Chrome / Edge).
    def self.supported?
      nav = JS.global[:navigator] # steep:ignore
      nav && nav[:serial] && nav[:serial].type != "undefined"
    end

    # Request a serial port via the browser picker.
    # Must be called from a user gesture handler (click, etc.).
    # Yields a JS::WebSerial instance (or nil if user cancelled) to the block.
    def self.request_port(filters: [], &block)
      port_obj = nil
      JS.global[:navigator][:serial].requestPort().then do |js_port| # steep:ignore
        port_obj = new(js_port)
      end
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
      JS::WebSerial._open_port(@js_port, options).then { @opened = true } # steep:ignore
      block.call(self) if block && @opened
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

    def opened?
      @opened
    end
  end
end
