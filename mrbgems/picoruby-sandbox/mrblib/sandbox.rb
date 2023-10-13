class Sandbox

  class Abort < Exception
  end

  TIMEOUT = 10_000 # 10 sec

  def wait(signal: true, timeout: TIMEOUT)
    n = 0
    # state 0: TASKSTATE_DORMANT == finished
    while self.state != 0 do
      if signal && (line = IO.read_nonblock(1)) && line[0]&.ord == 3
        puts "^C"
        interrupt
        return false
      end
      sleep_ms 50
      if timeout
        n += 50
        if timeout < n
          puts "Error: Timeout (sandbox.state: #{self.state})"
          return false
        end
      end
    end
    return true
  end

  def load_file(path, signal: true)
    f = File.open(path, "r")
    begin
      # PicoRuby compiler's bug
      # https://github.com/picoruby/picoruby/issues/120
      rb = f.read
      return nil unless rb
      started = if rb.to_s.start_with?("RITE0300")
        # assume mruby bytecode
        exec_mrb(rb)
      else
        # assume Ruby script
        compile(rb) and execute
      end
      if started && wait(signal: signal, timeout: nil) && error
        puts "#{error.message} (#{error.class})"
      end
    rescue => e
      p e
    ensure
      f.close
    end
  end
end
