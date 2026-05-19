class Shell
  class Pipeline
    def initialize(commands)
      @commands = commands
    end

    def exec
      return if @commands.empty?

      if @commands.size == 1
        Job.new(*@commands[0]).exec
        return
      end

      # Build one Task::Pipe per inter-stage link.
      # pipes[i] connects stage i (producer) to stage i+1 (consumer).
      pipes = [] #: Array[Task::Pipe]
      i = 0
      while i < @commands.size - 1
        pipes << Task::Pipe.new(Task::Queue.new)
        i += 1
      end

      jobs = [] #: Array[Job]
      i = 0
      while i < @commands.size
        out_pipe = i < pipes.size ? pipes[i] : nil
        in_pipe  = 0 < i ? pipes[i - 1] : nil
        jobs << Job.new(*@commands[i], stdout: out_pipe, stdin: in_pipe)
        i += 1
      end

      i = jobs.size - 1
      while 0 <= i
        jobs[i].exec_async
        i -= 1
      end

      finished = [] #: Array[bool]
      i = 0
      while i < jobs.size
        finished << false
        i += 1
      end

      interrupted = false
      begin
        remaining = jobs.size
        while 0 < remaining
          Machine.check_signal
          progressed = false
          i = 0
          while i < jobs.size
            unless finished[i]
              state = jobs[i].state
              if state == :DORMANT || state == :SUSPENDED
                finished[i] = true
                remaining -= 1
                progressed = true
                # Close the outgoing pipe of this stage so the downstream
                # consumer's pop() unblocks (SIGPIPE-equivalent).
                pipes[i].close if i < pipes.size
                # Close the incoming pipe of the preceding stage too so
                # a still-running producer gets a closed-queue error and stops.
                pipes[i - 1].close if 0 < i && !pipes[i - 1].closed?
              end
            end
            i += 1
          end
          sleep_ms 1 if 0 < remaining && !progressed
        end
      rescue Interrupt
        interrupted = true
        STDOUT.puts "^C"
        i = 0
        while i < jobs.size
          unless finished[i]
            jobs[i].terminate
          end
          i += 1
        end
        i = 0
        while i < pipes.size
          pipes[i].close unless pipes[i].closed?
          i += 1
        end
        Signal.raise(:INT)
      end

      unless interrupted
        i = 0
        while i < jobs.size
          jobs[i].report_error(ignore_closed_queue: i + 1 < jobs.size)
          i += 1
        end
      end
    ensure
      # Close any pipes that were not already closed (e.g. on exceptions).
      if pipes
        i = 0
        while i < pipes.size
          pipes[i].close unless pipes[i].closed?
          i += 1
        end
      end
    end
  end
end
