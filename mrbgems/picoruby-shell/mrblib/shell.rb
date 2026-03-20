require "env"
require "io/console"
require "picorubyvm"
require "sandbox"
require "crc"
require "picomodem"
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
      begin
        if ENV[key].nil? || ENV[key] == 'cyw43_led'
          CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
        else
          if pin = ENV[key]
            GPIO.new(pin.to_i, GPIO::OUT)
          else
            nil
          end
        end
      rescue NameError
        nil
      end
    else
      nil
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
    flawless = true
    i = 0
    while i < 10
      if File.file?(path)
        print "Checking: #{path}"
        File.open(path, "r") do |f|
          actual_len = f.size
          actual_crc = if f.respond_to?(:physical_address)
                         CRC.crc32_from_address(f.physical_address, code.size)
                       else
                         actual_code = f.read if 0 < actual_len
                         CRC.crc32(actual_code.to_s)
                       end
          if (actual_len == code.length) && ( crc.nil? || (actual_crc == crc) )
            puts " ... OK (#{code.length} bytes)"
            return flawless
          else
            puts " ... NG! (len: #{code.size}<=>#{actual_len} crc: #{crc}<=>#{actual_crc})"
            flawless = false
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
      i += 1
    end
    File.unlink(path) if File.file?(path)
    puts "Failed to save: #{path} (#{code.length} bytes)"
    return false
  end

  # To skip sanity check for each file, we create a description
  # file after all files are confirmed to be flawless.
  # If sanity check fails even once, the description file is
  # removed to force sanity check on the next boot.
  def self.sanity_check_system_files(root)
    desc_file = "#{root}/etc/ruby-description"
    if File.file?(desc_file)
      File.open(desc_file, "r") do |f|
        f.read.chomp == RUBY_DESCRIPTION and return
      end
    end
    flawless = true
    while exe = Shell.next_executable
      path = "#{root}#{exe[:path]}"
      unless self.ensure_system_file(path, exe[:code], exe[:crc])
        flawless = false
      end
    end
    path = "#{root}/etc/machine-id"
    unless self.ensure_system_file(path, Machine.unique_id, nil)
      flawless = false
    end
    if flawless
      File.open(desc_file, "w") do |f|
        f.write(RUBY_DESCRIPTION)
      end
    else
      File.unlink(desc_file) if File.file?(desc_file)
    end
  end

  def self.setup_system_files(root = nil)
    unless root.nil? || Dir.exist?(root)
      Dir.mkdir(root)
      puts "Created root directory: #{root}"
    end
    $LOAD_PATH = ["#{root}/lib"]
    ENV['HOME'] = "#{root}/home"
    ENV['PATH'] = "#{root}/bin"
    ENV['DFU_DIR'] = "#{root}/etc/dfu"
    ENV['WIFI_CONFIG_PATH'] = "#{root}/etc/network/wifi.yml"
    ENV["WIFI_MODULE"] = "none" # possibly overwritten in CYW43.init
    Dir.chdir(root || "/") do
      dirs = %w(bin home etc etc/init.d etc/network etc/dfu var var/log lib)
      di = 0
      while di < dirs.size
        dir = dirs[di]
        unless Dir.exist?(dir)
          puts "Creating directory: #{dir}"
          Dir.mkdir(dir)
          i = 0
          while !Dir.exist?(dir)
            sleep_ms 200
            i += 1
            if 10 < i
              raise "Failed to create directory: #{dir}. Please reboot"
            end
          end
        end
        di += 1
      end
      self.sanity_check_system_files(root || "")
    end
    Dir.chdir ENV['HOME'] || ENV_DEFAULT_HOME

    self.read_config
    require 'network'
  end

  def self.read_config
    config_file = "/etc/config.yml"
    return unless File.file?(config_file)
    # example of `config.yml`:
    #
    # device:
    #   gpio:
    #     trigger_nmble: 22
    #     led_ble: cyw43_led
    #     led_wifi: 23
    config = YAML.load_file(config_file)
    # @type var config: Hash[String, untyped]
    env = config['env']
    if env&.respond_to?(:keys)
      keys = env.keys
      ki = 0
      while ki < keys.size
        key = keys[ki]
        ENV[key.upcase] = env[key].to_s
        ki += 1
      end
    end
    device = config['device']
    if device&.respond_to?(:keys)
      dev_keys = device.keys
      dki = 0
      while dki < dev_keys.size
        type = dev_keys[dki]
        values = device[type]
        if values&.respond_to?(:keys)
          val_keys = values.keys
          vki = 0
          while vki < val_keys.size
            key = val_keys[vki]
            ENV["#{type}_#{key}".upcase] = values[key].to_s
            vki += 1
          end
        end
        dki += 1
      end
    end
  rescue => e
    puts "Failed to load config file: #{config_file}"
    puts "  #{e.message} (#{e.class})"
  end

  def self.bootstrap(file)
    unless File.file?(file)
      puts "File not found: #{file}"
      return false
    end
    puts "Press 's' to skip running #{file}"
    skip = false
    j = 0
    while j < 20
      print "."
      if STDIN.read_nonblock(1) == "s"
        skip = true
        break
      end
      sleep 0.1
      j += 1
    end
    STDIN.read_nonblock 1024 # discard remaining input
    if skip
      puts "Skip"
      return false
    end
    puts "\nLoading #{file}..."
    load file
    return true
  end

  def self.setup_rtc(rtc)
    require "adafruit_pcf8523"
    begin
      print "Initializing RTC... "
      ENV['TZ'] = "JST-9"
      Machine.set_hwclock(rtc.current_time.to_i)
      FAT.unixtime_offset = Time.unixtime_offset
      puts "Available (#{Time.now})"
    rescue => e
      puts "Not available"
      puts "#{e.message} (#{e.class})"
    end
  end

  def self.setup_sdcard(driver)
    begin
      print "Initializing SD card(#{driver.class})... "
      sd = FAT.new(:sd, label: "SD", driver: driver)
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

  def self.find_executable(name)
    if name.start_with?("/")
      return File.file?(name) ? name : nil
    end
    if name.start_with?("./") || name.start_with?("../")
      file = File.expand_path(name, Dir.pwd)
      return File.file?(name) ? name : nil
    end
    paths = ENV['PATH']&.split(";")
    if paths
      pi = 0
      while pi < paths.size
        file = "#{paths[pi]}/#{name}"
        return file if File.file? file
        pi += 1
      end
    end
    nil
  end

  def initialize(clean: false)
    require 'editor' # To save memory
    clean and IO.wait_terminal(timeout: 2) and IO.clear_screen
    @editor = Editor::Line.new
    @jobs = []
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
    y = 0
    while y < LOGO.size
      break if LOGO[y+1].nil?
      x = 0
      while x < LOGO[y].length
        if LOGO[y][x] == '1' && LOGO[y+1][x-1] == '0'
          LOGO[y+1][x-1] = '2'
        end
        x += 1
      end
      y += 1
    end

    grad_start = 160 + (6 * color_num)
    grad_end = grad_start + 5
    grad_slice = LOGO[0].length / 5
    shadow_offset = 144
    shadow = "\e[38;5;235m:"

    y2 = 0
    while y2 < LOGO.size
      print margin
      split_line = [] #: Array[String]
      i = 0
      while i < LOGO[y2].length
        split_line << (LOGO[y2][i, grad_slice] || raise("unreachable"))
        i += grad_slice
      end
      x = 0
      si = 0
      while si < split_line.size
        snip = split_line[si]
        color = grad_start + si
        ci = 0
        while ci < snip.length
          c = snip[ci]
          if c == '0'
            if y2 == LOGO.size - 1
              print "\e[38;5;#{AUTHOR_COLOR}m#{AUTHOR[x]}" # steep:ignore
            else
              print " "
            end
          elsif c == '1'
            print "\e[48;5;#{color}m\e[38;5;226m:\e[0m"
          elsif c == '2'
            print "\e[48;5;#{color - shadow_offset}m"
            if y2 == LOGO.size - 1
              a = AUTHOR[x] # steep:ignore
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
          x += 1 # steep:ignore
          ci += 1
        end
        si += 1
      end
      puts
      y2 += 1
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
      print "bye\e[0m"
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
    @editor.start do |editor, buffer, c|
      case c
      when 2 # Ctrl-B (STX) - enter machine communication mode
        buffer.clear
        puts "\n^B"
        $stdout.write("\x06") # ACK - signal browser to start binary capture
        PicoModem.session($stdin, $stdout)
        editor.raw_takeover
      when 3 # Ctrl-C
        buffer.clear
        puts "\n^C\e[0J"
      when 4 # Ctrl-D EOF
        if buffer.empty?
          puts "\n^D" # Cannot logout
        else
          c = 10
          redo # treat as Enter key to execute the command in buffer
        end
      when 10, 13
        puts
        command_line = buffer.dump.chomp.strip
        editor.save_history
        buffer.clear
        cleanup_jobs

        # Parse command line using new parser
        unless command_line.empty?
          begin
            parser = Parser.new(command_line)
            ast = parser.parse

            execute_ast(ast) if ast
          rescue => e
            puts e.message
          end
        end
      when 26 # Ctrl-Z
        puts "\n^Z\e[0J"
        Machine.exit(0)
      end
    end
  end

  def run_irb
    # Workaround for new mruby-task in mruby/mruby
    sandbox = Sandbox.new('irb')
    sandbox.compile("_ = nil")
    sandbox.execute
    sandbox.wait(timeout: nil)
    sandbox.suspend
    @editor.start do |editor, buffer, c|
      case c
      when 3 # Ctrl-C
        buffer.clear
        puts "\n^C\e[0J"
      when 4  # Ctrl-D EOF = logout
        break if buffer.empty?
      when 26 # Ctrl-Z
        Signal.trap(:CONT) do
          Machine.signal_self_manage
        end
        puts "\n^Z\e[0J"
        Signal.raise(:TSTP)
      when 10, 13 # LF(\n)=10, CR(\r)=13
        case script = buffer.dump.chomp
        when ""
          puts
        when "quit", "exit"
          break
        else
          if buffer.lines[-1][-1] == "\\" || !sandbox.compile("begin; _ = (#{script}); rescue => _; end; _")
            buffer.put :ENTER
          else
            editor.feed_at_bottom
            editor.save_history
            executed = false
            STDIN.cooked do
              executed = sandbox.execute
              sandbox.wait(timeout: nil)
              sandbox.suspend
            end
            if executed
              if sandbox.result.is_a?(Exception)
                puts "#{sandbox.result.message} (#{sandbox.result.class})"
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
    sandbox.terminate
  end

  def builtin?(name)
    self.respond_to?(name)
  end

  def execute_ast(ast)
    case ast.type
    when :command
      execute_command_node(ast)
    when :pipeline
      execute_pipeline_node(ast)
    else
      raise "Unknown AST node type: #{ast.type}"
    end
  end

  def execute_command_node(node)
    data = node.data
    name = data[:name]
    args = data[:args]
    redirects = data[:redirects]

    # Handle redirections
    redirect_in = nil
    redirect_out = nil
    redirect_mode = nil

    ri = 0
    while ri < redirects.size
      redir = redirects[ri]
      case redir.data[:type]
      when :input
        redirect_in = redir.data[:target]
      when :output
        redirect_out = redir.data[:target]
        redirect_mode = :write
      when :append
        redirect_out = redir.data[:target]
        redirect_mode = :append
      end
      ri += 1
    end

    # Check if builtin command
    if builtin?("_#{name}")
      if redirect_in || redirect_out
        # Execute builtin with file redirection
        execute_builtin_with_redirect(name, args, redirect_in, redirect_out, redirect_mode)
      else
        # Normal builtin execution
        send("_#{name}", *args)
      end
    else
      # Execute external command
      cmd_args = [name] + args

      if redirect_in || redirect_out
        # Execute with file redirection
        execute_with_file_redirect(cmd_args, redirect_in, redirect_out, redirect_mode)
      else
        # Normal execution
        job = Job.new(*cmd_args)
        @jobs << job
        job.exec
      end
    end
  end

  def execute_pipeline_node(node)
    commands = node.data[:commands]

    # Extract redirects from the last command
    last_cmd = commands.last
    redirects = last_cmd.data[:redirects]

    redirect_in = nil
    redirect_out = nil
    redirect_mode = nil

    ri = 0
    while ri < redirects.size
      redir = redirects[ri]
      case redir.data[:type]
      when :input
        redirect_in = redir.data[:target]
      when :output
        redirect_out = redir.data[:target]
        redirect_mode = :write
      when :append
        redirect_out = redir.data[:target]
        redirect_mode = :append
      end
      ri += 1
    end

    cmd_arrays = []
    ci = 0
    while ci < commands.size
      cmd_node = commands[ci]
      cmd_arrays << ([cmd_node.data[:name]] + cmd_node.data[:args])
      ci += 1
    end

    if redirect_in || redirect_out
      # Execute pipeline with file redirection
      old_stdin = $stdin
      old_stdout = $stdout

      begin
        if redirect_in
          input_file = redirect_in
          # @type var input_file: String
          if File.exist?(input_file)
            $stdin = File.open(input_file, 'r')
          else
            puts "#{input_file}: No such file or directory"
            return
          end
        end

        if redirect_out
          output_file = redirect_out
          mode = redirect_mode == :append ? 'a' : 'w'
          # @type var output_file: String
          $stdout = File.open(output_file, mode)
        end

        pipeline = Pipeline.new(cmd_arrays)
        pipeline.exec
      ensure
        $stdin.close if redirect_in && $stdin != old_stdin
        $stdout.close if redirect_out && $stdout != old_stdout
        $stdin = old_stdin
        $stdout = old_stdout
      end
    else
      pipeline = Pipeline.new(cmd_arrays)
      pipeline.exec
    end
  end

  def execute_with_file_redirect(cmd_args, redirect_in, redirect_out, redirect_mode)
    old_stdin = $stdin
    old_stdout = $stdout

    begin
      # Setup input redirection
      if redirect_in
        if File.exist?(redirect_in)
          $stdin = File.open(redirect_in, 'r')
        else
          puts "#{redirect_in}: No such file or directory"
          return
        end
      end

      # Setup output redirection
      if redirect_out
        mode = redirect_mode == :append ? 'a' : 'w'
        $stdout = File.open(redirect_out, mode)
      end

      # Execute command
      job = Job.new(*cmd_args)
      job.exec
    ensure
      # Close and restore
      $stdin.close if redirect_in && $stdin != old_stdin
      $stdout.close if redirect_out && $stdout != old_stdout
      $stdin = old_stdin
      $stdout = old_stdout
    end
  end

  def execute_builtin_with_redirect(name, args, redirect_in, redirect_out, redirect_mode)
    old_stdin = $stdin
    old_stdout = $stdout

    begin
      # Setup input redirection
      if redirect_in
        if File.exist?(redirect_in)
          $stdin = File.open(redirect_in, 'r')
        else
          puts "#{redirect_in}: No such file or directory"
          return
        end
      end

      # Setup output redirection
      if redirect_out
        mode = redirect_mode == :append ? 'a' : 'w'
        $stdout = File.open(redirect_out, mode)
      end

      # Execute builtin command
      send("_#{name}", *args)
    ensure
      # Close and restore
      $stdin.close if redirect_in && $stdin != old_stdin
      $stdout.close if redirect_out && $stdout != old_stdout
      $stdin = old_stdin
      $stdout = old_stdout
    end
  end

  private

  if RUBY_ENGINE == "mruby/c"
    def send(name, *args)
      case name
      when "_type"
        _type(*args)
      when "_echo"
        _echo(*args)
      when "_reboot"
        _reboot(*args)
      when "_pwd"
        _pwd(*args)
      when "_cd"
        _cd(*args)
      when "_exit", "_quit"
        _exit(*args)
      when "_jobs"
        _jobs(*args)
      when "_bg"
        _bg(*args)
      when "_fg"
        _fg(*args)
      when "_export"
        _export(*args)
      else
        raise NameError.new("undefined method `#{name}' for #{self.class}")
      end
    end
  end

  # Builtin command wishlist:
  #  alias
  #  cd
  #  echo
  #  export
  #  exec
  #  exit
  #  fc
  #  fg
  #  help
  #  history
  #  jobs
  #  kill
  #  pwd
  #  reboot
  #  type
  #  unset

  def _type(*args)
    i = 0
    while i < args.size
      name = args[i]
      puts if 0 < i
      if builtin?("_#{name}")
        print "#{name} is a shell builtin"
      elsif path = Shell.find_executable(name)
        print "#{name} is #{path}"
      else
        print "type: #{name}: not found"
      end
      i += 1
    end
    puts
  end

  def _echo(*args)
    i = 0
    while i < args.size
      print " " if 0 < i
      print args[i]
      i += 1
    end
    puts
  end

  def _reboot(*args)
    Machine.reboot 500
  end

  def _pwd(*args)
    puts(ENV['PWD'] = Dir.pwd)
  end

  def _cd(*args)
    dir = case args[0]
    when nil
      ENV['HOME'] || ENV_DEFAULT_HOME
    when "-"
      ENV['OLDPWD'] || Dir.pwd
    else
      args[0]
    end
    if File.file?(dir)
      puts "cd: #{dir}: Not a directory"
      return false
    elsif !Dir.exist?(dir)
      puts "cd: #{dir}: No such file or directory"
      return false
    end
    ENV['OLDPWD'] = Dir.pwd
    Dir.chdir(dir)
    ENV['PWD'] = Dir.pwd
  end

  def _exit(*args)
    print "bye\n\e[0m"
    Machine.exit(0)
  end
  alias _quit _exit

  def _jobs(*args)
    i = 0
    while i < @jobs.size
      job = @jobs[i]
      mark = if i == @jobs.size - 1
               "+"
             else
               " "
             end
      puts "[#{i}]#{mark}  #{job.state.to_s.ljust(16, ' ')}#{job.name}"
      i += 1
    end
  end

  def _bg(*args)
    # TODO
    puts "bg: not implemented"
  end

  def _fg(*args)
    if @jobs.empty?
      puts "fg: no jobs"
      return
    end
    num = args[0]&.to_i
    if num.nil?
      num = @jobs.size - 1
    end
    if job = @jobs[num]
      job.resume
    else
      puts "fg: #{num}: no such job"
    end
  end

  # export KEY=VALUE
  #  => args[0] = "KEY=VALUE"
  # export KEY="VALUE"
  #  => args[0] = "KEY"
  #     args[1] = "VALUE"
  # export KEY="VALUE=A"
  #  => args[0] = "KEY="
  #     args[1] = "VALUE=A"
  def _export(*args)
    if args.empty?
      env_keys = ENV.keys
      ei = 0
      while ei < env_keys.size
        key = env_keys[ei]
        puts "#{key}=\"#{ENV[key]}\""
        ei += 1
      end
      return
    end
    while arg = args.shift
      key, value = arg.split("=", 2)
      if value.nil?
        if arg = args.shift
          value = arg
        else
          value = nil
        end
      end
      ENV[key] = value || ""
    end
  end

  def cleanup_jobs
    i = @jobs.size - 1
    while 0 <= i
      if @jobs[i].state == :DORMANT
        @jobs.delete_at(i)
      end
      i -= 1
    end
  end

end

