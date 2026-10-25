require 'metaprog' if RUBY_ENGINE == 'mruby/c'

class Sandbox

  TIMEOUT = 10_000 # 10 sec

  # suspend and resume are job control (Ctrl-Z and fg in the shell). A
  # task that suspended itself waits for an event, and the event resumes
  # it: JS::Promise#await on wasm, or Task#suspend with a resume from
  # another task. suspend leaves such a task as it is, and resume does
  # not resume it. A resume from outside would run the task before the
  # event, and the resume from the event would then hit a later suspend
  # of the same task. @suspended_task records that suspend changed the
  # task state, so resume undoes only that.
  def suspend
    unless state == :SUSPENDED
      suspend_task
      @suspended_task = true
    end
    nil
  end

  def resume
    return false unless @suspended_task
    @suspended_task = false
    resume_task
  end

  def wait(timeout: TIMEOUT)
    sleep_ms 5
    signal_self_manage = Machine.pop_signal_self_manage
    loop(timeout, signal_self_manage)
  end

  def load_file(path, join: true)
    f = File.open(path, "r")
    begin
      # An empty file has nothing to run, like `require` of an empty file
      # in CRuby. Return without starting the task: an empty program never
      # reaches DORMANT on the mruby VM and wait would block forever. A
      # caller that needs content (an executable) checks the size itself.
      rb = f.read.to_s
      return nil if rb.empty?
      # exec_mrb keeps only a pointer into this string's data (it is not copied),
      # so retain it on the instance to keep it alive for the task's lifetime and
      # prevent GC from freeing the bytecode while the task is still running.
      @code = rb
      is_rite = rb.start_with?(RITE_VERSION)
      started = if is_rite
        exec_mrb(rb)
      else
        if rb.start_with?("RITE") # not valid RITE version
          raise RuntimeError, "#{path}: invalid RITE version: #{rb.byteslice(0, 8)}"
        end
        unless compile(rb, filename: path)
          raise RuntimeError, "#{path}: compile failed"
        end
        execute
      end
      # exec_mrb and execute return false when the VM rejects the bytecode.
      # Returning quietly here made a broken file look like a program that
      # ran and printed nothing.
      unless started
        raise RuntimeError, "#{path}: failed to load bytecode"
      end
      wait(timeout: nil) if join
    ensure
      f.close
    end
  end

  private

  def loop(timeout, signal_self_manage)
    n = 5
    while self.state != :DORMANT && self.state != :SUSPENDED do
      unless signal_self_manage
        # poll_signal reports the pending signal as a value (:INT / :TSTP / nil)
        # instead of raising Interrupt / SignalException, so Ctrl-C and Ctrl-Z
        # are handled with plain control flow rather than rescue clauses.
        case Machine.poll_signal
        when :INT
          begin
            Watchdog.disable
            puts "Watchdog disabled"
          rescue NameError
            # ignore. maybe POSIX
          end
          puts "^C"
          Signal.raise(:INT)
          self.stop
          # stop does not unwind the task, so an INT handler the program
          # installed (and did not remove from inside the handler) would
          # otherwise outlive its frame and fire again on the next Ctrl-C.
          Signal.trap(:INT, "DEFAULT")
          return true # should be false?
        when :TSTP
          Signal.raise(:TSTP)
          return false
        end
      end
      sleep_ms 5
      if timeout
        n += 5
        if timeout < n
          puts "Error: Timeout (sandbox.state: #{self.state})"
          return false
        end
      end
    end
    return true
  end

end
