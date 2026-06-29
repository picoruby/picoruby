require "picooptparse"

class LooperCLIOptions
  attr_reader :audio, :midi_out, :click_out, :midi_thru
  attr_reader :uart_unit, :rx, :tx, :baud
  attr_reader :left, :right, :ldac, :cs, :sck, :copi

  def self.build_parser
    parser = PicoOptionParser.new
    parser.flag("--help", "-h", help: true, desc: "show this help")
    parser.flag("--no-midi-thru", default: true, desc: "send recorded tracks only")
    parser.on("--midi-in", type: :string, choices: ["uart"], default: nil, desc: "MIDI input (uart only)")
    parser.on("--audio", type: :symbol, choices: [:mcp4922, :pwm, :off], default: :mcp4922, desc: "audio backend")
    parser.on("--midi-out", type: :symbol, choices: [:off, :uart], default: :off, desc: "MIDI output")
    parser.on("--click-out", type: :symbol, choices: [:audio, :midi, :both], default: :audio, desc: "metronome routing")
    parser.on("--uart-unit", type: :symbol, default: :RP2040_UART1, desc: "UART unit")
    parser.on("--rx", type: :integer, default: 5, desc: "UART RX pin")
    parser.on("--tx", type: :integer, default: 4, desc: "UART TX pin")
    parser.on("--baud", type: :integer, default: 31_250, desc: "UART baud")
    parser.on("--left", type: :integer, default: 10, desc: "PWM left pin")
    parser.on("--right", type: :integer, default: 11, desc: "PWM right pin")
    parser.on("--ldac", type: :integer, default: 12, desc: "MCP4922 LDAC pin")
    parser.on("--cs", type: :integer, default: 13, desc: "MCP4922 CS pin")
    parser.on("--sck", type: :integer, default: 14, desc: "MCP4922 SCK pin")
    parser.on("--copi", type: :integer, default: 15, desc: "MCP4922 COPI pin")
    parser
  end

  def parse(args)
    result = self.class.build_parser.parse(args)
    return :help if result == :help
    @audio = result[:audio]
    @midi_out = result[:midi_out]
    @click_out = result[:click_out]
    @midi_thru = result[:midi_thru]
    @uart_unit = result[:uart_unit]
    @rx = result[:rx]
    @tx = result[:tx]
    @baud = result[:baud]
    @left = result[:left]
    @right = result[:right]
    @ldac = result[:ldac]
    @cs = result[:cs]
    @sck = result[:sck]
    @copi = result[:copi]
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
end
