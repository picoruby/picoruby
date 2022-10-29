case RUBY_ENGINE
when "ruby"
  require "io/console"
  require_relative "../../picoruby-terminal/mrblib/terminal"
  require_relative "command"

  class Sandbox
    def initialize
      @binding = binding
      @result = nil
      @state = 0
    end

    attr_reader :result, :state, :error

    def compile(script)
      begin
        RubyVM::InstructionSequence.compile("_ = (#{script})")
        @script = script
      rescue SyntaxError => e
        return false
      end
      true
    end

    def suspend
    end

    def resume
      begin
        @result = eval "_ = (#{@script})", @binding
        @error = nil
      rescue => e
        puts e.message
        @error = e
        return false
      end
      true
    end
  end

when "mruby/c"
  require "terminal"
end

class Shell
  TIMEOUT = 10_000 # 10 sec
  def initialize
    @terminal = Terminal::Line.new
    if RUBY_ENGINE == "ruby"
      @terminal.debug_tty = ARGV[0]
    end
    @terminal.feed = :lf
    @sandbox = Sandbox.new
    @sandbox.compile("nil") # _ = nil
    @sandbox.resume
  end

  def start(mode = :prsh)
    case mode
    when :irb
      @terminal.prompt = "irb"
      run_irb
    when :prsh
      @terminal.prompt = "sh"
      run_prsh
      puts "\nbye"
      return
    end
  end

  def run_prsh
    sandbox = @sandbox
    command = Command.new
    command.feed = @terminal.feed
    @terminal.start do |terminal, buffer, c|
      case c
      when 10, 13 # TODO depenging on the the "terminal"
        case args = buffer.dump.chomp.strip.split(" ")
        when []
          puts
        when ["quit"], ["exit"]
          break
        else
          print terminal.feed
          command.exec(args)
          terminal.save_history
          buffer.clear
        end
      end
    end
  end

  def run_irb
    sandbox = @sandbox
    @terminal.start do |terminal, buffer, c|
      case c
      when 10, 13 # LF(\n)=10, CR(\r)=13
        case script = buffer.dump.chomp
        when ""
          puts
        when "quit", "exit"
          break
        else
          if buffer.lines[-1][-1] == "\\" || !sandbox.compile(script)
            buffer.put :ENTER
          else
            terminal.feed_at_bottom
            if sandbox.resume
              terminal.save_history
              n = 0
              # state 0: TASKSTATE_DORMANT == finished
              while sandbox.state != 0 do
                sleep_ms 50
                n += 50
                if n > TIMEOUT
                  puts "Error: Timeout (sandbox.state: #{sandbox.state})"
                end
              end
              if error = sandbox.error
                print "=> #{error.message} (#{error.class})"
              else
                print "=> #{sandbox.result.inspect}"
              end
              print terminal.feed
              sandbox.suspend
            end
            buffer.clear
            terminal.history_head
          end
        end
      else
        terminal.debug c
      end
    end
  end
end

