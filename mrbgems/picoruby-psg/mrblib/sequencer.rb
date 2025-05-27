# Usage:
# File.open('demo.vgm', 'r') do |f|
#   driver = PSG::Driver.new(:pwm, 15, 16)
#   parser = PSG::VGMParser.new(f).parse
#   seq = PSG::Sequencer.new(driver, parser.each_event.to_a)
#   seq.start
#   loop do
#     seq.service
#     break if seq.finished?
#     sleep_ms 5
#   end
# end

module PSG
  class Sequencer
    BUF_MARGIN = 256        # packets ahead of audio cursor

    def initialize(driver, events)
      @driver = driver
      @events = events
      @index  = 0
      @start_ms = nil
    end

    def start
      @start_ms = @driver.millis        # exported from C side
    end

    def service
      return if @start_ms.nil?
      # @type ivar @start_ms: Integer
      now_tick = (@driver.millis - @start_ms)
      while (e = @events[@index]) && (e[:tick] - now_tick) < BUF_MARGIN
        case e[:op]
        when :reg
          @driver.send_reg(e[:reg], e[:val])
        end
        @index += 1
      end
    end

    def finished?
      @index >= @events.size
    end
  end
end
