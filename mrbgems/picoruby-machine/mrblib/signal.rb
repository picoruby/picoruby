class Interrupt < Exception
end

module Signal
  HANDLERS = {
    0 => "DEFAULT",
    2 => "DEFAULT"
  }
  LIST = {
    "EXIT" => 0,
    "INT" => 2
  }

  def self.trap(signal, command = nil, &block)
    old_handler = HANDLERS[signum(signal)]
    HANDLERS[signum(signal)] = command || block
    old_handler
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
        raise RuntimeError.new("Unknown signal handler: #{handler}")
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
      raise ArgumentError.new("Unknown signal: #{signal}")
    end
    # @type var num: Integer
    num
  end

end
