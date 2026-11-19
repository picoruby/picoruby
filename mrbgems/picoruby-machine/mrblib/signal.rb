class SignalException < Exception
end

class Interrupt < SignalException
end

module Signal
  HANDLERS = {
    0 => "DEFAULT",
    2 => "DEFAULT",
    18 => "DEFAULT",
    20 => "DEFAULT",
  }
  LIST = {
    "EXIT" => 0,
    "INT" => 2,
    "CONT" => 18,
    "TSTP" => 20,
  }

  def self.trap(signal, command = nil, &block)
    # @type var signal: Signal::signal_t
    # @type var command: Signal::handler_t
    num = __trap_key(signal)
    old_handler = HANDLERS[num]
    handler = command || block
    HANDLERS[num] = handler
    # A POSIX process with no SIGINT handler of its own (the picoruby
    # command) catches the signal for a Proc; see __dispatch_pending. On the
    # mruby VM the method always exists and answers false where nothing is
    # caught (r2p2). mruby/c catches nothing, and its Machine has no C
    # methods before `require "machine"`, so it is not asked there.
    if num == 2 && RUBY_ENGINE == "mruby"
      Machine.__trap_sigint(handler.is_a?(Proc))
    end
    old_handler
  end

  # Run the handler of a SIGINT the process caught since the last call, as
  # CRuby would have when it arrived. A waiting loop calls this between
  # waits, where running Ruby code is safe.
  def self.__dispatch_pending
    # RUBY_ENGINE, not respond_to?: see trap
    if RUBY_ENGINE == "mruby" && Machine.__sigint_pending?
      self.raise(:INT)
    end
  end

  def self.list
    LIST
  end

  def self.raise(signal)
    handler = HANDLERS[signum(signal)]
    if handler.is_a?(Proc)
      handler.call
    else
      case handler
      when nil, "", "SIG_IGN", "DEFAULT"
        return
      else
        # Kernel's: a bare raise here is this method
        Kernel.raise RuntimeError.new("Unknown signal handler: #{handler}")
      end
    end
  end

  private

  def self.signum(signal)
    num = case signal
          when Integer
            if LIST.values.any? { |v| v == signal }
              signal
            else
              nil
            end
          when String, Symbol
            LIST[signal.to_s]
          end
    if num.nil?
      # Kernel's: a bare raise here is Signal.raise, which calls signum
      Kernel.raise ArgumentError.new("Unknown signal: #{signal}")
    end
    # @type var num: Integer
    num
  end

  # Where trap keeps the handler for signal: under its number for a signal
  # this module delivers, and for one only the host lists (Signal.list from
  # mruby-signal, as INFO) under its name or host number, apart from those,
  # as for a process that never receives it.
  def self.__trap_key(signal)
    host = list
    unless host == LIST
      case signal
      when String, Symbol
        name = signal.to_s
        return name if LIST[name].nil? && host[name]
      when Integer
        return signal if !LIST.values.include?(signal) && host.values.include?(signal)
      end
    end
    signum(signal)
  end

end

module Kernel
  # trap(signal, command = nil) { } is Signal.trap, as in CRuby
  def trap(signal, command = nil, &block)
    Signal.trap(signal, command, &block) # steep:ignore UnresolvedOverloading
  end
end
