require "env"
require "io/console"
require "picorubyvm"
require "sandbox"
require "crc"
require "picomodem"
require "machine"
require 'yaml'
require 'task-ext'
begin
  require 'gpio'
  require "littlefs"
  require "vfs"
rescue LoadError
  # ignore. maybe POSIX
  require "dir" if RUBY_ENGINE == 'mruby/c'
end

# On mruby/c, Sandbox#argv= (src/mrubyc/sandbox.c) mutates the top-level
# ARGV constant in place. r2p2 does not create that constant on startup,
# so we create it here before the Shell class block aliases Shell::ARGV
# to it. Both names then refer to the same Array object.
if RUBY_ENGINE == "mruby/c"
  ARGV = [] unless Object.const_defined?(:ARGV)
end

class Shell
  if RUBY_ENGINE == "mruby/c"
    ARGV = ::ARGV
  else
    class ShellARGV
      def method_missing(name, *args, &block)
        Task.current.argv.__send__(name, *args, &block)
      end

      def respond_to_missing?(name, include_private = false)
        Task.current.argv.respond_to?(name, include_private)
      end
    end

    ARGV = ShellARGV.new
  end

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
    fat = Littlefs.new(device, label: label)
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

  # Returns "flawless" or not
  def self.ensure_system_file(path, code, crc)
    if File.file?(path)
      print "Found: #{path}"
      File.open(path, "r") do |f|
        if f.size == code.bytesize && CRC.crc32(f.read) == crc
          puts " ... CRC match, skip writing"
          return true
        end
        print " ... CRC mismatch, "
      end
    else
      print "Not found: #{path}, "
    end
    puts "Writing #{code.bytesize} bytes"
    File.open(path, "w") do |f|
      f.write(code)
    end
    true
  rescue => e
    puts "Failed to save: #{path} (#{e.message})"
    false
  end

  # Write system executables to filesystem when firmware version
  # changes (or on first boot / recovery).
  # Fast path: skip if /etc/ruby-description matches current version.
  # Recovery: delete /etc/ruby-description and reboot to force rewrite.
  def self.setup_system_executables(root)
    desc_file = "#{root}/etc/ruby-description"
    id_file = "#{root}/etc/machine-id"
    unique_id = Machine.unique_id
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
    unless self.ensure_system_file(id_file, unique_id, CRC.crc32(unique_id))
      flawless = false
    end
    if flawless
      File.open(desc_file, "w") { |f| f.write(RUBY_DESCRIPTION) }
    elsif File.file?(desc_file)
      File.unlink(desc_file)
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
        end
        di += 1
      end
      self.setup_system_executables(root || "")
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
      Littlefs.unixtime_offset = Time.unixtime_offset
      puts "Available (#{Time.now})"
    rescue => e
      puts "Not available"
      puts "#{e.message} (#{e.class})"
    end
  end

  def self.setup_sdcard(driver)
    begin
      print "Initializing SD card(#{driver.class})... "
      sd = Littlefs.new(:sd, label: "SD", driver: driver)
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

  # Unified exception reporter. All shell-side error reporting funnels
  # through this so the user sees a consistent "<message> (<ExceptionClass>)"
  # format regardless of where the exception was raised (parser, Job
  # lifecycle, builtin dispatch, pipeline executor, ...).
  def self.report_exception(e)
    $stderr.puts "#{e.message} (#{e.class})"
  end

  def initialize(clean: false)
    require 'editor' # To save memory
    clean and IO.wait_terminal(timeout: 2) and IO.clear_screen
    @editor = Editor::Line.new
    @jobs = []
  end

  LOGO = if RUBY_ENGINE == "mruby/c"
    [
      "0111111101111111011100001110111111110011111100111111001100001101111110011000011",
      "0110000001100000011110011110000110000110000110110001101100001101100011001100110",
      "0111111001111110011011110110000110000110000110111111001100001101111110000111100",
      "0110000001100000011001100110000110000110000110110001101100001101100011000011000",
      "0110000001111111011000000110000110000011111100110001100111111001111110000011000",
      "0000000000000000000000000000000000000000000000000000000000000000000000000000000"
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
  LOGO_WIDTH = LOGO[0].bytesize
  author = "@hasumikin"
  space = " " * ((LOGO_WIDTH - author.bytesize) / 2)
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
      while x < LOGO[y].bytesize
        if LOGO[y][x] == '1' && LOGO[y+1][x-1] == '0'
          LOGO[y+1][x-1] = '2'
        end
        x += 1
      end
      y += 1
    end

    grad_start = 160 + (6 * color_num)
    grad_end = grad_start + 5
    grad_slice = LOGO[0].bytesize / 5
    shadow_offset = 144
    shadow = "\e[38;5;235m:"

    y2 = 0
    while y2 < LOGO.size
      print margin
      split_line = [] #: Array[String]
      i = 0
      while i < LOGO[y2].bytesize
        split_line << (LOGO[y2][i, grad_slice] || raise("unreachable"))
        i += grad_slice
      end
      x = 0
      si = 0
      while si < split_line.size
        snip = split_line[si]
        color = grad_start + si
        ci = 0
        while ci < snip.bytesize
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
          rescue Exception => e
            Shell.report_exception(e)
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
          if buffer.lines[-1][-1] == "\\" || !sandbox.compile("begin; _ = (#{script}); rescue Exception => _; end; _", filename: "(irb)")
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
              result = sandbox.result
              if result.is_a?(Exception)
                puts "#{result.message} (#{result.class})"
              else
                puts "=> #{result.inspect}"
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

  # Categorize a redirects AST array into a flat spec hash.
  # Returns: { in:, out:, out_mode:, err:, err_mode:, err_to_out: }
  # Later redirects of the same kind override earlier ones (last one wins).
  def classify_redirects(redirects)
    spec = {
      in: nil, out: nil, out_mode: nil,
      err: nil, err_mode: nil, err_to_out: false
    }
    ri = 0
    while ri < redirects.size
      redir = redirects[ri]
      case redir.data[:type]
      when :input
        spec[:in] = redir.data[:target]
      when :output
        spec[:out] = redir.data[:target]
        spec[:out_mode] = :write
      when :append
        spec[:out] = redir.data[:target]
        spec[:out_mode] = :append
      when :stderr_output
        spec[:err] = redir.data[:target]
        spec[:err_mode] = :write
        spec[:err_to_out] = false
      when :stderr_append
        spec[:err] = redir.data[:target]
        spec[:err_mode] = :append
        spec[:err_to_out] = false
      when :stderr_to_stdout
        spec[:err] = nil
        spec[:err_mode] = nil
        spec[:err_to_out] = true
      end
      ri += 1
    end
    spec
  end

  def redirect_spec_any?(spec)
    spec[:in] || spec[:out] || spec[:err] || spec[:err_to_out]
  end

  def execute_command_node(node)
    data = node.data
    name = data[:name]
    args = data[:args]
    background = data[:background]
    spec = classify_redirects(data[:redirects])

    if builtin?("_#{name}")
      # Builtins run synchronously in the shell main task; `&` cannot apply.
      if background
        puts "shell: '#{name}' is a builtin; '&' ignored"
      end
      if redirect_spec_any?(spec)
        execute_builtin_with_redirect(name, args, spec)
      else
        send("_#{name}", *args)
      end
    else
      cmd_args = [name] + args

      if redirect_spec_any?(spec)
        if background
          # The redirect file handles are scoped to the with_io_redirects
          # ensure block; running async would close them before the sandbox
          # task finishes writing. Not supported yet.
          puts "shell: background with redirect not supported yet; running in foreground"
        end
        execute_with_file_redirect(cmd_args, spec)
      elsif background
        job = Job.new(*cmd_args)
        @jobs << job
        job.exec_async
        puts "[#{@jobs.size - 1}] #{job.name}"
      else
        job = Job.new(*cmd_args)
        @jobs << job
        job.exec
      end
    end
  end

  def execute_pipeline_node(node)
    if RUBY_ENGINE == "mruby/c"
      # Pipelines require task-scoped refinements (PipeOUT / PipeIN) to
      # rewrite Kernel#puts/print/gets/getc per segment. mruby/c does not
      # support Module#refine and the feature is not planned, so we
      # surface a clear error instead of crashing somewhere downstream.
      raise "pipe (`|`) requires task-scoped refinements, which are not supported on FemtoRuby"
    end
    if node.data[:background]
      # Backgrounding a pipeline would require lifecycle tracking for the
      # whole segment group (queues, sandboxes, eventual PipelineIO.cleanup).
      # Not implemented yet.
      puts "shell: background pipeline not supported yet; running in foreground"
    end
    commands = node.data[:commands]

    # Builtins (cd, echo, pwd, jobs, ...) run synchronously in the shell
    # main task; they can't be sandboxed into a pipeline segment. Reject
    # the whole pipeline up front so the user sees a clear message instead
    # of a downstream "command not found".
    ci = 0
    while ci < commands.size
      bname = commands[ci].data[:name]
      if builtin?("_#{bname}")
        puts "shell: builtin '#{bname}' cannot be used as a pipeline segment"
        return
      end
      ci += 1
    end

    last_idx = commands.size - 1

    # Build a per-pipeline redirect spec.
    # Rules:
    # - input (`<`) is taken from the FIRST segment only.
    # - output (`>`, `>>`) and stderr (`2>`, `2>>`, `2>&1`) are taken from the
    #   LAST segment only.
    # - Other placements (middle segments, output on first, input on last,
    #   stderr on non-last) are warned about and ignored.
    spec = {
      in: nil, out: nil, out_mode: nil,
      err: nil, err_mode: nil, err_to_out: false
    }

    ci = 0
    while ci < commands.size
      cmd_redirects = commands[ci].data[:redirects]
      ri = 0
      while ri < cmd_redirects.size
        redir = cmd_redirects[ri]
        case redir.data[:type]
        when :input
          if ci == 0
            spec[:in] = redir.data[:target]
          else
            puts "Warning: input redirect on non-first segment ignored"
          end
        when :output
          if ci == last_idx
            spec[:out] = redir.data[:target]
            spec[:out_mode] = :write
          else
            puts "Warning: output redirect on non-last segment ignored"
          end
        when :append
          if ci == last_idx
            spec[:out] = redir.data[:target]
            spec[:out_mode] = :append
          else
            puts "Warning: append redirect on non-last segment ignored"
          end
        when :stderr_output
          # Stderr redirects are accepted on any segment because $stderr is
          # a global swapped once for the whole pipeline. Last writer wins.
          spec[:err] = redir.data[:target]
          spec[:err_mode] = :write
          spec[:err_to_out] = false
        when :stderr_append
          spec[:err] = redir.data[:target]
          spec[:err_mode] = :append
          spec[:err_to_out] = false
        when :stderr_to_stdout
          spec[:err] = nil
          spec[:err_mode] = nil
          spec[:err_to_out] = true
        end
        ri += 1
      end
      ci += 1
    end

    cmd_arrays = [] #: Array[Array[String]]
    ci = 0
    while ci < commands.size
      cmd_node = commands[ci]
      cmd_arrays << ([cmd_node.data[:name]] + cmd_node.data[:args])
      ci += 1
    end

    if redirect_spec_any?(spec)
      with_io_redirects(spec) do
        begin
          pipeline = Pipeline.new(cmd_arrays)
          pipeline.exec
        rescue Exception => e
          Shell.report_exception(e)
        end
      end
    else
      pipeline = Pipeline.new(cmd_arrays)
      pipeline.exec
    end
  end

  # Apply a redirect spec, run the given block, then restore globals.
  # Returns true on success, false if a required input file is missing.
  # Order applied: stdin -> stdout -> stderr (so `> out 2>&1` lands stderr on
  # the redirected stdout). `2>&1` written BEFORE the stdout redirect on the
  # same command will still resolve stderr to the post-redirect stdout in
  # this implementation, which differs slightly from bash; see plan doc.
  def with_io_redirects(spec, &block)
    saved_stdin = $stdin
    saved_stdout = $stdout
    saved_stderr = $stderr
    opened = []
    applied = false
    begin
      if spec[:in]
        unless File.exist?(spec[:in])
          puts "#{spec[:in]}: No such file or directory"
          return false
        end
        f = File.open(spec[:in], 'r')
        opened << f
        $stdin = f
      end
      if spec[:out]
        mode = spec[:out_mode] == :append ? 'a' : 'w'
        f = File.open(spec[:out], mode)
        opened << f
        $stdout = f
      end
      if spec[:err]
        mode = spec[:err_mode] == :append ? 'a' : 'w'
        f = File.open(spec[:err], mode)
        opened << f
        $stderr = f
      elsif spec[:err_to_out]
        # snapshot current $stdout (may already be a redirected file)
        $stderr = $stdout
      end
      applied = true
      block.call
      true
    ensure
      if applied
        opened.each do |f|
          begin
            f.close
          rescue Exception
            # ignore close errors
          end
        end
        $stdin = saved_stdin
        $stdout = saved_stdout
        $stderr = saved_stderr
      end
    end
  end

  def execute_with_file_redirect(cmd_args, spec)
    with_io_redirects(spec) do
      # Catch exceptions inside the redirect scope so that error messages
      # (including "command not found" raised in Job.new) reach the
      # redirected $stderr instead of the restored original one.
      begin
        job = Job.new(*cmd_args)
        job.exec
      rescue Exception => e
        Shell.report_exception(e)
      end
    end
  end

  def execute_builtin_with_redirect(name, args, spec)
    with_io_redirects(spec) do
      begin
        send("_#{name}", *args)
      rescue Exception => e
        Shell.report_exception(e)
      end
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
