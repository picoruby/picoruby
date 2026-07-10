module JS
  module WebAudio
    WAVEFORMS = ["sine", "square", "triangle", "sawtooth"]
    DEFAULT_SOURCE = :default
    DEFAULT_VOICE_COUNT = 16
    DEFAULT_MASTER_VOLUME = 0.8
    DEFAULT_PITCH_BEND_RANGE = 2
    ANALYSER_FFT_SIZE = 256
    NOTE_START_DELAY = 0.003
    VOICE_CLEANUP_INTERVAL = 0.05
    MIN_ATTACK = 0.004
    PERCUSSION_CHANNEL = 9
    PERCUSSION_NOTES = {
      35 => :kick,
      36 => :kick,
      38 => :snare,
      40 => :snare,
      41 => :low_tom,
      42 => :closed_hat,
      43 => :low_tom,
      44 => :closed_hat,
      45 => :mid_tom,
      46 => :open_hat,
      47 => :mid_tom,
      48 => :high_tom,
      50 => :high_tom
    }

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
