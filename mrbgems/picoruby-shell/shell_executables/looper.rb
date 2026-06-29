require "midibase-looper"
require "psg"
require "uart-midi"

resources = [] #: Array[untyped]
session = nil
begin
  options = LooperCLIOptions.new
  if options.parse(ARGV) == :help
    LooperConsole.usage
  else
    router = MIDIBASE::Router.new
    midi = UART::MIDI.new(
      unit: options.uart_unit,
      txd_pin: options.tx,
      rxd_pin: options.rx,
      baudrate: options.baud
    )
    looper = MIDIBASE::Looper.new(output: router)
    router.connect(:midi_in, looper, only: MIDIBASE::CHANNEL_EVENTS)

    driver = nil
    synth = nil
    if options.audio != :off
      if options.audio == :pwm
        driver = PSG::Driver.new(:pwm, left: options.left, right: options.right)
      else
        driver = PSG::Driver.new(
          :mcp4922,
          ldac: options.ldac,
          cs: options.cs,
          sck: options.sck,
          copi: options.copi
        )
      end
      voice_pools = nil #: Hash[Symbol, Array[Integer]]?
      if options.click_out == :audio || options.click_out == :both
        shared_voices = [0, 1, 2]
        voice_pools = {} #: Hash[Symbol, Array[Integer]]
        voice_pools[looper.live_source] = shared_voices
        i = 0
        track_sources = looper.track_sources
        while i < track_sources.size
          voice_pools[track_sources[i]] = shared_voices
          i += 1
        end
        voice_pools[looper.click_source] = [2]
      end
      synth = PSG::Synth.new(driver, voice_pools: voice_pools).start
      controller = PSG::MIDIController.new(synth, logger: STDOUT)
      router.connect(looper.live_source, controller, priority: 0, only: MIDIBASE::CHANNEL_EVENTS)
      i = 0
      track_sources = looper.track_sources
      while i < track_sources.size
        router.connect(track_sources[i], synth, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
        i += 1
      end
      if options.click_out == :audio || options.click_out == :both
        click_priority = PSG::Synth::DRUM_PRIORITY + 1
        router.connect(
          looper.click_source,
          synth,
          priority: click_priority,
          only: MIDIBASE::CHANNEL_EVENTS
        )
      end
    end

    if options.midi_out == :uart
      router.connect(looper.live_source, midi, only: MIDIBASE::WIRE_EVENTS) if options.midi_thru
      i = 0
      track_sources = looper.track_sources
      while i < track_sources.size
        router.connect(track_sources[i], midi, only: MIDIBASE::WIRE_EVENTS)
        i += 1
      end
      if options.click_out == :midi || options.click_out == :both
        router.connect(looper.click_source, midi, only: MIDIBASE::WIRE_EVENTS)
      end
    end

    pump = MIDIBASE::Looper::InputPump.new(midi, output: router, source: :midi_in)
    resources << pump
    resources << looper
    resources << synth if synth
    resources << driver if driver
    looper.start
    pump.start
    session = MIDIBASE::Session.new(*resources)
    session.run do
      LooperConsole.new(looper, options: options).run(session)
      session.stop
    end
  end
rescue => e
  unless session && session.stopped?
    i = 0
    while i < resources.size
      resource = resources[i]
      resource.stop if resource.respond_to?(:stop)
      resource.join if resource.respond_to?(:join)
      i += 1
    end
  end
  puts "looper: #{e.message}"
  LooperConsole.usage
end
