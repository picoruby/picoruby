require 'io/console'

module Kernel

  private

  def print(*args)
    $stdout.print(*args)
  end

  def puts(*args)
    $stdout.puts(*args)
  end

  # On POSIX, the IO library (mruby-io / posix-io) reads fd 0 directly and is
  # not signal-aware, so a blocking bareword `gets`/`getc` cannot be
  # interrupted by Ctrl-C. When stdin is the terminal, route through the HAL
  # ring buffer instead (the stdin reader thread turns Ctrl-C into a
  # pseudo-SIGINT, mirroring the microcontroller's UART RX interrupt). When
  # $stdin has been redirected to a file (e.g. `cmd < file`), or on the
  # microcontroller (where $stdin.gets already uses the HAL), fall back to
  # $stdin so redirection keeps working. Machine.posix? is checked first so
  # the MCU never calls IO#fileno (which it does not define).
  def getc
    if Machine.posix? && $stdin.fileno == 0
      Machine._stdin_getc
    else
      $stdin.getc
    end
  end

  def gets # steep:ignore MethodArityMismatch
    if Machine.posix? && $stdin.fileno == 0
      Machine._stdin_gets
    else
      $stdin.gets
    end
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
