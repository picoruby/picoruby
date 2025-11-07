# puts and print is written in C for MicroRuby
if RUBY_ENGINE == 'mruby/c'
  class IO
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
  end
end
