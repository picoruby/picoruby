#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require "io/console"
  require_relative "./buffer.rb"

  class Sandbox
    def initialize
      @binding = binding
      @result = nil
      @state = 0
    end

    attr_reader :result, :state

    def compile(script)
      begin
        RubyVM::InstructionSequence.compile(script)
      rescue SyntaxError => e
        puts e.message
        return false
      end
      begin
        @result = eval "_ = (#{script})", @binding
      rescue => e
        puts e.message
        return false
      end
      true
    end

    def resume
      true
    end

    def exit
    end
  end

  def getch
    STDIN.getch.ord
  end

  def gets_nonblock(max)
    STDIN.noecho{ |input| input.read_nonblock(max) }
  end

  def terminate_irb
    exit
  end

when "mruby/c"
  while !$buffer_lock
    relinquish
  end
end

def debug(text)
  #`echo "#{text}\n" > /dev/pts/8`
end

TIMEOUT = 10_000 # 10 sec
PROMPT = "picoirb"

def exit_irb(sandbox)
  puts "\nbye"
  sandbox.exit
  terminate_irb
end

buffer = Buffer.new(PROMPT)

sandbox = Sandbox.new
sandbox.compile("_ = nil")
sandbox.resume

while true
  buffer.refresh_screen
  c = getch
  case c
  when 3 # Ctrl-C
    buffer.clear
    buffer.adjust_screen
  when 4 # Ctrl-D
    exit_irb(sandbox)
    break
  when 9
    buffer.put :TAB
  when 10, 13
    script = buffer.dump.chomp
    case script
    when ""
      puts
    when "quit", "exit"
      exit_irb(sandbox)
      break
    else
      buffer.adjust_screen
      if buffer.lines[-1][-1] == "\\"
        buffer.put :ENTER
      else
        buffer.clear
        debug script
        if sandbox.compile(script)
          if sandbox.resume
            n = 0
            while sandbox.state != 0 do # 0: TASKSTATE_DORMANT == finished(?)
              sleep_ms 50
              n += 50
              if n > TIMEOUT
                puts "Error: Timeout (sandbox.state: #{sandbox.state})"
                break
              end
            end
            print "=> "
            p sandbox.result
          end
        end
      end
    end
  when 27 # ESC
    case gets_nonblock(10)
    when "[A"
      buffer.put :UP
    when "[B"
      buffer.put :DOWN
    when "[C"
      buffer.put :RIGHT
    when "[D"
      buffer.put :LEFT
    else
      break
    end
  when 8, 127 # 127 on UNIX
    buffer.put :BSPACE
  when 32..126
    buffer.put c.chr
  else
    # ignore
  end
  debug buffer.cursor
end
