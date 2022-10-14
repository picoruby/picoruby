class OS
  class File

    CHUNK_SIZE = 127

    class << self
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
        !VFS.directory?(name)
      end

      def unlink(*filenames)
        count = 0
        filenames.each do |name|
          count += VFS.unlink(name)
        end
        return count
      end
    end

    def initialize(path, mode = "r")
      @file = VFS::File.open(path, mode)
    end

    def tell
      @file.tell
    end
    alias pos tell

    # TODO seek(pos, whence = OS::SEEK_SET)
    def seek(pos)
      @file.seek(pos)
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
        raies ArgumentError.new("wrong number of arguments (expected 0..2)")
      end
      result = ""
      chunk_size = CHUNK_SIZE
      initial_pos = self.tell
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
          if pos = result.index(rs, [index_at - rs_adjust, 0].max)
            result = result[0, pos + 1 + rs_adjust]
            break
          end
          index_at += chunk_size
        end
      end
      if result.length == 0
        return nil
      else
        self.seek(initial_pos + result.length)
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
        (length / 255).times do
          buff = @file.read(255)
          buff ? (outbuf << buff) : break
        end
        outbuf << buff if buff = @file.read(length % 255)
      elsif length.nil?
        while buff = @file.read(255)
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
        len += @file.write(arg)
      end
      return len
    end

    def puts(*lines)
      lines.each do |line|
        @file.write line
        if @feed == :crlf
          @file.write "\r"
        end
        @file.write "\n"
      end
      return nil
    end

    def putc(ch)
      case ch.class
      when Integer
        @file.write ch.chr
      when String
        @file.write ch[0]
      else
        @file.write ch.to_i.chr
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
  end
end
