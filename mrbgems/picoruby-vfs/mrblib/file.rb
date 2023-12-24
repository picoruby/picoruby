require "time"

class File

  class Stat
    def initialize(path)
      volume, _path = VFS.sanitize_and_split(path)
      @stat = volume[:driver].class::Stat.new(volume[:driver].prefix, _path)
    end
    def directory? = @stat.directory?
    def mode = @stat.mode
    def mode_str = @stat.mode_str
    def writable? = @stat.writable?
    def mtime = @stat.mtime
    def birthtime = @stat.birthtime
    def size = @stat.size
  end

  CHUNK_SIZE = 127

  class << self
    def expand_path(path, default_path = '.')
      if path.start_with?("/")
        VFS.sanitize path
      else
        VFS.sanitize "#{default_path}/#{path}"
      end
    end

    def chmod(mode, *paths)
      count = 0
      paths.each do |path|
        count += 1 if VFS.chmod(mode, path) == 0
      end
      count
    end

    def open(path, mode = "r")
      if block_given?
        file = self.new(path, mode)
        yield file
        file.close
      else
        self.new(path, mode)
      end
    end

    def exist?(name)
      VFS.exist?(name)
    end

    def directory?(name)
      VFS.directory?(name)
    end

    def file?(name)
      VFS.exist?(name) && !VFS.directory?(name)
    end

    def unlink(*filenames)
      count = 0
      filenames.each do |name|
        count += VFS.unlink(name)
      end
      return count
    end

    def rename(from, to)
      VFS.rename(from, to)
    end

    def utime(atime, mtime, *filename)
      VFS::File.utime(atime, mtime, *filename)
    end
  end

  def initialize(path, mode = "r")
    @path = path
    @file = VFS::File.open(path, mode)
  end

  attr_reader :path

  def tell
    @file.tell
  end
  alias pos tell

  SEEK_SET = 0
  SEEK_CUR = 1
  SEEK_END = 2
  def seek(pos, whence = SEEK_SET)
    @file.seek(pos, whence)
  end
  alias pos= seek

  def rewind
    @file.seek(0)
  end

  def each_line(&block)
    while line = gets do
      block.call line
    end
  end

  # TODO: get(limit, chomp: false) when PicoRuby implements kargs
  def gets(*args)
    case args.count
    when 0
      rs = "\n"
      limit = nil
    when 1
      if args[0].is_a?(Integer)
        rs = "\n"
        limit = args[0]
      else
        rs = args[0]
        limit = nil
      end
    when 2
      rs = args[0].to_s
      limit = args[1].to_i
    else
      raise ArgumentError.new("wrong number of arguments (expected 0..2)")
    end
    result = ""
    chunk_size = CHUNK_SIZE
    initial_pos = self.tell
    rs = "" unless rs
    rs_adjust = rs.length - 1
    if limit
      (limit / chunk_size).times do
        if chunk = @file.read(limit)
          result << chunk
        end
      end
      if (chunk = @file.read(limit % chunk_size))
        result << chunk
      end
      if pos = result.index(rs)
        result = result[0, pos + 1 + rs_adjust]
      end
    else
      index_at = 0
      while chunk = @file.read(chunk_size) do
        result << chunk
        if pos = result.index(rs, [index_at - rs_adjust, 0].max || 0)
          result = result[0, pos + 1 + rs_adjust] || ""
          break
        end
        index_at += chunk_size
      end
    end
    if result&.length == 0
      return nil
    else
      self.seek(initial_pos + (result&.length || 0))
      return result
    end
  end

  def eof?
    @file.eof?
  end

  def read(length = nil, outbuf = "")
    if length && length < 0
      raise ArgumentError.new("negative length #{length} given")
    end
    if length.is_a?(Integer)
      outbuf << @file.read(length)
    elsif length.nil?
      while buff = @file.read(1024)
        outbuf << buff
      end
    else
      # ????
    end
    if 0 == outbuf.length
      (length.nil? || length == 0) ? "" : nil
    else
      outbuf
    end
  end

  def write(*args)
    len = 0
    args.each do |arg|
      len += @file.write(arg.to_s)
    end
    return len
  end

  def puts(*lines)
    lines.each do |line|
      @file.write line
      @file.write "\n"
    end
    return nil
  end

  def putc(ch)
    case ch.class
    when Integer
      # @type var ch: Integer
      @file.write ch.chr
    when String
      # @type var ch: String
      @file.write ch[0]
    else
      raise ArgumentError
    end
    return ch
  end

  def printf(farmat, *args)
    puts sprintf(farmat, *args)
    return nil
  end

  def close
    @file.close
  end

  def size
    @file.size
  end

  def expand(size)
    @file.expand(size)
  end

  def fsync
    @file.fsync
  end

end
