class File

  class Stat
    def initialize(path)
      @path = path
    end
    def directory? = FileTest.directory?(@path)
    def mode = puts("mode: Not Implemented")
    def mode_str = puts("mode_str: Not Impelented")
    def writable? = puts("writable?: Not Impelented")
    def mtime = File.open(@path){|f| f.mtime}
    def birthtime = puts("birthtime: Not Impelented")
    def size = FileTest.size(@path)
  end

end
