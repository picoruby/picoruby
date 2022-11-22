# Learn "Terminal modes"
# https://www.ibm.com/docs/en/linux-on-systems?topic=wysk-terminal-modes

case RUBY_ENGINE
when "ruby", "jruby"
  require "io/console"
  require_relative "./buffer.rb"

  def IO.getch
    STDIN.getch
  end
  def IO.get_nonblock(max)
    STDIN.noecho{ |input| input.read_nonblock(max) }
  rescue IO::EAGAINWaitReadable => e
    nil
  end
  def IO.get_cursor_position
    res = ""
    STDIN.raw do |stdin|
      STDOUT << "\e[6n"
      STDOUT.flush
      while (c = stdin.getc) != 'R'
        res << c if c
      end
    end
    STDIN.iflush
    _size = res.split(";")
    return [_size[0][2, 3].to_i, _size[1].to_i]
  end
when "mruby/c"
  require "io"
  require "filesystem-fat"
  require "vfs"
  class IO
    def get_nonblock(max)
      str = read_nonblock(max)
      str&.length == 0 ? nil : str
    end
  end
else
  raise RuntimeError.new("Unknown RUBY_ENGINE")
end

class Terminal

  def self.get_screen_size
    y, x = IO.get_cursor_position # save current position
    print "\e[999B\e[999C" # down * 999 and right * 999
    res = IO.get_cursor_position
    print "\e[#{y};#{x}H" # restore original position
    res
  end

  class Base

    def initialize
      @height, @width = Terminal.get_screen_size
      @buffer = Terminal::Buffer.new
    end

    attr_reader :width, :height
    attr_accessor :debug_tty

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
      @buffer.lines.each do |line|
        count += 1 + (@prompt_margin + line.length) / @width
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
      @history_index = @history.count - 1
    end

    def save_history
      if @history[-2] != @buffer.lines
        @history[@history.size - 1] = @buffer.lines
        @history << [""]
      end
      if MAX_HISTORY_COUNT < @history.count
        @history.shift
      end
      history_head
    end

    def load_history(dir)
      if dir == :up
        return if @history_index == 0
        @history_index -= 1
      else # :down
        return if @history_index == @history.count - 1
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
      _buffer_lines.each_with_index do |line, i|
        puts if 0 < i
        print @prompt,
          (i == 0 ? "> " : "* "),
          line,
          "\e[0K",
          # if the last letter is on the right most of the window,
          # move cursor to the next line's head
          ((_prompt_margin + line.length) % _width == 0 ? "\e[1E" : "")
      end

      # Delete all after cursor &&
      # move the cursor where supposed to be
      print "\e[0J\e[#{_line_count}F"

      @prev_cursor_y = -1
      _buffer_lines.each_with_index do |line, i|
        break if i == @buffer.cursor_y
        a = (_prompt_margin + line.length) / _width + 1
        print "\e[#{a}B"
        @prev_cursor_y += a
      end

      # Show cursor
      b = (_prompt_margin + _buffer_cursor_x) / _width + 1
      c = (_prompt_margin + _buffer_cursor_x) % _width
      print 0 < c ? "\e[#{b}B\e[#{c}C\e[?25h" : "\e[#{b}B\e[?25h"

      @prev_cursor_y += b
    end

    def start
      while true
        refresh
        case c = IO.getch.ord
        when 1 # Ctrl-A
          @buffer.head
        when 3 # Ctrl-C
          @buffer.bottom
          @buffer.tail
          puts "", "^C\e[0J"
          @prev_cursor_y = 0
          @buffer.clear
          history_head
        when 4 # Ctrl-D logout
          puts
          return
        when 5 # Ctrl-E
          @buffer.tail
        when 9
          @buffer.put :TAB
        when 12 # Ctrl-L
          @height, @width = Terminal.get_screen_size
          refresh
        when 26 # Ctrl-Z
          puts
          print "shunt" # Shunt into the background
          return
        when 27 # ESC
          case IO.read_nonblock(2)
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
            debug c
          end
        when 8, 127 # 127 on UNIX
          @buffer.put :BSPACE
        when 32..126
          @buffer.put c.chr
        else
          yield self, @buffer, c
        end
      end
    end

  end

  class Editor < Base
    def initialize
      @content_margin_height = 5
      @footer_height = 0
      @visual_offset = 0
      @visual_cursor_x = 0
      @visual_cursor_y = 0
      @quit_by_ctrl_c = true
      super
    end

    attr_accessor :footer_height, :quit_by_ctrl_c

    def load_file_into_buffer(filepath)
      if File.exist?(filepath)
        @buffer.lines.clear
        File.open(filepath, 'r') do |f|
          f.each_line do |line|
            @buffer.lines << line.chomp
          end
        end
        return true
      else
        return false
      end
    end

    def save_file_from_buffer(filepath)
      File.open(filepath, "w") do |f|
        @buffer.lines.each do |line|
          f.puts line
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
      first_lineno = nil
      first_line_skip_count = 0
      # Show the content
      @buffer.lines.each_with_index do |line, lineno|
        [1, ((line.length + content_width - 1) / content_width)].max.times do |i|
          if visual_offset < 0
            visual_offset += 1
          else
            unless first_lineno
              first_lineno = lineno
              first_line_skip_count = i
            end
            if 0 < i
              print "    "
            else
              print "#{lineno + 1} ".rjust(4)
            end
            print line[i * content_width, content_width]
            content_height -= 1
            break if content_height == 0
            next_head
          end
        end
        break if content_height == 0
      end
      # Adjust if cursor is close to the end of file
      if 0 < content_height && @visual_offset < 0
        print "\e[#{content_height}T" # Scroll up
        @visual_offset += content_height
        # Fill the blank made by the scroll
        blank_lines = []
        ((first_line_skip_count - content_height)..first_lineno).each do |lineno|
          line = @buffer.lines[lineno]
          [1, (line.length - 1) / content_width + 1].max.times do |i|
            break if lineno == first_lineno && first_line_skip_count - 1 < i
            str = if i == 0
              "\e[31m" + "#{lineno + 1} ".rjust(4)
            else
              "\e[31m    "
            end
            str << line[i * content_width, content_width]
            str << "\e[0m"
            blank_lines.unshift str
          end
        end
        blank_lines.each do |line|
          print "\e[#{content_height};1H#{line}"
          content_height -= 1
          break if content_height < 1
        end
      end
      print "\e[#{@height - @footer_height + 1};1H"
      @footer_proc&.call(self)
      @cursor_proc&.call(self)
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

    def calculate_visual_cursor
      content_width = @width - 4
      y = 0
      cursor_x = @buffer.cursor_x
      cursor_y = @buffer.cursor_y
      @buffer.lines.each_with_index do |line, i|
        break if i == cursor_y
        y += [1, (line.length + content_width - 1) / content_width].max
      end
      @visual_cursor_y = y + cursor_x / content_width + @visual_offset
      @visual_cursor_x = cursor_x % content_width
      if @visual_cursor_x == 0 && 0 < cursor_x && @buffer.current_line.length == cursor_x
        @visual_cursor_x = @width
        @visual_cursor_y -= 1
      end
    end

    def start
      print "\e[m"
      while true
        refresh
        case c = IO.getch.ord
        when 3 # Ctrl-C
          return if @quit_by_ctrl_c
        when 4 # Ctrl-D logout
          return
        when 12 # Ctrl-L
          # FIXME: in case that cursor has to relocate
          @height, @width = Terminal.get_screen_size
        else
          begin
            yield self, @buffer, c
          rescue => e
            return if e.message == "__quit()"
          end
        end
      end
    end
  end
end

