class VFS

  VOLUMES = Array.new

  class << self
    def mount(driver, mountpoint)
      mountpoint << "/" unless mountpoint.end_with?("/")
      if volume_index(mountpoint)
        raise RuntimeError.new "Mountpoint `#{mountpoint}` already exists"
      end
      unless mountpoint[0] == '/'
        raise ArgumentError.new "Mountpoint must start with `/`"
      end
      driver.mount(mountpoint) # It raises if error
      VOLUMES << { driver: driver, mountpoint: mountpoint }
    end

    def unmount(driver)
      mountpoint = driver.mountpoint
      mountpoint << "/" unless mountpoint.end_with?("/")
      unless index = volume_index(mountpoint)
        raise "Mountpoint `#{mountpoint}` doesn't exist"
      end
      if OS::ENV["PWD"].start_with?(mountpoint)
        raise "Can't unmount where you are"
      end
      driver.unmount
      VOLUMES.delete_at index
      if VOLUMES.empty?
        OS::ENV["PWD"] = nil
      end
    end

    def chdir(dir)
      sanitized_path = VFS.sanitize(dir)
      volume, _path = VFS.split(sanitized_path)
      volume[:driver]._chdir(_path)
      OS::ENV["PWD"] = sanitized_path
    end

    def pwd
      OS::ENV["PWD"]
    end

    def mkdir(path, mode = 0777)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]._mkdir(_path, mode)
    end

    def unlink(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]._unlink(_path)
    end

    def exist?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]._exist?(_path)
    end

    def directory?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]._directory?(_path)
    end

    # private

    def sanitize_and_split(path)
      split(sanitize path)
    end

    def sanitize(path)
      dirs = case path
      when "/"
        [""]
      when ""
        return OS::ENV["HOME"]
      else
        path.split("/")
      end
      if dirs[0] != "" # path.start_with?("/")
        # Relative path
        dirs = OS::ENV["PWD"].split("/") + dirs
      end
      sanitized_dirs = []
      dirs.each do |dir|
        next if dir == "." || dir == ""
        if dir == ".."
          sanitized_dirs.pop
        else
          sanitized_dirs << dir
        end
      end
      "/#{sanitized_dirs.join("/")}"
    end

    def split(sanitized_path)
      volume = nil
      VOLUMES.each do |v|
        if sanitized_path.start_with?(v[:mountpoint])
          if volume
            if volume[:mountpoint].length < v[:mountpoint].length
              volume = v
            end
          else
            volume = v
          end
        end
      end
      unless volume
        raise RuntimeError.new("No mounted volume found")
      end
      [volume, sanitized_path[volume[:mountpoint].length, 255]]
    end

    def volume_index(mountpoint)
      # mruby/c doesn't have Array#any?
      # also, mruby/c's Array#index doesn't take block argument
      VOLUMES.each_with_index do |v, i|
        return i if v[:mountpoint] == mountpoint
      end
      nil
    end

  end

  class Dir
    def self.open(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].open_dir(_path)
    end
  end

  class File
    def self.open(path, mode)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].open_file(_path, mode)
    end
  end

end
