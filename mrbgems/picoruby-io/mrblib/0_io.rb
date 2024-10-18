##
# IO
#
# ISO 15.2.20

require "metaprog"

class EOFError < IOError; end

class IO

  include Enumerable

  def self.open(*args, &block)
    io = self.new(*args)

    return io unless block

    begin
      yield io
    ensure
      begin
        io.close unless io.closed?
      rescue StandardError
      end
    end
  end

  def self.popen(command, mode = 'r', **opts, &block)
    if !self.respond_to?(:_popen)
      raise NotImplementedError, "popen is not supported on this platform"
    end
    opt_in  = opts[:in]  || 0
    opt_out = opts[:out] || 1
    opt_err = opts[:err] || 2
    io = self._popen(command, mode, opt_in, opt_out, opt_err)
    return io unless block

    begin
      yield io
    ensure
      begin
        io.close unless io.closed?
      rescue IOError
        # nothing
      end
    end
  end

  def self.pipe(&block)
    if !self.respond_to?(:_pipe)
      raise NotImplementedError, "pipe is not supported on this platform"
    end
    if block
      begin
        r, w = _pipe
        yield r, w
      ensure
        r.close unless r.closed?
        w.close unless w.closed?
      end
    else
      _pipe
    end
  end

  def self.read(path, length=nil, offset=0, mode: "r")
    str = ""
    fd = -1
    #io = nil
    begin
      fd = IO.sysopen(path, mode)
      io = IO.open(fd, mode)
      io.seek(offset) if offset > 0
      str = io.read(length)
    ensure
      if io
        io.close
      elsif fd != -1
        _sysclose(fd)
      end
    end
    str
  end

  def hash
    # We must define IO#hash here because IO includes Enumerable and
    # Enumerable#hash will call IO#read() otherwise
    self.__id__
  end

  def <<(str)
    write(str)
    self
  end

  def pos=(i)
    seek(i, SEEK_SET)
  end

  def rewind
    seek(0, SEEK_SET)
  end

  def ungetbyte(c)
    if c.is_a? String
      c = c.getbyte(0)
    else
      c &= 0xff
    end
    s = " "
    if c.nil?
      raise ArgumentError, "ungetbyte: nil byte"
    end
    ungetc c.chr
  end

  # 15.2.20.5.3
  def each(&block)
    unless block
      raise ArgumentError, "block not supplied"
    end

    while line = self.gets
      block.call(line)
    end
    self
  end

  # 15.2.20.5.4
  def each_byte(&block)
    unless block
      raise ArgumentError, "block not supplied"
    end

    while byte = self.getbyte
      block.call(byte)
    end
    self
  end

  # 15.2.20.5.5
  alias each_line each

  def each_char(&block)
    unless block
      raise ArgumentError, "block not supplied"
    end

    while char = self.getc
      block.call(char)
    end
    self
  end

  def puts(*args)
    i = 0
    len = args.size
    while i < len
      s = args[i]
      if s.is_a?(Array)
        puts(*s)
      else
        s = s.to_s
        write s
        write "\n" if (s[-1] != "\n")
      end
      i += 1
    end
    write "\n" if len == 0
    nil
  end

  def print(*args)
    i = 0
    len = args.size
    while i < len
      write args[i].to_s
      i += 1
    end
  end

  def printf(*args)
    write sprintf(*args)
    nil
  end

end

STDIN  = IO.open(0, "r")
STDOUT = IO.open(1, "w")
STDERR = IO.open(2, "w")

$stdin  = STDIN
$stdout = STDOUT
$stderr = STDERR

