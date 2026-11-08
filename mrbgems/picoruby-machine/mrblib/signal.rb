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
    num = signum(signal)
    old_handler = HANDLERS[num]
    handler = command || block
    HANDLERS[num] = handler
    # A POSIX process with no SIGINT handler of its own (the picoruby
    # command) catches the signal for a Proc; see __dispatch_pending
    if num == 2 && Machine.respond_to?(:__trap_sigint)
      Machine.__trap_sigint(handler.is_a?(Proc))
    end
    old_handler
  end

  # Run the handler of a SIGINT the process caught since the last call, as
  # CRuby would have when it arrived. A waiting loop calls this between
  # waits, where running Ruby code is safe.
  def self.__dispatch_pending
    if Machine.respond_to?(:__sigint_pending?) && Machine.__sigint_pending?
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
        raise RuntimeError.new("Unknown signal handler: #{handler}") # steep:ignore
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
      raise ArgumentError.new("Unknown signal: #{signal}") # steep:ignore
    end
    # @type var num: Integer
    num
  end

end

module Kernel
  # trap(signal, command = nil) { } is Signal.trap, as in CRuby
  def trap(signal, command = nil, &block)
    Signal.trap(signal, command, &block) # steep:ignore UnresolvedOverloading
  end
end
