require "sandbox"
require "filesystem-fat"
require "vfs"
require "terminal"
# ENV = {} # This moved to 0_out_of_steep.rb
ARGV = []


class Shell
  def initialize(clean: false)
    clean and IO.wait_terminal(timeout: 2) and IO.clear_screen
    @terminal = Terminal::Line.new
  end

  def simple_question(question, &block)
    while true
      print question
      answer = ""
      while true
        case c = IO.getch.ord
        when 0x0d, 0x0a
          puts
          break
        when 32..126
          answer << c.chr
          print c.chr
        end
      end
      break if block.call(answer)
    end
  end

  LOGO_COLOR = "\e[32;1m"
  AUTHOR_COLOR = "\e[36;1m"
  LOGO_LINES = [
    ' ____  _           ____        _',
    '|  _ \(_) ___ ___ |  _ \ _   _| |,_  _   _',
    '| |_) | |/ __/ _ \| |_) | | | | \'_ \| | | |',
    '|  __/| | (_| (_) |  _ <| |_| | |_) | |_| |',
    '|_|   |_|\___\___/|_| \_\\__,_|_.__/ \__, |',
    "               #{AUTHOR_COLOR}by hasumikin#{LOGO_COLOR}          |___/"
  ]
  SHORT_LOGO_LINES = ["PicoRuby", "   by", "hasumikin"]

  def show_logo
    return nil if ENV['TERM'] == "dumb"
    logo_width = LOGO_LINES[0, LOGO_LINES.size - 1]&.map{|l| l.length}&.max || 0
    if logo_width < @terminal.width
      logo_lines = LOGO_LINES
    else
      logo_width = SHORT_LOGO_LINES.map{|l| l.length}.max || 0
      logo_lines = SHORT_LOGO_LINES
    end
    return nil if @terminal.width < logo_width
    margin = " " * ((@terminal.width - logo_width) / 2)
    puts LOGO_COLOR
    logo_lines.each do |line|
      print margin
      puts line
    end
    puts "\e[0m"
  end

  def setup_root_volume(device, label: "PicoRuby")
    sleep 1 if device == :sd
    return if VFS.volume_index("/")
    fat = FAT.new(device, label: label)
    retry_count = 0
    begin
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
      %w(bin lib var home).each do |dir|
        Dir.mkdir(dir) unless Dir.exist?(dir)
      end
      while exe = Shell.next_executable
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
    else
      raise ArgumentError, "Unknown mode: #{mode}"
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
          buffer.clear
          return
        else
          puts
          command.exec(*args)
          terminal.save_history
          buffer.clear
        end
      end
    end
  end

  def run_irb
    sandbox = Sandbox.new(true)
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
            terminal.save_history
            if sandbox.execute
              sandbox.wait(timeout: nil)
              if e = sandbox.error
                puts "=> #{e.message} (#{e.class})"
              else
                puts "=> #{sandbox.result.inspect}"
              end
          #    $sandbox.free_parser
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

