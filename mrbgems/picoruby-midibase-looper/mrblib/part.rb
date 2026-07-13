module MIDIBASE
  class Looper
    class Part
      attr_reader :id, :tracks
      attr_accessor :bars

      def initialize(id, bars:)
        @id = id
        @bars = bars
        @tracks = [] #: Array[Track]
      end

      def copy(id)
        part = Part.new(id, bars: @bars)
        copied = part.tracks
        tracks = @tracks
        i = 0
        tracks_size = tracks.size
        while i < tracks_size
          copied << tracks[i].copy
          i += 1
        end
        part
      end
    end
  end
end
