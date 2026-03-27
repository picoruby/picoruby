class VFS

  VOLUMES = Array.new

  class << self
    def mount(driver, mountpoint)
      if volume_index(mountpoint)
        raise RuntimeError.new "Mountpoint `#{mountpoint}` already exists"
      end
      unless mountpoint[0] == '/'
        raise ArgumentError.new "Mountpoint must start with `/`"
      end
      driver.mount(mountpoint) # It raises if error
      VOLUMES << { driver: driver, mountpoint: mountpoint }
      ENV["PWD"] = mountpoint if ENV["PWD"]&.empty?
    end

    def unmount(driver, force = false)
      mountpoint = driver.mountpoint
      mountpoint << "/" unless mountpoint.end_with?("/")
      unless index = volume_index(mountpoint)
        raise "Mountpoint `#{mountpoint}` doesn't exist"
      end
      if !force && ENV["PWD"]&.start_with?(mountpoint)
        raise "Can't unmount where you are"
      end
      driver.unmount
      VOLUMES.delete_at index
      if VOLUMES.empty?
        ENV["PWD"] = ""
      end
    end

    def chdir(dir)
      sanitized_path = VFS.sanitize(dir)
      volume, path = VFS.split(sanitized_path)
      if volume[:driver]&.chdir(path)
        index = 1
        while [".", "/"].include?(sanitized_path[index].to_s)
          index += 1
        end
        ENV["PWD"] = sanitized_path[index - 1, sanitized_path.length].to_s
      else
        print "No such directory: #{dir}"
      end
      return 0
    end

    def pwd
      if path = ENV["PWD"]
        return path
      elsif path = ENV_DEFAULT_HOME
        ENV["PWD"] = path
        return path
      else
        # This should not happen
        raise "No current working directory"
      end
    end

    def mkdir(path, mode = 0777)
      volume, path = VFS.sanitize_and_split(path)
      volume[:driver]&.mkdir(path, mode)
      0
    end

    def unlink(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].unlink(_path)
    end

    def rename(from, to)
      unless exist?(from)
        puts "No such file or directory: #{from}"
        return 0
      end
      if exist?(to)
        puts "File exists: #{to}. Can't overwrite"
        return 0
      end
      volume_from, _from = VFS.sanitize_and_split(from)
      volume_to, _to = VFS.sanitize_and_split(to)
      if volume_from != volume_to
        raise "Can't rename across volumes"
      end
      volume_from[:driver].rename(_from, _to)
    end

    def chmod(mode, path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].chmod(mode, _path)
    end

    def exist?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].exist?(_path)
    end

    def directory?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].directory?(_path)
    end

    def contiguous?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].contiguous?(_path)
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
        return ENV["HOME"].to_s
      else
        path.split("/")
      end
      if dirs[0] != "" # path.start_with?("/")
        # Relative path
        dirs = (ENV["PWD"] || "").split("/") + dirs
      end
      sanitized_dirs = [] #: Array[String]
      prefix_dirs = [] #: Array[String]
      i = 0
      while i < dirs.size
        dir = dirs[i]
        unless dir == "." || dir == ""
          if dir == ".."
            if sanitized_dirs.empty?
              prefix_dirs << ".."
            else
              sanitized_dirs.pop
            end
          else
            sanitized_dirs << dir
          end
        end
        i += 1
      end
      "#{prefix_dirs.join("/")}/#{sanitized_dirs.join("/")}"
    end

    def split(sanitized_path)
      volume = nil
      best_len = -1
      i = 0
      while i < VOLUMES.size
        v = VOLUMES[i]
        if sanitized_path.start_with?(v[:mountpoint]) && best_len < v[:mountpoint].length
          volume = v
          best_len = v[:mountpoint].length
        end
        i += 1
      end
      if volume
        # @type var volume: volume_t
        cut = volume[:mountpoint] == "/" ? 0 : 1
        [volume, "/#{sanitized_path[volume[:mountpoint].length + cut, 255]}"]
      else
        [VOLUMES[0], sanitized_path]
      end
    end

    def volume_index(mountpoint)
      i = 0
      while i < VOLUMES.size
        return i if VOLUMES[i][:mountpoint] == mountpoint
        i += 1
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

    def self.utime(atime, mtime, *filenames)
      count = 0
      i = 0
      while i < filenames.size
        volume, path = VFS.sanitize_and_split(filenames[i])
        count += volume[:driver].utime(atime, mtime, path)
        i += 1
      end
      return count
    end
  end

end
