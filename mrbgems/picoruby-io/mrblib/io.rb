
class EOFError < IOError; end

class IO
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
end
