class File
  class Stat
    def initialize(entry)
      @entry = entry
    end
    def size
      FileTest.size(@entry)
    end
    def directory?
      FileTest.directory?(@entry)
    end
    def mtime
      File.new(@entry).mtime
    end
  end
end
