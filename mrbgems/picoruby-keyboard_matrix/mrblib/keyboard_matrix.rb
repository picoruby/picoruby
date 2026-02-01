require 'gpio'
require 'machine'
require 'usb/hid'

class KeyboardMatrix
  # Start scanning loop
  def start
    unless block_given?
      raise ArgumentError, "A block is required to handle key events"
    end
    loop do
      if event = scan
        yield event
      end
      sleep_ms(1)
    end
  end
end
