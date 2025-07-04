require "env"
require "metaprog"
require "picorubyvm"
require "sandbox"
require "crc"
require "machine"
require 'yaml'
begin
  require 'gpio'
  require "filesystem-fat"
  require "vfs"
rescue LoadError
  # ignore. maybe POSIX
  require "dir"
end

# ENV = {} # This moved to 0_out_of_steep.rb
ARGV = []


class Shell

  DeviceInstances = {}

  def self.get_device(type, name)
    key = "#{type}_#{name}".upcase
    DeviceInstances[key] ||= case key
    when 'GPIO_TRIGGER_NMBLE'
      GPIO.new((ENV[key] || 22).to_i, GPIO::IN|GPIO::PULL_UP)
    when 'GPIO_LED_BLE', 'GPIO_LED_WIFI'
      if ENV[key].nil? || ENV[key] == 'cyw43_led'
        CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
      else
        GPIO.new(ENV[key].to_i, GPIO::OUT)
      end
    else
      raise "Unknown GPIO key: #{key}"
    end
  end

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

  def self.ensure_system_file(path, code, crc = nil)
    10.times do
      if File.file?(path)
        print "Checking: #{path}"
        File.open(path, "r") do |f|
          actual_len = f.size
          actual_crc = if f.respond_to?(:physical_address)
                         CRC.crc32_from_address(f.physical_address, code.size)
                       else
                         actual_code = f.read if 0 < actual_len
                         CRC.crc32(actual_code)
                       end
          if (actual_len == code.length) && ( crc.nil? || (actual_crc == crc) )
            puts " ... OK (#{code.length} bytes)"
            return
          else
            puts " ... NG! (len: #{code.size}<=>#{actual_len} crc: #{crc}<=>#{actual_crc})"
          end
        end
        File.unlink(path)
        sleep_ms 100
      else
        puts "Writing : #{path}"
        File.open(path, "w") do |f|
          f.expand(code.length) if f.respond_to?(:expand)
          f.write(code)
        end
        sleep_ms 100
      end
    end
    File.unlink(path) if File.file?(path)
    puts "Failed to save: #{path} (#{code.length} bytes)"
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
      %w(bin home etc etc/init.d etc/network var lib).each do |dir|
        4.times do |i|
          break i if Dir.exist?(dir)
          begin
            puts "Creating directory: #{dir} (trial #{i + 1})"
            Dir.mkdir(dir)
            sleep 1
          rescue
            if i < 3
              puts " Failed to create directory: #{dir}. Retrying..."
              sleep 1
            else
              raise "Failed to create directory: #{dir}. Please reboot"
            end
          end
          sleep 1 # This likely ensure the directory is created
        end
      end
      while exe = Shell.next_executable
        path = "#{root}#{exe[:path]}"
        self.ensure_system_file(path, exe[:code], exe[:crc])
      end
      path = "#{root}/etc/machine-id"
      self.ensure_system_file(path, Machine.unique_id, nil)
    end
    Dir.chdir ENV['HOME']

    config_file = "/etc/config.yml"
    # example of `config.yml`:
    #
    # device:
    #   gpio:
    #     trigger_nmble: 22
    #     led_ble: cyw43_led
    #     led_wifi: 23
    begin
      if File.file?(config_file)
        config = YAML.load_file(config_file)
        # @type var config: Hash[String, untyped]
        device = config['device']
        if device&.respond_to?(:each)
          device.each do |type, values|
            values&.each do |key, value|
              ENV["#{type}_#{key}".upcase] = value.to_s
            end
          end
        end
      end
    rescue => e
      puts "Failed to load config file: #{config_file}"
      puts "  #{e.message} (#{e.class})"
    end

    begin
      require "cyw43"
      if CYW43.respond_to?(:enable_sta_mode)
        ENV['WIFI_MODULE'] = "cwy43"
      end
    rescue
    end
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

  def self.simple_question(question, &block)
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

  def initialize(clean: false)
    require 'editor' # To save memory
    clean and IO.wait_terminal(timeout: 2) and IO.clear_screen
    @editor = Editor::Line.new
  end

  AUTHOR_COLOR = "\e[38;5;217m"
  if RUBY_ENGINE == "mruby/c"
    LOGO_LINES = [
      "\e[38;5;197m ██████\e[38;5;198m╗\e[38;5;204m  ██╗  ██████╗  ████\e[38;5;210m██╗  ██████╗  ██╗   █\e[38;5;217m█╗ ██████╗  ██╗   ██╗\e[39m",
      "\e[38;5;197m ██╔══█\e[38;5;198m█\e[38;5;204m╗ ██║ ██╔════╝ ██╔══\e[38;5;210m═██╗ ██╔══██╗ ██║   █\e[38;5;217m█║ ██╔══██╗ ╚██╗ ██╔╝\e[39m",
      "\e[38;5;197m ██████\e[38;5;198m╔\e[38;5;204m╝ ██║ ██║      ██║  \e[38;5;210m ██║ ██████╔╝ ██║   █\e[38;5;217m█║ ██████╔╝  ╚████╔╝ \e[39m",
      "\e[38;5;197m ██╔═══\e[38;5;198m╝\e[38;5;204m  ██║ ██║      ██║  \e[38;5;210m ██║ ██╔══██╗ ██║   █\e[38;5;217m█║ ██╔══██╗   ╚██╔╝  \e[39m",
      "\e[38;5;197m ██║   \e[38;5;198m \e[38;5;204m  ██║ ╚██████╗ ╚████\e[38;5;210m██╔╝ ██║  ██║ ╚██████\e[38;5;217m╔╝ ██████╔╝    ██║   \e[39m",
      "\e[38;5;197m ╚═╝   \e[38;5;198m \e[38;5;204m  ╚═╝  ╚═════╝  ╚═══\e[38;5;210m══╝  ╚═╝  ╚═╝  ╚═════\e[38;5;217m╝  ╚═════╝     ╚═╝   \e[39m",
      "                            #{AUTHOR_COLOR}by hasumikin\e[39m"
    ]
    LOGO_WIDTH = 70
  elsif RUBY_ENGINE == "mruby"
    LOGO_LINES = [
      "\e[38;5;197m ███╗   \e[38;5;198m█\e[38;5;204m██╗ ██╗  ██████╗ ██████\e[38;5;210m╗   ██████╗  ██████╗  █\e[38;5;211m█\e[38;5;217m╗   ██╗ ██████╗  ██╗   \e[39m\e[38;5;197m█\e[38;5;210m█\e[38;5;217m╗\e[39m",
      "\e[38;5;197m ████╗ █\e[38;5;204m███║ ██║ ██╔════╝ ██╔══█\e[38;5;210m█╗ ██╔═══██╗ ██╔══██╗ \e[38;5;211m█\e[38;5;217m█║   ██║ ██╔══██╗ ╚██╗ \e[39m\e[38;5;197m█\e[38;5;204m█\e[38;5;210m╔\e[38;5;217m╝\e[39m",
      "\e[38;5;197m ██╔████\e[38;5;204m╔██║ ██║ ██║      ████\e[38;5;210m██╔╝ ██║   ██║ ██████╔\e[38;5;217m╝ ██║   ██║ ██████╔╝  \e[39m\e[38;5;197m╚\e[38;5;204m██\e[38;5;210m██\e[38;5;217m╔╝ \e[39m",
      "\e[38;5;197m ██║╚██╔\e[38;5;198m╝\e[38;5;204m██║ ██║ ██║      ██╔══██\e[38;5;210m╗ ██║   ██║ ██╔══██╗ ██\e[38;5;217m║   ██║ ██╔══██╗   ╚██╔╝\e[39m\e[38;5;197m \e[38;5;217m \e[39m",
      "\e[38;5;197m ██║ ╚═╝\e[38;5;198m \e[38;5;204m██║ ██║ ╚██████╗ ██║  ██\e[38;5;210m║ ╚██████╔╝ ██║  ██║ ╚█\e[38;5;217m█████╔╝ ██████╔╝    ██║ \e[39m\e[38;5;197m \e[38;5;217m \e[39m",
      "\e[38;5;197m ╚═╝    \e[38;5;198m \e[38;5;204m╚═╝ ╚═╝  ╚═════╝ ╚═╝  ╚═\e[38;5;210m╝  ╚═════╝  ╚═╝  ╚═╝  ╚\e[38;5;217m═════╝  ╚═════╝     ╚═╝ \e[39m\e[38;5;197m \e[38;5;217m \e[39m",
      "                               #{AUTHOR_COLOR}by hasumikin\e[39m"
    ]
    LOGO_WIDTH = 81
  end

  def show_logo
    return nil if ENV['TERM'] == "dumb"
    return nil if @editor.width < LOGO_WIDTH
    margin = " " * ((@editor.width - LOGO_WIDTH) / 2)
    LOGO_LINES.each do |line|
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

