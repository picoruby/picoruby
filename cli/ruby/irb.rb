#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require "io/console"
  require_relative "./buffer.rb"
  class Sandbox
    def bind
      binding
    end
  end
  $bind = Sandbox.new.bind
  def getch
    STDIN.getch.ord
  end
  def gets_nonblock(max)
    STDIN.noecho{ |input| input.read_nonblock(max) }
  end
  def invoke_ruby(script)
    $sandbox_result = eval script, $bind
    true
  end
  def sandbox_result
    $sandbox_result
  end
  def sandbox_state
    0
  end
when "mruby/c"
  while !$buffer_lock
    relinquish
  end
end

TIMEOUT = 10_000 # 10 sec
PROMPT = "picoirb"

def debug(text)
  #`echo "#{text}\n" > /dev/pts/8`
end

def exit_irb
  puts "\nbye"
  terminate_sandbox
end

buffer = Buffer.new

print "#{PROMPT}> "
while true
  c = getch
  case c
  when 3 # Ctrl-C
    buffer.clear
    print "\r\n#{PROMPT}> "
  when 4 # Ctrl-D
    exit_irb
    break
  when 10, 13
    script = buffer.dump.chomp
    case script
    when ""
      print "\r\n#{PROMPT}> "
    when "quit", "exit"
      exit_irb
      break
    else
      print "\r\n"
      debug script
      if compile_ruby(script)
        buffer.clear
        if invoke_ruby
          n = 0
          while sandbox_state != 0 do # 0: TASKSTATE_DORMANT == finished(?)
            sleep_ms 50
            n += 50
            if n > TIMEOUT
              puts "Error: Timeout (sandbox_state: #{sandbox_state})"
              break
            end
          end
          print "=> "
          puts sandbox_result.inspect
          print "#{PROMPT}> "
        else
          puts "Error: Compile failed"
          print "#{PROMPT}> "
        end
      else
        print "#{PROMPT}* "
        buffer.tail
        buffer.put :ENTER
      end
    end
  when 27 # ESC
    case gets_nonblock(10)
    when "[A"
      # buffer.put :UP
    when "[B"
      # buffer.put :DOWN
    when "[C"
      if buffer.current_tail(0).length > 0
        buffer.put :RIGHT
        print "\e[C"
      end
    when "[D"
      if buffer.cursor[:x] > 1
        buffer.put :LEFT
        print "\e[D"
      end
    else
      break
    end
  when 8, 127 # 127 on UNIX
    print "\b" if buffer.lines[0].length > 0
    print "\e[0K"
    buffer.put :BSPACE
    tail = buffer.current_tail(0)
    print tail
    print "\e[#{tail.length}D" if tail.length > 1
  when 32..126
    buffer.put c.chr
    tail = buffer.current_tail
    print tail
    print "\e[#{tail.length - 1}D" if tail.length > 1
  else
    # ??
  end
  debug buffer.cursor
end
