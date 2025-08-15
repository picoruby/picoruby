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

  def printf(*args)
    $stdout.printf(*args)
  end

end

class Object
  include Kernel
end
