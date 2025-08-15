module Kernel

  private

  def print(*args)
    $stdout.print(*args)
  end

  def puts(*args)
    $stdout.puts(*args)
  end

  def getc
    $stdin.getc
  end

  def gets
    $stdin.gets
  end

  def p(*args)
    $stdout.p(*args)
  end
end

STDIN  = IO.open(0, "r")
STDOUT = IO.open(1, "w")
#STDERR = IO.open(2, "w")

$stdin = STDIN
$stdout = STDOUT
#$stderr = STDERR
