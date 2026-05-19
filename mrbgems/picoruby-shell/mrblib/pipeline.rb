class Shell
  class Pipeline
    $pid_counter = 0

    def initialize(commands)
      @commands = commands
    end

    def exec
      return if @commands.empty?

      if @commands.size == 1
        Job.new(*@commands[0]).exec
        return
      end

      pids = []
      i = 0
      while i < @commands.size
        $pid_counter += 1
        pids << $pid_counter.to_s
        i += 1
      end

      i = 0
      while i < pids.size - 1
        PipelineIO.connect(pids[i], pids[i + 1])
        i += 1
      end

      jobs = []
      i = 0
      while i < @commands.size
        jobs << Job.new(*@commands[i], pid: pids[i])
        i += 1
      end

      i = jobs.size - 1
      while 0 <= i
        jobs[i].exec_async
        i -= 1
      end

      finished = []
      i = 0
      while i < jobs.size
        finished << false
        i += 1
      end

      interrupted = false
      begin
        remaining = jobs.size
        while 0 < remaining
          # Surface Ctrl-C from the OS / signal queue as Interrupt.
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
                PipelineIO.close_outbox(pids[i]) if i + 1 < pids.size
                PipelineIO.close_outbox(pids[i - 1]) if 0 < i
              end
            end
            i += 1
          end
          sleep_ms 1 if 0 < remaining && !progressed
        end
      rescue Interrupt
        interrupted = true
        # Use STDOUT (the original IO object) rather than $stdout because the
        # caller may have swapped $stdout to a file for output redirection.
        STDOUT.puts "^C"
        # Force every still-running job to DORMANT. Terminating a producer
        # closes its end implicitly; the queue closes below take care of any
        # consumer blocked in pop.
        i = 0
        while i < jobs.size
          unless finished[i]
            jobs[i].terminate
          end
          i += 1
        end
        # Close every outbox so any pending push/pop unblocks.
        i = 0
        while i < pids.size - 1
          PipelineIO.close_outbox(pids[i])
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
      PipelineIO.cleanup(pids) if pids
    end
  end
end
