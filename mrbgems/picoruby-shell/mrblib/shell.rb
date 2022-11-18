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

    def execute
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
  require "sandbox"
  require "filesystem-fat"
  require "vfs"
  require "terminal"
  ENV = {}
  ARGV = []
end


class Shell
  def initialize
    @terminal = Terminal::Line.new
    if RUBY_ENGINE == "ruby"
      @terminal.debug_tty = ARGV[0]
    end
  end

  LOGO_LINES = [
    "   888888  88   88888  88888",
    "   88   88 88  88     88   88",
    "   888888  88 88     88     88",
    "   88      88  88     88   88",
    "   88      88   88888  88888",
    "",
    "888888  88    88 888888 88    88",
    "88   88 88    88 88   88 88  88",
    "888888  88    88 888888   8888",
    "88   88  88  88  88   88   88",
    "88    888 8888   888888    88"
  ]

  def show_logo
    logo_width = LOGO_LINES.map{|l| l.length}.max
    margin = " " * ((@terminal.width - logo_width) / 2)
    puts "\e[33;1m"
    LOGO_LINES.each do |line|
      print margin
      puts line
    end
    puts "\e[32;0m"
  end

  def self.setup(drive)
    return if VFS.volume_index("/")
    fat = FAT.new(drive)
    retry_count = 0
    begin
      VFS.mount(fat, "/")
    rescue => e
      puts e.message
      fat.mkfs
      retry_count += 1
      retry if retry_count == 1
      raise e
    end
    begin
      %w(bin var home).each do |dir|
        Dir.mkdir dir
      end
    rescue => e
      puts e.message
    end
    while exe = _next_executable
      f = File.open "/bin/#{exe[:name]}", "w"
      f.expand exe[:code].length
      f.write exe[:code]
      f.close
    end
    Dir.chdir ENV['HOME']
  end

  def start(mode = :shell)
    case mode
    when :irb
      @terminal.prompt = "irb"
      run_irb
      puts
    when :shell
      show_logo
      run_shell
      print "\nbye\e[0m"
      exit
      return
    end
  end

  def run_shell
    command = Command.new
    @terminal.start do |terminal, buffer, c|
      case c
      when 10, 13
        case args = buffer.dump.chomp.strip.split(" ")
        when []
          puts
        when ["quit"], ["exit"]
          return
        else
          puts
          command.exec(args)
          terminal.save_history
          buffer.clear
        end
      end
    end
  end

  def run_irb
    sandbox = Sandbox.new
    @terminal.start do |terminal, buffer, c|
      case c
      when 10, 13 # LF(\n)=10, CR(\r)=13
        case script = buffer.dump.chomp
        when ""
          puts
        when "quit", "exit"
          break
        else
          if buffer.lines[-1][-1] == "\\" || !sandbox.compile("_ = (#{script})")
            buffer.put :ENTER
          else
            terminal.feed_at_bottom
            if sandbox.execute
              terminal.save_history
              if sandbox.wait && error = sandbox.error
                print "=> #{error.message} (#{error.class})"
              else
                print "=> #{sandbox.result.inspect}"
              end
              sandbox.suspend
            end
            puts
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

