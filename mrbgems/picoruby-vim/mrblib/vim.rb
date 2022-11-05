#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require_relative "../../picoruby-terminal/mrblib/terminal"
when "mruby/c"
  require "terminal"
else
  raise RuntimeError.new("Unknown RUBY_ENGINE")
end

class Vim
  def initialize(filepath)
    @filepath = File.expand_path filepath, Dir.getwd if filepath
    @mode = :normal
    @terminal = Terminal::Editor.new
    @terminal.footer_height = 2
    @command_buffer = Terminal::Buffer.new
    if @filepath
      unless @terminal.load_file_into_buffer(@filepath)
        @command_buffer.lines[0] == "No found"
      end
    end
    @terminal.refresh_footer do |terminal|
      #     foreground  background
      print "\e[37;1m" ,"\e[48;5;239m"
      if @filepath
        print " ", @filepath[0, terminal.width - 1].ljust(terminal.width - 1)
      else
        print " ", "[No Name]".ljust(terminal.width - 1)
      end
      print "\e[m" # reset color
      print "\e[1E"
      print @command_buffer.lines[0]
      if @mode == :command
        print "\e[#{terminal.width};1H"
        print "\e[#{@command_buffer.lines[0].size}C"
      end
    end
    @terminal.refresh_cursor do |terminal|
      unless @mode == :command
        terminal.calculate_visual_cursor
        terminal.show_cursor
      end
    end
    if RUBY_ENGINE == "ruby"
      @terminal.debug_tty = ARGV[1]
      @terminal.debug "debug start"
    end
  end

  def start
    print "\e[?1049h" # DECSET 1049
    _start
    print "\e[?1049l" # DECRST 1049
  end

  def _start
    @terminal.start do |terminal, buffer, c|
      case @mode
      when :normal
        if c < 108
          case c
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
          when  46 # . redo
          when  58 # ; command
          when  59 # : command
            @mode = :command
            @command_buffer.put ':'
          when  65 # A
          when  86 # V
          when  98 # b begin
          when 100 # d
          when 101 # e end
          when 103 # g
          when 104 # h left
            buffer.put :LEFT
          when 97, 105 # a, i insert
            right if c == 97 # a
            @mode = :insert
            @command_buffer.lines[0] = "Insert"
          when 106 # j down
            buffer.put :DOWN
          when 107 # k up
            buffer.put :UP
          end
        else
          case c
          when 108 # l right
            buffer.put :RIGHT
          when 111 # o replace
          when 112 # p paste
          when 114 # r replace
          when 117 # u undo
          when 118 # v visual
          when 119 # w word
          when 120 # x delete
          when 121 # y yank
          else
            puts c
          end
        end
      when :command
        case c
        when 27 # ESC
          case IO.get_nonblock(2)
          when "[C" # right
            @command_buffer.put :RIGHT
          when "[D" # left
            @command_buffer.put :LEFT
          when nil
            @command_buffer.clear
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
          case IO.get_nonblock(2)
          when "[A" # up
            buffer.put :UP
          when "[B" # down
            buffer.put :DOWN
          when "[C" # right
            buffer.put :RIGHT
          when "[D" # left
            buffer.put :LEFT
          when nil
            @command_buffer.clear
            @mode = :normal
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
      end
    end
  end
end

