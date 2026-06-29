class LooperCLIOptions
  attr_reader :audio, :midi_out, :click_out, :midi_thru
  attr_reader :uart_unit, :rx, :tx, :baud
  attr_reader :left, :right, :ldac, :cs, :sck, :copi

  def initialize
    @audio = :mcp4922
    @midi_out = :off
    @click_out = :audio
    @midi_thru = true
    @uart_unit = :RP2040_UART1
    @rx = 5
    @tx = 4
    @baud = 31_250
    @left = 10
    @right = 11
    @ldac = 12
    @cs = 13
    @sck = 14
    @copi = 15
  end

  def parse(args)
    i = 0
    args_size = args.size
    while i < args_size
      arg = args[i]
      case arg
      when "--help", "-h"
        return :help
      when "--no-midi-thru"
        @midi_thru = false
      when "--midi-in"
        value = option_value(args, i, arg)
        raise ArgumentError, "--midi-in supports only uart" unless value == "uart"
        i += 1
      when "--audio"
        @audio = parse_choice(option_value(args, i, arg), arg, [:mcp4922, :pwm, :off])
        i += 1
      when "--midi-out"
        @midi_out = parse_choice(option_value(args, i, arg), arg, [:off, :uart])
        i += 1
      when "--click-out"
        @click_out = parse_choice(option_value(args, i, arg), arg, [:audio, :midi, :both])
        i += 1
      when "--uart-unit"
        @uart_unit = option_value(args, i, arg).to_sym
        i += 1
      when "--rx"
        @rx = integer_value(args, i, arg)
        i += 1
      when "--tx"
        @tx = integer_value(args, i, arg)
        i += 1
      when "--baud"
        @baud = integer_value(args, i, arg)
        i += 1
      when "--left"
        @left = integer_value(args, i, arg)
        i += 1
      when "--right"
        @right = integer_value(args, i, arg)
        i += 1
      when "--ldac"
        @ldac = integer_value(args, i, arg)
        i += 1
      when "--cs"
        @cs = integer_value(args, i, arg)
        i += 1
      when "--sck"
        @sck = integer_value(args, i, arg)
        i += 1
      when "--copi"
        @copi = integer_value(args, i, arg)
        i += 1
      else
        raise ArgumentError, "unknown option: #{arg}"
      end
      i += 1
    end
    normalize
    self
  end

  private def normalize
    if @audio == :off && @click_out == :audio && @midi_out == :uart
      @click_out = :midi
    end
    if @audio == :off && @midi_out == :off
      raise ArgumentError, "--audio off requires --midi-out uart"
    end
    if (@click_out == :midi || @click_out == :both) && @midi_out == :off
      raise ArgumentError, "MIDI click output requires --midi-out uart"
    end
    if @click_out == :both && @audio == :off
      @click_out = :midi
    end
    self
  end

  private def option_value(args, index, option)
    value = args[index + 1]
    raise ArgumentError, "#{option} requires a value" if value.nil?
    value
  end

  private def integer_value(args, index, option)
    value = option_value(args, index, option)
    number = value.to_i
    if number.to_s != value && value != "0"
      raise ArgumentError, "#{option} requires an Integer"
    end
    number
  end

  private def parse_choice(value, option, choices)
    selected = value.tr("-", "_").to_sym
    i = 0
    choices_size = choices.size
    while i < choices_size
      return selected if choices[i] == selected
      i += 1
    end
    raise ArgumentError, "invalid #{option}: #{value}"
  end
end
