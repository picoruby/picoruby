class IO
  def self.raw(&block)
    raise Exception.new("no block given") unless block
    self.raw!
    while true do
      break if res = block.call
    end
  ensure
    self.cooked!
    return res
  end

  def self.getch
    self.raw do
      getc&.chr
    end
  end

  def self.get_cursor_position
    row = 0
    col = 0
    raw do
      print "\e[6n"
      while true
        c = getc
        if c == 59 # ";"
          break
        elsif 0x30 <= c && c <= 0x39
          row = row * 10 + (c - 0x30)
        end
      end
      while true
        c = getc
        if c == 82 # "R"
          break
        elsif 0x30 <= c && c <= 0x39
          col = col * 10 + (c - 0x30)
        else
          raise Exception.new("get_cursor_position failed")
        end
      end
    end
    return [row, col]
  end

  def self.wait_and_clear
    IO.raw do
      while true
        print "\e[5n"
        sleep 0.1
        break if IO.read_nonblock(10) == "\e[0n"
      end
    end
    print "\e[2J\e[1;1H"
  end
end

# To save current termios state
IO.raw!
IO.cooked!
