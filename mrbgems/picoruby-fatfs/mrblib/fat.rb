class FAT
  def initialize(drive = "0") # "0".."9"
    @prefix = "#{drive}:"
  end

  attr_reader :mountpoint

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

  class Dir
  end

  class File
  end
end
