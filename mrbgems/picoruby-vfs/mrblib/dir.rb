class Dir
  class << self
    def open(path, encoding: nil)
      if block_given?
        begin
          dir = self.new(path)
          # steep bug: `class << self` causes `instance` type to resolve to `singleton(Dir)` instead of `Dir`
          res = yield dir # steep:ignore ArgumentTypeMismatch
        rescue => e
          puts e.message
        ensure
          dir&.close # dir becomes nil, why?????
        end
        res
      else
        self.new(path)
      end
    end

    def glob(pattern, flags = 0, base: "", sort: true)
      if block_given?
        nil
      else
        ary = [] #: Array[String]
        pattern = [pattern].flatten
        self.open(ENV['PWD'].to_s) do |dir|
          pattern.each do |pat|
            dir.pat = pat
            while entry = dir.findnext
              ary << entry
            end
          end
        end
        ary
      end
    end

    def exist?(path)
      # @type var path: String
      VFS.directory?(path)
    end

    def empty?(path)
      # @type var path: String
      dir = self.open(path)
      res = dir.read
      dir.close
      !res
    end
    alias zero? empty?

    def chdir(path = "")
      # @type var path: String
      # block_given? ? object : 0
      _pwd = pwd
      if block_given?
        VFS.chdir(path)
        result = yield("")
        VFS.chdir(_pwd) if _pwd
        result
      else
        VFS.chdir(path)
      end
    end

    def pwd
      VFS.pwd
    end
    alias getwd pwd

    def mkdir(path, mode = 0777)
      # @type var path: String
      # @type var mode: Integer
      VFS.mkdir(path, mode)
    end

    def unlink(path)
      # @type var path: String
      VFS.unlink(path)
    end
    alias delete unlink
    alias rmdir unlink
  end

  def initialize(path, encoding: nil)
    # @type var path: String
    @dir = VFS::Dir.open(path)
  end

  def path
    @dir.fullpath
  end

  def close
    @dir.close
  end

  def each(&block)
    while filename = self.read do
      block&.call(filename)
    end
    self
  end

  def read
    @dir.read
  end

  def rewind
    @dir.rewind
    self
  end

  def pat=(pattern)
    @dir.pat = pattern
  end

  def findnext
    @dir.findnext
  end

end
