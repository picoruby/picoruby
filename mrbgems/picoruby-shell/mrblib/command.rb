class Shell
  class Command
    def initialize
      @sandbox = Sandbox.new
    end

    BUILTIN = %w(
      free
      alias
      cd
      echo
      export
      exec
      exit
      fc
      help
      history
      kill
      pwd
      type
      unset
    )

    def find_executable(name)
      # Unlike UNIX, $PATH is an array of Ruby
      ENV['PATH']&.split(":")&.each do |path|
        file = "#{path}/#{name}"
        return file if File.exist? file
      end
      if name.include?("/") && File.exist?(name)
        return name
      end
      return nil
    end

    def exec(*params)
      ARGV.clear
      params[1, params.length - 1]&.each do |param|
        ARGV << param
      end
      command = params[0]
      case command
      when "echo"
        _echo
      when "type"
        _type
      when "pwd"
        puts Dir.pwd
      when "cd"
        dir = ARGV[0] || ENV['HOME'] || ""
        unless Dir.exist?(dir)
          puts "cd: #{dir}: No such file or directory"
          return
        end
        unless File::Stat.new(dir).directory?
          puts "cd: #{dir}: Not a directory"
          return
        end
        Dir.chdir(dir)
      when "free"
        PicoRubyVM.memory_statistics.each do |k, v|
          puts "#{k.to_s.ljust(5)}: #{v.to_s.rjust(8)}"
        end
      else
        if exefile = find_executable(command)
          f = File.open(exefile, "r")
          begin
            # PicoRuby compiler's bug
            # https://github.com/picoruby/picoruby/issues/120
            rb = f.read
            started = if rb.to_s.start_with?("RITE0300")
              # assume mruby bytecode
              @sandbox.exec_mrb(rb)
            else
              # assume Ruby script
              @sandbox.compile(rb) and @sandbox.execute
            end
            if started && @sandbox.wait(signal: (command != "irb"), timeout: nil) && error = @sandbox.error
              puts "#{error.message} (#{error.class})"
            end
          rescue => e
            p e
          ensure
            f.close
          end
        else
          puts "#{command}: command not found"
        end
      end
    end

    #
    # Builtin commands
    #

    def _type
      ARGV.each_with_index do |name, index|
        puts if 0 < index
        if BUILTIN.include?(name)
          print "#{name} is a shell builtin"
        elsif path = find_executable(name)
          print "#{name} is #{path}"
        else
          print "type: #{name}: not found"
        end
      end
      puts
    end

    def _echo
      ARGV.each_with_index do |param, index|
        print " " if 0 < index
        print param
      end
      puts
    end

  end
end
