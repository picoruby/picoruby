if RUBY_ENGINE == "mruby/c"
  class Array
    def insert(index, *vals)
      index_int = index.to_i
      if index_int < 0
        raise ArgumentError, "Negative index doesn't work"
      end
      tail = self[index_int, self.length]
      vals.each_with_index do |val, i|
        self[index_int + i] = val
      end
      if tail
        tail_at = index_int + vals.count
        tail.each do |elem|
          self[tail_at] = elem
          tail_at += 1
        end
      end
      self
    end
  end
end

module Editor
  class Buffer
    def initialize
      @cursor_x = 0
      @cursor_y = 0
      clear
    end

    attr_accessor :lines, :changed
    attr_reader :cursor_x, :cursor_y

    def current_line
      @lines[@cursor_y]
    end

    def current_char
      current_line[@cursor_x]
    end

    def clear
      @lines = [""]
      home
    end

    def dump
      @lines.map do |line|
        line[-1] == "\\" ? line[0, line.length - 1] : line
      end.join("\n")
    end

    def home
      @cursor_x = 0
      @cursor_y = 0
    end

    def head
      @cursor_x = 0
    end

    def tail
      @cursor_x = current_line.length
    end

    def bottom
      @cursor_y = @lines.count - 1
    end

    def left
      if 0 < @cursor_x && 0 < current_line.length
        tail if current_line.length < @cursor_x
        @cursor_x -= 1
      elsif 0 < @cursor_y
        up
        tail
      end
    end

    def right
      if @cursor_x < current_line.length
        @cursor_x += 1
      else
        if @cursor_y + 1 < @lines.length
          down
          head
        end
      end
    end

    def up
      if 0 < @cursor_y
        @cursor_y -= 1
        @prev_c = :UP
      end
    end

    def down
      if @cursor_y + 1 < @lines.length
        @cursor_y += 1
        @prev_c = :DOWN
      end
    end

    def put(c)
      line = current_line
      tail if current_line.length < @cursor_x
      if c.is_a?(String)
        @changed = true
        line = line[0, @cursor_x].to_s + c + line[@cursor_x, 65535].to_s
        @lines[@cursor_y] = line
        right
      else
        case c
        when :TAB
          @changed = true
          put " "
          put " "
        when :ENTER
          @changed = true
          new_line = line[@cursor_x, 65535]
          @lines[@cursor_y] = line[0, @cursor_x].to_s
          @lines.insert(@cursor_y + 1, new_line) if new_line
          head
          down
        when :BSPACE
          @changed = true
          if 0 < @cursor_x
            if current_line.length == @cursor_x
              @lines[@cursor_y][-1] = ""
              tail
            else
              @lines[@cursor_y][@cursor_x - 1] = ""
              left
            end
          else
            if 0 < @cursor_y
              @cursor_x = @lines[@cursor_y - 1].length
              @lines[@cursor_y - 1] += current_line
              @lines.delete_at @cursor_y
              up
            end
          end
        when :DOWN
          down
        when :UP
          up
        when :RIGHT
          right
        when :LEFT
          left
        when :HOME
          home
        end
      end
    end

    def delete
      @lines[@cursor_y][@cursor_x] = ""
    end

    def current_tail(n = 1)
      current_line[@cursor_x - n, 65535].to_s
    end

  end

end

