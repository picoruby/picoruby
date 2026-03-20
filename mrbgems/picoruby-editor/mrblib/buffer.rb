if RUBY_ENGINE == "mruby/c"
  class Array
    def insert(index, *vals)
      # @type var index: Integer
      index_int = index.to_i
      if index_int < 0
        raise ArgumentError, "Negative index doesn't work"
      end
      tail = self[index_int, self.length]
      i = 0
      while i < vals.size
        self[index_int + i] = vals[i]
        i += 1
      end
      if tail
        tail_at = index_int + vals.size
        ti = 0
        while ti < tail.size
          self[tail_at] = tail[ti]
          tail_at += 1
          ti += 1
        end
      end
      self
    end
  end
end

module Editor

  # Returns byte length of a UTF-8 character from its lead byte
  def self.utf8_byte_length(lead_byte)
    return 0 unless lead_byte
    if lead_byte < 0x80
      1
    elsif lead_byte < 0xE0
      2
    elsif lead_byte < 0xF0
      3
    else
      4
    end
  end

  # Returns byte length of the character at byte_pos in str
  def self.char_bytesize_at(str, byte_pos)
    bs = str.bytesize
    return 0 if byte_pos >= bs || byte_pos < 0
    return 0 unless byte = str.getbyte(byte_pos)
    utf8_byte_length(byte)
  end

  # Returns the start byte position of the character before byte_pos
  def self.prev_char_byte_pos(str, byte_pos)
    return 0 if byte_pos <= 0
    pos = byte_pos - 1
    while pos > 0 && (str.getbyte(pos) & 0xC0) == 0x80
      pos -= 1
    end
    pos
  end

  # Returns the full UTF-8 character at byte_pos
  def self.char_at_bytepos(str, byte_pos)
    len = char_bytesize_at(str, byte_pos)
    return nil if len == 0
    str.byteslice(byte_pos, len)
  end

  # Returns display width of a single character (1 for ASCII, 2 for multibyte)
  def self.char_display_width(ch)
    return 0 unless ch
    if ch.bytesize > 1
      2
    else
      1
    end
  end

  # Returns total display width of a string
  def self.display_width(str)
    width = 0
    pos = 0
    bs = str.bytesize
    while pos < bs
      clen = utf8_byte_length(str.getbyte(pos))
      width += clen > 1 ? 2 : 1
      pos += clen
    end
    width
  end

  # Converts byte offset to display column
  def self.byte_to_display_col(str, byte_pos)
    col = 0
    pos = 0
    bs = str.bytesize
    while pos < byte_pos && pos < bs
      clen = utf8_byte_length(str.getbyte(pos))
      col += clen > 1 ? 2 : 1
      pos += clen
    end
    col
  end

  # Converts display column to byte offset
  def self.display_col_to_byte(str, col)
    pos = 0
    c = 0
    bs = str.bytesize
    while c < col && pos < bs
      clen = utf8_byte_length(str.getbyte(pos))
      c += clen > 1 ? 2 : 1
      pos += clen
    end
    pos
  end

  # Slices a string by display columns (for line wrapping)
  def self.display_slice(str, start_col, max_width)
    byte_start = display_col_to_byte(str, start_col)
    result = ""
    pos = byte_start
    width = 0
    bs = str.bytesize
    while pos < bs
      clen = utf8_byte_length(str.getbyte(pos))
      cw = clen > 1 ? 2 : 1
      break if width + cw > max_width
      result << (str.byteslice(pos, clen) || "")
      width += cw
      pos += clen
    end
    result
  end

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
      Editor.char_at_bytepos(current_line, @cursor_x)
    end

    def clear
      @lines = [""]
      clear_selection
      home
    end

    def empty?
      @lines.length == 1 && @lines[0].bytesize == 0
    end

    def dump
      result = [] #: Array[String]
      li = 0
      while li < @lines.size
        line = @lines[li]
        if line.bytesize > 0 && line.getbyte(line.bytesize - 1) == 0x5C # '\\'
          result << (line.byteslice(0, line.bytesize - 1) or raise)
        else
          result << line
        end
        li += 1
      end
      result.join("\n")
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
      @cursor_x = current_line.bytesize
      mark_dirty(:cursor)
    end

    def bottom
      @cursor_y = @lines.size - 1
      mark_dirty(:cursor)
    end

    def left
      if 0 < @cursor_x && 0 < current_line.bytesize
        tail if current_line.bytesize < @cursor_x
        @cursor_x = Editor.prev_char_byte_pos(current_line, @cursor_x)
        mark_dirty(:cursor)
      elsif 0 < @cursor_y
        up
        tail
      end
    end

    def right
      if @cursor_x < current_line.bytesize
        @cursor_x += Editor.char_bytesize_at(current_line, @cursor_x)
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
      tail if current_line.bytesize < @cursor_x
      if c.is_a?(String)
        @changed = true
        line = line.byteslice(0, @cursor_x).to_s + c + line.byteslice(@cursor_x, 65535).to_s
        @lines[@cursor_y] = line
        @cursor_x += c.bytesize
        mark_dirty(:content)
      else
        case c
        when :TAB
          @changed = true
          put " "
          put " "
        when :ENTER
          @changed = true
          new_line = line.byteslice(@cursor_x, 65535)
          @lines[@cursor_y] = line.byteslice(0, @cursor_x).to_s
          @lines.insert(@cursor_y + 1, new_line) if new_line
          mark_dirty(:structure)
          head
          down
        when :BSPACE
          @changed = true
          if 0 < @cursor_x
            prev_pos = Editor.prev_char_byte_pos(current_line, @cursor_x)
            clen = @cursor_x - prev_pos
            l = @lines[@cursor_y]
            @lines[@cursor_y] = l.byteslice(0, prev_pos).to_s + l.byteslice(@cursor_x, 65535).to_s
            @cursor_x = prev_pos
            mark_dirty(:content)
          else
            if 0 < @cursor_y
              @cursor_x = @lines[@cursor_y - 1].bytesize
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
      clen = Editor.char_bytesize_at(@lines[@cursor_y], @cursor_x)
      return if clen == 0
      l = @lines[@cursor_y]
      @lines[@cursor_y] = l.byteslice(0, @cursor_x).to_s + l.byteslice(@cursor_x + clen, 65535).to_s
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
      return if @cursor_x >= line.bytesize
      clen = Editor.char_bytesize_at(line, @cursor_x)
      @lines[@cursor_y] = line.byteslice(0, @cursor_x).to_s + ch + line.byteslice(@cursor_x + clen, 65535).to_s
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
      len = line.bytesize
      if @cursor_x >= len
        if @cursor_y + 1 < @lines.length
          down
          head
        end
        return
      end
      x = @cursor_x
      ch = Editor.char_at_bytepos(line, x)
      if word_char?(ch)
        while x < len
          ch = Editor.char_at_bytepos(line, x)
          break unless word_char?(ch)
          x += Editor.char_bytesize_at(line, x)
        end
      elsif ch != " "
        while x < len
          ch = Editor.char_at_bytepos(line, x)
          break if word_char?(ch) || ch == " "
          x += Editor.char_bytesize_at(line, x)
        end
      end
      while x < len
        ch = Editor.char_at_bytepos(line, x)
        break unless ch == " "
        x += Editor.char_bytesize_at(line, x)
      end
      if x >= len && @cursor_y + 1 < @lines.length
        down
        head
      else
        if x < len
          @cursor_x = x
        else
          # Move to last character
          @cursor_x = len > 0 ? Editor.prev_char_byte_pos(line, len) : 0
        end
        mark_dirty(:cursor)
      end
    end

    def word_backward
      if @cursor_x == 0
        if 0 < @cursor_y
          up
          tail
          line = current_line
          @cursor_x = line.bytesize > 0 ? Editor.prev_char_byte_pos(line, line.bytesize) : 0
          mark_dirty(:cursor)
        end
        return
      end
      line = current_line
      x = Editor.prev_char_byte_pos(line, @cursor_x)
      ch = Editor.char_at_bytepos(line, x)
      while x > 0 && ch == " "
        x = Editor.prev_char_byte_pos(line, x)
        ch = Editor.char_at_bytepos(line, x)
      end
      if word_char?(ch)
        while x > 0
          prev = Editor.prev_char_byte_pos(line, x)
          pch = Editor.char_at_bytepos(line, prev)
          break unless word_char?(pch)
          x = prev
        end
      elsif x > 0
        while x > 0
          prev = Editor.prev_char_byte_pos(line, x)
          pch = Editor.char_at_bytepos(line, prev)
          break if word_char?(pch) || pch == " "
          x = prev
        end
      end
      @cursor_x = x
      mark_dirty(:cursor)
    end

    def word_end
      line = current_line
      len = line.bytesize
      last_char_pos = len > 0 ? Editor.prev_char_byte_pos(line, len) : 0
      if @cursor_x >= last_char_pos
        if @cursor_y + 1 < @lines.length
          down
          line = current_line
          len = line.bytesize
          @cursor_x = 0
        else
          return
        end
      else
        @cursor_x += Editor.char_bytesize_at(line, @cursor_x)
      end
      x = @cursor_x
      while x < len
        ch = Editor.char_at_bytepos(line, x)
        break unless ch == " "
        x += Editor.char_bytesize_at(line, x)
      end
      ch = Editor.char_at_bytepos(line, x)
      if word_char?(ch)
        while x < len
          ch = Editor.char_at_bytepos(line, x)
          break unless word_char?(ch)
          x += Editor.char_bytesize_at(line, x)
        end
      else
        while x < len
          ch = Editor.char_at_bytepos(line, x)
          break if word_char?(ch) || ch == " "
          x += Editor.char_bytesize_at(line, x)
        end
      end
      @cursor_x = x > 0 ? Editor.prev_char_byte_pos(line, x) : 0
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
        line = @lines[sy]
        line.byteslice(sx, ex - sx + 1).to_s
      else
        result = @lines[sy].byteslice(sx, 65535).to_s
        i = sy + 1
        while i < ey
          result << "\n" << @lines[i].to_s
          i += 1
        end
        result << "\n" << @lines[ey].byteslice(0, ex + 1).to_s
        result
      end
    end

    def delete_selected_text
      text = selected_text
      range = selection_range
      return nil unless range && text
      sy, sx, ey, ex = range
      if @selection_mode == :line
        i = 0
        while i < (ey - sy + 1)
          @lines.delete_at(sy)
          i += 1
        end
        @lines << "" if @lines.empty?
        @cursor_y = sy
        @cursor_y = @lines.length - 1 if @cursor_y >= @lines.length
        @cursor_x = 0
        mark_dirty(:structure)
      elsif sy == ey
        line = @lines[sy]
        @lines[sy] = line.byteslice(0, sx).to_s + line.byteslice(ex + 1, 65535).to_s
        @cursor_y = sy
        @cursor_x = sx
        bs = @lines[sy].bytesize
        if bs > 0 && @cursor_x >= bs
          @cursor_x = Editor.prev_char_byte_pos(@lines[sy], bs)
        end
        @cursor_x = 0 if @cursor_x < 0
        mark_dirty(:content)
      else
        after = @lines[ey].byteslice(ex + 1, 65535).to_s
        @lines[sy] = @lines[sy].byteslice(0, sx).to_s + after
        j = 0
        while j < (ey - sy)
          @lines.delete_at(sy + 1)
          j += 1
        end
        @cursor_y = sy
        @cursor_x = sx
        bs = @lines[sy].bytesize
        if bs > 0 && @cursor_x >= bs
          @cursor_x = Editor.prev_char_byte_pos(@lines[sy], bs)
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
      i = 0
      while i < lines_to_insert.size
        @lines.insert(insert_at + i, lines_to_insert[i])
        i += 1
      end
      @cursor_y = insert_at
      @cursor_x = 0
      mark_dirty(:structure)
      @changed = true
    end

    def insert_string_after_cursor(str)
      return unless str
      pos = @cursor_x + Editor.char_bytesize_at(current_line, @cursor_x)
      len = current_line.bytesize
      pos = len if pos > len
      parts = str.split("\n")
      if parts.length <= 1
        s = parts[0].to_s
        line = current_line
        @lines[@cursor_y] = line.byteslice(0, pos).to_s + s + line.byteslice(pos, 65535).to_s
        end_pos = pos + s.bytesize
        @cursor_x = end_pos > 0 ? Editor.prev_char_byte_pos(@lines[@cursor_y], end_pos) : 0
        @cursor_x = 0 if @cursor_x < 0
        mark_dirty(:content)
      else
        line = current_line
        after = line.byteslice(pos, 65535).to_s
        @lines[@cursor_y] = line.byteslice(0, pos).to_s + parts[0].to_s
        i = 1
        while i < parts.length
          @lines.insert(@cursor_y + i, parts[i].to_s)
          i += 1
        end
        last_idx = @cursor_y + parts.length - 1
        @lines[last_idx] = @lines[last_idx].to_s + after
        @cursor_y = last_idx
        last_part = parts[-1].to_s
        @cursor_x = last_part.bytesize > 0 ? Editor.prev_char_byte_pos(@lines[last_idx], last_part.bytesize) : 0
        mark_dirty(:structure)
      end
      @changed = true
    end

    def current_tail(n = 1)
      pos = @cursor_x
      k = 0
      while k < n
        pos = Editor.prev_char_byte_pos(current_line, pos)
        k += 1
      end
      current_line.byteslice(pos, 65535).to_s
    end

  end

end

