require "env"
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
  require "dir" if RUBY_ENGINE == 'mruby/c'
end

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
    ENV["WIFI_MODULE"] = "none" # possibly overwritten in CYW43.init
    ENV["TZ"] = "JST-9" # TODO. maybe in CYW43
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
      Machine.set_hwclock(rtc.current_time.to_i, 0)
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

  LOGO = if RUBY_ENGINE == "mruby"
    [
      "01110000111001100011111100111111000011111100011111100011000011001111110011000011",
      "01111001111001100110000000110001100110000110011000110011000011001100011001100110",
      "01101111011001100110000000111111000110000110011111100011000011001111110000111100",
      "01100110011001100110000000110001100110000110011000110011000011001100011000011000",
      "01100000011001100011111100110001100011111100011000110001111110001111110000011000",
      "00000000000000000000000000000000000000000000000000000000000000000000000000000000"
    ]
  else
    [
      "01111110001100011111100011111100011111100011000011001111110011000011",
      "01100011001100110000000110000110011000110011000011001100011001100110",
      "01111110001100110000000110000110011111100011000011001111110000111100",
      "01100000001100110000000110000110011000110011000011001100011000011000",
      "01100000001100011111100011111100011000110001111110001111110000011000",
      "00000000000000000000000000000000000000000000000000000000000000000000"
    ]
  end
  LOGO_WIDTH = LOGO[0].length
  author = "@hasumikin"
  space = " " * ((LOGO_WIDTH - author.length) / 2)
  AUTHOR = space + author + space
  AUTHOR_COLOR = 207

  def show_logo(color_num = 6) # color_num: 0..11
    return if ENV['TERM'] == "dumb"
    return if @editor.width < LOGO_WIDTH
    margin = " " * ((@editor.width - LOGO_WIDTH) / 2)

    # Add shadow
    LOGO.size.times do |y|
      break if LOGO[y+1].nil?
      LOGO[y].length.times do |x|
        if LOGO[y][x] == '1' && LOGO[y+1][x-1] == '0'
          LOGO[y+1][x-1] = '2'
        end
      end
    end

    grad_start = 160 + (6 * color_num)
    grad_end = grad_start + 5
    grad_slice = LOGO[0].length / 5
    shadow_offset = 144
    shadow = "\e[38;5;235m:"

    LOGO.size.times do |y|
      print margin
      split_line = []
      i = 0
      while i < LOGO[y].length
        split_line << LOGO[y][i, grad_slice]
        i += grad_slice
      end
      x = 0
      split_line.each_with_index do |snip, i|
        color = grad_start + i
        snip.each_char do |c|
          if c == '0'
            if y == LOGO.size - 1
              print "\e[38;5;#{AUTHOR_COLOR}m#{AUTHOR[x]}"
            else
              print " "
            end
          elsif c == '1'
            print "\e[48;5;#{color}m\e[38;5;226m:\e[0m"
          elsif c == '2'
            print "\e[48;5;#{color - shadow_offset}m"
            if y == LOGO.size - 1
              a = AUTHOR[x]
              if a == " "
                print shadow
              else
                print "\e[38;5;#{AUTHOR_COLOR}m#{a}"
              end
            else
              print shadow
            end
            print "\e[0m"
          end
          x += 1
        end
      end
      puts
    end
    puts "\e[0m"
  end

  def start(mode = :shell)
    case mode
    when :irb
      @editor.prompt = "irb"
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
          print "\nbye\n\e[0m"
          Machine.exit(0)
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
            end
            buffer.clear
            editor.history_head
          end
        end
      else
        editor.debug c
      end
    end
  ensure
    puts
    sandbox.terminate
  end
end

