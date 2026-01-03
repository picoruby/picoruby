require 'io/console'
require 'metaprog' if RUBY_ENGINE == 'mruby/c'

class Sandbox

  TIMEOUT = 10_000 # 10 sec

  def wait(timeout: TIMEOUT)
    sleep_ms 5
    signal_self_manage = ENV.delete('SIGNAL_SELF_MANAGE')
    loop(timeout, signal_self_manage)
  end

  def load_file(path, join: true)
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
      if join && started
        wait(timeout: nil)
      end
    ensure
      f.close
    end
  end

  case RUBY_ENGINE
  when "mruby/c"
    # For mruby/c: state_code and state_reason are defined in C side
    # state method is composed in Ruby side
    # Maps C's TASKSTATUS enum values from picoruby-mruby/include/task.h
    # TASKSTATUS_DORMANT=0x00, TASKSTATUS_READY=0x02, TASKSTATUS_RUNNING=0x03,
    # TASKSTATUS_WAITING=0x04, TASKSTATUS_SUSPENDED=0x08
    TASKSTATUS_CODE = { 0 => :DORMANT, 2 => :READY, 3 => :RUNNING, 4 => :WAITING, 8 => :SUSPENDED }
    # Maps C's TASKREASON enum values from picoruby-mruby/include/task.h
    # TASKREASON_SLEEP=0x01, TASKREASON_MUTEX=0x02, TASKREASON_JOIN=0x04
    TASKREASON_CODE = { 1 => :SLEEP, 2 => :MUTEX, 4 => :JOIN }

    def state
      code = state_code
      reason = state_reason
      state_sym = TASKSTATUS_CODE[code] || :UNKNOWN

      if reason && TASKREASON_CODE[reason]
        "#{state_sym}#{TASKREASON_CODE[reason]}".to_sym
      else
        state_sym
      end
    end
  when "mruby"
    # For mruby: state method is implemented in C side
    # No implementation in Ruby side
  end

  private

  def loop(timeout, signal_self_manage)
    n = 5
    while self.state != :DORMANT && self.state != :SUSPENDED do
      STDIN.read_nonblock(1) unless signal_self_manage
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
  rescue Interrupt
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
  rescue SignalException => e
    if e.message == "SIGTSTP"
      Signal.raise(:TSTP)
    end
    return false
  end

end
