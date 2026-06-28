module MIDIBASE
  module MML
    class Sequence
      attr_reader :ppqn, :time_signature

      def initialize(tracks, channels: nil, loop: false, ppqn: PPQN, time_signature: [4, 4], exception: true)
        raise ArgumentError, "tracks must not be empty" if tracks.empty?
        raise ArgumentError, "MML supports at most 16 tracks" if 16 < tracks.size
        @tracks = tracks
        @channels = channels
        @loop = loop
        @ppqn = ppqn
        @time_signature = time_signature
        @exception = exception
        reset
      end

      def reset
        @parsers = [] #: Array[Parser]
        @ticks = [] #: Array[Integer?]
        @events = [] #: Array[Array[untyped]?]
        @previous_tick = 0
        i = 0
        tracks = @tracks
        tracks_size = tracks.size
        channels = @channels
        ppqn = @ppqn
        loop_enabled = @loop
        exception = @exception
        parsers = @parsers
        ticks = @ticks
        events = @events
        while i < tracks_size
          channel = channels ? channels[i] : i
          raise ArgumentError, "MIDI channel must be in 0..15" unless channel && 0 <= channel && channel <= 15
          parser = Parser.new(tracks[i], channel: channel, ppqn: ppqn, loop: loop_enabled, exception: exception)
          parsers << parser
          item = parser.next_event
          if item
            ticks << item[0]
            events << item[1]
          else
            ticks << nil
            events << nil
          end
          i += 1
        end
        self
      end

      def next_event
        track = earliest_track
        return nil if track.nil?
        ticks = @ticks
        events = @events
        parsers = @parsers
        tick = ticks[track]
        event = events[track]
        # @type var tick: Integer
        # @type var event: Array[untyped]
        delta = tick - @previous_tick
        @previous_tick = tick
        item = parsers[track].next_event
        if item
          ticks[track] = tick + item[0]
          events[track] = item[1]
        else
          ticks[track] = nil
          events[track] = nil
        end
        [delta, event]
      end

      private def earliest_track
        selected = nil
        selected_tick = nil
        i = 0
        ticks = @ticks
        ticks_size = ticks.size
        while i < ticks_size
          tick = ticks[i]
          if tick
            current_selected_tick = selected_tick
            if current_selected_tick.nil?
              selected = i
              selected_tick = tick
            else
              # @type var current_selected_tick: Integer
              if tick < current_selected_tick
                selected = i
                selected_tick = tick
              end
            end
          end
          i += 1
        end
        selected
      end
    end
  end
end
