case RUBY_ENGINE
when "ruby"
  require "io/console"
  require "./buffer.rb"
  class Sandbox
    def bind
      binding
    end
  end
  $bind = Sandbox.new.bind
  def getch
    STDIN.getch.ord
  end
  def getc_nonblock(max)
    STDIN.noecho{ |input| input.read_nonblock(max) }
  end
  def invoke_ruby(script)
    $sandbox_state = 1
    $sandbox_result = eval script, $bind
    $sandbox_state = 0
    true
  end
  def sandbox_result
    $sandbox_result
  end
  def sandbox_state
    $sandbox_state
  end
when "mruby/c"
  while !$buffer_lock
    relinquish
  end
end

buffer = Buffer.new

print "picoirb> "
while true
  c = getch
  case c
  when 10
    # ignore
  when 13
    script = buffer.lines[0].chomp
    buffer.clear
    case script
    when ""
      print "\r\npicoirb> "
    when "quit", "exit"
      puts "bye"
      break
    else
      print "\r\n"
      if invoke_ruby(script)
        n = 0
        while sandbox_state != 0 do # 0: TASKSTATE_DORMANT == finished(?)
          sleep_ms 50
          n += 1
          if n > 20
            puts "Error: Timeout"
            break
          end
        end
        print "=> "
        puts sandbox_result.inspect
        print "picoirb> "
      else
        puts "Error: Compile failed"
      end
    end
  when 27 # ESC
    case getc_nonblock(100)
    when "[A"
      # buffer.put :UP
    when "[B"
      # buffer.put :DOWN
    when "[C"
      if buffer.current_tail.length > 1
        buffer.put :RIGHT
        print "\e[C"
      end
    when "[D"
      buffer.put :LEFT
      print "\e[D"
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
end
