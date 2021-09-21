#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require "ripper"
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
  def sandbox_picorbc(script)
    # TODO: upgrade picoirb to make `A::B` and `rescue => e` valid
    #begin
    #  RubyVM::InstructionSequence.compile(script)
    #  $sandbox_result = eval script, $bind
    #rescue => e
    #  puts e.message
    #  return false
    #end
    #true
    if Ripper.sexp(script)
      $sandbox_result = eval script, $bind
    else
      puts "Syntax error"
      false
    end
  end
  def sandbox_resume
    true
  end
  def sandbox_result
    $sandbox_result
  end
  def sandbox_state
    0
  end
  def terminate_sandbox
    exit
  end
  def debug(text)
    #`echo "#{text}\n" > /dev/pts/8`
  end
when "mruby/c"
  def debug(text)
  end
  while !$buffer_lock
    relinquish
  end
end

TIMEOUT = 10_000 # 10 sec
PROMPT = "picoirb"

def exit_irb
  puts "\nbye"
  terminate_sandbox
end

buffer = Buffer.new(PROMPT)

while true
  buffer.refresh_screen
  c = getch
  case c
  when 3 # Ctrl-C
    buffer.clear
    buffer.adjust_screen
  when 4 # Ctrl-D
    exit_irb
    break
  when 9
    buffer.put :TAB
  when 10, 13
    script = buffer.dump.chomp
    case script
    when ""
      puts
    when "quit", "exit"
      exit_irb
      break
    else
      buffer.adjust_screen
      if buffer.lines[-1][-1] == "\\"
        buffer.put :ENTER
      else
        buffer.clear
        debug script
        if sandbox_picorbc(script)
          if sandbox_resume
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
            p sandbox_result
          else
            puts "Error: Compile failed"
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
