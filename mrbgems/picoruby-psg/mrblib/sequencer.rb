# Usage:
# File.open('demo.vgm', 'r') do |f|
#   parser = PSG::VGMParser.new(f).parse
#   seq = PSG::Sequencer.new(parser.each_event.to_a)
#   seq.start
#   loop do
#     seq.service
#     break if seq.finished?
#     sleep_ms 5
#   end
# end

module PSG
  class Sequencer
    TICK_HZ = 44_100        # same as audio sample rate
    BUF_MARGIN = 256        # packets ahead of audio cursor

    def initialize(events)
      @events = events
      @index  = 0
      @start_ms = nil
    end

    def start
      @start_ms = PSG.millis        # exported from C side
    end

    def service
      return unless @start_ms
      now_tick = ((PSG.millis - @start_ms) * TICK_HZ / 1000).to_i
      while (e = @events[@index]) && (e[:tick] - now_tick) < BUF_MARGIN
        case e[:op]
        when :reg
          PSG.send_reg(e[:reg], e[:val])
        end
        @index += 1
      end
    end

    def finished?
      @index >= @events.size
    end
  end
end
