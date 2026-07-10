require "midibase"

module MIDIBASE
  class Looper
    PPQN = 480
    DEFAULT_TEMPO = 120
    DEFAULT_MAX_EVENTS = 256
    DEFAULT_VOICE_CAPACITY = 3
    DEFAULT_MAX_PARTS = 8
    DEFAULT_MAX_ARRANGEMENT_STEPS = 64
    MIDI_MESSAGE_POOL_SIZE = 16
    CLICK_DURATION_US = 50_000
    CLICK_VELOCITY = 127

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
    PART_IDS = [
      :A, :B, :C, :D, :E, :F, :G, :H, :I, :J, :K, :L, :M,
      :N, :O, :P, :Q, :R, :S, :T, :U, :V, :W, :X, :Y, :Z
    ]

    attr_reader :state, :live_source, :click_source, :track_sources

    def initialize(
          output:,
          tempo: DEFAULT_TEMPO,
          time_signature: [4, 4],
          bars: 4,
          count_in_bars: 1,
          quantize: :sixteenth,
          metronome: :count_in,
          voice_capacity: DEFAULT_VOICE_CAPACITY,
          click_voice_cost: 1,
          max_parts: DEFAULT_MAX_PARTS,
          max_arrangement_steps: DEFAULT_MAX_ARRANGEMENT_STEPS,
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
      unless click_voice_cost.is_a?(Integer) && 0 <= click_voice_cost && click_voice_cost <= 1
        raise ArgumentError, "click_voice_cost must be 0 or 1"
      end
      unless max_parts.is_a?(Integer) && 1 <= max_parts && max_parts <= 26
        raise ArgumentError, "max_parts must be in 1..26"
      end
      unless max_arrangement_steps.is_a?(Integer) && 0 < max_arrangement_steps
        raise ArgumentError, "max_arrangement_steps must be a positive Integer"
      end

      @output = output
      @fast_output = output.respond_to?(:emit_midi)
      unless time_source.respond_to?(:uptime_us)
        raise ArgumentError, "time_source must respond to uptime_us"
      end
      @time_source = time_source
      @voice_capacity = voice_capacity
      @click_voice_cost = click_voice_cost
      @max_parts = max_parts
      @max_arrangement_steps = max_arrangement_steps
      @max_events_per_track = max_events_per_track
      @live_source = LIVE_SOURCE
      @click_source = CLICK_SOURCE
      @track_sources = [] #: Array[Symbol]
      i = 0
      while i < voice_capacity
        @track_sources << "looper_track_#{i}".to_sym
        i += 1
      end

      @parts = [] #: Array[Part]
      @selected_part = Part.new(:A, bars: bars)
      @parts << @selected_part
      @active_part = nil
      @queued_part = nil
      @record_part = nil
      @play_mode = :part
      @part_start_tick = 0
      @next_part_tick = nil
      # Flat records: Part, repeat count.
      @arrangement = [] #: Array[untyped]
      @arrangement_step = nil
      @arrangement_repeat = nil
      @pending_song = false
      @undo_action = nil
      @redo_action = nil
      # Flat records: channel, note, timestamp_us, recorded, velocity.
      @live_notes = [] #: Array[untyped]
      @click_notes = [] #: Array[Integer]
      @queue = Task::Queue.new
      @free_midi_messages = [] #: Array[Array[untyped]]
      i = 0
      while i < MIDI_MESSAGE_POOL_SIZE
        @free_midi_messages << [:midi, nil, nil]
        i += 1
      end
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
      validate_bars(bars)
      set_count_in_bars(count_in_bars)
      set_quantize(quantize)
      set_metronome(metronome)
    end

    def tracks
      @selected_part.tracks
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

    def handle(event, source: LIVE_SOURCE, priority: 0, timestamp_us: nil, **_context)
      handle_midi(event, source, priority, timestamp_us || now_us)
    end

    # Router fast path. MIDI message envelopes are recycled after the engine
    # consumes them, avoiding one short-lived Array for every input event.
    def handle_midi(event, _source, _priority, timestamp_us)
      return false if @shutdown
      if @task
        free_messages = @free_midi_messages
        message = free_messages.empty? ? [:midi, nil, nil] : free_messages.pop
        # @type var message: Array[untyped]
        message[1] = event
        message[2] = timestamp_us
        @queue.push(message)
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

    def play_song
      submit(:play_song)
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

    def redo
      submit(:redo)
    end

    def clear
      submit(:clear)
    end

    def create_part(id = nil, bars: nil)
      submit(:create_part, id, bars)
    end

    def copy_part(source, id = nil)
      submit(:copy_part, source, id)
    end

    def select_part(id)
      submit(:select_part, id)
    end

    def clear_part(id = nil)
      submit(:clear_part, id)
    end

    def delete_part(id)
      submit(:delete_part, id)
    end

    def arrangement=(entries)
      submit(:arrangement, entries)
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
      while (boundary = next_boundary_tick) && boundary <= current_tick
        recording_end = @state == :recording && @record_end_tick == boundary
        process_clicks(boundary) unless recording_end
        process_track_events(boundary, exclusive: true)
        if recording_end
          finish_recording(boundary, now_us)
        elsif (@state == :armed || @state == :count_in) && @record_start_tick == boundary
          begin_recording(boundary, now_us)
        elsif @next_part_tick == boundary
          process_part_boundary(boundary, now_us)
        end
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
          # The engine task may wake after a recording boundary. Move transport
          # to the captured input time before deciding whether to record it.
          advance(message[2])
          process_midi(message[1], message[2])
          message[1] = nil
          message[2] = nil
          @free_midi_messages << message
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
      when :play_song then command_play_song(now_us)
      when :transport_stop then command_transport_stop(now_us)
      when :record then command_record(args[0], now_us)
      when :mute then command_mute(args[0], now_us)
      when :unmute then track_at(args[0]).muted = false
      when :delete then command_delete(args[0], now_us)
      when :undo then command_undo(now_us)
      when :redo then command_redo(now_us)
      when :clear then command_clear(now_us)
      when :create_part then command_create_part(args[0], args[1])
      when :copy_part then command_copy_part(args[0], args[1])
      when :select_part then command_select_part(args[0])
      when :clear_part then command_clear_part(args[0], now_us)
      when :delete_part then command_delete_part(args[0])
      when :arrangement then command_arrangement(args[0])
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
      ensure_not_recording
      @play_mode = :part
      @pending_song = false
      @arrangement_step = nil
      @arrangement_repeat = nil
      if @state == :stopped
        start_transport(now_us)
        activate_part(@selected_part, 0, now_us)
        @state = :playing
      elsif @active_part != @selected_part
        @queued_part = @selected_part
        @next_part_tick = next_active_part_boundary(tick_at_time(now_us))
      end
      configure_click(tick_at_time(now_us))
      self
    end

    private def command_play_song(now_us)
      ensure_not_recording
      raise RuntimeError, "arrangement is empty" if @arrangement.empty?
      if @state == :stopped
        start_transport(now_us)
        begin_song(0, now_us)
        @state = :playing
      else
        @pending_song = true
        @queued_part = nil
        @next_part_tick = next_active_part_boundary(tick_at_time(now_us))
      end
      self
    end

    private def command_transport_stop(now_us)
      silence_all(now_us)
      @state = :stopped
      @recorder = nil
      @record_voice_limit = nil
      @record_start_tick = nil
      @record_end_tick = nil
      @record_part = nil
      @anchor_us = nil
      @anchor_tick = 0
      @next_click_tick = nil
      @active_part = nil
      @queued_part = nil
      @next_part_tick = nil
      @pending_song = false
      @arrangement_step = nil
      @arrangement_repeat = nil
      reset_all_tracks
      self
    end

    private def command_record(voices, now_us)
      unless voices.is_a?(Integer) && 0 < voices
        raise ArgumentError, "voices must be a positive Integer"
      end
      unless @state == :stopped || @state == :playing
        raise RuntimeError, "recording is already armed"
      end
      target = @selected_part
      available = available_recording_voices(target)
      if available < voices
        raise ArgumentError, "only #{available} recording voice(s) available"
      end
      if target.tracks.size >= @track_sources.size
        raise RuntimeError, "track limit reached"
      end

      @last_error = nil
      @record_voice_limit = voices
      @record_part = target
      limit_live_notes(voices, now_us)
      @play_mode = :part
      @pending_song = false
      @arrangement_step = nil
      @arrangement_repeat = nil
      if @state == :stopped
        start_transport(now_us)
        activate_part(target, 0, now_us)
        current = tick_at_time(now_us)
        if target.tracks.empty?
          count_ticks = @count_in_bars * ticks_per_bar
          @record_start_tick = current + count_ticks
          @state = count_ticks == 0 ? :recording : :count_in
          if @state == :recording
            begin_recording(current, now_us)
            configure_click(current)
          else
            configure_click(current)
          end
        else
          @record_start_tick = part_ticks(target)
          @state = :armed
          configure_click(current)
        end
      else
        current = tick_at_time(now_us)
        @record_start_tick = next_active_part_boundary(current)
        @queued_part = target if @active_part != target
        @next_part_tick = nil
        @state = :armed
        configure_click(current)
      end
      self
    end

    private def command_mute(index, now_us)
      track = track_at(index)
      silence_track(track, now_us) if @active_part == @selected_part
      track.muted = true
      self
    end

    private def command_delete(index, now_us)
      part = @selected_part
      track = track_at(index)
      silence_track(track, now_us) if @active_part == part
      part.tracks.delete_at(index)
      remember([:track_insert, part.id, index, track])
      self
    end

    private def command_undo(now_us)
      action = @undo_action
      raise RuntimeError, "nothing to undo" unless action
      ensure_stopped_for_structure if structural_history?(action)
      inverse = apply_history(action, now_us)
      @undo_action = nil
      @redo_action = inverse
      self
    end

    private def command_redo(now_us)
      action = @redo_action
      raise RuntimeError, "nothing to redo" unless action
      ensure_stopped_for_structure if structural_history?(action)
      inverse = apply_history(action, now_us)
      @redo_action = nil
      @undo_action = inverse
      self
    end

    private def command_clear(now_us)
      old_parts = @parts
      old_selected = @selected_part
      old_arrangement = @arrangement
      command_transport_stop(now_us)
      @selected_part = Part.new(:A, bars: old_selected.bars)
      @parts = [@selected_part]
      @arrangement = [] #: Array[untyped]
      remember([:song_replace, old_parts, old_selected.id, old_arrangement])
      self
    end

    private def command_create_part(id, bars)
      ensure_structure_change_allowed
      part_id = id ? normalize_part_id(id) : next_part_id
      raise ArgumentError, "part #{part_id} already exists" if find_part(part_id)
      raise RuntimeError, "part limit reached" if @max_parts <= @parts.size
      part_bars = bars.nil? ? @selected_part.bars : validate_bars(bars)
      part = Part.new(part_id, bars: part_bars)
      @parts << part
      @selected_part = part
      remember([:part_remove, @parts.size - 1])
      part
    end

    private def command_copy_part(source_id, id)
      ensure_structure_change_allowed
      source = part_for(source_id)
      part_id = id ? normalize_part_id(id) : next_part_id
      raise ArgumentError, "part #{part_id} already exists" if find_part(part_id)
      raise RuntimeError, "part limit reached" if @max_parts <= @parts.size
      part = source.copy(part_id)
      @parts << part
      @selected_part = part
      remember([:part_remove, @parts.size - 1])
      part
    end

    private def command_select_part(id)
      ensure_not_recording
      @selected_part = part_for(id)
      @selected_part
    end

    private def command_clear_part(id, _now_us)
      ensure_stopped_for_structure
      part = id ? part_for(id) : @selected_part
      old_tracks = part.tracks
      part.instance_variable_set(:@tracks, [])
      remember([:tracks_replace, part.id, old_tracks])
      part
    end

    private def command_delete_part(id)
      ensure_stopped_for_structure
      raise RuntimeError, "the last part cannot be deleted" if @parts.size == 1
      part = part_for(id)
      index = @parts.index(part)
      raise RuntimeError, "part index is missing" unless index
      # @type var index: Integer
      old_arrangement = @arrangement
      old_selected_id = @selected_part.id
      @parts.delete_at(index)
      remove_part_from_arrangement(part)
      @selected_part = @parts[0] if @selected_part == part
      remember([:part_insert, index, part, old_arrangement, old_selected_id])
      part
    end

    private def command_arrangement(entries)
      ensure_stopped_for_structure
      unless entries.is_a?(Array)
        raise ArgumentError, "arrangement must be an Array"
      end
      if @max_arrangement_steps < entries.size
        raise ArgumentError, "arrangement is limited to #{@max_arrangement_steps} steps"
      end
      normalized = [] #: Array[untyped]
      i = 0
      entries_size = entries.size
      while i < entries_size
        entry = entries[i]
        unless entry.is_a?(Array) && entry.size == 2
          raise ArgumentError, "arrangement entries must be [part, repeats]"
        end
        repeats = entry[1]
        unless repeats.is_a?(Integer) && 1 <= repeats && repeats <= 255
          raise ArgumentError, "arrangement repeats must be in 1..255"
        end
        normalized << part_for(entry[0])
        normalized << repeats
        i += 1
      end
      old = @arrangement
      @arrangement = normalized
      remember([:arrangement_replace, old])
      normalized
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
      live_notes = @live_notes
      while capacity * 5 <= live_notes.size
        process_live_note_off([:note_off, live_notes[0], live_notes[1], 0], timestamp_us)
      end
      entry_offset = live_notes.size
      live_notes << channel
      live_notes << note
      live_notes << timestamp_us
      live_notes << false
      live_notes << velocity
      emit(@live_source, event, timestamp_us)

      record_start_tick = @record_start_tick
      recorder = @recorder
      part = @record_part
      if @state == :recording && record_start_tick && recorder && part
        raw_tick = tick_at_time(timestamp_us) - record_start_tick
        ticks = part_ticks(part)
        if 0 <= raw_tick && raw_tick < ticks
          recorder.note_on(raw_tick, channel, note, velocity)
          live_notes[entry_offset + 3] = true
        end
      end
    end

    private def process_live_note_off(event, timestamp_us)
      index = live_note_index(event[1], event[2])
      return event if index.nil?
      live_notes = @live_notes
      recorded = live_notes[index + 3]
      live_notes.delete_at(index)
      live_notes.delete_at(index)
      live_notes.delete_at(index)
      live_notes.delete_at(index)
      live_notes.delete_at(index)
      emit(@live_source, event, timestamp_us)
      record_start_tick = @record_start_tick
      recorder = @recorder
      part = @record_part
      if recorded && recorder && record_start_tick && part
        raw_tick = tick_at_time(timestamp_us) - record_start_tick
        ticks = part_ticks(part)
        raw_tick = ticks if ticks < raw_tick
        raw_tick = 0 if raw_tick < 0
        recorder.note_off(raw_tick, event[1], event[2], event[3])
      end
      event
    end

    private def begin_recording(start_tick, now_us)
      voice_limit = @record_voice_limit
      part = @record_part
      unless voice_limit && part
        raise RuntimeError, "recording target is incomplete"
      end
      if @active_part != part
        activate_part(part, start_tick, now_us)
      end
      @queued_part = nil
      @next_part_tick = nil
      @record_start_tick = start_tick
      ticks = part_ticks(part)
      @record_end_tick = start_tick + ticks
      recorder = Recorder.new(
        loop_ticks: ticks,
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
        on_tick = tick_at_time(live_notes[i + 2])
        if start_tick - lead_ticks <= on_tick
          recorder.note_on(0, live_notes[i], live_notes[i + 1], live_notes[i + 4])
          live_notes[i + 3] = true
        end
        i += 5
      end
      @state = :recording
      configure_click(start_tick + 1)
      self
    end

    private def finish_recording(boundary, now_us)
      part = @record_part
      recorder = @recorder
      voice_limit = @record_voice_limit
      unless recorder && voice_limit && part
        raise RuntimeError, "recording state is incomplete"
      end
      first_track = part.tracks.empty?
      buffer = recorder.finish
      silence_live(now_us)
      if buffer
        source = next_track_source
        track = Track.new(
          buffer,
          source: source,
          voice_limit: voice_limit,
          channel_mask: recorder.channel_mask
        )
        part.tracks << track
        remember([:track_remove, part.id, part.tracks.size - 1])
      else
        @last_error = "recording exceeded #{@max_events_per_track} events"
      end
      @recorder = nil
      @record_voice_limit = nil
      @record_start_tick = nil
      @record_end_tick = nil
      @record_part = nil
      @state = :playing

      if first_track
        @part_start_tick = boundary
        reset_part_tracks(part)
      else
        track = part.tracks[-1]
        relative = boundary - @part_start_tick
        track.seek(relative, part_ticks(part)) if track && buffer
      end
      configure_click(boundary)
      boundary
    end

    private def next_boundary_tick
      selected = nil #: Integer?
      if @state == :recording
        selected = @record_end_tick
      elsif @state == :armed || @state == :count_in
        selected = @record_start_tick
      end
      part_tick = @next_part_tick
      if part_tick && (selected.nil? || part_tick < selected)
        selected = part_tick
      end
      selected
    end

    private def process_part_boundary(boundary, now_us)
      if @pending_song
        @pending_song = false
        begin_song(boundary, now_us)
        return
      end
      if @play_mode == :song
        step = @arrangement_step
        repeat = @arrangement_repeat
        unless step && repeat
          raise RuntimeError, "song playback state is incomplete"
        end
        arrangement = @arrangement
        repeats = arrangement[step + 1]
        if repeat < repeats
          @arrangement_repeat = repeat + 1
          active = @active_part
          raise RuntimeError, "active song part is missing" unless active
          @next_part_tick = boundary + part_ticks(active)
          return
        end
        step += 2
        if arrangement.size <= step
          command_transport_stop(time_at_tick(boundary))
          return
        end
        @arrangement_step = step
        @arrangement_repeat = 1
        part_value = arrangement[step]
        raise RuntimeError, "arrangement part is missing" unless part_value
        part = part_value #: Part
        activate_part(part, boundary, now_us)
        @next_part_tick = boundary + part_ticks(part)
      elsif @queued_part
        part = @queued_part #: Part
        @queued_part = nil
        activate_part(part, boundary, now_us)
        @next_part_tick = nil
      else
        @next_part_tick = nil
      end
    end

    private def begin_song(start_tick, now_us)
      @play_mode = :song
      @queued_part = nil
      @arrangement_step = 0
      @arrangement_repeat = 1
      part = @arrangement[0]
      activate_part(part, start_tick, now_us)
      @next_part_tick = start_tick + part_ticks(part)
      configure_click(start_tick + 1)
      self
    end

    private def activate_part(part, start_tick, _now_us)
      timestamp_us = time_at_tick(start_tick)
      active = @active_part
      silence_part(active, timestamp_us) if active
      @active_part = part
      @part_start_tick = start_tick
      reset_part_tracks(part)
      part
    end

    private def next_active_part_boundary(current_tick)
      part = @active_part
      return current_tick unless part
      ticks = part_ticks(part)
      elapsed = current_tick - @part_start_tick
      elapsed = 0 if elapsed < 0
      @part_start_tick + (elapsed / ticks + 1) * ticks
    end

    private def process_track_events(current_tick, exclusive: false)
      while true
        due = earliest_track_tick
        break if due.nil? || current_tick < due || (exclusive && due == current_tick)
        dispatch_track_phase(due, :note_off)
        dispatch_track_phase(due, :note_on)
      end
    end

    private def earliest_track_tick
      part = @active_part
      return nil unless part
      selected = nil #: Integer?
      i = 0
      tracks = part.tracks
      tracks_size = tracks.size
      ticks = part_ticks(part)
      while i < tracks_size
        tick = tracks[i].next_tick(ticks)
        tick += @part_start_tick if tick
        current_selected = selected
        selected = tick if tick && (current_selected.nil? || tick < current_selected)
        i += 1
      end
      selected
    end

    private def dispatch_track_phase(due_tick, command)
      part = @active_part
      return unless part
      i = 0
      tracks = part.tracks
      tracks_size = tracks.size
      ticks = part_ticks(part)
      relative_due = due_tick - @part_start_tick
      timestamp_us = time_at_tick(due_tick)
      while i < tracks_size
        track = tracks[i]
        while track.next_tick(ticks) == relative_due && track.current_event[0] == command
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
        timestamp_us = time_at_tick(tick)
        emit(@click_source, [:note_on, 15, note, CLICK_VELOCITY], timestamp_us)
        click_notes = @click_notes
        click_notes << timestamp_us + CLICK_DURATION_US
        click_notes << note
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
      record_part = @record_part
      return true if @state == :recording && record_part && record_part.tracks.empty?
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
      while 0 < notes.size && notes[0] <= now_us
        timestamp_us = notes.shift
        note = notes.shift
        emit(@click_source, [:note_off, 15, note, 0], timestamp_us)
      end
    end

    private def start_transport(now_us)
      @anchor_us = now_us
      @anchor_tick = 0
      reset_all_tracks
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
      if @fast_output
        @output.emit_midi(source, event, timestamp_us)
      else
        @output.emit(source, event, timestamp_us: timestamp_us)
      end
    end

    private def monitoring_voice_limit
      @record_voice_limit || @voice_capacity
    end

    private def continuous_click?
      @metronome == :beat || @metronome == :subdivision
    end

    private def available_recording_voices(part = @selected_part)
      used = continuous_click? ? @click_voice_cost : 0
      i = 0
      tracks = part.tracks
      tracks_size = tracks.size
      while i < tracks_size
        used += tracks[i].voice_limit
        i += 1
      end
      if tracks.empty? && @metronome != :off
        used += @click_voice_cost unless continuous_click?
      end
      @voice_capacity - used
    end

    private def limit_live_notes(limit, timestamp_us)
      live_notes = @live_notes
      while limit * 5 < live_notes.size
        process_live_note_off([:note_off, live_notes[0], live_notes[1], 0], timestamp_us)
      end
    end

    private def live_note_index(channel, note)
      i = 0
      notes = @live_notes
      notes_size = notes.size
      while i < notes_size
        return i if notes[i] == channel && notes[i + 1] == note
        i += 5
      end
      nil
    end

    private def silence_live(timestamp_us)
      live_notes = @live_notes
      while 0 < live_notes.size
        emit(@live_source, [:note_off, live_notes[0], live_notes[1], 0], timestamp_us)
        live_notes.shift
        live_notes.shift
        live_notes.shift
        live_notes.shift
        live_notes.shift
      end
    end

    private def silence_track(track, timestamp_us)
      active = track.take_active_notes
      i = 0
      active_size = active.size
      while i < active_size
        emit(track.source, [:note_off, active[i], active[i + 1], 0], timestamp_us)
        i += 2
      end
    end

    private def silence_all(timestamp_us)
      silence_live(timestamp_us)
      active = @active_part
      silence_part(active, timestamp_us) if active
      notes = @click_notes
      while 0 < notes.size
        notes.shift
        note = notes.shift
        emit(@click_source, [:note_off, 15, note, 0], timestamp_us)
      end
    end

    private def silence_part(part, timestamp_us)
      tracks = part.tracks
      i = 0
      tracks_size = tracks.size
      while i < tracks_size
        silence_track(tracks[i], timestamp_us)
        i += 1
      end
    end

    private def reset_part_tracks(part)
      i = 0
      tracks = part.tracks
      tracks_size = tracks.size
      while i < tracks_size
        tracks[i].reset
        i += 1
      end
    end

    private def reset_all_tracks
      parts = @parts
      i = 0
      parts_size = parts.size
      while i < parts_size
        reset_part_tracks(parts[i])
        i += 1
      end
    end

    private def next_track_source
      sources = @track_sources
      i = 0
      sources_size = sources.size
      part = @record_part || @selected_part
      part_tracks = part.tracks
      while i < sources_size
        source = sources[i]
        used = false
        j = 0
        tracks = part_tracks
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
      tracks = @selected_part.tracks
      unless index.is_a?(Integer) && 0 <= index && index < tracks.size
        raise IndexError, "track index out of range"
      end
      tracks[index]
    end

    private def status_snapshot(now_us)
      parts_status = [] #: Array[Hash[Symbol, untyped]]
      selected_tracks = nil #: Array[Hash[Symbol, untyped]]?
      selected_part = @selected_part
      i = 0
      parts = @parts
      parts_size = parts.size
      while i < parts_size
        part = parts[i]
        track_status = [] #: Array[Hash[Symbol, untyped]]
        j = 0
        tracks = part.tracks
        tracks_size = tracks.size
        while j < tracks_size
          track = tracks[j]
          channels = [] #: Array[Integer]
          channel = 0
          mask = track.channel_mask
          while channel < 16
            channels << channel + 1 if mask & (1 << channel) != 0
            channel += 1
          end
          track_status << {
            index: j,
            voices: track.voice_limit,
            events: track.events.count,
            muted: track.muted,
            source: track.source,
            channels: channels
          }
          j += 1
        end
        parts_status << {id: part.id, bars: part.bars, tracks: track_status}
        selected_tracks = track_status if part == selected_part
        i += 1
      end
      arrangement_status = [] #: Array[Hash[Symbol, untyped]]
      i = 0
      arrangement = @arrangement
      while i < arrangement.size
        arrangement_status << {part: arrangement[i].id, repeats: arrangement[i + 1]}
        i += 2
      end
      current_tick = @anchor_us ? tick_at_time(now_us) : 0
      active = @active_part
      display_tick = active ? (current_tick - @part_start_tick) % part_ticks(active) : 0
      song_step = @arrangement_step
      {
        state: @state,
        play_mode: @play_mode,
        tempo: @tempo,
        time_signature: @time_signature,
        bars: selected_part.bars,
        count_in_bars: @count_in_bars,
        quantize: @quantize,
        metronome: @metronome,
        tick: display_tick,
        tracks: selected_tracks || [],
        parts: parts_status,
        selected_part: selected_part.id,
        active_part: active&.id,
        queued_part: (@queued_part || @record_part)&.id,
        arrangement: arrangement_status,
        song_step: song_step ? song_step / 2 : nil,
        song_repeat: @arrangement_repeat,
        available_voices: available_recording_voices(selected_part),
        can_undo: !@undo_action.nil?,
        can_redo: !@redo_action.nil?,
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
      raise RuntimeError, "meter is locked after recording" if any_tracks?
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
      raise RuntimeError, "bars can only be changed while stopped" unless @state == :stopped
      part = @selected_part
      raise RuntimeError, "bars are locked after recording" unless part.tracks.empty?
      part.bars = validate_bars(value)
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
      if (value == :beat || value == :subdivision) && @voice_capacity < max_used_track_voices + @click_voice_cost
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
      part_ticks(@selected_part)
    end

    private def part_ticks(part)
      ticks_per_bar * part.bars
    end

    private def used_track_voices(part = @selected_part)
      total = 0
      i = 0
      tracks = part.tracks
      tracks_size = tracks.size
      while i < tracks_size
        total += tracks[i].voice_limit
        i += 1
      end
      total
    end

    private def max_used_track_voices
      maximum = 0
      i = 0
      parts = @parts
      parts_size = parts.size
      while i < parts_size
        used = used_track_voices(parts[i])
        maximum = used if maximum < used
        i += 1
      end
      maximum
    end

    private def validate_bars(value)
      unless value.is_a?(Integer) && 1 <= value && value <= 16
        raise ArgumentError, "bars must be in 1..16"
      end
      if 0xFFFF < ticks_per_bar * value
        raise ArgumentError, "loop is too long"
      end
      value
    end

    private def normalize_part_id(value)
      text = value.to_s.upcase
      unless text.size == 1
        raise ArgumentError, "part ID must be A..Z"
      end
      index = text.getbyte(0) - 65
      unless 0 <= index && index < @max_parts
        raise ArgumentError, "part ID must be A..#{PART_IDS[@max_parts - 1]}"
      end
      PART_IDS[index]
    end

    private def find_part(id)
      i = 0
      parts = @parts
      parts_size = parts.size
      while i < parts_size
        return parts[i] if parts[i].id == id
        i += 1
      end
      nil
    end

    private def part_for(value)
      id = normalize_part_id(value)
      part = find_part(id)
      raise ArgumentError, "part #{id} does not exist" unless part
      part
    end

    private def next_part_id
      i = 0
      while i < @max_parts
        id = PART_IDS[i]
        return id unless find_part(id)
        i += 1
      end
      raise RuntimeError, "part limit reached"
    end

    private def remove_part_from_arrangement(part)
      result = [] #: Array[untyped]
      arrangement = @arrangement
      i = 0
      while i < arrangement.size
        unless arrangement[i] == part
          result << arrangement[i]
          result << arrangement[i + 1]
        end
        i += 2
      end
      @arrangement = result
    end

    private def any_tracks?
      i = 0
      parts = @parts
      parts_size = parts.size
      while i < parts_size
        return true unless parts[i].tracks.empty?
        i += 1
      end
      false
    end

    private def remember(action)
      @undo_action = action
      @redo_action = nil
      action
    end

    private def apply_history(action, now_us)
      kind = action[0]
      if kind == :track_insert
        part = part_for(action[1])
        index = action[2]
        track = action[3]
        part.tracks.insert(index, track)
        if part == @active_part
          relative = tick_at_time(now_us) - @part_start_tick
          track.seek(relative, part_ticks(part))
        end
        return [:track_remove, part.id, index]
      elsif kind == :track_remove
        part = part_for(action[1])
        index = action[2]
        track = part.tracks[index]
        raise RuntimeError, "undo track no longer exists" unless track
        silence_track(track, now_us) if part == @active_part
        part.tracks.delete_at(index)
        return [:track_insert, part.id, index, track]
      elsif kind == :part_remove
        index = action[1]
        part = @parts[index]
        raise RuntimeError, "undo part no longer exists" unless part
        arrangement = @arrangement
        @parts.delete_at(index)
        remove_part_from_arrangement(part)
        @selected_part = @parts[0] if @selected_part == part
        return [:part_insert, index, part, arrangement, part.id]
      elsif kind == :part_insert
        index = action[1]
        part = action[2]
        current_arrangement = @arrangement
        @parts.insert(index, part)
        @arrangement = action[3]
        selected = find_part(action[4])
        @selected_part = selected if selected
        return [:part_remove, index, current_arrangement]
      elsif kind == :tracks_replace
        part = part_for(action[1])
        current = part.tracks
        part.instance_variable_set(:@tracks, action[2])
        return [:tracks_replace, part.id, current]
      elsif kind == :arrangement_replace
        current = @arrangement
        @arrangement = action[1]
        return [:arrangement_replace, current]
      elsif kind == :song_replace
        current_parts = @parts
        current_selected = @selected_part.id
        current_arrangement = @arrangement
        @parts = action[1]
        @selected_part = find_part(action[2]) || @parts[0]
        @arrangement = action[3]
        return [:song_replace, current_parts, current_selected, current_arrangement]
      end
      raise RuntimeError, "unknown undo action"
    end

    private def structural_history?(action)
      kind = action[0]
      kind != :track_insert && kind != :track_remove
    end

    private def ensure_not_recording
      unless @state == :stopped || @state == :playing
        raise RuntimeError, "operation is unavailable while recording is armed"
      end
    end

    private def ensure_structure_change_allowed
      ensure_not_recording
    end

    private def ensure_stopped_for_structure
      raise RuntimeError, "operation requires stopped transport" unless @state == :stopped
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
