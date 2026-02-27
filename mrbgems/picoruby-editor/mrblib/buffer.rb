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
        tail_at = index_int + vals.size
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
      @dirty = :none
      clear
    end

    attr_accessor :lines, :changed
    attr_reader :cursor_x, :cursor_y, :dirty
    attr_reader :selection_start_x, :selection_start_y, :selection_mode

    def mark_dirty(level)
      case @dirty
      when :structure
        # never downgrade
      when :content
        @dirty = level if level == :structure
      when :cursor
        @dirty = level if level == :content || level == :structure
      else
        @dirty = level
      end
    end

    def clear_dirty
      @dirty = :none
    end

    def current_line
      @lines[@cursor_y]
    end

    def current_char
      current_line[@cursor_x]
    end

    def clear
      @lines = [""]
      clear_selection
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
      mark_dirty(:cursor)
    end

    def head
      @cursor_x = 0
      mark_dirty(:cursor)
    end

    def tail
      @cursor_x = current_line.length
      mark_dirty(:cursor)
    end

    def bottom
      @cursor_y = @lines.size - 1
      mark_dirty(:cursor)
    end

    def left
      if 0 < @cursor_x && 0 < current_line.length
        tail if current_line.length < @cursor_x
        @cursor_x -= 1
        mark_dirty(:cursor)
      elsif 0 < @cursor_y
        up
        tail
      end
    end

    def right
      if @cursor_x < current_line.length
        @cursor_x += 1
        mark_dirty(:cursor)
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
        mark_dirty(:cursor)
      end
    end

    def down
      if @cursor_y + 1 < @lines.length
        @cursor_y += 1
        @prev_c = :DOWN
        mark_dirty(:cursor)
      end
    end

    def put(c)
      line = current_line
      tail if current_line.length < @cursor_x
      if c.is_a?(String)
        @changed = true
        line = line[0, @cursor_x].to_s + c + line[@cursor_x, 65535].to_s
        @lines[@cursor_y] = line
        mark_dirty(:content)
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
          mark_dirty(:structure)
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
            mark_dirty(:content)
          else
            if 0 < @cursor_y
              @cursor_x = @lines[@cursor_y - 1].length
              @lines[@cursor_y - 1] += current_line
              @lines.delete_at @cursor_y
              mark_dirty(:structure)
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

    def delete_line
      @lines.delete_at @cursor_y
    end

    def insert_line(line)
      return unless line
      @lines.insert @cursor_y, line
    end

    def replace_char(ch)
      line = current_line
      return if @cursor_x >= line.length
      @lines[@cursor_y] = line[0, @cursor_x].to_s + ch + line[@cursor_x + 1, 65535].to_s
      @changed = true
      mark_dirty(:content)
    end

    def word_char?(ch)
      return false unless ch
      c = ch.ord
      (c >= 48 && c <= 57) ||   # 0-9
      (c >= 65 && c <= 90) ||   # A-Z
      (c >= 97 && c <= 122) ||  # a-z
      c == 95                   # _
    end

    def word_forward
      line = current_line
      len = line.length
      if @cursor_x >= len
        if @cursor_y + 1 < @lines.length
          down
          head
        end
        return
      end
      x = @cursor_x
      if word_char?(line[x])
        x += 1 while x < len && word_char?(line[x])
      elsif line[x] != " "
        x += 1 while x < len && !word_char?(line[x]) && line[x] != " "
      end
      x += 1 while x < len && line[x] == " "
      if x >= len && @cursor_y + 1 < @lines.length
        down
        head
      else
        @cursor_x = x < len ? x : [len - 1, 0].max
        mark_dirty(:cursor)
      end
    end

    def word_backward
      if @cursor_x == 0
        if 0 < @cursor_y
          up
          tail
          @cursor_x = [current_line.length - 1, 0].max
          mark_dirty(:cursor)
        end
        return
      end
      line = current_line
      x = @cursor_x - 1
      x -= 1 while x > 0 && line[x] == " "
      if word_char?(line[x])
        x -= 1 while x > 0 && word_char?(line[x - 1])
      elsif x > 0
        x -= 1 while x > 0 && !word_char?(line[x - 1]) && line[x - 1] != " "
      end
      @cursor_x = x
      mark_dirty(:cursor)
    end

    def word_end
      line = current_line
      len = line.length
      if @cursor_x >= len - 1
        if @cursor_y + 1 < @lines.length
          down
          line = current_line
          len = line.length
          @cursor_x = 0
        else
          return
        end
      else
        @cursor_x += 1
      end
      x = @cursor_x
      x += 1 while x < len && line[x] == " "
      if word_char?(line[x])
        x += 1 while x < len && word_char?(line[x])
      else
        x += 1 while x < len && !word_char?(line[x]) && line[x] != " "
      end
      @cursor_x = x > 0 ? x - 1 : 0
      mark_dirty(:cursor)
    end

    def move_to(x, y)
      @cursor_x = x
      @cursor_y = y
      mark_dirty(:cursor)
    end

    def start_selection(mode)
      @selection_mode = mode
      @selection_start_x = @cursor_x
      @selection_start_y = @cursor_y
    end

    def clear_selection
      @selection_mode = nil
      @selection_start_x = nil
      @selection_start_y = nil
    end

    def has_selection?
      !@selection_mode.nil?
    end

    def selection_range
      return nil unless has_selection?
      sy = @selection_start_y.to_i
      sx = @selection_start_x.to_i
      ey = @cursor_y
      ex = @cursor_x
      if sy > ey || (sy == ey && sx > ex)
        [ey, ex, sy, sx]
      else
        [sy, sx, ey, ex]
      end
    end

    def selected_text
      range = selection_range
      return nil unless range
      sy, sx, ey, ex = range
      if @selection_mode == :line
        result = ""
        i = sy
        while i <= ey
          result << "\n" unless i == sy
          result << @lines[i].to_s
          i += 1
        end
        result
      elsif sy == ey
        @lines[sy][sx, ex - sx + 1].to_s
      else
        result = @lines[sy][sx, 65535].to_s
        i = sy + 1
        while i < ey
          result << "\n" << @lines[i].to_s
          i += 1
        end
        result << "\n" << @lines[ey][0, ex + 1].to_s
        result
      end
    end

    def delete_selected_text
      text = selected_text
      range = selection_range
      return nil unless range && text
      sy, sx, ey, ex = range
      if @selection_mode == :line
        (ey - sy + 1).times { @lines.delete_at(sy) }
        @lines << "" if @lines.empty?
        @cursor_y = sy
        @cursor_y = @lines.length - 1 if @cursor_y >= @lines.length
        @cursor_x = 0
        mark_dirty(:structure)
      elsif sy == ey
        @lines[sy] = @lines[sy][0, sx].to_s + @lines[sy][ex + 1, 65535].to_s
        @cursor_y = sy
        @cursor_x = sx
        if @lines[sy].length > 0 && @cursor_x >= @lines[sy].length
          @cursor_x = @lines[sy].length - 1
        end
        @cursor_x = 0 if @cursor_x < 0
        mark_dirty(:content)
      else
        after = @lines[ey][ex + 1, 65535].to_s
        @lines[sy] = @lines[sy][0, sx].to_s + after
        (ey - sy).times { @lines.delete_at(sy + 1) }
        @cursor_y = sy
        @cursor_x = sx
        if @lines[sy].length > 0 && @cursor_x >= @lines[sy].length
          @cursor_x = @lines[sy].length - 1
        end
        @cursor_x = 0 if @cursor_x < 0
        mark_dirty(:structure)
      end
      @changed = true
      text
    end

    def insert_lines_below(lines_to_insert)
      return unless lines_to_insert
      insert_at = @cursor_y + 1
      lines_to_insert.each_with_index do |line, i|
        @lines.insert(insert_at + i, line)
      end
      @cursor_y = insert_at
      @cursor_x = 0
      mark_dirty(:structure)
      @changed = true
    end

    def insert_string_after_cursor(str)
      return unless str
      pos = @cursor_x + 1
      len = current_line.length
      pos = len if pos > len
      parts = str.split("\n")
      if parts.length <= 1
        s = parts[0].to_s
        line = current_line
        @lines[@cursor_y] = line[0, pos].to_s + s + line[pos, 65535].to_s
        @cursor_x = pos + s.length - 1
        @cursor_x = 0 if @cursor_x < 0
        mark_dirty(:content)
      else
        line = current_line
        after = line[pos, 65535].to_s
        @lines[@cursor_y] = line[0, pos].to_s + parts[0].to_s
        i = 1
        while i < parts.length
          @lines.insert(@cursor_y + i, parts[i].to_s)
          i += 1
        end
        last_idx = @cursor_y + parts.length - 1
        @lines[last_idx] = @lines[last_idx].to_s + after
        @cursor_y = last_idx
        last_part = parts[-1].to_s
        @cursor_x = last_part.length > 0 ? last_part.length - 1 : 0
        mark_dirty(:structure)
      end
      @changed = true
    end

    def current_tail(n = 1)
      current_line[@cursor_x - n, 65535].to_s
    end

  end

end

