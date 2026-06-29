class LooperConsole
  QUANTIZE = {
    "off" => :off,
    "1/4" => :quarter,
    "1/8" => :eighth,
    "1/16" => :sixteenth,
    "1/8t" => :eighth_triplet
  }
  METRONOME = {
    "off" => :off,
    "count-in" => :count_in,
    "beat" => :beat,
    "subdivision" => :subdivision
  }

  def initialize(looper, options: nil)
    @looper = looper
    @options = options
    @record_watch_task = nil
    @console_stopped = false
    @skip_lf = false
  end

  def self.usage
    puts "Usage: looper [options]"
    puts "  --audio mcp4922|pwm|off       (default: mcp4922)"
    puts "  --midi-out off|uart           (default: off)"
    puts "  --click-out audio|midi|both   (default: audio)"
    puts "  --no-midi-thru"
    puts "  --uart-unit NAME --rx PIN --tx PIN --baud RATE"
    puts "  --ldac PIN --cs PIN --sck PIN --copi PIN"
    puts "  --left PIN --right PIN"
  end

  def run(session)
    puts "PicoRuby MIDI Looper"
    puts "Type `help` for commands."
    print_io_configuration
    while !session.stopped?
      print "looper> "
      line = read_console_line(session)
      if line.nil?
        session.stop
        break
      end
      begin
        break if execute(line.strip, session)
      rescue => e
        puts "Error: #{e.message}"
      end
    end
  ensure
    @console_stopped = true
    @record_watch_task&.terminate
  end

  private def read_console_line(session)
    line = ""
    while !session.stopped?
      char = read_console_char
      if char == "\x03"
        session.stop
        return nil
      end
      if char.nil?
        # Native `gets` blocks every other PicoRuby task on R2P2. Polling the
        # console keeps the looper clock, UART input, and PSG synth runnable.
        sleep_ms 1
        next
      end
      if @skip_lf
        @skip_lf = false
        next if char == "\n"
      end
      if char == "\r"
        @skip_lf = true
        return line
      elsif char == "\n"
        @skip_lf = false
        return line
      end
      return nil if char == "\x04" && line.empty?
      if char == "\x08" || char == "\x7f"
        line = line[0...-1] || "" unless line.empty?
      else
        line << char
      end
    end
    nil
  end

  private def read_console_char
    STDIN.read_nonblock(1)
  rescue Interrupt
    "\x03"
  end

  private def execute(line, session)
    return false if line.empty?
    args = line.split
    command = args[0]
    case command
    when "play"
      @looper.play
      puts "Playback started."
    when "stop"
      @looper.transport_stop
      puts "Stopped."
    when "record"
      voices = args[1] ? required_integer(args[1], "voices") : 1
      record_and_report(voices)
    when "tracks"
      print_tracks
    when "mute"
      index = track_index(args[1])
      @looper.mute(index)
      puts "Track #{index + 1} muted."
    when "unmute"
      index = track_index(args[1])
      @looper.unmute(index)
      puts "Track #{index + 1} unmuted."
    when "delete"
      index = track_index(args[1])
      @looper.delete(index)
      puts "Track #{index + 1} deleted."
    when "undo"
      @looper.undo
      puts "Last track deleted."
    when "clear"
      @looper.clear
      puts "All tracks cleared."
    when "tempo"
      @looper.tempo = required_integer(args[1], "tempo")
      puts "Tempo: #{@looper.status[:tempo]} BPM"
    when "meter"
      @looper.time_signature = parse_meter(args[1])
      signature = @looper.status[:time_signature]
      puts "Meter: #{signature[0]}/#{signature[1]}"
    when "bars"
      @looper.bars = required_integer(args[1], "bars")
      puts "Loop length: #{@looper.status[:bars]} bar(s)"
    when "countin"
      @looper.count_in_bars = required_integer(args[1], "countin")
      puts "Count-in: #{@looper.status[:count_in_bars]} bar(s)"
    when "quantize"
      value = QUANTIZE[args[1]]
      raise ArgumentError, "quantize must be off, 1/4, 1/8, 1/16, or 1/8t" unless value
      @looper.quantize = value
      puts "Quantize: #{args[1]}"
    when "metronome"
      value = METRONOME[args[1]]
      raise ArgumentError, "metronome must be off, count-in, beat, or subdivision" unless value
      @looper.metronome = value
      puts "Metronome: #{args[1]}"
    when "status"
      print_status
    when "help"
      print_help
    when "quit", "exit"
      session.stop
      return true
    else
      raise ArgumentError, "unknown command: #{command}"
    end
    false
  end

  private def record_and_report(voices)
    before = @looper.status
    before_tracks = before[:tracks].size
    @looper.record(voices: voices)
    status = @looper.status
    state = status[:state]
    report_recording_state(state, status)
    console = self
    # A task is required here because the console must remain able to accept `stop`.
    @record_watch_task = Task.new(name: "LooperConsole::record") do
      console.watch_recording(before_tracks, state)
    end
  end

  def watch_recording(before_tracks, state)
    while recording_pending?(state) && !@console_stopped
      sleep_ms 20
      current = @looper.state
      if current != state
        state = current
        if recording_pending?(state)
          print "\n"
          report_recording_state(state, @looper.status)
          print "looper> "
        end
      end
    end
    return if @console_stopped

    status = @looper.status
    tracks = status[:tracks]
    print "\n"
    if before_tracks < tracks.size
      track = tracks[-1]
      puts "Track #{tracks.size} recorded: voices=#{track[:voices]}, events=#{track[:events]}."
      puts "Playback continues automatically."
    elsif status[:last_error]
      puts "Recording failed: #{status[:last_error]}"
    else
      puts "Recording stopped without creating a track."
    end
    print "looper> "
  rescue RuntimeError
    nil
  end

  private def recording_pending?(state)
    state == :armed || state == :count_in || state == :recording
  end

  private def report_recording_state(state, status)
    if state == :armed
      puts "Armed. Recording starts at the next loop boundary."
    elsif state == :count_in
      puts "Count-in: #{status[:count_in_bars]} bar(s) via #{click_destination}."
      puts "Start playing after the count-in."
    elsif state == :recording
      puts "Recording #{status[:bars]} bar(s) at #{status[:tempo]} BPM..."
    end
  end

  private def print_io_configuration
    options = @options
    return unless options
    puts "MIDI in: #{options.uart_unit} RX GPIO#{options.rx} @ #{options.baud} baud"
    if options.audio == :mcp4922
      puts "Audio out: MCP4922 LDAC/CS/SCK/COPI GPIO#{options.ldac}/#{options.cs}/#{options.sck}/#{options.copi}"
    elsif options.audio == :pwm
      puts "Audio out: PWM L/R GPIO#{options.left}/#{options.right}"
    else
      puts "Audio out: off"
    end
    if options.midi_out == :uart
      puts "MIDI out: #{options.uart_unit} TX GPIO#{options.tx}"
    else
      puts "MIDI out: off"
    end
    puts "Metronome out: #{click_destination}"
  end

  private def click_destination
    options = @options
    options ? options.click_out.to_s : "configured output"
  end

  private def required_integer(value, name)
    raise ArgumentError, "#{name} requires an Integer" if value.nil?
    number = value.to_i
    if number.to_s != value && value != "0"
      raise ArgumentError, "#{name} requires an Integer"
    end
    number
  end

  private def track_index(value)
    index = required_integer(value, "track")
    raise ArgumentError, "track numbers start at 1" if index < 1
    index - 1
  end

  private def parse_meter(value)
    raise ArgumentError, "meter requires N/D" unless value
    parts = value.split("/")
    raise ArgumentError, "meter requires N/D" unless parts.size == 2
    [required_integer(parts[0], "meter"), required_integer(parts[1], "meter")]
  end

  private def print_tracks
    tracks = @looper.status[:tracks]
    if tracks.empty?
      puts "No tracks"
      return
    end
    i = 0
    tracks_size = tracks.size
    while i < tracks_size
      track = tracks[i]
      state = track[:muted] ? "muted" : "playing"
      puts "#{i + 1}: #{state}, voices=#{track[:voices]}, events=#{track[:events]}"
      i += 1
    end
  end

  private def print_status
    status = @looper.status
    signature = status[:time_signature]
    puts "state=#{status[:state]} tick=#{status[:tick]} tempo=#{status[:tempo]}"
    puts "meter=#{signature[0]}/#{signature[1]} bars=#{status[:bars]} countin=#{status[:count_in_bars]}"
    puts "quantize=#{status[:quantize]} metronome=#{status[:metronome]} available_voices=#{status[:available_voices]}"
    puts "last_error=#{status[:last_error]}" if status[:last_error]
  end

  private def print_help
    puts "play | stop | record [voices] | tracks"
    puts "  record: count-in, then record the configured number of bars"
    puts "mute N | unmute N | delete N | undo | clear"
    puts "tempo N | meter N/D | bars N | countin N"
    puts "quantize off|1/4|1/8|1/16|1/8t"
    puts "metronome off|count-in|beat|subdivision"
    puts "status | help | quit"
  end
end
