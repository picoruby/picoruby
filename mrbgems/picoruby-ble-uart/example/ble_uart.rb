require 'ble'

# BLE UART Echo Server
# Echoes back any data received from a BLE Central (e.g. browser JS::BLE::UART)
#
# Usage:
#   Copy this file to your PicoRuby project and flash to Pico W.
#   Connect from browser using JS::BLE::UART and send text.

uart = BLE::UART.new(name: "PicoRuby")
uart.debug = true

uart.start do
  if uart.available?
    data = uart.read_nonblock(256)
    if data
      uart.puts("echo: #{data}")
    end
  end
end
