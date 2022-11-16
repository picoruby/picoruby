class Dir
  class << self
    def open(path)
      if block_given?
        begin
          dir = self.new(path)
          yield dir
        rescue => e
          puts e.message
        ensure
          dir.close
        end
      else
        self.new(path)
      end
    end

    def glob(pattern, flags = 0, base: nil, sort: true)
      # block_given? ? nil : [String]
      self.open(ENV['PWD']) do |dir|
        dir.pat = pattern
        while entry = dir.findnext
          puts entry
        end
      end
    end

    def exist?(path)
      VFS.directory?(path)
    end

    def empty?(path)
      dir = self.open(path)
      res = dir.read
      dir.close
      !res
    end
    alias zero? empty?

    def chdir(path)
      # block_given? ? object : 0
      _pwd = pwd
      if block_given?
        VFS.chdir(path)
        result = yield
        VFS.chdir(_pwd)
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
      VFS.mkdir(path, mode)
    end

    def unlink(path)
      VFS.unlink(path)
    end
    alias delete unlink
    alias rmdir unlink
  end

  def initialize(path)
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
      block.call(filename)
    end
  end

  def pat=(pattern)
    @dir.pat = pattern
  end

  def findnext
    @dir.findnext
  end

  def read
    @dir.read
  end

  def rewind
    @dir.rewind
    self
  end

  def stat_label
    @dir.stat_label
  end

end
