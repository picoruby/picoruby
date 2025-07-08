require 'io/console'
require 'metaprog' if RUBY_ENGINE == 'mruby/c'

class Sandbox

  class Abort < StandardError
  end

  TIMEOUT = 10_000 # 10 sec

  def wait(signal: true, timeout: TIMEOUT)
    n = 0
    # state 0: TASKSTATE_DORMANT == finished
    while self.state != 0 do
      if signal && (line = STDIN.read_nonblock(1)) && line[0]&.ord == 3
        puts "^C"
        interrupt
        begin
          Watchdog.disable
          puts "Watchdog disabled"
        rescue NameError
          # ignore. maybe POSIX
        end
        return false
      end
      sleep_ms 5
      #Machine.delay_ms 50
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

  def load_file(path, signal: true, join: true)
    f = File.open(path, "r")
    # Executables in /bin/ were allocated in contiguous blocks by "File#expand"
    # See Shell#setup_system_files
    if f.respond_to?(:physical_address) && (f.size < f.sector_size || path.start_with?("/bin/"))
      physical_address = f.physical_address
      rb = ""
      is_rite = (Machine.read_memory(physical_address, 8) == "RITE0300")
    else
      physical_address = nil
      return nil unless rb = f.read
      is_rite = rb.start_with?("RITE0300")
    end
    begin
      started = if is_rite
        # assume mruby bytecode
        physical_address ? exec_mrb_from_memory(physical_address) : exec_mrb(rb)
      else
        # assume Ruby script
        if physical_address
          unless compile_from_memory(physical_address, f.size)
            raise RuntimeError, "#{path}: compile failed"
          end
        else
          unless compile(rb)
            raise RuntimeError, "#{path}: compile failed"
          end
        end
        execute
      end
      if join && started && wait(signal: signal, timeout: nil) && error = self.error
        puts "#{error.message} (#{error})"
      end
    ensure
      f.close
    end
  end
end
