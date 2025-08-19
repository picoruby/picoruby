class Shell
  class Job
    def initialize(*params)
      if params.empty?
        raise ArgumentError, "Job requires at least one parameter"
      end
      @params = params
      @sandbox = Sandbox.new(@params[0])
    end

    def exec
      command = @params.shift
      if command.nil?
        raise ArgumentError, "Job requires a command to execute"
      end
      ARGV.clear
      @params&.each do |param|
        ARGV << param
      end
      if exefile = Shell.find_executable(command)
        @sandbox.load_file(exefile)
        @sandbox.suspend
      else
        puts "#{command}: command not found"
        return false
      end
      return true
    rescue Exception => e
      puts "#{e.message} (#{e.class})"
      # backtrace uses a lot of memory
      #if e.respond_to?(:backtrace)
      #  e.backtrace.each do |line|
      #    puts "  #{line}"
      #  end
      #end
      return false
    end

  end
end
