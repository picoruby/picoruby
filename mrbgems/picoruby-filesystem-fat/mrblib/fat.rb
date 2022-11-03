class FAT

  class Dir
  end

  class File
  end

  def self._setup(num)
    drive = case num
    when 0
      :ram
    when 1
      :flash
    end
    fat = FAT.new(drive)
    retry_count = 0
    begin
      VFS.mount(fat, "/")
    rescue => e
      puts e.message
      fat.mkfs
      retry_count += 1
      retry if retry_count == 1
      raise e
    end
    File.open("test.txt", "w") do |f|
      f.puts "hello"
      f.puts "world"
    end
  end

  # drive can be "0".."9", :ram, :flash, etc
  # The name is case-insensitive
  def initialize(drive = "0")
    @prefix = "#{drive}:"
  end

  attr_reader :mountpoint

  def mkfs
    self._mkfs(@prefix)
    self
  end

  def sector_count
    res = self._getfree(@prefix)
    {total: (res >> 16), free: (res & 0b1111111111111111) }
  end

  def mount(mountpoint)
    @mountpoint = mountpoint
    @fatfs = self._mount("#{@prefix}#{mountpoint}")
  end

  def unmount
    self._unmount(@prefix)
    @fatfs = nil
  end

  def open_dir(path)
    FAT::Dir.new("#{@prefix}#{path}")
  end

  def open_file(path, mode)
    FAT::File.new("#{@prefix}#{path}", mode)
  end

  def chdir(path)
    # FatFs where FF_STR_VOLUME_ID == 2 configured
    # calls f_chdrive internally in f_chdir.
    # This is the reason of passing also @prefix
    FAT._chdir("#{@prefix}#{path}")
  end

  def mkdir(path, mode)
    FAT._mkdir("#{@prefix}#{path}", mode)
  end
end
