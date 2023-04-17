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
  # ENV = {} # This moved to 0_out_of_steep.rb
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
    ' ____  _           ____        _',
    '|  _ \(_) ___ ___ |  _ \ _   _| |,_  _   _',
    '| |_) | |/ __/ _ \| |_) | | | | \'_ \| | | |',
    '|  __/| | (_| (_) |  _ <| |_| | |_) | |_| |',
    '|_|   |_|\___\___/|_| \_\\__,_|_.__/ \__, |'
  ]
  LOGO_COLOR = "\e[36;1m"
  AUTHOR_COLOR = "\e[36;1m"

  def show_logo
    logo_width = LOGO_LINES.map{|l| l.length}.max || 0
    margin = " " * ((@terminal.width - logo_width) / 2)
    puts LOGO_COLOR
    LOGO_LINES.each do |line|
      print margin
      puts line
    end
    print margin
    puts "               #{AUTHOR_COLOR}by hasumikin#{LOGO_COLOR}          |___/"
    puts "\e[0m"
  end

  def setup_root_volume(device, label: "PicoRuby")
    sleep 1 if device == :sd
    return if VFS.volume_index("/")
    fat = FAT.new(device, label: label)
    retry_count = 0
    begin
      puts "#{fat.class} #{fat}"
      VFS.mount(fat, "/")
    rescue => e
      puts e.message
      puts "Try to format the volume..."
      fat.mkfs
      retry_count += 1
      retry if retry_count == 1
      raise e
    end
    Dir.chdir ENV['HOME'].to_s
  end

  def setup_system_files(force: false)
    Dir.chdir("/") do
      %w(bin var home).each do |dir|
        Dir.mkdir(dir) unless Dir.exist?(dir)
      end
      while exe = _next_executable
        if force || !File.exist?("/bin/#{exe[:name]}")
          f = File.open "/bin/#{exe[:name]}", "w"
          f.expand exe[:code].length
          f.write exe[:code]
          f.close
        end
      end
    end
  end

  def start(mode = :shell)
    case mode
    when :irb
      @terminal.prompt = "irb"
      run_irb
      puts
    when :shell
      run_shell
      print "\nbye\e[0m"
      exit
      return
    end
  end

  def exit(code = 0)
    raise # to restart
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
    $sandbox = Sandbox.new unless $sandbox
    @terminal.start do |terminal, buffer, c|
      case c
      when 10, 13 # LF(\n)=10, CR(\r)=13
        case script = buffer.dump.chomp
        when ""
          puts
        when "quit", "exit"
          break
        else
          if buffer.lines[-1][-1] == "\\" || !$sandbox.compile("_ = (#{script})")
            buffer.put :ENTER
          else
            terminal.feed_at_bottom
            if $sandbox.execute
              terminal.save_history
              if $sandbox.wait && error = $sandbox.error
                print "=> #{error.message} (#{error.class})"
              else
                print "=> #{$sandbox.result.inspect}"
              end
              $sandbox.suspend
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

