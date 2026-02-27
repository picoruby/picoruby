#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby", "jruby"
  require_relative "../../picoruby-editor/mrblib/editor"
when "mruby/c", "mruby"
  require "editor"
else
  raise "Unknown RUBY_ENGINE: #{RUBY_ENGINE}"
end

class Vim
  def initialize(filepath)
    unless filepath.to_s.empty?
      @filepath = File.expand_path filepath, Dir.getwd
    end
    @mode = :normal
    @editor = Editor::Screen.new
    @editor.quit_by_sigint = false
    @editor.footer_height = 2
    @command_buffer = Editor::Buffer.new
    @message = nil
    if @filepath
      unless @editor.load_file_into_buffer(@filepath)
        @message = "\"#{filepath}\" [New]"
      end
    end
    @editor.refresh_footer do |editor|
      print "\e[37;1m\e[48;5;239m " # foreground & background
      if @filepath
        print @filepath.to_s[0, editor.width - 1]&.ljust(editor.width - 1)
      else
        print "[No Name]".ljust(editor.width - 1)
      end
      print "\e[m\e[1E"
      if @message
        print "\e[31m", @message.to_s, "\e[m"
        @message = nil
      else
        print "\e[m", @command_buffer.lines[0]
      end
      if @mode == :command
        print "\e[#{editor.width};1H\e[#{@command_buffer.lines[0].size}C"
      end
    end
    @editor.refresh_cursor do |editor|
      unless @mode == :command
        editor.calculate_visual_cursor
        editor.show_cursor
      end
    end
    if RUBY_ENGINE == "ruby"
      @editor.debug_tty = ARGV[0]
      @editor.debug "debug start"
    end
  end

  def start
    print "\e[?1049h" # DECSET 1049
    _start
    print "\e[?1049l" # DECRST 1049
  end

  def _start
    @editor.start do |editor, buffer, c|
      case @mode
      when :normal
        if c < 112
          case c
          when  3 # Ctrl-C
            @command_buffer.lines[0] = "Type  :q  and press <Enter> to exit"
          when  13 # Enter: move to next line head
            if buffer.cursor_y + 1 < buffer.lines.length
              buffer.put :DOWN
              buffer.head
            end
          when  27 # ESC
            case STDIN.read_nonblock(2)
            when "[A" # up
              buffer.put :UP
            when "[B" # down
              buffer.put :DOWN
            when "[C" # right
              buffer.put :RIGHT
            when "[D" # left
              buffer.put :LEFT
            end
          when  18 # Ctrl-R
          when  46 # `.` redo
          when  58 # `;` command
          when  59 # `:` command
            @mode = :command
            @command_buffer.put ':'
          when 65, 73, 97, 105, 111 # A, I, a, i, o -> insert
            case c
            when 65 # A
              buffer.tail
            when 73 # I
              buffer.head
              while true
                break unless buffer.current_char == " "
                buffer.put :RIGHT
              end
            when 97 # a
              buffer.put :RIGHT
            when 111 # o
              buffer.tail
              buffer.put :ENTER
            end
            @mode = :insert
            @command_buffer.lines[0] = "Insert"
          when 106 # j down
            buffer.put :DOWN
          when 107 # k up
            buffer.put :UP
          when 108 # l right
            buffer.put :RIGHT
          when  86 # V visual line
            @mode = :visual_line
            buffer.start_selection(:line)
            @command_buffer.lines[0] = "Visual Line"
          when  98 # b word backward
            buffer.word_backward
          when 100 # d
            @mode = :cut
            @command_buffer.lines[0] = "d"
          when 101 # e word end
            buffer.word_end
          when 103 # g
            cc = STDIN.getch.ord
            if cc == 103 # gg: go to top
              buffer.home
            end
          when 104 # h left
            buffer.put :LEFT
          end
        else
          case c
          when 112 # p paste
            if @paste_board
              if @paste_type == :char
                buffer.insert_string_after_cursor(@paste_board)
              else
                paste_lines = @paste_board.split("\n") # steep:ignore
                buffer.insert_lines_below(paste_lines)
              end
            end
          when 114 # r replace
            rc = STDIN.getch
            if rc && rc.ord >= 32 && rc.ord <= 126
              buffer.replace_char(rc)
            end
          when 117 # u undo
          when 118 # v visual
            @mode = :visual
            buffer.start_selection(:char)
            @command_buffer.lines[0] = "Visual"
          when 119 # w word forward
            buffer.word_forward
          when 120 # x delete
            buffer.delete
          when 121 # y yank
            yc = STDIN.getch.ord
            if yc == 121 # yy: yank line
              @paste_board = buffer.current_line.dup
              @paste_type = :line
            end
          else
            puts c.chr
          end
        end
      when :command
        case c
        when 10, 13
          case res = exec_command(buffer)
          when :quit
            raise "__quit()"
          else
            # @type var res: String | nil
            @message = res
            clear_command_buffer
            @mode = :normal
          end
        when 27 # ESC
          case STDIN.read_nonblock(2)
          when "[C" # right
            @command_buffer.put :RIGHT
          when "[D" # left
            @command_buffer.put :LEFT
          when nil, ""
            clear_command_buffer
            @mode = :normal
          end
          editor.redraw_mode = :footer
        when 8, 127 # 127 on UNIX
          @command_buffer.put :BSPACE
          if @command_buffer.lines[0] == ""
            @mode = :normal
          end
          editor.redraw_mode = :footer
        when 32..126
          @command_buffer.put c.chr
          editor.redraw_mode = :footer
        end
      when :insert
        case c
        when 27 # ESC
          case cc = STDIN.read_nonblock(2)
          when "[A" # up
            buffer.put :UP
          when "[B" # down
            buffer.put :DOWN
          when "[C" # right
            buffer.put :RIGHT
          when "[D" # left
            buffer.put :LEFT
          when nil, ""
            clear_command_buffer
            @mode = :normal
            buffer.put :LEFT if 0 < buffer.cursor_x
          end
        when 8, 127 # 127 on UNIX
          buffer.put :BSPACE
        when 9
          buffer.put :TAB
        when 10, 13
          buffer.put :ENTER
        when 32..126
          buffer.put c.chr
        end
      when :visual, :visual_line
        case c
        when 27 # ESC
          case STDIN.read_nonblock(2)
          when "[A"
            buffer.put :UP
          when "[B"
            buffer.put :DOWN
          when "[C"
            buffer.put :RIGHT
          when "[D"
            buffer.put :LEFT
          when nil, ""
            buffer.clear_selection
            @mode = :normal
            clear_command_buffer
          end
        when 104 # h left
          buffer.put :LEFT
        when 106 # j down
          buffer.put :DOWN
        when 107 # k up
          buffer.put :UP
        when 108 # l right
          buffer.put :RIGHT
        when 119 # w word forward
          buffer.word_forward
        when 98 # b word backward
          buffer.word_backward
        when 101 # e word end
          buffer.word_end
        when 103 # g
          cc = STDIN.getch.ord
          if cc == 103 # gg: go to top
            buffer.home
          end
        when 121 # y yank
          range = buffer.selection_range
          @paste_board = buffer.selected_text
          @paste_type = buffer.selection_mode
          if range
            buffer.move_to(range[1], range[0])
          end
          buffer.clear_selection
          @mode = :normal
          clear_command_buffer
        when 100, 120 # d, x delete
          @paste_type = buffer.selection_mode
          @paste_board = buffer.delete_selected_text
          buffer.clear_selection
          @mode = :normal
          clear_command_buffer
        when 118 # v toggle visual
          if @mode == :visual
            buffer.clear_selection
            @mode = :normal
            clear_command_buffer
          else
            buffer.clear_selection
            buffer.start_selection(:char)
            @mode = :visual
            @command_buffer.lines[0] = "Visual"
          end
        when 86 # V toggle visual line
          if @mode == :visual_line
            buffer.clear_selection
            @mode = :normal
            clear_command_buffer
          else
            buffer.clear_selection
            buffer.start_selection(:line)
            @mode = :visual_line
            @command_buffer.lines[0] = "Visual Line"
          end
        end
        if @mode == :visual || @mode == :visual_line
          editor.redraw_mode = :all
        end
      when :visual_block
      when :cut
        if c == 100 # d delete (cut)
          @paste_board = buffer.delete_line
          @paste_type = :line
        end
        @mode = :normal
        clear_command_buffer
      end
    end
  end

  def exec_command(buffer)
    params = @command_buffer.lines[0].split(" ")
    case params.size
    when 0
      # should not happen
    when 1
      case params[0]
      when ":q"
        if buffer.changed
          "No write since last change"
        else
          :quit
        end
      when ":q!"
        :quit
      when ":w"
        save_file
      when ":wq"
        save_file
        :quit
      else
        "Not an editor command: #{@command_buffer.lines[0]}"
      end
    when 2
      case params[0]
      when ":w"
        @filepath = File.expand_path params[1], Dir.getwd
        save_file
      else
        "Not an editor command: #{@command_buffer.lines[0]}"
      end
    else
      "Not an editor command: #{@command_buffer.lines[0]}"
    end
  end

  def save_file
    return "No file name" unless @filepath
    @editor.save_file_from_buffer(@filepath)
  end

  def clear_command_buffer
    @command_buffer.clear
  end
end

