require "time"

class Littlefs
  class VFSMethods
  end

  # littlefs type constants (from lfs.h)
  TYPE_REG = 0x001
  TYPE_DIR = 0x002

  class Stat

    LABEL = "size      datetime                   name"

    def initialize(prefix, path)
      @stat_hash = if path == "/"
        { mode: TYPE_DIR }
      else
        _stat("#{prefix}#{path}")
      end
    end

    def mode
      @stat_hash[:mode]
    end

    def writable?
      true
    end

    def mtime
      @mtime ||= Time.at(@stat_hash[:unixtime] || 0)
    end

    def birthtime
      raise "NotImplementedError"
    end

    def size
      @stat_hash[:size] || 0
    end

    def directory?
      0 < mode & TYPE_DIR
    end
  end

  class Dir
  end

  class File
  end

  def initialize(device, label: "PICORUBY", driver: nil)
    @prefix = ""
    @label = label
  end

  attr_reader :mountpoint, :prefix

  def mkfs
    self._mkfs
    self
  end

  def setlabel
    0
  end

  def getlabel
    @label || ""
  end

  def sector_count
    res = self.getfree
    {total: (res >> 16), free: (res & 0b1111111111111111)}
  end

  def mount(mountpoint)
    @mountpoint = mountpoint
    self._mount
    0
  end

  def unmount
    self._unmount
    nil
  end

  def open_dir(path)
    Littlefs::Dir.new("#{@prefix}#{path}")
  end

  def open_file(path, mode)
    Littlefs::File.new("#{@prefix}#{path}", mode)
  end

  def chdir(path)
    path = "" if path == "/"
    if path == "" || _exist?("#{@prefix}#{path}")
      _chdir("#{@prefix}#{path}")
    else
      0
    end
  end

  def erase
    _erase
  end

  def utime(atime, _mtime, path)
    _utime(atime.to_i, "#{@prefix}#{path}")
  end

  def mkdir(path, mode = TYPE_DIR)
    _mkdir("#{@prefix}#{path}", mode)
  end

  def chmod(mode, path)
    _chmod(mode, "#{@prefix}#{path}")
  end

  def exist?(path)
    _exist?("#{@prefix}#{path}")
  end

  def unlink(path)
    _unlink("#{@prefix}#{path}")
  end

  def rename(from, to)
    _rename("#{@prefix}#{from}", "#{@prefix}#{to}")
  end

  def directory?(path)
    return true if path == "/"
    _directory?("#{@prefix}#{path}")
  end

  def contiguous?(path)
    false
  end
end
