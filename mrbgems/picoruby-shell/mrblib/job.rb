class Shell
  class Job
    def initialize(*params, pid: nil)
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
      @in_pipeline = !pid.nil?
      if @in_pipeline
        @sandbox = Sandbox.new(pid.to_s, using: PIPELINE_USING)
        @task_name = pid.to_s
      else
        @sandbox = Sandbox.new(command)
        @task_name = command
      end
    end

    attr_reader :name

    def resume
      Signal.raise(:CONT)
      trap
      @sandbox.resume
      @sandbox.wait(timeout: nil)
      if error = @sandbox.error
        Shell.report_exception(error)
      end
      return true
    rescue Exception => e
      Shell.report_exception(e)
      return false
    end

    def state
      @sandbox.state
    end

    def exec
      @sandbox.argv = @params.dup
      trap
      @sandbox.load_file(@exefile)
      if error = @sandbox.error
        Shell.report_exception(error)
      end
      return true
    rescue Exception => e
      Shell.report_exception(e)
      # backtrace uses a lot of memory
      #if e.respond_to?(:backtrace)
      #  e.backtrace.each do |line|
      #    puts "  #{line}"
      #  end
      #end
      return false
    end

    def exec_async
      @sandbox.argv = @params.dup
      trap
      @sandbox.load_file(@exefile, join: false)
      return true
    rescue Exception => e
      Shell.report_exception(e)
      return false
    end

    def wait
      @sandbox.wait(timeout: nil)
      report_error
      return true
    rescue Exception => e
      Shell.report_exception(e)
      return false
    end

    def error
      @sandbox.error
    end

    # Force-terminate the underlying sandbox task.
    # Safe to call on an already-DORMANT task; swallows any error so the
    # pipeline executor can cancel several jobs without aborting mid-cleanup.
    def terminate
      @sandbox.terminate
    rescue Exception
      # ignore: task may already be DORMANT or in a transient state
    end

    def report_error(ignore_closed_queue: false)
      return unless error = @sandbox.error
      if ignore_closed_queue &&
         error.class.to_s == "Task::Error" &&
         error.message == "queue closed"
        return
      end
      Shell.report_exception(error)
    end

    private

    def trap
      return if @in_pipeline
      Signal.trap(:TSTP) do
        @sandbox.suspend
        puts "Suspended"
      end
    end
  end
end
