require 'io/console'

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

  def gets # steep:ignore MethodArityMismatch
    $stdin.gets
  end

  def p(*args)
    for e in args
      $stdout.write e.inspect
      $stdout.write "\n"
    end
    len = args.size
    return nil if len == 0
    return args[0] if len == 1 # steep:ignore ReturnTypeMismatch
    args
  end

  def exit(status = 0) # steep:ignore MethodBodyTypeMismatch
    # @type var status: Integer
    Machine.exit(status) # steep:ignore ArgumentTypeMismatch
  end
end

STDIN  = IO.open(0, "r")
STDOUT = IO.open(1, "w")
STDERR = IO.open(2, "w")

$stdin = STDIN
$stdout = STDOUT
$stderr = STDERR
