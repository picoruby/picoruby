#
# Note:
#   mruby/c does not distinguish between singleton methods and class methods.
#   So, we need to use `STDOUT.print` instead of `print` in the IO class.
#

class IO
  def raw(&block)
    raw!
    res = block.call(self)
  ensure
    _restore_termios
    return res
  end

  def cooked(&block)
    cooked!
    res = block.call(self)
  ensure
    _restore_termios
    return res
  end

  def getch
    raw do |io|
      while true
        c = io.getc
        break if 0 < c.to_s.length
      end
      c
    end
  end

  def get_cursor_position
    return [0, 0] if ENV['TERM'] == "dumb"
    row, col = 0, 0
    STDIN.read_nonblock(100) # discard buffer
    IO.raw do
      STDOUT.print "\e[6n"
  #    sleep_ms 1
      while true
        c = STDIN.read_nonblock(1)&.ord || 0
        if 0x30 <= c && c <= 0x39 # "0".."9"
          row = row * 10 + c - 0x30
        elsif c == 0x3B # ";"
          break
        else
          sleep_ms 1
        end
      end
      while true
        c = STDIN.read_nonblock(1)&.ord || 0
        if 0x30 <= c && c <= 0x39
          col = col * 10 + c - 0x30
        elsif c == 0x52 # "R"
          break
        elsif c == 0
          sleep_ms 1
        else
          raise "Invalid cursor position response"
        end
      end
    end
    return [row.to_i, col.to_i]
  end

  def self.clear_screen
    STDOUT.print "\e[2J\e[1;1H"
  end

  def self.wait_terminal(timeout: 65535)
    res = ""
    STDIN.read_nonblock(100) # clear buffer
    STDOUT.print "\e[5n" # CSI DSR 5 to request terminal status report
    (timeout * 1000).to_i.times do |i|
      res << STDIN.read_nonblock(1).to_s
      break i if 3 < res.length
      sleep_ms 1
    end
    ENV['TERM'] = res.start_with?("\e[0n") ? "ansi" : "dumb"
  end
end

## To save current termios state
#IO.raw!
#IO.cooked!
