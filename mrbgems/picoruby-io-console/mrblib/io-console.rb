#
# Note:
#   mruby/c does not distinguish between singleton methods and class methods.
#   So, we need to use `STDOUT.print` instead of `print` in the IO class.
#

class IO
  def self.raw(&block)
    self.raw!
    res = block.call
  ensure
    self.cooked!
    return res
  end

  def self.getch
    self.raw do
      while true
        c = STDIN.getc
        break if 0 < c.to_s.length
      end
      c
    end
  end

  def self.get_cursor_position
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
    res = false
    IO.raw do
      while true
        timer += 0.1
        IO.read_nonblock(1000) # clear buffer
        STDOUT.print "\e[5n" # CSI DSR 5 to request terminal status report
        sleep 0.1
        if IO.read_nonblock(10) == "\e[0n"
          res = true
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
    res
  end
end

## To save current termios state
#IO.raw!
#IO.cooked!
