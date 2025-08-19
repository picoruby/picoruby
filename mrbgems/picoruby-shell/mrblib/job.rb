class Shell
  class Job
    def initialize
      @sandbox = Sandbox.new('command')
    end

    def exec(*params)
      ARGV.clear
      params[1, params.length - 1]&.each do |param|
        ARGV << param
      end
      command = params[0]
      if exefile = Shell.find_executable(command)
        @sandbox.load_file(exefile)
        @sandbox.suspend
      else
        puts "#{command}: command not found"
        return false
      end
      return true
    rescue Interrupt
      puts "^C"
      return false
    rescue => e
      puts "#{e.message} (#{e.class})"
      if e.respond_to?(:backtrace)
        e.backtrace.each do |line|
          puts "  #{line}"
        end
      end
      return false
    end

  end
end
