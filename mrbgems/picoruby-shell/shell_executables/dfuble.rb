require 'dfu'

begin
  require 'ble'
  BLE
rescue LoadError, NameError
  puts "dfuble: ble library not available"
  exit 1
end

name = "RubyOTA"
path = ARGV[0]  # optional: destination path (skips A/B slot and meta)
puts "DFU BLE server (name: #{name})"

uart = BLE::UART.new(name: name)
uart.debug = true

dfu_buf = ""
expected_total = nil
reboot_required = false

uart.start do
  data = uart.read_nonblock(65536)
  dfu_buf << data if data

  if expected_total.nil? && dfu_buf.bytesize >= DFU::Updater::HEADER_SIZE
    total = DFU::Updater.expected_size(dfu_buf)
    if total
      expected_total = total
      puts "DFU: header ok, expecting #{expected_total} bytes total"
    else
      uart.puts("ERROR: invalid DFU header\n")
      dfu_buf = ""
    end
  end

  # @type var expected_total: Integer
  if !expected_total.nil? && expected_total <= dfu_buf.bytesize
    puts "DFU: received #{dfu_buf.bytesize} bytes, processing..."
    begin
      updater = DFU::Updater.new(path: path)
      updater.receive(BLE::UART::BufferIO.new(dfu_buf))
      if path
        uart.puts("OK\n")
        puts "DFU: file saved to #{path}"
      else
        DFU.confirm
        uart.puts("OK\n")
        puts "DFU: update complete."
        reboot_required = true
      end
    rescue => e
      uart.puts("ERROR: #{e.message}\n")
      puts "DFU: error - #{e.message}"
    end
    dfu_buf = ""
    expected_total = nil
  end

  Machine.reboot if reboot_required
end
