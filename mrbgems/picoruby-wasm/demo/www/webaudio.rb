class WebAudioApp < Funicular::Component
  NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

  def initialize_state
    {
      audio_state: "disabled",
      serial_state: "disconnected",
      terminal_state: "disconnected",
      error: nil,
      selected_channel: 0,
      program: 0,
      voices: [],
      channels: [],
      waveform: "sine",
      attack: 0.005,
      decay: 0.08,
      sustain: 0.75,
      release: 0.15,
      detune: 0.0,
      cutoff: 20_000.0,
      resonance: 0.0
    }
  end

  def component_mounted
    @router = MIDIBASE::Router.new
    setup_terminal
    component = self
    @poll_task = Task.new(name: "JS::WebAudio UI poll") do
      while component.mounted
        component.refresh_runtime_state
        sleep_ms 200
      end
    end
  end

  def component_will_unmount
    @input.stop if @input
    disconnect_terminal
    @synth.stop if @synth
    @visualizer.stop if @visualizer
    @terminal_data_subscription.dispose if @terminal_data_subscription
    @terminal_resize_observer.disconnect if @terminal_resize_observer
  end

  def enable_audio(_event = nil)
    unless @synth
      @synth = JS::WebAudio::Synth.new
      @router.connect(:webserial, @synth)
      @router.connect(:ui, @synth)
    end
    @synth.resume
    attach_visualizer
    patch(audio_state: "running", error: nil)
  rescue => e
    message = "#{e.class}: #{e.message}"
    $stderr.puts "JS::WebAudio enable failed: #{message}"
    patch(audio_state: "error", error: message)
  end

  def connect_serial(_event = nil)
    enable_audio unless @synth
    component = self
    @input ||= JS::WebAudio::WebSerialMIDIInput.new(
      router: @router,
      on_state_change: proc do |connection_state, error|
        component.patch(
          serial_state: connection_state.to_s,
          error: error ? error.message : nil
        )
      end
    )
    @input.connect
  end

  def disconnect_serial(_event = nil)
    @input.disconnect if @input
  end

  def connect_terminal(_event = nil)
    return unless @terminal
    disconnect_terminal if @terminal_port
    @terminal.reset
    component = self
    JS::WebSerial.connect(baud_rate: 115_200) do |port|
      @terminal_port = port
      port.start_terminal_read(@terminal)
      port.on_disconnect do
        component.terminal_disconnected
      end
      patch(terminal_state: "connected", error: nil)
      @terminal.focus
    end
  rescue => e
    @terminal_port = nil
    patch(terminal_state: "error", error: "CDC0: #{e.class}: #{e.message}")
  end

  def disconnect_terminal(_event = nil)
    port = @terminal_port
    @terminal_port = nil
    port.close if port && port.opened?
    patch(terminal_state: "disconnected") if mounted
  rescue => e
    patch(terminal_state: "error", error: "CDC0: #{e.class}: #{e.message}") if mounted
  end

  def terminal_disconnected
    @terminal_port = nil
    patch(terminal_state: "disconnected") if mounted
  end

  def clear_terminal(_event = nil)
    @terminal.clear if @terminal
  end

  def select_channel(event)
    channel = event.target[:value].to_i
    snapshot = @synth ? @synth.channel_state(source: :webserial, channel: channel) : nil
    tone = snapshot ? snapshot[:tone] : {}
    values = { selected_channel: channel, program: snapshot ? snapshot[:program] : 0 }
    tone.each { |key, value| values[key] = value }
    patch(**values)
  end

  def tone_changed(event)
    return unless @synth
    target = event.target
    key = target.getAttribute("name").to_sym
    raw = target[:value]
    value = key == :waveform ? raw.to_s : raw.to_f
    channel = state.selected_channel
    @synth.update_tone(source: :webserial, channel: channel, **{ key => value })
    @synth.update_tone(source: :ui, channel: channel, **{ key => value })
    patch(**{ key => value, error: nil })
  rescue => e
    patch(error: e.message)
  end

  def program_changed(event)
    return unless @synth
    program = event.target[:value].to_i
    channel = state.selected_channel
    @router.emit(:webserial, [:program_change, channel, program])
    @router.emit(:ui, [:program_change, channel, program])
    tone = @synth.channel_state(source: :webserial, channel: channel)[:tone]
    patch(program: program, **tone)
  end

  def pitch_changed(event)
    return unless @synth
    value = event.target[:value].to_i
    channel = state.selected_channel
    @router.emit(:webserial, [:pitch_bend, channel, value])
    @router.emit(:ui, [:pitch_bend, channel, value])
  end

  def reset_pitch(_event = nil)
    return unless @synth
    channel = state.selected_channel
    @router.emit(:webserial, [:pitch_bend, channel, 8192])
    @router.emit(:ui, [:pitch_bend, channel, 8192])
  end

  def channel_control_changed(event)
    return unless @synth
    target = event.target
    controller = target.getAttribute("data-controller").to_i
    value = target[:value].to_i
    channel = state.selected_channel
    @router.emit(:webserial, [:control_change, channel, controller, value])
    @router.emit(:ui, [:control_change, channel, controller, value])
  end

  def reset_channel_control(event)
    return unless @synth
    target = event.target
    controller = target.getAttribute("data-controller").to_i
    value = target.getAttribute("data-reset").to_i
    channel = state.selected_channel
    @router.emit(:webserial, [:control_change, channel, controller, value])
    @router.emit(:ui, [:control_change, channel, controller, value])
  end

  def reset_tone_control(event)
    return unless @synth
    key = event.target.getAttribute("data-key").to_sym
    value = JS::WebAudio::DEFAULT_TONE[key]
    channel = state.selected_channel
    @synth.update_tone(source: :webserial, channel: channel, **{ key => value })
    @synth.update_tone(source: :ui, channel: channel, **{ key => value })
    patch(**{ key => value, error: nil })
  rescue => e
    patch(error: e.message)
  end

  def play_note(event)
    enable_audio unless @synth
    note = event.target.getAttribute("data-note").to_i
    channel = state.selected_channel
    @router.emit(:ui, [:note_on, channel, note, 110])
    component = self
    JS.global.setTimeout(240) do
      component.router_emit_note_off(channel, note)
    end
  end

  def router_emit_note_off(channel, note)
    @router.emit(:ui, [:note_off, channel, note, 0]) if @router
  end

  def refresh_runtime_state
    return unless @synth
    channels = []
    channel = 0
    while channel < 16
      channels << @synth.channel_state(source: :webserial, channel: channel)
      channel += 1
    end
    patch(voices: @synth.active_voices, channels: channels)
  end

  def setup_terminal
    constructor = JS.global[:Terminal]
    unless constructor.is_a?(JS::Object)
      patch(terminal_state: "unavailable", error: "xterm.js is unavailable")
      return
    end

    options = JS.global.create_object
    options[:scrollback] = 1_000
    options[:cursorBlink] = true
    options[:convertEol] = true
    options[:fontFamily] = "ui-monospace, SFMono-Regular, Menlo, monospace"
    options[:fontSize] = 13
    theme = JS.global.create_object
    theme[:background] = "#050b13"
    theme[:foreground] = "#dceef6"
    options[:theme] = theme

    @terminal = constructor.new(options)
    @terminal_fit_addon = JS.global[:FitAddon][:FitAddon].new
    @terminal.loadAddon(@terminal_fit_addon)
    container = refs[:terminal]
    @terminal.method_missing(:open, container)
    @terminal_fit_addon.fit

    app = self
    @terminal_data_subscription = @terminal.onData do |data|
      port = app.instance_variable_get(:@terminal_port)
      port.write(data.to_s) if port && port.opened?
    end
    @terminal_resize_observer = JS.global[:ResizeObserver].new do |_entries|
      app.fit_terminal
    end
    @terminal_resize_observer.observe(container)
  rescue => e
    patch(terminal_state: "error", error: "xterm.js: #{e.class}: #{e.message}")
  end

  def fit_terminal
    @terminal_fit_addon.fit if @terminal_fit_addon
  end

  def attach_visualizer
    return if @visualizer
    canvas = refs[:scope]
    return unless canvas
    constructor = JS.global[:PicoRubyWebAudioVisualizer]
    voice_analysers = JS::Bridge.to_js(@synth.voice_analysers)
    @visualizer = constructor.new(canvas, @synth.mix_analyser, voice_analysers)
    @visualizer.start
  end

  def render
    div(class: "app-shell") do
      header(class: "hero") do
        div do
          p(class: "eyebrow") { "PICORUBY · WEBSERIAL · JS::WEBAUDIO" }
          h1 { "CDC Synth Scope" }
          p(class: "lede") { "Raw MIDI from CDC2, interpreted by PicoRuby.WASM." }
        end
        div(class: "actions") do
          button(onclick: :enable_audio, class: "primary") { "Enable audio" }
          button(onclick: :connect_serial) { "Connect CDC2" }
          button(onclick: :disconnect_serial) { "Disconnect" }
        end
      end

      div(class: "status-row") do
        span(class: "status") { "Audio: #{state.audio_state}" }
        span(class: "status") { "Serial: #{state.serial_state}" }
        span(class: "status") { "Voices: #{state.voices.size}" }
        span(class: "error") { state.error.to_s }
      end

      section(class: "scope-panel") do
        canvas(ref: :scope, id: "scope")
      end

      div(class: "workspace") do
        section(class: "panel controls") do
          h2 { "Channel & tone" }
          label do
            span { "Channel" }
            select(onchange: :select_channel, value: state.selected_channel.to_s) do
              16.times { |index| option(value: index.to_s) { (index + 1).to_s } }
            end
          end
          label do
            span { "Program" }
            select(onchange: :program_changed, value: selected_channel_value(:program, 0).to_s) do
              JS::WebAudio::WAVEFORMS.each_with_index do |waveform, index|
                option(value: index.to_s) { "#{index}: #{waveform}" }
              end
            end
          end
          tone_select
          tone_slider("Attack", :attack, 0, 2, 0.005)
          tone_slider("Decay", :decay, 0, 2, 0.005)
          tone_slider("Sustain", :sustain, 0, 1, 0.01)
          tone_slider("Release", :release, 0, 3, 0.01)
          tone_slider("Detune", :detune, -1200, 1200, 1, reset: true)
          tone_slider("Cutoff", :cutoff, 20, 20_000, 10)
          tone_slider("Resonance", :resonance, 0, 30, 0.1)
          channel_slider("Volume", 7, 0, 127, selected_channel_value(:volume, 100), reset: 100)
          channel_slider("Pan", 10, 0, 127, selected_channel_value(:pan, 64), reset: 64)
          label do
            bend = selected_channel_value(:pitch_bend, 8192)
            div(class: "control-label") do
              span { "Pitch bend: #{bend}" }
              button(type: "button", class: "reset-control", onclick: :reset_pitch) { "Reset" }
            end
            input(type: "range", min: 0, max: 16_383, step: 1,
                  value: bend, oninput: :pitch_changed)
          end
        end

        section(class: "panel performance") do
          h2 { "Audition" }
          div(class: "keyboard") do
            note = 60
            while note <= 72
              name = NOTE_NAMES[note % 12]
              button(
                onclick: :play_note,
                class: name.include?("#") ? "key black" : "key",
                "data-note": note.to_s
              ) { "#{name}#{note / 12 - 1}" }
              note += 1
            end
          end
          h2 { "Active voices" }
          div(class: "voice-list") do
            if state.voices.empty?
              span(class: "muted") { "No active notes" }
            else
              state.voices.each do |voice|
                span(class: "voice-chip") { "v#{voice[:id]} ch#{voice[:channel] + 1} n#{voice[:note]} #{voice[:status]}" }
              end
            end
          end
          channel_grid
        end

        section(class: "panel terminal-panel") do
          div(class: "terminal-heading") do
            h2 { "R2P2 Console · CDC0" }
            div(class: "terminal-actions") do
              span(class: "terminal-status") { state.terminal_state }
              button(onclick: :connect_terminal) { "Connect CDC0" }
              button(onclick: :disconnect_terminal) { "Disconnect" }
              button(onclick: :clear_terminal) { "Clear" }
            end
          end
          div(ref: :terminal, id: "terminal-container")
        end
      end
    end
  end

  def tone_select
    label do
      span { "Waveform" }
      select(name: "waveform", onchange: :tone_changed, value: state.waveform) do
        JS::WebAudio::WAVEFORMS.each do |waveform|
          option(value: waveform) { waveform }
        end
      end
    end
  end

  def tone_slider(label_text, key, min, max, step, reset: false)
    label do
      div(class: "control-label") do
        span { "#{label_text}: #{state[key]}" }
        if reset
          button(type: "button", class: "reset-control", onclick: :reset_tone_control,
                 "data-key": key.to_s) { "Reset" }
        end
      end
      input(type: "range", name: key.to_s, min: min, max: max, step: step,
            value: state[key], oninput: :tone_changed)
    end
  end

  def channel_slider(label_text, controller, min, max, value, reset: nil)
    label do
      div(class: "control-label") do
        span { "#{label_text}: #{value}" }
        unless reset.nil?
          button(type: "button", class: "reset-control", onclick: :reset_channel_control,
                 "data-controller": controller.to_s, "data-reset": reset.to_s) { "Reset" }
        end
      end
      input(type: "range", min: min, max: max, value: value,
            "data-controller": controller.to_s, oninput: :channel_control_changed)
    end
  end

  def selected_channel_value(key, fallback)
    channel = state.channels[state.selected_channel]
    channel ? channel[key] : fallback
  end

  def channel_grid
    div(class: "channel-grid") do
      state.channels.each do |channel|
        active = 0
        state.voices.each { |voice| active += 1 if voice[:channel] == channel[:channel] }
        div(class: active > 0 ? "channel-card active" : "channel-card") do
          span(class: "channel-number") { (channel[:channel] + 1).to_s }
          span { JS::WebAudio::WAVEFORMS[channel[:program] % 4] }
          span { "vol #{channel[:volume]}" }
          span { "#{active} voice" }
        end
      end
    end
  end
end

Funicular.start(WebAudioApp, container: "app")
