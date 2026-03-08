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
      # @type var path: String
      if path.start_with?("/")
        VFS.sanitize path
      else
        VFS.sanitize "#{default_path}/#{path}"
      end
    end

    def chmod(mode, *paths)
      # @type var mode: Integer
      count = 0
      paths.each do |path|
        # @type var path: String
        count += 1 if VFS.chmod(mode, path) == 0
      end
      count
    end

    def open(path, mode = "r", perm = 0666, &block)
      # @type var path: String
      # @type var mode: String
      if block
        file = self.new(path, mode)
        result = block.call(file)
        file.close
        result
      else
        self.new(path, mode)
      end
    end

    def exist?(name)
      # @type var name: String
      VFS.exist?(name)
    end

    def directory?(name)
      # @type var name: String
      VFS.directory?(name)
    end

    def file?(name)
      # @type var name: String
      VFS.exist?(name) && !VFS.directory?(name)
    end

    def unlink(*filenames)
      count = 0
      filenames.each do |name|
        # @type var name: String
        count += VFS.unlink(name)
      end
      return count
    end

    def rename(from, to)
      # @type var from: String
      # @type var to: String
      VFS.rename(from, to)
    end

    def utime(atime, mtime, *filename)
      # @type var atime: Time
      # @type var mtime: Time
      # @type var filename: Array[String]
      VFS::File.utime(atime, mtime, *filename)
    end

    def contiguous?(path)
      # @type var path: String
      VFS.contiguous?(path)
    end

    # Alternative to File.read
    def load_file(path, length = nil, offset = nil)
      # @type var path: String
      File.open(path) do |f| # steep:ignore BlockBodyTypeMismatch
        f.seek(offset) if offset
        f.read(length)
      end
    end
  end

  def initialize(path, mode = "r", perm = 0666)
    # @type var path: String
    # @type var mode: String
    @path = path
    @file = VFS::File.open(path, mode)
  end

  attr_reader :path

  def sector_size
    @file.sector_size
  end

  def physical_address
    @file.physical_address
  end

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

  def each_line(sep = $/, limit = nil, chomp: false)
    while line = gets do
      yield line
    end
  end

  def gets(sep = $/, limit = nil, chomp: false)
    # @type var sep: String | nil
    # @type var limit: Integer | nil
    if sep.is_a?(Integer)
      limit = sep
      rs = "\n"
    else
      rs = sep || "\n"
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
    if result&.length == 0 || result.nil?
      return nil
    else
      self.seek(initial_pos + (result&.length || 0))
      return chomp ? result.chomp(rs) : result
    end
  end

  def eof?
    @file.eof?
  end

  def read(length = nil, outbuf = "")
    # @type var length: Integer | nil
    # @type var outbuf: String
    if length && length < 0
      raise ArgumentError.new("negative length #{length} given")
    end
    if length.is_a?(Integer)
      outbuf << @file.read(length).to_s
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

  def getbyte
    @file.getbyte
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
    # @type var ch: String | Integer
    case ch
    when Integer
      @file.write ch.chr
    when String
      @file.write ch[0].to_s
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
    @file.expand(size) if @file.respond_to? :expand
    size
  end

  def fsync
    @file.fsync
  end

end
