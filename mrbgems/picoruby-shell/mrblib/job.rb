class Shell
  class Job
    def initialize(*params, pool: nil)
      if params.empty?
        raise ArgumentError, "Job requires at least one parameter"
      end
      @name = params.join(" ")
      command = params.shift
      if command.nil?
        raise ArgumentError, "Job requires a command"
      end
      @params = params
      unless @exefile = Shell.find_executable(command)
        raise "#{command}: command not found"
      end
      @pool = pool
      @sandbox = nil
      @command = command
    end

    attr_reader :name

    def resume
      Signal.raise(:CONT)
      trap
      @sandbox.resume
      @sandbox.wait(timeout: nil)
      if error = @sandbox.error
        puts "#{error.message} (#{error.class})"
      end
      return true
    rescue Exception => e
      puts "#{e.message} (#{e.class})"
      return false
    end

    def state
      @sandbox.state || :DORMANT
    end

    def exec
      # Acquire sandbox from pool or create new one
      @sandbox = if @pool
        @pool&.acquire
      else
        Sandbox.new(@command)
      end

      ARGV.clear
      @params.each do |param|
        ARGV << param
      end
      trap
      @sandbox.load_file(@exefile)
      if error = @sandbox.error
        puts "\n#{error.message} (#{error.class})"
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

    def release_sandbox
      if @pool && @sandbox
        @pool&.release(@sandbox)
        @sandbox = nil
      elsif @sandbox
        @sandbox.terminate
        @sandbox = nil
      end
    end

    private

    def trap
      Signal.trap(:TSTP) do
        @sandbox.suspend
        puts "Suspended"
      end
    end
  end
end
