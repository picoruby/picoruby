#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby", "jruby"
  require_relative "../../picoruby-editor/mrblib/editor"
when "mruby/c"
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
    @editor.quit_by_ctrl_c = false
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
          when  13 # LF
          when  27 # ESC
            case IO.get_nonblock(2)
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
          when  86 # V
          when  98 # b begin
          when 100 # d
            @mode = :cut
            @command_buffer.lines[0] = "d"
          when 101 # e end
          when 103 # g
          when 104 # h left
            buffer.put :LEFT
          end
        else
          case c
          when 112 # p paste
            if @paste_board
              buffer.put :DOWN
              buffer.insert_line(@paste_board)
            end
          when 114 # r replace
          when 117 # u undo
          when 118 # v visual
          when 119 # w word
          when 120 # x delete
            buffer.delete
          when 121 # y yank
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
          case IO.get_nonblock(2)
          when "[C" # right
            @command_buffer.put :RIGHT
          when "[D" # left
            @command_buffer.put :LEFT
          when nil, ""
            clear_command_buffer
            @mode = :normal
          end
        when 8, 127 # 127 on UNIX
          @command_buffer.put :BSPACE
          if @command_buffer.lines[0] == ""
            @mode = :normal
          end
        when 32..126
          @command_buffer.put c.chr
        end
      when :insert
        case c
        when 27 # ESC
          case cc = IO.get_nonblock(2)
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
      when :visual
      when :visual_line
      when :visual_block
      when :cut
        if c == 100 # d delete (cut)
          @paste_board = buffer.delete_line
        end
        @mode = :normal
        clear_command_buffer
      end
    end
  end

  def exec_command(buffer)
    params = @command_buffer.lines[0].split(" ")
    case params.count
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

