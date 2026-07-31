class Shell
  class Job
    def initialize(*params)
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
      @sandbox = Sandbox.new(command)
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
      @sandbox.state
    end

    def close
      sandbox = @sandbox
      return if sandbox.nil?
      sandbox.close
      @sandbox = nil
    end

    def exec
      ARGV.clear
      i = 0
      while i < @params.size
        ARGV << @params[i]
        i += 1
      end
      trap
      before = task_ids
      begin
        @sandbox.load_file(@exefile)
      rescue Exception
        reap(before)
        raise
      end
      if error = @sandbox.error
        reap(before)
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

    private

    # Terminate tasks the job started but left behind, and ONLY when the
    # job died: a script killed by e.g. NoMemoryError never reaches the
    # code that would stop its own tasks, and such an orphan keeps running
    # with nothing left to stop it -- burning CPU and heap until the shell
    # itself starves. A job that ends normally keeps its tasks, so a script
    # can still start a long-lived background worker on purpose.
    def reap(before)
      Task.list.each do |t|
        # Identity has to go through object_id: mruby/c compares two
        # objects of the same class as equal, so Array#include? on the
        # Task objects themselves would treat every task as "seen".
        next if before.include?(t.object_id)
        begin
          t.terminate
        rescue Exception
          # a task that already finished is not an error
        end
      end
    rescue Exception
      # never let cleanup mask the job's own result
    end

    def task_ids
      ids = []
      Task.list.each { |t| ids << t.object_id }
      ids
    end

    def trap
      Signal.trap(:TSTP) do
        @sandbox.suspend
        puts "Suspended"
      end
    end
  end
end
