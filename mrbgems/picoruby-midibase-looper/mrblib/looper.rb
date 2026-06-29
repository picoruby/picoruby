require "midibase"

module MIDIBASE
  class Looper
    PPQN = 480
    DEFAULT_TEMPO = 120
    DEFAULT_MAX_EVENTS = 256
    DEFAULT_VOICE_CAPACITY = 3

    LIVE_SOURCE = :looper_live
    CLICK_SOURCE = :looper_click

    QUANTIZE_GRIDS = {
      off: nil,
      quarter: 480,
      eighth: 240,
      sixteenth: 120,
      eighth_triplet: 160
    }
    METRONOME_MODES = [:off, :count_in, :beat, :subdivision]
    TIME_SIGNATURES = [[3, 4], [4, 4], [6, 8], [12, 8]]

    attr_reader :state, :tracks, :live_source, :click_source, :track_sources

    def initialize(
          output:,
          tempo: DEFAULT_TEMPO,
          time_signature: [4, 4],
          bars: 4,
          count_in_bars: 1,
          quantize: :sixteenth,
          metronome: :count_in,
          voice_capacity: DEFAULT_VOICE_CAPACITY,
          max_events_per_track: DEFAULT_MAX_EVENTS,
          time_source: Machine)
      unless output.respond_to?(:emit)
        raise ArgumentError, "output must respond to emit"
      end
      unless voice_capacity.is_a?(Integer) && 0 < voice_capacity
        raise ArgumentError, "voice_capacity must be a positive Integer"
      end
      unless max_events_per_track.is_a?(Integer) && 0 < max_events_per_track
        raise ArgumentError, "max_events_per_track must be a positive Integer"
      end

      @output = output
      unless time_source.respond_to?(:uptime_us)
        raise ArgumentError, "time_source must respond to uptime_us"
      end
      @time_source = time_source
      @voice_capacity = voice_capacity
      @max_events_per_track = max_events_per_track
      @live_source = LIVE_SOURCE
      @click_source = CLICK_SOURCE
      @track_sources = [] #: Array[Symbol]
      i = 0
      while i < voice_capacity
        @track_sources << "looper_track_#{i}".to_sym
        i += 1
      end

      @tracks = [] #: Array[Track]
      @live_notes = [] #: Array[Array[untyped]]
      @click_notes = [] #: Array[Array[Integer]]
      @queue = Task::Queue.new
      @task = nil
      @shutdown = false
      @state = :stopped
      @anchor_us = nil
      @anchor_tick = 0
      @next_click_tick = nil
      @recorder = nil
      @record_voice_limit = nil
      @record_start_tick = nil
      @record_end_tick = nil
      @last_error = nil

      set_tempo(tempo)
      set_time_signature(time_signature)
      set_bars(bars)
      set_count_in_bars(count_in_bars)
      set_quantize(quantize)
      set_metronome(metronome)
    end

    def start
      return self if @task
      @shutdown = false
      looper = self
      @task = Task.new(name: "MIDIBASE::Looper") { looper.run }
      self
    end

    def run
      while !@shutdown
        drain_queue
        advance(now_us)
        sleep_ms 1
      end
    ensure
      silence_all(now_us)
    end

    def stop
      return self if @shutdown
      @shutdown = true
      @queue.close unless @queue.closed?
      silence_all(now_us) unless @task
      self
    end

    def join
      @task&.join
      self
    rescue RuntimeError
      Task.run
      self
    end

    def handle(event, source: nil, priority: 0, timestamp_us: nil, **_context)
      return false if @shutdown
      timestamp_us ||= now_us
      if @task
        @queue.push([:midi, event, timestamp_us])
      else
        process_midi(event, timestamp_us)
      end
      true
    rescue Task::Error
      false
    end

    def play
      submit(:play)
    end

    def transport_stop
      submit(:transport_stop)
    end

    def record(voices: 1)
      submit(:record, voices)
    end

    def mute(index)
      submit(:mute, index)
    end

    def unmute(index)
      submit(:unmute, index)
    end

    def delete(index)
      submit(:delete, index)
    end

    def undo
      submit(:undo)
    end

    def clear
      submit(:clear)
    end

    def tempo=(value)
      submit(:tempo, value)
    end

    def time_signature=(value)
      submit(:time_signature, value)
    end

    def bars=(value)
      submit(:bars, value)
    end

    def count_in_bars=(value)
      submit(:count_in_bars, value)
    end

    def quantize=(value)
      submit(:quantize, value)
    end

    def metronome=(value)
      submit(:metronome, value)
    end

    def status
      submit(:status)
    end

    def advance(now_us)
      flush_click_note_offs(now_us)
      return self if @state == :stopped

      current_tick = tick_at_time(now_us)
      record_start_tick = @record_start_tick
      if @state == :count_in && record_start_tick && record_start_tick <= current_tick
        begin_recording(record_start_tick)
      elsif @state == :armed && record_start_tick && record_start_tick <= current_tick
        begin_recording(record_start_tick)
      end

      record_end_tick = @record_end_tick
      if @state == :recording && record_end_tick && record_end_tick <= current_tick
        current_tick = finish_recording(now_us)
      end

      process_clicks(current_tick)
      process_track_events(current_tick)
      self
    end

    private def submit(command, *args)
      if @task
        reply = Task::Queue.new
        @queue.push([:command, command, args, reply])
        result = reply.pop
        raise result if result.is_a?(Exception)
        result
      else
        process_command(command, args, now_us)
      end
    rescue Task::Error
      raise RuntimeError, "looper is stopped"
    end

    private def drain_queue
      queue = @queue
      while !queue.empty?
        message = queue.pop(true)
        if message[0] == :midi
          process_midi(message[1], message[2])
        else
          reply = message[3]
          begin
            reply.push(process_command(message[1], message[2], now_us))
          rescue => e
            reply.push(e)
          end
        end
      end
    rescue Task::Error
      nil
    end

    private def process_command(command, args, now_us)
      case command
      when :play then command_play(now_us)
      when :transport_stop then command_transport_stop(now_us)
      when :record then command_record(args[0], now_us)
      when :mute then command_mute(args[0], now_us)
      when :unmute then track_at(args[0]).muted = false
      when :delete then command_delete(args[0], now_us)
      when :undo then command_undo(now_us)
      when :clear then command_clear(now_us)
      when :tempo then command_tempo(args[0], now_us)
      when :time_signature then set_time_signature(args[0])
      when :bars then set_bars(args[0])
      when :count_in_bars then set_count_in_bars(args[0])
      when :quantize then set_quantize(args[0])
      when :metronome then set_metronome(args[0])
      when :status then status_snapshot(now_us)
      else raise ArgumentError, "unknown looper command: #{command}"
      end
    end

    private def command_play(now_us)
      return self unless @state == :stopped
      start_transport(now_us)
      @state = :playing
      configure_click(tick_at_time(now_us))
      self
    end

    private def command_transport_stop(now_us)
      silence_all(now_us)
      @state = :stopped
      @recorder = nil
      @record_voice_limit = nil
      @record_start_tick = nil
      @record_end_tick = nil
      @anchor_us = nil
      @anchor_tick = 0
      @next_click_tick = nil
      reset_tracks
      self
    end

    private def command_record(voices, now_us)
      unless voices.is_a?(Integer) && 0 < voices
        raise ArgumentError, "voices must be a positive Integer"
      end
      unless @state == :stopped || @state == :playing
        raise RuntimeError, "recording is already armed"
      end
      available = available_recording_voices
      if available < voices
        raise ArgumentError, "only #{available} recording voice(s) available"
      end
      if @tracks.size >= @track_sources.size
        raise RuntimeError, "track limit reached"
      end

      @last_error = nil
      @record_voice_limit = voices
      limit_live_notes(voices, now_us)
      if @tracks.empty?
        start_transport(now_us) if @state == :stopped
        current = tick_at_time(now_us)
        count_ticks = @count_in_bars * ticks_per_bar
        @record_start_tick = current + count_ticks
        @state = count_ticks == 0 ? :recording : :count_in
        if @state == :recording
          begin_recording(current)
        else
          configure_click(current)
        end
      else
        start_transport(now_us) if @state == :stopped
        current = tick_at_time(now_us)
        @record_start_tick = (current / loop_ticks + 1) * loop_ticks
        @state = :armed
        configure_click(current)
      end
      self
    end

    private def command_mute(index, now_us)
      track = track_at(index)
      silence_track(track, now_us)
      track.muted = true
      self
    end

    private def command_delete(index, now_us)
      track = track_at(index)
      silence_track(track, now_us)
      @tracks.delete_at(index)
      self
    end

    private def command_undo(now_us)
      raise RuntimeError, "no track to undo" if @tracks.empty?
      command_delete(@tracks.size - 1, now_us)
    end

    private def command_clear(now_us)
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        silence_track(tracks[i], now_us)
        i += 1
      end
      tracks.clear
      command_transport_stop(now_us)
    end

    private def command_tempo(value, now_us)
      current_tick = @anchor_us ? tick_at_time(now_us) : 0
      set_tempo(value)
      if @anchor_us
        @anchor_us = now_us
        @anchor_tick = current_tick
      end
      @tempo
    end

    private def process_midi(event, timestamp_us)
      command = event[0]
      if command == :note_on
        process_live_note_on(event, timestamp_us)
      elsif command == :note_off
        process_live_note_off(event, timestamp_us)
      else
        emit(@live_source, event, timestamp_us)
      end
      event
    end

    private def process_live_note_on(event, timestamp_us)
      channel = event[1]
      note = event[2]
      velocity = event[3]
      process_live_note_off([:note_off, channel, note, 0], timestamp_us) unless live_note_index(channel, note).nil?
      capacity = monitoring_voice_limit
      while capacity <= @live_notes.size
        stolen = @live_notes[0]
        process_live_note_off([:note_off, stolen[0], stolen[1], 0], timestamp_us)
      end
      entry = [channel, note, timestamp_us, false, velocity]
      @live_notes << entry
      emit(@live_source, event, timestamp_us)

      record_start_tick = @record_start_tick
      recorder = @recorder
      if @state == :recording && record_start_tick && recorder
        raw_tick = tick_at_time(timestamp_us) - record_start_tick
        if 0 <= raw_tick && raw_tick < loop_ticks
          recorder.note_on(raw_tick, channel, note, velocity)
          entry[3] = true
        end
      end
    end

    private def process_live_note_off(event, timestamp_us)
      index = live_note_index(event[1], event[2])
      return event if index.nil?
      entry = @live_notes.delete_at(index)
      emit(@live_source, event, timestamp_us)
      record_start_tick = @record_start_tick
      recorder = @recorder
      if entry[3] && recorder && record_start_tick
        raw_tick = tick_at_time(timestamp_us) - record_start_tick
        raw_tick = loop_ticks if loop_ticks < raw_tick
        raw_tick = 0 if raw_tick < 0
        recorder.note_off(raw_tick, event[1], event[2], event[3])
      end
      event
    end

    private def begin_recording(start_tick)
      voice_limit = @record_voice_limit
      raise RuntimeError, "recording voice limit is missing" unless voice_limit
      @record_start_tick = start_tick
      @record_end_tick = start_tick + loop_ticks
      recorder = Recorder.new(
        loop_ticks: loop_ticks,
        grid_ticks: @grid_ticks,
        voice_limit: voice_limit,
        max_events: @max_events_per_track
      )
      @recorder = recorder
      grid_ticks = @grid_ticks
      lead_ticks = grid_ticks ? grid_ticks / 2 : 0
      i = 0
      live_notes = @live_notes
      live_notes_size = live_notes.size
      while i < live_notes_size
        entry = live_notes[i]
        on_tick = tick_at_time(entry[2])
        if start_tick - lead_ticks <= on_tick
          recorder.note_on(0, entry[0], entry[1], entry[4])
          entry[3] = true
        end
        i += 1
      end
      @state = :recording
      configure_click(start_tick)
      self
    end

    private def finish_recording(now_us)
      first_track = @tracks.empty?
      recorder = @recorder
      voice_limit = @record_voice_limit
      boundary = @record_end_tick
      unless recorder && voice_limit && boundary
        raise RuntimeError, "recording state is incomplete"
      end
      buffer = recorder.finish
      silence_live(now_us)
      if buffer
        source = next_track_source
        track = Track.new(buffer, source: source, voice_limit: voice_limit)
        @tracks << track
      else
        @last_error = "recording exceeded #{@max_events_per_track} events"
      end
      @recorder = nil
      @record_voice_limit = nil
      @record_start_tick = nil
      @record_end_tick = nil
      @state = :playing

      if first_track
        boundary_us = time_at_tick(boundary)
        @anchor_us = boundary_us
        @anchor_tick = 0
        reset_tracks
      else
        track = @tracks[-1]
        track.seek(boundary, loop_ticks) if track && buffer
      end
      current = tick_at_time(now_us)
      configure_click(current)
      current
    end

    private def process_track_events(current_tick)
      while true
        due = earliest_track_tick
        break if due.nil? || current_tick < due
        dispatch_track_phase(due, :note_off)
        dispatch_track_phase(due, :note_on)
      end
    end

    private def earliest_track_tick
      selected = nil #: Integer?
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      ticks = loop_ticks
      while i < tracks_size
        tick = tracks[i].next_tick(ticks)
        current_selected = selected
        selected = tick if tick && (current_selected.nil? || tick < current_selected)
        i += 1
      end
      selected
    end

    private def dispatch_track_phase(due_tick, command)
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      ticks = loop_ticks
      timestamp_us = time_at_tick(due_tick)
      while i < tracks_size
        track = tracks[i]
        while track.next_tick(ticks) == due_tick && track.current_event[0] == command
          event = track.current_event
          unless track.muted
            emit(track.source, event, timestamp_us)
            if command == :note_on
              track.note_started(event)
            else
              track.note_stopped(event)
            end
          end
          track.advance(ticks)
        end
        i += 1
      end
    end

    private def process_clicks(current_tick)
      active = click_active?
      unless active
        @next_click_tick = nil
        return
      end
      configure_click(current_tick) if @next_click_tick.nil?
      interval = click_interval
      while (tick = @next_click_tick) && tick <= current_tick
        accent = tick % ticks_per_bar == 0
        note = accent ? 84 : 76
        velocity = accent ? 127 : 96
        timestamp_us = time_at_tick(tick)
        emit(@click_source, [:note_on, 15, note, velocity], timestamp_us)
        @click_notes << [timestamp_us + 10_000, note]
        @next_click_tick = tick + interval
      end
    end

    private def configure_click(current_tick)
      if click_active?
        interval = click_interval
        @next_click_tick = ((current_tick + interval - 1) / interval) * interval
      else
        @next_click_tick = nil
      end
    end

    private def click_active?
      return false if @metronome == :off
      return true if @state == :count_in
      @metronome == :beat || @metronome == :subdivision
    end

    private def click_interval
      if @metronome == :subdivision
        PPQN * 4 / @time_signature[1]
      elsif @time_signature[1] == 8 && 6 <= @time_signature[0]
        PPQN * 3 / 2
      else
        PPQN * 4 / @time_signature[1]
      end
    end

    private def flush_click_note_offs(now_us)
      notes = @click_notes
      while 0 < notes.size && notes[0][0] <= now_us
        item = notes.shift
        emit(@click_source, [:note_off, 15, item[1], 0], item[0])
      end
    end

    private def start_transport(now_us)
      @anchor_us = now_us
      @anchor_tick = 0
      reset_tracks
    end

    private def tick_at_time(timestamp_us)
      anchor_us = @anchor_us
      return 0 unless anchor_us
      elapsed = timestamp_us - anchor_us
      elapsed = 0 if elapsed < 0
      @anchor_tick + elapsed * @tempo * PPQN / 60_000_000
    end

    private def time_at_tick(tick)
      anchor_us = @anchor_us || now_us
      delta = tick - @anchor_tick
      delta = 0 if delta < 0
      anchor_us + delta * 60_000_000 / (@tempo * PPQN)
    end

    private def emit(source, event, timestamp_us)
      @output.emit(source, event, timestamp_us: timestamp_us)
    end

    private def monitoring_voice_limit
      @record_voice_limit || @voice_capacity
    end

    private def continuous_click?
      @metronome == :beat || @metronome == :subdivision
    end

    private def available_recording_voices
      used = continuous_click? ? 1 : 0
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        used += tracks[i].voice_limit
        i += 1
      end
      if tracks.empty? && @metronome != :off && 0 < @count_in_bars
        used += 1 unless continuous_click?
      end
      @voice_capacity - used
    end

    private def limit_live_notes(limit, timestamp_us)
      while limit < @live_notes.size
        entry = @live_notes[0]
        process_live_note_off([:note_off, entry[0], entry[1], 0], timestamp_us)
      end
    end

    private def live_note_index(channel, note)
      i = 0
      notes = @live_notes
      notes_size = notes.size
      while i < notes_size
        item = notes[i]
        return i if item[0] == channel && item[1] == note
        i += 1
      end
      nil
    end

    private def silence_live(timestamp_us)
      while 0 < @live_notes.size
        item = @live_notes[0]
        emit(@live_source, [:note_off, item[0], item[1], 0], timestamp_us)
        @live_notes.shift
      end
    end

    private def silence_track(track, timestamp_us)
      active = track.take_active_notes
      i = 0
      active_size = active.size
      while i < active_size
        item = active[i]
        emit(track.source, [:note_off, item[0], item[1], 0], timestamp_us)
        i += 1
      end
    end

    private def silence_all(timestamp_us)
      silence_live(timestamp_us)
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        silence_track(tracks[i], timestamp_us)
        i += 1
      end
      notes = @click_notes
      while 0 < notes.size
        item = notes.shift
        emit(@click_source, [:note_off, 15, item[1], 0], timestamp_us)
      end
    end

    private def reset_tracks
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        tracks[i].reset
        i += 1
      end
    end

    private def next_track_source
      sources = @track_sources
      i = 0
      sources_size = sources.size
      while i < sources_size
        source = sources[i]
        used = false
        j = 0
        tracks = @tracks
        tracks_size = tracks.size
        while j < tracks_size
          if tracks[j].source == source
            used = true
            break
          end
          j += 1
        end
        return source unless used
        i += 1
      end
      raise RuntimeError, "track source limit reached"
    end

    private def track_at(index)
      unless index.is_a?(Integer) && 0 <= index && index < @tracks.size
        raise IndexError, "track index out of range"
      end
      @tracks[index]
    end

    private def status_snapshot(now_us)
      track_status = [] #: Array[Hash[Symbol, untyped]]
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        track = tracks[i]
        track_status << {
          index: i,
          voices: track.voice_limit,
          events: track.events.count,
          muted: track.muted,
          source: track.source
        }
        i += 1
      end
      {
        state: @state,
        tempo: @tempo,
        time_signature: @time_signature,
        bars: @bars,
        count_in_bars: @count_in_bars,
        quantize: @quantize,
        metronome: @metronome,
        tick: @anchor_us ? tick_at_time(now_us) % loop_ticks : 0,
        tracks: track_status,
        available_voices: available_recording_voices,
        last_error: @last_error
      }
    end

    private def set_tempo(value)
      unless value.is_a?(Integer) && 30 <= value && value <= 300
        raise ArgumentError, "tempo must be in 30..300"
      end
      @tempo = value
    end

    private def set_time_signature(value)
      ensure_configuration_change_allowed
      raise RuntimeError, "meter is locked after recording" unless @tracks.empty?
      valid = false
      i = 0
      while i < TIME_SIGNATURES.size
        if TIME_SIGNATURES[i] == value
          valid = true
          break
        end
        i += 1
      end
      raise ArgumentError, "meter must be 3/4, 4/4, 6/8, or 12/8" unless valid
      @time_signature = [value[0], value[1]]
    end

    private def set_bars(value)
      ensure_configuration_change_allowed
      raise RuntimeError, "bars are locked after recording" unless @tracks.empty?
      unless value.is_a?(Integer) && 1 <= value && value <= 16
        raise ArgumentError, "bars must be in 1..16"
      end
      @bars = value
      raise ArgumentError, "loop is too long" if 0xFFFF < loop_ticks
      @bars
    end

    private def set_count_in_bars(value)
      ensure_configuration_change_allowed
      unless value.is_a?(Integer) && 0 <= value && value <= 4
        raise ArgumentError, "count-in bars must be in 0..4"
      end
      @count_in_bars = value
    end

    private def set_quantize(value)
      ensure_configuration_change_allowed
      unless QUANTIZE_GRIDS.key?(value)
        raise ArgumentError, "invalid quantize value"
      end
      @quantize = value
      @grid_ticks = QUANTIZE_GRIDS[value]
    end

    private def set_metronome(value)
      ensure_configuration_change_allowed
      unless METRONOME_MODES.include?(value)
        raise ArgumentError, "invalid metronome mode"
      end
      if (value == :beat || value == :subdivision) && @voice_capacity < used_track_voices + 1
        raise RuntimeError, "no voice available for continuous metronome"
      end
      @metronome = value
      configure_click(@anchor_us ? tick_at_time(now_us) : 0)
      value
    end

    private def ticks_per_bar
      PPQN * @time_signature[0] * 4 / @time_signature[1]
    end

    private def loop_ticks
      ticks_per_bar * @bars
    end

    private def used_track_voices
      total = 0
      i = 0
      tracks = @tracks
      tracks_size = tracks.size
      while i < tracks_size
        total += tracks[i].voice_limit
        i += 1
      end
      total
    end

    private def ensure_configuration_change_allowed
      if @state == :armed || @state == :count_in || @state == :recording
        raise RuntimeError, "configuration is locked while recording is armed"
      end
    end

    private def now_us
      @time_source.uptime_us
    end
  end
end
