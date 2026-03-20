require 'ble'

# BLE UART Central
# Scans for a NUS (Nordic UART Service) peripheral, connects automatically,
# and bridges BLE UART to the PicoRuby console (STDIN/STDOUT).
#
# Usage:
#   1. Flash ble_uart.rb (peripheral) to one Pico W.
#   2. Flash this file to another Pico W.
#   3. Open a serial terminal on the central Pico W.
#      Anything you type is sent over BLE; received text is printed.

uart = BLE::UART.new(role: :central)
uart.debug = true

uart.start do
  # Forward STDIN -> BLE
  if (line = $stdin.gets_nonblock)
    uart.write(line)
  end

  # Forward BLE -> STDOUT
  if uart.available?
    print uart.read_nonblock(256)
  end
end
