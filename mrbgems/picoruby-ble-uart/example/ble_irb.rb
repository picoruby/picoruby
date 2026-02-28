require 'ble'

# BLE IRB Server
# Receives Ruby one-liners from a BLE Central (e.g. ble_irb_demo.html),
# evals them, and notifies the result back.

uart = BLE::UART.new(name: "PicoIRB")
uart.debug = true
sandbox = Sandbox.new('ble-irb')

uart.start do
  if (line = uart.gets_nonblock) && (code = line.chomp) && !code.empty?
    if sandbox.compile("begin; _ = (#{code}); rescue => _; end; _")
      executed = sandbox.execute
      sandbox.wait(timeout: nil)
      sandbox.suspend
      if executed
        if sandbox.result.is_a?(Exception)
          uart.puts "=> #{sandbox.result.message} (#{sandbox.result.class})"
        else
          uart.puts "=> #{sandbox.result.inspect}"
        end
      end
    else
      uart.puts "=> SyntaxError"
    end
  end
end
