module Kernel
  def `(cmd)
    IO.popen(cmd) { |io| io.read }
  end

  def open(file, mode, &block)
    raise ArgumentError unless file.is_a?(String)

    if file[0] == "|"
      IO.popen(file[1, file.size - 1].to_s, mode, &block)
    else
      File.open(file, mode, &block)
    end
  end

  def print(*args)
    $stdout.print(*args)
  end

  def puts(*args)
    $stdout.puts(*args)
  end

  def printf(*args)
    $stdout.printf(*args)
  end

  def gets(rs = nil)
    $stdin.gets(rs)
  end

  def p(*args)
    case args.size
    when 0
      return nil
    when 1
      $stdout.puts args[0].inspect
      args[0]
    else
      args.each do|arg|
        $stdout.puts arg.inspect
      end
      args
    end
  end

end

class Object
  include Kernel
end
