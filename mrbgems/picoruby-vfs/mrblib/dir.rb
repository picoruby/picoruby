class Dir
  class << self
    def open(path, encoding: nil)
      if block_given?
        begin
          dir = self.new(path)
          res = yield dir
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

    def glob(pattern, flags = 0, base: "")
      if block_given?
        nil
      else
        ary = []
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
      VFS.directory?(path)
    end

    def empty?(path)
      dir = self.open(path)
      res = dir.read
      dir.close
      !res
    end
    alias zero? empty?

    def chdir(path = "")
      # block_given? ? object : 0
      _pwd = pwd
      if block_given?
        VFS.chdir(path)
        result = yield("")
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

  def initialize(path, encoding: nil)
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
    block ? self : self.each # For steep check
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

end
