require 'metaprog' if RUBY_ENGINE == 'mruby/c'

class Sandbox

  TIMEOUT = 10_000 # 10 sec

  def wait(timeout: TIMEOUT)
    sleep_ms 5
    signal_self_manage = Machine.pop_signal_self_manage
    loop(timeout, signal_self_manage)
  end

  def load_file(path, join: true)
    f = File.open(path, "r")
    begin
      return nil unless rb = f.read
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
      if join && started
        wait(timeout: nil)
      end
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
