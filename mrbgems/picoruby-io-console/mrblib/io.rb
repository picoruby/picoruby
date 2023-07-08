class IO
  def self.raw(&block)
    raise Exception.new("no block given") unless block
    self.raw!
    res = block.call
  ensure
    self.cooked!
    return res
  end

  def self.getch
    self.raw do
      getc
    end
  end

  def self.get_cursor_position
    return [0, 0] if ENV && ENV['TERM'] == "dumb"
    row, col = 0, 0
    raw do
      print "\e[6n"
      while true
        case c = getc&.ord
        when 59 # ";"
          break
        when 0x30..0x39
          row = row * 10 + (c.to_i - 0x30)
        end
      end
      while true
        case c = getc&.ord
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
    print "\e[2J\e[1;1H"
  end

  def self.wait_terminal(timeout: nil)
    timer = 0.0
    IO.raw do
      while true
        timer += 0.1
        print "\e[5n"
        sleep 0.1
        if "\e[0n" == IO.read_nonblock(10) || (timeout && timeout.to_f < timer)
          break
        end
      end
      sleep 0.1
      IO.read_nonblock(10) # discard the rest
    end
    # to avoid "get_cursor_position failed"
    if timeout && timeout.to_f < timer
      ENV['TERM'] = "dumb"
      return false
    else
      return true
    end
  end
end

# To save current termios state
IO.raw!
IO.cooked!
