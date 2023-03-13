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
      ENV["PWD"] ||= mountpoint
    end

    def unmount(driver, force = false)
      mountpoint = driver.mountpoint
      mountpoint << "/" unless mountpoint.end_with?("/")
      unless index = volume_index(mountpoint)
        raise "Mountpoint `#{mountpoint}` doesn't exist"
      end
      if !force && ENV["PWD"].to_s.start_with?(mountpoint)
        raise "Can't unmount where you are"
      end
      driver.unmount
      VOLUMES.delete_at index
      if VOLUMES.empty?
        ENV["PWD"] = nil
      end
    end

    def chdir(dir)
      sanitized_path = VFS.sanitize(dir)
      volume, path = VFS.split(sanitized_path)
      if volume[:driver]&.chdir(path)
        index = 1
        while [".", "/"].include?(sanitized_path[index])
          index += 1
        end
        ENV["PWD"] = sanitized_path[index - 1, sanitized_path.length]
      else
        print "No such directory: #{dir}"
      end
      return 0
    end

    def pwd
      ENV["PWD"].to_s
    end

    def mkdir(path, mode = 0777)
      volume, path = VFS.sanitize_and_split(path)
      volume[:driver]&.mkdir(path, mode)
    end

    def unlink(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]&.unlink(_path)
    end

    def chmod(mode, path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver].chmod(mode, _path)
    end

    def stat(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]&.stat(_path)
    end

    def exist?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]&.exist?(_path)
    end

    def directory?(path)
      volume, _path = VFS.sanitize_and_split(path)
      volume[:driver]&.directory?(_path)
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
        dirs = ENV["PWD"].to_s.split("/") + dirs
      end
      sanitized_dirs = []
      prefix_dirs = []
      dirs.each do |dir|
        next if dir == "." || dir == ""
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
      "#{prefix_dirs.join("/")}/#{sanitized_dirs.join("/")}"
    end

    def split(sanitized_path)
      found = false
      volume = VOLUMES.map { |v|
        sanitized_path.start_with?(v[:mountpoint]) ? v : nil
      }.max {|v| v ? v[:mountpoint].length : -1}
      if volume
        [volume, "/#{sanitized_path[volume[:mountpoint].length, 255]}"]
      else
        [VOLUMES[0], sanitized_path] # fallback
      end
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
