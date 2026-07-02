module JS
  module WebAudio
    WAVEFORMS = ["sine", "square", "triangle", "sawtooth"]
    DEFAULT_SOURCE = :default
    DEFAULT_VOICE_COUNT = 16
    DEFAULT_MASTER_VOLUME = 0.8
    DEFAULT_PITCH_BEND_RANGE = 2
    ANALYSER_FFT_SIZE = 256

    DEFAULT_TONE = {
      waveform: "sine",
      attack: 0.005,
      decay: 0.08,
      sustain: 0.75,
      release: 0.15,
      detune: 0.0,
      cutoff: 20_000.0,
      resonance: 0.0
    }

    PRESETS = [
      DEFAULT_TONE,
      DEFAULT_TONE.merge(waveform: "square", cutoff: 8_000.0),
      DEFAULT_TONE.merge(waveform: "triangle", attack: 0.015),
      DEFAULT_TONE.merge(waveform: "sawtooth", cutoff: 4_500.0, resonance: 2.0)
    ]

    def self.supported?
      global = JS.global
      global[:AudioContext].is_a?(JS::Object) ||
        global[:webkitAudioContext].is_a?(JS::Object)
    end

    def self.create_context
      global = JS.global
      constructor = global[:AudioContext]
      constructor = global[:webkitAudioContext] unless constructor.is_a?(JS::Object)
      unless constructor.is_a?(JS::Object)
        raise NotImplementedError, "Web Audio API is not available"
      end
      constructor.new
    end
  end
end
