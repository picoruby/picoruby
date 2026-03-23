# Learn "Terminal modes"
# https://www.ibm.com/docs/en/linux-on-systems?topic=wysk-terminal-modes

require "env"
require "io/console"

case RUBY_ENGINE
when "ruby", "jruby"
  require_relative "./buffer.rb"

  def IO.get_cursor_position
    res = ""
    STDOUT << "\e[6n"
    STDOUT.flush
    while (c = STDIN.read_nonblock(1)) != 'R'
      res << c if c
    end
    STDIN.iflush
    _size = res.split(";")
    return [_size[0][2, 3].to_i, _size[1].to_i]
  end
when "mruby/c", "mruby"
  begin
    require "littlefs"
    require "vfs"
  rescue LoadError
  end
else
  raise RuntimeError.new("Unknown RUBY_ENGINE: #{RUBY_ENGINE}")
end

module Editor

  def self.get_screen_size
    if ENV && ENV['TERM'] == "dumb"
      IO.wait_terminal(timeout: 0.1)
      return [24, 80] if ENV['TERM'] == "dumb"
    end
    y, x = IO.get_cursor_position # save current position
    print "\e[999B\e[999C" # down * 999 and right * 999
    res = IO.get_cursor_position
    print "\e[#{y};#{x}H" # restore original position
    res
  end

  class Base

    def initialize
      @height, @width = Editor.get_screen_size
      @buffer = Editor::Buffer.new
    end

    attr_reader :width, :height
    attr_accessor :debug_tty

    def raw_takeover
      @raw_takeover = true
    end

    def put_buffer(chr)
      @buffer.put chr
    end

    def dump_buffer
      @buffer.dump.chomp
    end

    def clear_buffer
      @buffer.clear
    end

    def clear
      print "\e[2J"
    end

    def home
      print "\e[1;1H"
    end

    def next_head
      print "\e[1E"
    end

    def physical_line_count
      count = 0
      return count if @width == 0
      li = 0
      while li < @buffer.lines.size
        count += 1 + (@prompt_margin + Editor.display_width(@buffer.lines[li])) / @width
        li += 1
      end
      count
    end

    def debug(text)
      if @debug_tty
        system "echo '#{text}' > /dev/pts/#{@debug_tty}"
      end
    end

  end

  class Line < Base
    def initialize
      super
      @history = [[""]]
      @history_index = 0
      @prev_cursor_y = 0
      self.prompt = "$"
    end

    MAX_HISTORY_COUNT = 10

    def prompt=(word)
      @prompt = word
      @prompt_margin = 2 + @prompt.length
    end

    def history_head
      @history_index = @history.size - 1
    end

    def save_history
      if @history[-2] != @buffer.lines
        @history[@history.size - 1] = @buffer.lines
        @history << [""]
      end
      if MAX_HISTORY_COUNT < @history.size
        @history.shift
      end
      history_head
    end

    def load_history(dir)
      if dir == :up
        return if @history_index == 0
        @history_index -= 1
      else # :down
        return if @history_index == @history.size - 1
        @history_index += 1
      end
      unless @history.empty?
        @buffer.lines = @history[@history_index]
        @buffer.bottom
        @buffer.tail
      end
      refresh
    end

    def feed_at_bottom
      adjust = physical_line_count - @prev_cursor_y - 1
      print "\e[#{adjust}B" if 0 < adjust
      puts "\e[999C" # right * 999
      @prev_cursor_y = 0
    end

    def refresh
      # Cache values on local registers for performance
      _line_count = physical_line_count
      _buffer_lines = @buffer.lines
      _buffer_cursor_x = @buffer.cursor_x
      _buffer_cursor_display_x = Editor.byte_to_display_col(@buffer.current_line, _buffer_cursor_x)
      _prompt_margin = @prompt_margin
      _width = @width

      # Hide cursor &&
      # Move cursor to the top of the snippet
      if 0 < @prev_cursor_y
        print "\e[?25l\e[#{@prev_cursor_y}F"
      else
        print "\e[?25l\e[1G"
      end

      # Scroll screen if necessary
      if 0 < scroll = (_line_count - (@height - IO.get_cursor_position[0]) - 1)
        print "\e[#{scroll}S\e[#{scroll}A"
      end

      # Write the buffer content
      i = 0
      while i < _buffer_lines.size
        line = _buffer_lines[i]
        puts if 0 < i
        _dw = Editor.display_width(line)
        print @prompt,
          (i == 0 ? "> " : "* "),
          line,
          "\e[0K",
          # if the last letter is on the right most of the window,
          # move cursor to the next line's head
          ((_prompt_margin + _dw) % _width == 0 ? "\e[1E" : "")
        i += 1
      end

      # Delete all after cursor &&
      # move the cursor where supposed to be
      print "\e[0J\e[#{_line_count}F"

      @prev_cursor_y = -1
      i = 0
      while i < _buffer_lines.size
        break if i == @buffer.cursor_y
        a = (_prompt_margin + Editor.display_width(_buffer_lines[i])) / _width + 1
        print "\e[#{a}B"
        @prev_cursor_y += a
        i += 1
      end

      # Show cursor
      b = (_prompt_margin + _buffer_cursor_display_x) / _width + 1
      c = (_prompt_margin + _buffer_cursor_display_x) % _width
      print 0 < c ? "\e[#{b}B\e[#{c}C\e[?25h" : "\e[#{b}B\e[?25h"

      @prev_cursor_y += b
    end

    def start
      refresh
      while true
        begin
          line = STDIN.read_nonblock(255) # This size affects pasting text
        rescue Interrupt
          @buffer.bottom
          @buffer.tail
          puts "\n^C\e[0J"
          @prev_cursor_y = 0
          @buffer.clear
          history_head
          refresh
        rescue SignalException
          line = "\x1a" # Ctrl-Z
        end
        unless line
          sleep_ms 1
          next
        end
        while true
          ch = line[0]
          break unless ch
          c = ch.getbyte(0) || 0
          line[0] = ''
          case c
          when 1 # Ctrl-A
            @buffer.head
          when 5 # Ctrl-E
            @buffer.tail
          when 9
            @buffer.put :TAB
          when 12 # Ctrl-L
            @height, @width = Editor.get_screen_size
            refresh
          when 27 # ESC
            rest = line[0, 2]
            line[0, 2] = ''
            case rest
            when "[A"
              if @prev_cursor_y == 0
                load_history :up
              else
                @buffer.put :UP
              end
            when "[B"
              if physical_line_count == @prev_cursor_y + 1
                load_history :down
              else
                @buffer.put :DOWN
              end
            when "[C"
              @buffer.put :RIGHT
            when "[D"
              @buffer.put :LEFT
            else
              line = rest.to_s + line
            end
          when 8, 127 # 127 on UNIX
            @buffer.put :BSPACE
          else
            if c >= 32
              @buffer.put ch
            else
              @raw_takeover = false
              yield self, @buffer, c
              if @raw_takeover
                line = ''
                refresh
                break
              end
            end
          end
          refresh
        end
      end
    end

  end

  class Screen < Base
    def initialize
      @content_margin_height = 5
      @footer_height = 0
      @visual_offset = 0
      @visual_cursor_x = 0
      @visual_cursor_y = 0
      @quit_by_sigint = true
      @redraw_mode = nil
      @cursor_line_wraps = 0
      super
    end

    attr_accessor :footer_height, :quit_by_sigint, :redraw_mode

    def load_file_into_buffer(filepath)
      if File.file?(filepath)
        @buffer.lines.clear
        File.open(filepath, 'r') do |f|
          content = f.read
          file_lines = content.split("\n")
          fli = 0
          while fli < file_lines.size
            @buffer.lines << file_lines[fli]
            fli += 1
          end
        end
        return true
      elsif File.directory?(filepath)
        raise "Is a directory: #{filepath}"
      else
        return false
      end
    end

    def save_file_from_buffer(filepath)
      File.open(filepath, "w") do |f|
        sli = 0
        while sli < @buffer.lines.size
          f.puts @buffer.lines[sli]
          sli += 1
        end
      end
      @buffer.changed = false
      return "File saved: #{filepath}"
    rescue => e
      return "Failed to save: #{e.message}"
    end

    def refresh
      content_height = @height - @footer_height
      content_width = @width - 4
      calculate_visual_cursor
      if (offset = @visual_cursor_y - @content_margin_height) < 0
        # Cursor is upper than margin top
        @visual_offset -= offset
        @visual_offset = 0 if 0 < @visual_offset # Adjustment
        calculate_visual_cursor
      elsif (offset = content_height - @content_margin_height - @visual_cursor_y - 1) < 0
        # Cursor is lower than margin bottom (Adjustment scroll will run later)
        @visual_offset += offset
        calculate_visual_cursor
      end
      visual_offset = @visual_offset
      clear
      home
      first_lineno = -1
      first_line_skip_count = 0
      # Show the content
      lineno = 0
      while lineno < @buffer.lines.size
        line = @buffer.lines[lineno]
        dw = Editor.display_width(line)
        max_i = [1, ((dw + content_width - 1) / content_width)].max || 0
        i = 0
        while i < max_i
          if visual_offset < 0
            visual_offset += 1
          else
            if first_lineno < 0
              first_lineno = lineno
              first_line_skip_count = i
            end
            if 0 < i
              print "    "
            else
              print "#{lineno + 1} ".rjust(4)
            end
            print highlighted_segment(line, lineno, i * content_width, content_width)
            content_height -= 1
            break 0 if content_height == 0
            next_head
          end
          i += 1
        end
        break if content_height == 0
        lineno += 1
      end
      # Adjust if cursor is close to the end of file
      if 0 < content_height && @visual_offset < 0
        print "\e[#{content_height}T" # Scroll up
        @visual_offset += content_height
        # Fill the blank made by the scroll
        blank_lines = [] #: Array[String]
        bln = first_line_skip_count - content_height
        while bln <= first_lineno
          bln_i = bln.to_i
          bline = @buffer.lines[bln_i]
          # @type var dw: Integer
          dw = Editor.display_width(bline)
          max_i = ([1, (dw - 1) / content_width + 1].max || 0)
          # @type var i: Integer
          # @type var max_i: Integer
          i = 0
          while i < max_i
            break 0 if bln_i == first_lineno && i > first_line_skip_count - 1
            str = if i == 0
              "\e[31m" + "#{bln_i + 1} ".rjust(4)
            else
              "\e[31m    "
            end
            str << Editor.display_slice(bline, i * content_width, content_width)
            str << "\e[0m"
            blank_lines.unshift str
            i += 1
          end
          bln += 1
        end
        bli = 0
        while bli < blank_lines.size
          print "\e[#{content_height};1H#{blank_lines[bli]}"
          content_height -= 1
          break if content_height < 1
          bli += 1
        end
      end
      print "\e[#{@height - @footer_height + 1};1H"
      @footer_proc&.call(self)
      @cursor_proc&.call(self)
      update_cursor_line_wraps
    end

    def refresh_cursor(&block)
      @cursor_proc = block
    end

    def refresh_footer(&block)
      @footer_proc = block
    end

    def show_cursor
      print "\e[#{@visual_cursor_y + 1};#{@visual_cursor_x + 5}H"
    end

    def dirty_to_mode(dirty)
      case dirty
      when :cursor    then :cursor
      when :content   then :current_line
      when :structure then :all
      else :all
      end
    end

    def smart_refresh
      mode = @redraw_mode || dirty_to_mode(@buffer.dirty)
      @redraw_mode = nil
      @buffer.clear_dirty

      case mode
      when :cursor
        refresh_cursor_and_footer
      when :footer
        refresh_footer_only
      when :current_line
        refresh_current_line
      else
        refresh
      end
    end

    def refresh_cursor_and_footer
      old_visual_offset = @visual_offset
      calculate_visual_cursor
      content_height = @height - @footer_height
      if (offset = @visual_cursor_y - @content_margin_height) < 0
        @visual_offset -= offset
        @visual_offset = 0 if 0 < @visual_offset
      elsif (offset = content_height - @content_margin_height - @visual_cursor_y - 1) < 0
        @visual_offset += offset
      end
      if @visual_offset != old_visual_offset
        refresh
        return
      end
      calculate_visual_cursor
      print "\e[#{@height - @footer_height + 1};1H"
      print "\e[0J"
      @footer_proc&.call(self)
      @cursor_proc&.call(self)
    end

    def refresh_footer_only
      print "\e[#{@height - @footer_height + 1};1H"
      print "\e[0J"
      @footer_proc&.call(self)
      @cursor_proc&.call(self)
    end

    def refresh_current_line
      old_visual_offset = @visual_offset
      calculate_visual_cursor
      content_height = @height - @footer_height
      if (offset = @visual_cursor_y - @content_margin_height) < 0
        @visual_offset -= offset
        @visual_offset = 0 if 0 < @visual_offset
      elsif (offset = content_height - @content_margin_height - @visual_cursor_y - 1) < 0
        @visual_offset += offset
      end
      if @visual_offset != old_visual_offset
        refresh
        return
      end
      calculate_visual_cursor
      content_width = @width - 4
      line = @buffer.current_line
      dw = Editor.display_width(line)
      new_wraps = [1, ((dw + content_width - 1) / content_width)].max || 1
      if new_wraps != @cursor_line_wraps
        refresh
        return
      end
      # Calculate visual Y of current line start
      cursor_display_x = Editor.byte_to_display_col(line, @buffer.cursor_x)
      visual_y = @visual_cursor_y - cursor_display_x / content_width
      # Redraw all visual rows of the current line
      i = 0
      while i < new_wraps
        row = visual_y + i + 1
        print "\e[#{row};1H\e[2K"
        if i == 0
          print "#{@buffer.cursor_y + 1} ".rjust(4)
        else
          print "    "
        end
        print highlighted_segment(line, @buffer.cursor_y, i * content_width, content_width)
        i += 1
      end
      print "\e[#{@height - @footer_height + 1};1H"
      print "\e[0J"
      @footer_proc&.call(self)
      @cursor_proc&.call(self)
      update_cursor_line_wraps
    end

    def update_cursor_line_wraps
      content_width = @width - 4
      line = @buffer.current_line
      dw = Editor.display_width(line)
      @cursor_line_wraps = [1, ((dw + content_width - 1) / content_width)].max || 1
    end

    def calculate_visual_cursor
      content_width = @width - 4
      y = 0
      cursor_x = @buffer.cursor_x
      cursor_y = @buffer.cursor_y
      i = 0
      while i < @buffer.lines.size
        break if i == cursor_y
        dw = Editor.display_width(@buffer.lines[i])
        y += [1, (dw + content_width - 1) / content_width].max || 0
        i += 1
      end
      cursor_display_x = Editor.byte_to_display_col(@buffer.current_line, cursor_x)
      @visual_cursor_y = y + cursor_display_x / content_width + @visual_offset.to_i
      @visual_cursor_x = cursor_display_x % content_width
      if @visual_cursor_x == 0 && 0 < cursor_x && @buffer.current_line.bytesize == cursor_x
        @visual_cursor_x = @width
        @visual_cursor_y -= 1
      end
    end

    def highlighted_segment(line, lineno, start_col, max_width)
      segment = Editor.display_slice(line, start_col, max_width)
      return segment unless @buffer.has_selection?
      range = @buffer.selection_range
      return segment unless range
      sy, sx, ey, ex = range
      if @buffer.selection_mode == :line
        if lineno >= sy && lineno <= ey
          return "\e[7m" + segment + "\e[m"
        end
        return segment
      end
      return segment if lineno < sy || lineno > ey
      # Convert byte offsets to display columns for this line
      sel_start_col = (lineno == sy) ? Editor.byte_to_display_col(line, sx) : 0
      line_dw = line ? Editor.display_width(line) : 0
      # ex is byte offset of last selected byte; convert to display col of end of that char
      if lineno == ey
        clen = Editor.char_bytesize_at(line, ex)
        sel_end_col = Editor.byte_to_display_col(line, ex) + (clen > 1 ? 2 : 1) - 1
      else
        sel_end_col = line_dw - 1
      end
      seg_dw = Editor.display_width(segment)
      seg_end_col = start_col + seg_dw - 1
      sel_start_col = sel_start_col > start_col ? sel_start_col : start_col
      sel_end_col = sel_end_col < seg_end_col ? sel_end_col : seg_end_col
      return segment if sel_start_col > seg_end_col || sel_end_col < start_col
      result = ""
      before = Editor.display_slice(segment, 0, sel_start_col - start_col)
      result << before if before.bytesize > 0
      sel_width = sel_end_col - sel_start_col + 1
      mid = Editor.display_slice(segment, sel_start_col - start_col, sel_width)
      result << "\e[7m" << mid << "\e[m"
      after_col = sel_end_col - start_col + 1
      after = Editor.display_slice(segment, after_col, seg_dw - after_col)
      result << after if after.bytesize > 0
      result
    end

    def start
      print "\e[m"
      refresh
      while true
        smart_refresh
        begin
          ch = STDIN.getch
          c = ch.getbyte(0)
        rescue Interrupt
          return if @quit_by_sigint
        rescue SignalException
          c = 26 # Ctrl-Z
          ch = nil
        end
        case c
        when 12 # Ctrl-L
          # FIXME: in case that cursor has to relocate
          @height, @width = Editor.get_screen_size
        when nil
          # should not happen
        else
          begin
            yield self, @buffer, c, ch
          rescue => e
            return if e.message == "__quit()"
          end
        end
      end
    end
  end
end

