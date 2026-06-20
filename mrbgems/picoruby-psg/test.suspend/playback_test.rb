class PSGPlaybackFakeDriver
  attr_reader :commands, :direct_commands, :flush_count, :join_count

  def initialize(failures: 0)
    @commands = []
    @direct_commands = []
    @flush_count = 0
    @join_count = 0
    @failures = failures
  end

  def push_command(command)
    if 0 < @failures
      @failures -= 1
      return false
    end
    @commands << command
    true
  end

  def send_reg(reg, val, tick_delay = 0)
    push_command([:send_reg, reg, val, tick_delay])
  end

  def mute(ch, flag, tick_delay = 0)
    push_command([:mute, ch, flag, tick_delay])
  end

  def set_pan(ch, pan, tick_delay = 0)
    push_command([:set_pan, ch, pan, tick_delay])
  end

  def set_timbre(ch, timbre, tick_delay = 0)
    push_command([:set_timbre, ch, timbre, tick_delay])
  end

  def set_legato(ch, legato, tick_delay = 0)
    push_command([:set_legato, ch, legato, tick_delay])
  end

  def set_lfo(ch, depth, rate, tick_delay = 0)
    push_command([:set_lfo, ch, depth, rate, tick_delay])
  end

  def write_reg_direct(reg, val)
    @direct_commands << [:write_reg_direct, reg, val]
  end

  def mute_direct(ch, flag)
    @direct_commands << [:mute_direct, ch, flag]
  end

  def buffer_flush
    @flush_count += 1
  end

  def join
    @join_count += 1
  end
end

class PSGPlaybackTest < Picotest::Test
  def setup
    unless Object.const_defined?(:Task) && Task.const_defined?(:Queue)
      skip "Task::Queue is not available"
    end
  end

  def test_enqueue_mml_play_event_as_immediate_register_writes
    driver = PSGPlaybackFakeDriver.new
    playback = PSG::Playback.new(driver, ["c"], loop: false)

    playback.__enqueue_mml_event(1, :play, 0x123, 0)

    assert_equal [:send_reg, 2, 0x23], playback.queue.pop(true)
    assert_equal [:send_reg, 3, 0x01], playback.queue.pop(true)
  end

  def test_compile_multi_yields_second_event_argument
    lfo_event = nil

    MML.compile_multi(["j1,7 c"], loop: false) do |_delta, _tr, command, arg0, arg1|
      lfo_event = [command, arg0, arg1] if command == :lfo
    end

    assert_equal [:lfo, 100, 7], lfo_event
  end

  def test_sound_loop_retries_when_driver_buffer_is_full
    driver = PSGPlaybackFakeDriver.new(failures: 1)
    playback = PSG::Playback.new(driver, ["c"], loop: false)

    playback.__write_command([:send_reg, 4, 55])

    assert_equal [[:send_reg, 4, 55, 0]], driver.commands
  end

  def test_playback_runs_mml_through_queue
    driver = PSGPlaybackFakeDriver.new
    playback = PSG::Playback.new(driver, ["t60000 l4 c"], loop: false)

    playback.start.join

    assert_true playback.queue.closed?
    assert_true driver.commands.include?([:mute, 0, 0, 0])
    assert_true !driver.commands.find { |cmd| cmd[0] == :send_reg && cmd[1] == 0 }.nil?
    assert_true driver.commands.include?([:send_reg, 8, 0, 0])
    assert_true driver.commands.include?([:mute, 0, 1, 0])
  end

  def test_stop_closes_queue_and_silences_directly
    driver = PSGPlaybackFakeDriver.new
    playback = PSG::Playback.new(driver, ["t60 l1 c"], loop: false)

    playback.start
    Task.new do
      sleep_ms 1
      playback.stop
    end
    Task.run

    assert_true playback.stopped?
    assert_true playback.queue.closed?
    assert_equal 1, driver.flush_count
    assert_true driver.direct_commands.include?([:write_reg_direct, 8, 0])
    assert_true driver.direct_commands.include?([:mute_direct, 0, 1])
  end
end
