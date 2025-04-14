require "env"
require "metaprog"
require "picorubyvm"
require "sandbox"
begin
  require "filesystem-fat"
  require "vfs"
rescue LoadError
  # ignore. maybe POSIX
  require "dir"
end

# ENV = {} # This moved to 0_out_of_steep.rb
ARGV = []


class Shell
  def self.setup_root_volume(device, label: "PicoRuby")
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
  end

  def self.setup_system_files(root = nil, force: false)
    unless root.nil? || Dir.exist?(root)
      Dir.mkdir(root)
      puts "Created root directory: #{root}"
    end
    ENV['HOME'] = "#{root}/home"
    ENV['PATH'] = "#{root}/bin"
    ENV['WIFI_CONFIG_PATH'] = "#{root}/etc/network/wifi.yml"
    Dir.chdir(root || "/") do
      %w(bin lib var home etc etc/init.d etc/network).each do |dir|
        Dir.mkdir(dir) unless Dir.exist?(dir)
        sleep_ms 100
      end
      while exe = Shell.next_executable
        path = "#{root}#{exe[:path]}"
        if force || !File.file?(path)
          f = File.open path, "w"
          f.expand exe[:code].length if f.respond_to? :expand
          f.write exe[:code]
          f.close
          sleep_ms 100
        end
      end
      path = "#{root}/etc/machine-id"
      unless File.file?(path)
        f = File.open path, "w"
        f.write Machine.unique_id
        f.close
      end
    end
    Dir.chdir ENV['HOME']
  end

  def self.bootstrap(file)
    unless File.file?(file)
      puts "File not found: #{file}"
      return false
    end
    puts "Press 's' to skip running #{file}"
    skip = false
    20.times do
      print "."
      if STDIN.read_nonblock(1) == "s"
        skip = true
        break 0
      end
      sleep 0.1
    end
    STDIN.read_nonblock 1024 # discard remaining input
    if skip
      puts "Skip"
      return false
    end
    puts "\nLoading #{file}..."
    system file
    return true
  end

  def self.setup_rtc(rtc)
    require "adafruit_pcf8523"
    begin
      print "Initializing RTC... "
      ENV['TZ'] = "JST-9"
      Time.set_hwclock rtc.current_time
      FAT.unixtime_offset = Time.unixtime_offset
      puts "Available (#{Time.now})"
    rescue => e
      puts "Not available"
      puts "#{e.message} (#{e.class})"
    end
  end

  def self.setup_sdcard(spi)
    begin
      print "Initializing SD card... "
      sd = FAT.new(:sd, label: "SD", driver: spi)
      sd_mountpoint = "/sd"
      VFS.mount(sd, sd_mountpoint)
      puts "Available at #{sd_mountpoint}"
    rescue => e
      puts "Not available"
      puts "#{e.message} (#{e.class})"
    end
  end

  def initialize(clean: false)
    require 'editor' # To save memory
    clean and IO.wait_terminal(timeout: 2) and IO.clear_screen
    @editor = Editor::Line.new
  end

  def simple_question(question, &block)
    while true
      print question
      answer = ""
      while true
        case c = STDIN.getch.ord
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
  if RUBY_ENGINE == "mruby/c"
    LOGO_LINES = [
      ' ____  _           ____        _',
      '|  _ \(_) ___ ___ |  _ \ _   _| |,_  _   _',
      '| |_) | |/ __/ _ \| |_) | | | | \'_ \| | | |',
      '|  __/| | (_| (_) |  _ <| |_| | |_) | |_| |',
      '|_|   |_|\___\___/|_| \_\\___,_|_.__/ \__, |',
      "               #{AUTHOR_COLOR}by hasumikin#{LOGO_COLOR}          |___/"
    ]
  elsif RUBY_ENGINE == "mruby"
    LOGO_LINES = [
      ' __  __ _                ____        _',
      '|  \/  (_) ___ _ __ ___ |  _ \ _   _| |__  _   _',
      '| |\/| | |/ __| \'__/ _ \| |_) | | | | \'_ \| | | |',
      '| |  | | | (__| | | (_) |  _ <| |_| | |_) | |_| |',
      '|_|  |_|_|\___|_|  \___/|_| \_\\\\__,_|_.__/ \__, |',
      "                  #{AUTHOR_COLOR}by hasumikin#{LOGO_COLOR}             |___/"
    ]
  end
  SHORT_LOGO_LINES = ["PicoRuby", "   by", "hasumikin"]

  def show_logo
    return nil if ENV['TERM'] == "dumb"
    logo_width = LOGO_LINES[0, LOGO_LINES.size - 1]&.map{|l| l.length}&.max || 0
    if logo_width < @editor.width
      logo_lines = LOGO_LINES
    else
      logo_width = SHORT_LOGO_LINES.map{|l| l.length}.max || 0
      logo_lines = SHORT_LOGO_LINES
    end
    return nil if @editor.width < logo_width
    margin = " " * ((@editor.width - logo_width) / 2)
    puts LOGO_COLOR
    logo_lines.each do |line|
      print margin
      puts line
    end
    puts "\e[0m"
  end

  def start(mode = :shell)
    case mode
    when :irb
      @editor.prompt = "irb"
      run_irb
      puts
      # puts Task.stat
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
    @editor.start do |editor, buffer, c|
      case c
      when 10, 13
        case args = buffer.dump.chomp.strip.split(" ")
        when []
          puts
        when ["reboot"]
          begin
            puts "\nrebooting..."
            Watchdog.reboot 1000
          rescue NameError
            buffer.clear
            puts "\nerror: reboot is not available on this platform"
          end
        when ["quit"], ["exit"]
          buffer.clear
          return
        else
          puts
          command.exec(*args)
          editor.save_history
          buffer.clear
        end
      end
    end
  end

  def run_irb
    sandbox = Sandbox.new('irb')
    @editor.start do |editor, buffer, c|
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
            editor.feed_at_bottom
            editor.save_history
           # echo_save = STDIN.echo?
            result = STDIN.cooked do
              r = sandbox.execute
              sandbox.wait(timeout: nil)
              sandbox.suspend
              r
            end
            if result
              if e = sandbox.error
                puts "=> #{e.message} (#{e.class})"
              else
                puts "=> #{sandbox.result.inspect}"
              end
          #    $sandbox.free_parser
            end
            buffer.clear
            editor.history_head
          end
        end
      else
        editor.debug c
      end
    end
  end
end

