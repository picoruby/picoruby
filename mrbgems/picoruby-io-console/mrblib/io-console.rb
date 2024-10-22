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

  def get_nonblock(maxlen)
    buf = ""

  end

  def get_cursor_position
    return [0, 0] if ENV && ENV['TERM'] == "dumb"
    row, col = 0, 0
    raw do
      STDOUT.print "\e[6n"
      while true
        case c = STDIN.getc&.ord
        when 59 # ";"
          break
        when 0x30..0x39
          row = row * 10 + (c.to_i - 0x30)
        end
      end
      while true
        case c = STDIN.getc&.ord
        when 82 # "R"
          break
        when 0x30..0x39
          col = col * 10 + (c.to_i - 0x30)
        else
          raise Exception.new("get_cursor_position failed")
        end
      end
    end
    return [row, col]
  end

  def self.clear_screen
    STDOUT.print "\e[2J\e[1;1H"
  end

  def self.wait_terminal(timeout: nil)
    timer = 0.0
    STDIN.raw do
      while true
        timer += 0.1
        STDIN.read_nonblock(100) # clear buffer
        STDOUT.print "\e[5n" # CSI DSR 5 to request terminal status report
        sleep 0.1
        if STDIN.read_nonblock(4) == "\e[0n"
          ENV['TERM'] = "ansi"
          break
        end
        if timeout && timeout.to_f < timer
          ENV['TERM'] = "dumb"
          # TODO: refactor after fixing the bug
          # https://github.com/picoruby/picoruby/issues/148
          return false
        end
      end
    end
    true
  end
end

## To save current termios state
#IO.raw!
#IO.cooked!
