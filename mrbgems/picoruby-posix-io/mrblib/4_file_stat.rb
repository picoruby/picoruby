class File

  class Stat
    def initialize(path)
      @path = path
    end
    def directory? = FileTest.directory?(@path)
    def mode = puts("mode: Not Implemented") # steep:ignore MethodBodyTypeMismatch
    def mode_str = puts("mode_str: Not Implemented") # steep:ignore MethodBodyTypeMismatch
    def writable? = puts("writable?: Not Implemented") # steep:ignore MethodBodyTypeMismatch
    def mtime = File.open(@path){|f| f.mtime}
    def birthtime = puts("birthtime: Not Implemented") # steep:ignore MethodBodyTypeMismatch
    def size = FileTest.size(@path)
  end

end
