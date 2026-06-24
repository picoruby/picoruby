class PSGMIDIFakeDriver
  attr_reader :commands

  def initialize
    @commands = []
  end

  def send_reg(reg, value, delay = 0)
    @commands << [:send_reg, reg, value, delay]
    true
  end

  def mute(channel, flag, delay = 0)
    @commands << [:mute, channel, flag, delay]
    true
  end

  def set_pan(channel, pan, delay = 0)
    @commands << [:set_pan, channel, pan, delay]
    true
  end
end

class PSGMIDIPlayerTest < Picotest::Test
  def command_for_register?(commands, register)
    commands.each do |command|
      return true if command[0] == :send_reg && command[1] == register
    end
    false
  end

  def setup
    @driver = PSGMIDIFakeDriver.new
    @player = PSG::MIDIPlayer.new(@driver)
  end

  def test_note_on_and_note_off
    @player.handle([:note_on, 0, 69, 127])
    assert_equal 0, @player.allocator.voice_for(0, 69)
    assert_true @driver.commands.include?([:send_reg, 8, 15, 0])
    assert_true @driver.commands.include?([:mute, 0, 0, 0])

    @player.handle([:note_off, 0, 69, 64])
    assert_nil @player.allocator.voice_for(0, 69)
    assert_equal [:mute, 0, 1, 0], @driver.commands[-1]
  end

  def test_oldest_voice_is_stolen
    @player.handle([:note_on, 0, 60, 100])
    @player.handle([:note_on, 0, 64, 100])
    @player.handle([:note_on, 0, 67, 100])
    @player.handle([:note_on, 0, 69, 100])
    assert_nil @player.allocator.voice_for(0, 60)
    assert_equal 0, @player.allocator.voice_for(0, 69)
  end

  def test_sustain_holds_and_releases_note
    @player.handle([:note_on, 0, 60, 100])
    @player.handle([:control_change, 0, 64, 127])
    @player.handle([:note_off, 0, 60, 0])
    assert_equal 0, @player.allocator.voice_for(0, 60)
    @player.handle([:control_change, 0, 64, 0])
    assert_nil @player.allocator.voice_for(0, 60)
  end

  def test_volume_pan_and_pitch_bend_update_active_voice
    @player.handle([:note_on, 2, 69, 127])
    original_size = @driver.commands.size
    @player.handle([:control_change, 2, 7, 64])
    @player.handle([:control_change, 2, 10, 127])
    @player.handle([:pitch_bend, 2, 16_383])
    commands = @driver.commands[original_size, @driver.commands.size - original_size]
    assert_true command_for_register?(commands, 8)
    assert_true commands.include?([:set_pan, 0, 15, 0])
    assert_true command_for_register?(commands, 0)
  end

  def test_channel_filter
    player = PSG::MIDIPlayer.new(@driver, channels: [2])
    player.handle([:note_on, 1, 60, 100])
    assert_nil player.allocator.voice_for(1, 60)
    player.handle([:note_on, 2, 60, 100])
    assert_equal 0, player.allocator.voice_for(2, 60)
  end

  def test_external_bpm_overrides_manual_scale
    skip "private scale wrapper requires Class" if femtoruby?
    playback_class = Class.new(PSG::Playback) do
      def effective_scale_for_test
        effective_tempo_scale_mille
      end
    end
    playback = playback_class.new(@driver, ["t120 c"], loop: false)
    playback.tempo_scale = 2.0
    playback.external_bpm = 90
    assert_equal 750, playback.effective_scale_for_test
    playback.external_bpm = nil
    assert_equal 2000, playback.effective_scale_for_test
  end

  def test_mml_emits_tempo_event
    tempo = nil
    MML.compile_multi(["t90 c"], loop: false) do |_delta, _track, command, value, _arg1|
      tempo = value if command == :tempo
    end
    assert_equal 90, tempo
  end
end
