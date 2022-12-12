if RUBY_ENGINE == 'mruby/c'
  require "mml"
end

class Sounder
  SONGS = {
    beepo:    "T140 a32 < a32",
    pobeep:   "T140 O5 a32 > a32",
    beepbeep: "T250 O2 c8 r8 c4",
    oneup:    "T320 L8 O5 e g < e c d g",
  }

  def initialize(pin)
    puts "Init Sounder"
    sounder_init pin
    @playing_until = 0
    @total_duration = 0
    @locked = false
  end

  attr_accessor :playing

  def lock
    @locked = true
  end

  def unlock
    @locked = false
  end

  def add_song(name, *measures)
    SONGS[name] = measures.join
  end

  def play(*measures)
    compile(*measures)
    replay
  end

  def compile(*measures)
    if (name = measures[0]) && name.is_a?(Symbol)
      return if @last_compiled_song == name
      clear_song
      @total_duration = MML.new.compile SONGS[name].to_s do |pitch, duration|
        add_note pitch, duration
      end
      @last_compiled_song = name
    else
      clear_song
      mml = MML.new
      @total_duration = 0
      measures.each do |measure|
        @total_duration += (mml.compile measure.to_s do |pitch, duration|
          add_note pitch, duration
        end)
      end
      @last_compiled_song = nil
    end
  end

  # If `sounder.replay` in keymap.rb doesn't work,
  # inserting `sleep_ms 1` may solve the problem.
  def replay
    return if @locked
    now = board_millis
    return if now < @playing_until
    @playing_until = now + @total_duration
    sounder_replay
  end
end
