class File

  class Stat
    def initialize(path)
      @path = path
    end
    def directory? = FileTest.directory?(@path)
    def mode = raise(NotImplementedError, "mode: Not Implemented")
    def mode_str = raise(NotImplementedError, "mode_str: Not Implemented")
    def writable? = raise(NotImplementedError, "writable?: Not Implemented")
    def mtime = File.open(@path){|f| f.mtime}
    def birthtime = raise(NotImplementedError, "birthtime: Not Implemented")
    def size = FileTest.size(@path)
  end

end
